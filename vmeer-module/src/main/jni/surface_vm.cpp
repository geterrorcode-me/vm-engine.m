#include <jni.h>
#include <android/log.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <dlfcn.h>
#include <stdint.h>
#include "shadowhook.h"
#include "include/surface_vm.h"

#define LOG_TAG "vMeer_Graphics"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// Rujukan ke thread render GLES di egl_bridge.cpp
extern "C" void start_egl_bridge(ANativeWindow* window);
extern "C" void stop_egl_bridge();

// Definisi pointer fungsi asli libgui
typedef int (*p_dequeue_buffer_t)(void* base, ANativeWindow_Buffer** buffer, int* fenceFd);
static p_dequeue_buffer_t orig_dequeue = nullptr;

static int proxy_dequeue_buffer(void* base, ANativeWindow_Buffer** buffer, int* fenceFd) {
    if (orig_dequeue) {
        int result = orig_dequeue(base, buffer, fenceFd);
        // Interseptor produsen buffer aplikasi tamu aktif di sini jika dibutuhkan
        return result;
    }
    return -1;
}

void start_graphics_proxy() {
    LOGI("vMeer: Menghubungkan Jembatan Substitusi Grafis...");
    
    // Inisialisasi aman: SHADOWHOOK_MODE_SHARED agar berbagi resource dengan vmeer_engine
    int sh_status = shadowhook_init(SHADOWHOOK_MODE_SHARED, false);
    if (sh_status != 0) {
        LOGW("vMeer_Graphics: ShadowHook shared instance terdeteksi (%d). Melanjutkan...", sh_status);
    }

    // Mengunci simbol dequeueBuffer milik Android Surface Pipeline
    void* stub = shadowhook_hook_sym_name(
        "libgui.so",
        "_ZN7android7Surface13dequeueBufferEPP19ANativeWindowBufferPi",
        (void*)proxy_dequeue_buffer,
        (void**)&orig_dequeue
    );
    
    if (stub) {
        LOGI("vMeer: Jembatan Substitusi Grafis Terkunci (100%%)");
    } else {
        int err_num = shadowhook_get_errno();
        LOGE("vMeer: Gagal intercept libgui! Error: %s", shadowhook_to_errmsg(err_num));
    }
}

// SINKRONISASI JNI: Disesuaikan dengan package com.vmeer.io di /vMeerOS Anda
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
            // Jalankan EGL Render Loop mandiri untuk mengusir Layar Hitam Dashboard
            start_egl_bridge(window);
        }
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_vmeer_io_GraphicsEngine_nativeSurfaceDestroyed(JNIEnv* env, jobject thiz) {
    (void)thiz;
    // Matikan thread rendering GLES saat aplikasi masuk ke background
    stop_egl_bridge();
}
