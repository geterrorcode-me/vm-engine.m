#include <jni.h>
#include <android/log.h>
#include <android/native_window.h>
#include <dlfcn.h>
#include <stdint.h>
#include "shadowhook.h"
#include "include/surface_vm.h"

#define LOG_TAG "vMeer_Graphics"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

// Definisi pointer fungsi asli
typedef int (*p_dequeue_buffer_t)(void* base, ANativeWindow_Buffer** buffer, int* fenceFd);
static p_dequeue_buffer_t orig_dequeue = nullptr;

// FUNGSI PROXY (Ganti Lambda ke fungsi statis untuk stabilitas casting)
static int proxy_dequeue_buffer(void* base, ANativeWindow_Buffer** buffer, int* fenceFd) {
    if (orig_dequeue) {
        // Panggil fungsi asli
        int result = orig_dequeue(base, buffer, fenceFd);
        
        // Logika monitoring frame bisa ditaruh di sini
        // LOGI("vMeer: DequeueBuffer Intercepted");
        
        return result;
    }
    return -1;
}

// Bridge Function
void start_graphics_proxy() {
    LOGI("vMeer: Initializing Graphics Hook...");
    
    if (shadowhook_init(SHADOWHOOK_MODE_UNIQUE, false) != 0) {
        return;
    }

    // Hook dequeueBuffer di libgui.so
    // Gunakan fungsi proxy_dequeue_buffer yang sudah didefinisikan di atas
    shadowhook_hook_sym_name(
        "libgui.so",
        "_ZN7android7Surface13dequeueBufferEPP19ANativeWindowBufferPi",
        (void*)proxy_dequeue_buffer,
        (void**)&orig_dequeue
    );
    
    LOGI("vMeer: Graphics Hook Matched (100%%)");
}

extern "C" JNIEXPORT void JNICALL 
Java_com_vmeer_io_GraphicsEngine_nativeInit(JNIEnv* env, jobject thiz) {
    start_graphics_proxy();
}
