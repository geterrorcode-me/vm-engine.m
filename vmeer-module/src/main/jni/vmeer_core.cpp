#include <jni.h>
#include <android/log.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <stdint.h>
#include "shadowhook.h"

// Memastikan rujukan ke header internal proyek Anda terpetakan dengan benar
#include "include/vmeer_system.h"
#include "include/egl_bridge.h"
#include "include/vmeer_stealth.h"

#define LOG_TAG "vMeer_CoreEngine"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// ====================================================================
// FORWARD DECLARATIONS (Simbol Eksternal dari Modul C++ Anda yang Lain)
// ====================================================================
extern "C" {
    void init_binder_isolation(); // Dari binder/binder_rewriter.cpp atau binder_vm.cpp
    void start_graphics_proxy();   // Dari surface_vm.cpp (Jika membutuhkan hook dequeueBuffer)
    void start_virtual_system_services(); // Pendukung start_virtual_system_services()
}

// ====================================================================
// PURE NATIVE BYPASS: PENJINAK HIDDEN API ANDROID 15 (ART GUARD)
// ====================================================================
// Penampung rujukan fungsi asli internal libart.so
static bool (*orig_ShouldBlockAccessToMember)(void*, void*, int, int) = nullptr;

/**
 * Hook Proxy untuk fungsi internal runtime ART Android.
 * Memaksa sistem untuk mengembalikan nilai 'false' (TIDAK ADA akses API yang diblokir).
 */
bool hook_ShouldBlockAccessToMember(void* member, void* ctx, int access_ctx, int access_method) {
    (void)member; (void)ctx; (void)access_ctx; (void)access_method;
    return false; 
}

/**
 * Inisialisasi awal ShadowHook yang aman untuk digunakan bersama
 * oleh vmeer_core, vmeer_vfs, maupun surface_vm.
 */
void init_vmeer_native_guards() {
    int sh_status = shadowhook_init(SHADOWHOOK_MODE_SHARED, false);
    if (sh_status != 0) {
        LOGI("vMeer Core: ShadowHook Shared Instance sudah diaktifkan di modul lain. Melanjutkan...");
    }

    // Melakukan intercept pada fungsi penegak aturan Hidden API milik libart.so
    void* art_hook = shadowhook_hook_sym_name(
        "libart.so", 
        "_ZN3art9hiddenapi25ShouldBlockAccessToMember*", 
        (void*)hook_ShouldBlockAccessToMember, 
        (void**)&orig_ShouldBlockAccessToMember
    );

    if (art_hook != nullptr) {
        LOGI("vMeer Core: Native Hidden API Bypass Terkunci Sukses (Android 15 Guard).");
    } else {
        LOGE("vMeer Core: CRITICAL - Gagal mengunci Native Hidden API Bypass!");
    }
}

// ====================================================================
// JNI INTERACTION 1: ORKESTRASI ENGINE UTAMA (VMEERCORE.JAVA)
// ====================================================================
extern "C" JNIEXPORT void JNICALL
Java_com_vmeer_io_VMeerCore_nativeLaunch(JNIEnv *env, jobject thiz) {
    (void)env; (void)thiz;
    LOGI("vMeer: === Orchestrating Stealth Virtualization (Android 15 Edition) ===");

    // Tahap 0: Lumpuhkan aturan restriksi Hidden API tingkat native di libart.so
    init_vmeer_native_guards();

    // Tahap 1: Cloaking (Menghapus jejak injeksi memori berkas dari /proc/self/maps)
    init_vmeer_stealth(); 

    // Tahap 2: Binder Virtualization & Isolation (Pembelokan transaksi IPC driver)
    init_binder_isolation();

    // Tahap 3: System Services (Palsukan context aplikasi & daftarkan stub internal)
    start_virtual_system_services();

    // Tahap 4: Graphics Monitoring (Mengaktifkan interseptor dequeueBuffer jika dipasang)
    start_graphics_proxy();

    LOGI("vMeer: === All Native Core Systems Operational & Invisible ===");
}

// ====================================================================
// JNI INTERACTION 2: PIPELINE GRAFIS (GRAPHICSENGINE.JAVA)
// ====================================================================
/**
 * Menangkap callback pembuatan permukaan grafis (SurfaceView) dari layer Java
 * dan melemparkannya ke standalone thread rendering EGL untuk menyingkirkan layar hitam.
 */
extern "C" JNIEXPORT void JNICALL
Java_com_vmeer_io_GraphicsEngine_nativeSurfaceCreated(JNIEnv* env, jobject thiz, jobject surface) {
    (void)thiz;
    if (surface != nullptr) {
        // Konversi objek Surface Java menjadi pointer ANativeWindow C++ secara legal
        ANativeWindow* window = ANativeWindow_fromSurface(env, surface);
        if (window != nullptr) {
            LOGI("[Core Graphics] Mengikat Surface ke Pipeline GLES Render Loop...");
            // Memicu pembuatan EGLContext & loop eglSwapBuffers (dari egl_bridge.cpp)
            start_egl_bridge(window);
        } else {
            LOGE("[Core Graphics] Gagal mengonversi Java Surface ke ANativeWindow!");
        }
    }
}

/**
 * Menghentikan thread rendering secara aman saat aplikasi host ditutup atau masuk background.
 */
extern "C" JNIEXPORT void JNICALL
Java_com_vmeer_io_GraphicsEngine_nativeSurfaceDestroyed(JNIEnv* env, jobject thiz) {
    (void)env; (void)thiz;
    LOGI("[Core Graphics] Surface dihancurkan. Menghentikan thread render GLES...");
    // Mematikan eglSwapBuffers loop dengan aman (dari egl_bridge.cpp)
    stop_egl_bridge();
}
