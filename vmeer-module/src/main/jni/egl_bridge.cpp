#include <EGL/egl.h>
#include <GLESv2/gl2.h>
#include <android/native_window.h>
#include <android/log.h>
#include <pthread.h>

#define LOG_TAG "vMeer_EGLBridge"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

static pthread_t render_thread;
static ANativeWindow* g_window = nullptr;
static bool is_rendering = false;

// Driver EGL Handles
static EGLDisplay display = EGL_NO_DISPLAY;
static EGLContext context = EGL_NO_CONTEXT;
static EGLSurface surface = EGL_NO_SURFACE;

/**
 * Thread pekerja GLES Loop untuk menstabilkan render screen Guest OS
 */
static void* egl_render_loop(void* arg) {
    (void)arg;
    LOGI("[EGL Thread] Memulai siklus inisialisasi hardware display...");
    
    display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    eglInitialize(display, nullptr, nullptr);

    EGLint attribs[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_BLUE_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_RED_SIZE, 8,
        EGL_NONE
    };

    EGLConfig config;
    EGLint num_configs;
    eglChooseConfig(display, attribs, &config, 1, &num_configs);

    EGLint context_attribs[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
    context = eglCreateContext(display, config, EGL_NO_CONTEXT, context_attribs);
    surface = eglCreateWindowSurface(display, config, g_window, nullptr);

    if (eglMakeCurrent(display, surface, surface, context) == EGL_FALSE) {
        LOGE("[EGL Thread] Gagal mengikat EGL Context ke Native Window!");
        return nullptr;
    }

    LOGI("[EGL Thread] Hardware virtual berhasil dikunci. Memulai loop penumpas layar hitam.");
    is_rendering = true;

    while (is_rendering) {
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f); // Bersihkan dengan warna hitam solid standar engine
        glClear(GL_COLOR_BUFFER_BIT);
        
        eglSwapBuffers(display, surface);
        
        // Batasi frame-rate agar tidak memakan siklus CPU secara berlebihan
        usleep(16666); // Rentang waktu stabil ~60 FPS
    }

    // Pembersihan resource saat thread dimatikan
    eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroySurface(display, surface);
    eglDestroyContext(display, context);
    eglTerminate(display);
    
    LOGI("[EGL Thread] Pipeline render dihentikan secara aman.");
    return nullptr;
}

extern "C" {

void start_egl_bridge(ANativeWindow* window) {
    if (window == nullptr) return;
    g_window = window;
    ANativeWindow_acquire(g_window);
    pthread_create(&render_thread, nullptr, egl_render_loop, nullptr);
}

void stop_egl_bridge() {
    is_rendering = false;
    pthread_join(render_thread, nullptr);
    if (g_window != nullptr) {
        ANativeWindow_release(g_window);
        g_window = nullptr;
    }
}

EGLDisplay vmeer_eglGetDisplay(EGLNativeDisplayType display_id) {
    return eglGetDisplay(display_id);
}

}
