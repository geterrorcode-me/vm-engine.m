#include <jni.h>
#include <android/log.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <dlfcn.h>
#include <stdint.h>
#include "shadowhook.h"

// Kita hapus #include <gui/...> dan <ui/...> karena tidak ada di NDK publik.
// Sebagai gantinya, kita definisikan namespace dan class yang dibutuhkan secara manual.

#define LOG_TAG "vMeer_Graphics"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// =============================================================================
// BAGIAN 0: FORWARD DECLARATIONS & TYPES
// =============================================================================
namespace android {
    class Surface;
    class SurfaceControl;
    // Definisikan tipe untuk fence agar tidak butuh header ui/
}

// Definisikan struktur ashmem jika header linux/ashmem.h bermasalah di beberapa NDK
#ifndef ASHMEM_SET_NAME
#define ASHMEM_SET_NAME _IOW(__ASHEM_TYPE, 1, char[ASHMEM_NAME_LEN])
#define ASHMEM_SET_SIZE _IOW(__ASHEM_TYPE, 3, size_t)
#endif

// =============================================================================
// BAGIAN 1: INTERNAL GRAPHICS SYMBOLS
// =============================================================================
typedef void (*p_set_layer_t)(void* surface_control, uint32_t layer);
static p_set_layer_t orig_set_layer = nullptr;

// Signature dequeueBuffer disesuaikan dengan libgui.so versi terbaru
typedef int (*p_dequeue_buffer_t)(void* base, ANativeWindowBuffer** buffer, int* fenceFd);
static p_dequeue_buffer_t orig_dequeue = nullptr;

void init_graphics_internals() {
    void* h_gui = dlopen("libgui.so", RTLD_NOW);
    if (h_gui) {
        // Simbol untuk mengatur urutan layer (Z-Order)
        orig_set_layer = (p_set_layer_t)dlsym(h_gui, "_ZN7android14SurfaceControl8setLayerEi");
        LOGI("vMeer: Graphics internal symbols resolved via dlopen.");
    } else {
        LOGE("vMeer: Failed to load libgui.so");
    }
}

// =============================================================================
// BAGIAN 2: BUFFER SHARING LOGIC (Ashmem Bridge)
// =============================================================================
class BufferBridge {
public:
    static int createSharedBuffer(size_t size) {
        int fd = open("/dev/ashmem", O_RDWR);
        if (fd < 0) return -1;
        
        // Gunakan ioctl langsung untuk set name dan size
        ioctl(fd, ASHMEM_SET_NAME, "vmeer_graphics_buffer");
        ioctl(fd, ASHMEM_SET_SIZE, size);
        return fd; 
    }
};

// =============================================================================
// BAGIAN 3: HOOK HANDLERS
// =============================================================================

int hook_dequeue_buffer(void* base, ANativeWindowBuffer** buffer, int* fenceFd) {
    // Panggil original
    int res = orig_dequeue(base, buffer, fenceFd);
    
    if (res == 0 && buffer != nullptr) {
        // Keuntungan: Kita bisa memantau frame tanpa perlu include header internal
        // LOGI("vMeer: Frame Buffer Captured.");
    }
    return res;
}

void* hook_create_surface(void* inst, void* client, void* name, uint32_t w, uint32_t h, int fmt, uint32_t flg, void* meta) {
    LOGI("vMeer: Intercepting Surface Creation for: %ux%u", w, h);
    return inst; 
}

// =============================================================================
// BAGIAN 4: JNI ENTRY & STABILIZATION (FIXED LOG)
// =============================================================================

extern "C" JNIEXPORT void JNICALL Java_com_vmeer_io_GraphicsEngine_nativeInit(JNIEnv* env, jobject thiz) {
    // FIX: Gunakan %% untuk mencetak karakter persen agar tidak dianggap format specifier
    LOGI("vMeer: Maturing Graphics Pipeline [START]");

    init_graphics_internals();

    if (shadowhook_init(SHADOWHOOK_MODE_UNIQUE, false) != 0) {
        LOGE("vMeer: ShadowHook init failed in Graphics Module");
    }

    // 1. Hook DequeueBuffer (libgui.so)
    void* stub_dq = shadowhook_hook_sym_name(
        "libgui.so",
        "_ZN7android7Surface13dequeueBufferEPP19ANativeWindowBufferPi",
        (void*)hook_dequeue_buffer,
        (void**)&orig_dequeue
    );

    // 2. Hook SurfaceControl Constructor
    void* stub_sc = shadowhook_hook_sym_name(
        "libgui.so",
        "_ZN7android14SurfaceControlC1ERKNS_2spINS_14SurfaceComposerClientEEERKNS_7String8EjjijPNS_11LayerMetadataE",
        (void*)hook_create_surface,
        nullptr
    );

    if (stub_dq && stub_sc) {
        LOGI("vMeer: Graphics Pipeline Matched & Stable (100%%)");
    } else {
        LOGE("vMeer: Partial hook failure. Symbols might be different on this Android version.");
    }
}
