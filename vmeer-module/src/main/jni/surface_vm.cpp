#include <jni.h>
#include <android/log.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <dlfcn.h>
#include <stdint.h>
#include "shadowhook.h"

#define LOG_TAG "vMeer_SurfaceGraphics"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// Definisi signature fungsi penumpas layar hitam milik egl_bridge
typedef void (*fn_start_egl_bridge)(ANativeWindow*);
typedef void (*fn_stop_egl_bridge)();

// Pointer fungsi asli untuk monitoring libgui
typedef int (*p_dequeue_buffer_t)(void* base, ANativeWindow_Buffer** buffer, int* fenceFd);
static p_dequeue_buffer_t orig_dequeue = nullptr;

static int proxy_dequeue_buffer(void* base, ANativeWindow_Buffer** buffer, int* fenceFd) {
    if (orig_dequeue) {
        return orig_dequeue(base, buffer, fenceFd);
    }
    return -1;
}

/**
 * Jembatan Pintar Dinamis: Menghubungkan Surface dengan Thread EGL
 * tanpa menciptakan dependensi link-time yang merusak modul standalone.
 */
static void dispatch_to_egl_bridge(ANativeWindow* window, bool trigger_start) {
    // Cari handle memori libvmeer_engine.so di mana tempat egl_bridge.cpp terkompilasi
    void* handle = dlopen("libvmeer_engine.so", RTLD_NOW | RTLD_GLOBAL);
    if (!handle) {
        LOGW("[Graphics Gate] Berjalan dalam mode mandiri tipis. Melewati proses render loop GLES.");
        return;
    }

    if (trigger_start && window != nullptr) {
        auto start_fn = (fn_start_egl_bridge)dlsym(handle, "start_egl_bridge");
        if (start_fn) {
            start_fn(window);
        } else {
            LOGE("[Graphics Gate] Simbol start_egl_bridge gagal dimuat dari Core Engine!");
        }
    } else if (!trigger_start) {
        auto stop_fn = (fn_stop_egl_bridge)dlsym(handle, "stop_egl_bridge");
        if (stop_fn) {
            stop_fn();
        }
    }
    dlclose(handle);
}

void start_graphics_proxy() {
    int sh_status = shadowhook_init(SHADOWHOOK_MODE_SHARED, false);
    if (sh_status != 0) {
        LOGW("vMeer Graphics: ShadowHook shared instance terdeteksi (%d). Melanjutkan...", sh_status);
    }

    void* stub = shadowhook_hook_sym_name(
        "libgui.so",
        "_ZN7android7Surface13dequeueBufferEPP19ANativeWindowBufferPi",
        (void*)proxy_dequeue_buffer,
        (void**)&orig_dequeue
    );
    
    if (stub) {
        LOGI("vMeer Graphics: Monitoring Hook libgui.so Berhasil Dipasang.");
    }
}

// ==========================================================
// JNI BINDINGS SINKRON (Sesuai Struktur /vMeerOS Host Anda)
// ==========================================================
extern "C" JNIEXPORT void JNICALL 
Java_com_vmeer_io_GraphicsEngine_nativeInit(JNIEnv* env, jobject thiz) {
    (void)env; (void)thiz;
    start_graphics_proxy();
}

extern "C" JNIEXPORT void JNICALL
Java_com_vmeer_io_GraphicsEngine_nativeSurfaceCreated(JNIEnv* env, jobject thiz, jobject surface) {
    (void)thiz;
    if (surface != nullptr) {
        ANativeWindow* window = ANativeWindow_fromSurface(env, surface);
        if (window != nullptr) {
            LOGI("[Graphics JNI] Surface terdeteksi. Mengirim perintah render ke Core EGL...");
            dispatch_to_egl_bridge(window, true);
        }
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_vmeer_io_GraphicsEngine_nativeSurfaceDestroyed(JNIEnv* env, jobject thiz) {
    (void)env; (void)thiz;
    LOGI("[Graphics JNI] Surface dihancurkan. Mematikan Core EGL...");
    dispatch_to_egl_bridge(nullptr, false);
}
