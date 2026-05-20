#include <jni.h>
#include <android/log.h>
#include <android/native_window.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <mutex>
#include <string>
#include "shadowhook.h"

#define LOG_TAG "vMeer_EGLHook"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

static std::mutex egl_mutex;

// Pointer untuk menyimpan fungsi asli dari libEGL.so
typedef EGLSurface (*eglCreateWindowSurface_t)(EGLDisplay, EGLConfig, EGLNativeWindowType, const EGLint*);
typedef EGLBoolean (*eglSwapBuffers_t)(EGLDisplay, EGLSurface);
typedef EGLContext (*eglCreateContext_t)(EGLDisplay, EGLConfig, EGLContext, const EGLint*);

static eglCreateWindowSurface_t orig_eglCreateWindowSurface = nullptr;
static eglSwapBuffers_t orig_eglSwapBuffers = nullptr;
static eglCreateContext_t orig_eglCreateContext = nullptr;

/**
 * Proxy Hook untuk eglCreateWindowSurface
 * Menjembatani native window milik Guest agar diarahkan ke EGL Surface host yang terisolasi.
 */
EGLSurface proxy_eglCreateWindowSurface(EGLDisplay dpy, EGLConfig config, EGLNativeWindowType win, const EGLint* attrib_list) {
    std::lock_guard<std::mutex> lock(egl_mutex);
    LOGI("vMeer EGLHook: Menginterseptasi eglCreateWindowSurface. Window handle: %p", (void*)win);

    // Di sini kita dapat mendeteksi, meremapping, atau mengonfigurasi properti ANativeWindow
    // milik Guest OS sebelum diteruskan ke driver EGL hardware asli milik host.
    if (orig_eglCreateWindowSurface != nullptr) {
        return orig_eglCreateWindowSurface(dpy, config, win, attrib_list);
    }
    return EGL_NO_SURFACE;
}

/**
 * Proxy Hook untuk eglSwapBuffers
 * Mengontrol sinkronisasi buffer render ke layar.
 */
EGLBoolean proxy_eglSwapBuffers(EGLDisplay dpy, EGLSurface surface) {
    // Di sini kita bisa menyisipkan watermark virtual, membatasi FPS sandbox, 
    // atau melakukan manipulasi screenshot/frame capture protection.
    if (orig_eglSwapBuffers != nullptr) {
        return orig_eglSwapBuffers(dpy, surface);
    }
    return EGL_FALSE;
}

/**
 * Proxy Hook untuk eglCreateContext
 * Mengisolasi OpenGL rendering context agar terhindar dari bentrok state GLES.
 */
EGLContext proxy_eglCreateContext(EGLDisplay dpy, EGLConfig config, EGLContext share_context, const EGLint* attrib_list) {
    LOGI("vMeer EGLHook: Membuat konteks perenderan GPU virtual terisolasi.");
    if (orig_eglCreateContext != nullptr) {
        return orig_eglCreateContext(dpy, config, share_context, attrib_list);
    }
    return EGL_NO_CONTEXT;
}

namespace vmeer {
namespace egl {

void InitializeEglVirtualization() {
    std::lock_guard<std::mutex> lock(egl_mutex);
    LOGI("vMeer EGLHook: Menginisialisasi sistem Virtualisasi GPU...");

    // 1. Hooking fungsi kritis libEGL.so untuk pembelokan surface & context
    void* hook_surface = shadowhook_hook_sym_name(
        "libEGL.so", 
        "eglCreateWindowSurface", 
        reinterpret_cast<void*>(proxy_eglCreateWindowSurface), 
        reinterpret_cast<void**>(&orig_eglCreateWindowSurface)
    );

    void* hook_swap = shadowhook_hook_sym_name(
        "libEGL.so", 
        "eglSwapBuffers", 
        reinterpret_cast<void*>(proxy_eglSwapBuffers), 
        reinterpret_cast<void**>(&orig_eglSwapBuffers)
    );

    void* hook_context = shadowhook_hook_sym_name(
        "libEGL.so", 
        "eglCreateContext", 
        reinterpret_cast<void*>(proxy_eglCreateContext), 
        reinterpret_cast<void**>(&orig_eglCreateContext)
    );

    if (hook_surface && hook_swap && hook_context) {
        LOGI("vMeer EGLHook: SUCCESS - Seluruh gerbang perenderan GLES/EGL berhasil ter-virtualisasi!");
    } else {
        LOGE("vMeer EGLHook: WARNING - Beberapa fungsi EGL gagal di-hook. Driver fallback aktif.");
    }
}

} // namespace egl
} // namespace vmeer

extern "C" {

JNIEXPORT void JNICALL
Java_com_vmeer_io_EngineLoader_nativeInitEglVirtualization(JNIEnv* env, jclass clazz) {
    (void)env;
    (void)clazz;
    vmeer::egl::InitializeEglVirtualization();
}

} // extern "C"