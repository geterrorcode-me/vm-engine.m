#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include "include/egl_bridge.h"
#include <android/log.h>
#include "shadowhook.h"

#define LOG_TAG "vMeer_EGL"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

// Pointer fungsi asli
typedef EGLBoolean (*p_eglSwapBuffers_t)(EGLDisplay dpy, EGLSurface surface);
static p_eglSwapBuffers_t orig_eglSwapBuffers = nullptr;

typedef EGLSurface (*p_eglCreateWindowSurface_t)(EGLDisplay dpy, EGLConfig config, EGLNativeWindowType win, const EGLint *attrib_list);
static p_eglCreateWindowSurface_t orig_eglCreateWindowSurface = nullptr;

// 1. Hook CreateWindowSurface: Di sini kita tahu window mana yang akan dipakai
EGLSurface hook_eglCreateWindowSurface(EGLDisplay dpy, EGLConfig config, EGLNativeWindowType win, const EGLint *attrib_list) {
    LOGI("vMeer: eglCreateWindowSurface intercepted. Window: %p", win);
    // Di sini kita bisa melakukan "Window Tracking" untuk memetakan PID ke Surface
    return orig_eglCreateWindowSurface(dpy, config, win, attrib_list);
}

// 2. Hook SwapBuffers: Di sini kita eksekusi interupsi frame
EGLBoolean hook_eglSwapBuffers(EGLDisplay dpy, EGLSurface surface) {
    // SEMUA OPERASI SEBELUM SWAP ADALAH "SAFE ZONE"
    // Contoh: Inject Watermark, Frame Capture, atau Syncing
    
    // Panggil fungsi asli untuk menyerahkan buffer ke BLAST/SurfaceFlinger
    return orig_eglSwapBuffers(dpy, surface);
}

extern "C" void start_egl_bridge() {
    LOGI("vMeer: Initializing EGL Bridge...");

    // Hooking EGL via ShadowHook
    shadowhook_hook_sym_name("libEGL.so", "eglSwapBuffers", 
        (void*)hook_eglSwapBuffers, (void**)&orig_eglSwapBuffers);
        
    shadowhook_hook_sym_name("libEGL.so", "eglCreateWindowSurface", 
        (void*)hook_eglCreateWindowSurface, (void**)&orig_eglCreateWindowSurface);

    LOGI("vMeer: EGL Bridge is Live and Stable.");
}
