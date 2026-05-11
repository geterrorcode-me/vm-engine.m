#include <jni.h>
#include <android/log.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <gui/Surface.h>
#include <gui/SurfaceControl.h>
#include <ui/GraphicBuffer.h>
#include <sys/mman.h>
#include <linux/ashmem.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <dlfcn.h>
#include "shadowhook.h"

#define LOG_TAG "vMeer_Graphics"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

using namespace android;

// =============================================================================
// BAGIAN 1: INTERNAL GRAPHICS SYMBOLS
// =============================================================================
typedef void (*p_set_layer_t)(void* surface_control, uint32_t layer);
static p_set_layer_t orig_set_layer = nullptr;

typedef int (*p_dequeue_buffer_t)(void* base, ANativeWindowBuffer** buffer, int* fenceFd);
static p_dequeue_buffer_t orig_dequeue = nullptr;

void init_graphics_internals() {
    void* h_gui = dlopen("libgui.so", RTLD_NOW);
    if (h_gui) {
        // Simbol untuk mengatur urutan layer (Z-Order)
        orig_set_layer = (p_set_layer_t)dlsym(h_gui, "_ZN7android14SurfaceControl8setLayerEi");
        LOGI("vMeer: Graphics internal symbols initialized.");
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
        ioctl(fd, ASHMEM_SET_NAME, "vmeer_graphics_buffer");
        ioctl(fd, ASHMEM_SET_SIZE, size);
        return fd; 
    }
};

// =============================================================================
// BAGIAN 3: HOOK HANDLERS
// =============================================================================

// Handler untuk DequeueBuffer (Mencegat alokasi frame)
int hook_dequeue_buffer(void* base, ANativeWindowBuffer** buffer, int* fenceFd) {
    int res = orig_dequeue(base, buffer, fenceFd);
    if (res == 0 && buffer != nullptr) {
        // Di sini kita bisa memantau buffer yang diberikan ke aplikasi
        // LOGI("vMeer: Frame Buffer Dequeued, ready for rendering.");
    }
    return res;
}

// Handler untuk SurfaceControl Constructor
static void* (*orig_surface_ctrl_ctor)(void*, void*, void*, uint32_t, uint32_t, int, uint32_t, void*) = nullptr;

void* hook_create_surface(void* inst, void* client, void* name, uint32_t w, uint32_t h, int fmt, uint32_t flg, void* meta) {
    LOGI("vMeer: Intercepting Surface Creation: %ux%u", w, h);
    // Kembalikan ke constructor asli (harus di-cast sesuai signature)
    return inst; 
}

// =============================================================================
// BAGIAN 4: JNI ENTRY & STABILIZATION (FIXED)
// =============================================================================

extern "C" JNIEXPORT void JNICALL Java_com_vmeer_io_GraphicsEngine_nativeInit(JNIEnv* env, jobject thiz) {
    LOGI("vMeer: Maturing Graphics Pipeline [START]");

    // 1. Inisialisasi Simbol
    init_graphics_internals();

    // 2. Inisialisasi ShadowHook (Jika belum diinit di engine utama)
    shadowhook_init(SHADOWHOOK_MODE_UNIQUE, false);

    // 3. Hook DequeueBuffer (Untuk stabilitas aliran frame)
    void* stub_dq = shadowhook_hook_sym_name(
        "libgui.so",
        "_ZN7android7Surface13dequeueBufferEPP19ANativeWindowBufferPi",
        (void*)hook_dequeue_buffer,
        (void**)&orig_dequeue
    );

    // 4. Hook SurfaceControl (Untuk kontrol tampilan/layer)
    void* stub_sc = shadowhook_hook_sym_name(
        "libgui.so",
        "_ZN7android14SurfaceControlC1ERKNS_2spINS_14SurfaceComposerClientEEERKNS_7String8EjjijPNS_11LayerMetadataE",
        (void*)hook_create_surface,
        nullptr
    );

    if (stub_dq && stub_sc) {
        LOGI("vMeer: Graphics Pipeline Matched & Stable.");
    } else {
        LOGE("vMeer: Partial hook failure. Check symbols!");
    }
}
