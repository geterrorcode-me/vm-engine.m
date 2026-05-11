#include <jni.h>
#include <android/log.h>
#include <android/native_window.h>
#include <dlfcn.h>
#include <stdint.h>
#include "shadowhook.h"
#include "include/surface_vm.h"

#define LOG_TAG "vMeer_Graphics"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

typedef int (*p_dequeue_buffer_t)(void* base, ANativeWindow_Buffer** buffer, int* fenceFd);
static p_dequeue_buffer_t orig_dequeue = nullptr;

// Bridge Function: Bisa dipanggil oleh vmeer_core.cpp
void start_graphics_proxy() {
    LOGI("vMeer: Initializing Graphics Hook...");
    
    // Inisialisasi ShadowHook jika belum
    shadowhook_init(SHADOWHOOK_MODE_UNIQUE, false);

    // Hook dequeueBuffer di libgui.so
    shadowhook_hook_sym_name(
        "libgui.so",
        "_ZN7android7Surface13dequeueBufferEPP19ANativeWindowBufferPi",
        (void*)[](void* base, ANativeWindow_Buffer** buffer, int* fenceFd) -> int {
            if (orig_dequeue) {
                return orig_dequeue(base, buffer, fenceFd);
            }
            return -1;
        },
        (void**)&orig_dequeue
    );
    
    LOGI("vMeer: Graphics Hook Matched (100%%)");
}

// JNI Entry: Bisa dipanggil langsung dari Java GraphicsEngine.java
extern "C" JNIEXPORT void JNICALL 
Java_com_vmeer_io_GraphicsEngine_nativeInit(JNIEnv* env, jobject thiz) {
    start_graphics_proxy();
}
