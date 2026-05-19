#include <EGL/egl.h>
#include <GLES2/gl2.h> // FIX UTAMA: Menggunakan struktur path Android NDK resmi
#include <android/native_window.h>
#include <android/log.h>
#include <pthread.h>
#include <unistd.h>

#define LOG_TAG "vMeer_EGLBridge"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

static pthread_t render_thread;
static ANativeWindow* g_window = nullptr;
static bool is_rendering = false;

static EGLDisplay gDisplay = EGL_NO_DISPLAY;
static EGLSurface gSurface = EGL_NO_SURFACE;
static EGLContext gContext = EGL_NO_CONTEXT;

bool initEGL(ANativeWindow* window) {
    gDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (gDisplay == EGL_NO_DISPLAY) {
        LOGE("Gagal mendapatkan EGL Display!");
        return false;
    }

    if (!eglInitialize(gDisplay, nullptr, nullptr)) {
        LOGE("Gagal inisialisasi EGL!");
        return false;
    }

    EGLConfig config;
    EGLint numConfigs;
    EGLint attribs[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_BLUE_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_RED_SIZE, 8,
        EGL_NONE
    };

    if (!eglChooseConfig(gDisplay, attribs, &config, 1, &numConfigs) || numConfigs < 1) {
        LOGE("Gagal memilih EGL Config!");
        return false;
    }

    gSurface = eglCreateWindowSurface(gDisplay, config, window, nullptr);
    if (gSurface == EGL_NO_SURFACE) {
        LOGE("Gagal membuat EGL Window Surface!");
        return false;
    }

    EGLint ctxAttribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE
    };

    gContext = eglCreateContext(gDisplay, config, EGL_NO_CONTEXT, ctxAttribs);
    if (gContext == EGL_NO_CONTEXT) {
        LOGE("Gagal membuat EGL Context!");
        return false;
    }

    if (!eglMakeCurrent(gDisplay, gSurface, gSurface, gContext)) {
        LOGE("Gagal mengikat context eglMakeCurrent!");
        return false;
    }

    return true;
}

void renderFrame() {
    // Menyesuaikan viewport grafis target
    glViewport(0, 0, 1080, 1920);
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    eglSwapBuffers(gDisplay, gSurface);
}

static void* egl_render_loop(void* arg) {
    (void)arg;
    if (!initEGL(g_window)) {
        LOGE("[EGL Thread] Gagal inisialisasi pipeline grafis minimal.");
        return nullptr;
    }

    LOGI("[EGL Thread] Pipeline render minimal aktif untuk penumpas layar hitam.");
    is_rendering = true;

    while (is_rendering) {
        renderFrame();
        usleep(16666); // Mengunci rentang waktu render stabil di kisaran ~60 FPS
    }

    // Pelepasan Context
    if (gDisplay != EGL_NO_DISPLAY) {
        eglMakeCurrent(gDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (gSurface != EGL_NO_SURFACE) eglDestroySurface(gDisplay, gSurface);
        if (gContext != EGL_NO_CONTEXT) eglDestroyContext(gDisplay, gContext);
        eglTerminate(gDisplay);
    }

    gDisplay = EGL_NO_DISPLAY;
    gSurface = EGL_NO_SURFACE;
    gContext = EGL_NO_CONTEXT;
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

}
