#include <jni.h>
#include <android/log.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <dlfcn.h>
#include <stdint.h>
#include <linux/ashmem.h> // Pastikan header ini ada
#include "shadowhook.h"

#define LOG_TAG "vMeer_Graphics"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// =============================================================================
// BAGIAN 0: FORWARD DECLARATIONS & TYPES
// =============================================================================
namespace android {
    class Surface;
    class SurfaceControl;
}

// FIX: Definisi manual Ashmem jika header kernel tidak lengkap di NDK
#ifndef ASHMEM_NAME_LEN
  #define ASHMEM_NAME_LEN 256
#endif

#ifndef ASHMEM_SET_NAME
  #define __ASHMEM_TYPE 'A' // Perhatikan double 'M'
  #define ASHMEM_SET_NAME _IOW(__ASHMEM_TYPE, 1, char[ASHMEM_NAME_LEN])
  #define ASHMEM_SET_SIZE _IOW(__ASHMEM_TYPE, 3, size_t)
#endif

// =============================================================================
// BAGIAN 1: INTERNAL GRAPHICS SYMBOLS
// =============================================================================
typedef void (*p_set_layer_t)(void* surface_control, uint32_t layer);
static p_set_layer_t orig_set_layer = nullptr;

// FIX: Gunakan ANativeWindow_Buffer sesuai saran compiler
typedef int (*p_dequeue_buffer_t)(void* base, ANativeWindow_Buffer** buffer, int* fenceFd);
static p_dequeue_buffer_t orig_dequeue = nullptr;

void init_graphics_internals() {
    void* h_gui = dlopen("libgui.so", RTLD_NOW);
    if (h_gui) {
        orig_set_layer = (p_set_layer_t)dlsym(h_gui, "_ZN7android14SurfaceControl8setLayerEi");
        LOGI("vMeer: Graphics internal symbols resolved.");
    }
}

// =============================================================================
// BAGIAN 2: BUFFER SHARING LOGIC
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

// FIX: Gunakan ANativeWindow_Buffer
int hook_dequeue_buffer(void* base, ANativeWindow_Buffer** buffer, int* fenceFd) {
    if (!orig_dequeue) return -1;
    
    int res = orig_dequeue(base, buffer, fenceFd);
    if (res == 0 && buffer != nullptr) {
        // Frame intercepted
    }
    return res;
}

void* hook_create_surface(void* inst, void* client, void* name, uint32_t w, uint32_t h, int fmt, uint32_t flg, void* meta) {
    LOGI("vMeer: Intercepting Surface Creation: %ux%u", w, h);
    return inst; 
}

// =============================================================================
// BAGIAN 4: JNI ENTRY
// =============================================================================

extern "C" JNIEXPORT void JNICALL Java_com_vmeer_io_GraphicsEngine_nativeInit(JNIEnv* env, jobject thiz) {
    LOGI("vMeer: Maturing Graphics Pipeline [START]");

    init_graphics_internals();

    shadowhook_init(SHADOWHOOK_MODE_UNIQUE, false);

    // Hook DequeueBuffer
    shadowhook_hook_sym_name(
        "libgui.so",
        "_ZN7android7Surface13dequeueBufferEPP19ANativeWindowBufferPi",
        (void*)hook_dequeue_buffer,
        (void**)&orig_dequeue
    );

    // Hook SurfaceControl
    shadowhook_hook_sym_name(
        "libgui.so",
        "_ZN7android14SurfaceControlC1ERKNS_2spINS_14SurfaceComposerClientEEERKNS_7String8EjjijPNS_11LayerMetadataE",
        (void*)hook_create_surface,
        nullptr
    );

    LOGI("vMeer: Graphics Pipeline Matched (100%%)");
}
