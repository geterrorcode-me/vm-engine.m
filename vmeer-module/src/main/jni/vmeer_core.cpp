#include <jni.h>
#include <android/log.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <stdint.h>
#include "shadowhook.h"

// Rujukan ke header internal proyek Anda yang lain
#include "include/vmeer_system.h"
#include "include/vmeer_stealth.h"

#define LOG_TAG "vMeer_CoreEngine"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// ====================================================================
// SOLUSI FIX: ISOLASI DEKLARASI EKSTERNAL (Membungkam Error Header Lama)
// ====================================================================
extern "C" {
    // 1. Fungsi dari binder_rewriter.cpp / binder_vm.cpp
    void init_binder_isolation(); 
    
    // 2. Fungsi dari surface_vm.cpp / graphics proxy
    void start_graphics_proxy();   
    
    // 3. Fungsi bawaan internal vMeer System Services
    void start_virtual_system_services(); 
    
    // 4. Sinkronisasi murni dengan implementasi baru di egl_bridge.cpp
    // Mendeklarasikan start_egl_bridge dengan 1 argumen (ANativeWindow*) 
    // dan stop_egl_bridge() agar dikenali 100% oleh kompiler clang.
    void start_egl_bridge(ANativeWindow* window);
    void stop_egl_bridge();
}

// ====================================================================
// BYPASS NATIVE: ATURAN HIDDEN API ART ANDROID 15
// ====================================================================
// Penampung pointer fungsi asli milik runtime libart.so
static bool (*orig_ShouldBlockAccessToMember)(void*, void*, int, int) = nullptr;

/**
 * Hook Proxy untuk memotong pemeriksaan pembatasan API internal Android.
 * Mengembalikan nilai 'false' secara paksa agar semua Hidden API terbuka untuk Guest OS.
 */
bool hook_ShouldBlockAccessToMember(void* member, void* ctx, int access_ctx, int access_method) {
    (void)member; (void)ctx; (void)access_ctx; (void)access_method;
    return false; 
}

/**
 * Mengunci inisialisasi ShadowHook dalam mode SHARED
 * untuk melumpuhkan filter restriksi pada libart.so sebelum aplikasi tamu berjalan.
 */
void init_vmeer_native_guards() {
    int sh_status = shadowhook_init(SHADOWHOOK_MODE_SHARED, false);
    if (sh_status != 0) {
        LOGI("vMeer Core: ShadowHook Shared Instance terdeteksi aktif di modul lain. Melanjutkan...");
    }

    // Melakukan hooking ke fungsi penegak aturan Hidden API milik Android 15
    void* art_hook = shadowhook_hook_sym_name(
        "libart.so", 
        "_ZN3art9hiddenapi25ShouldBlockAccessToMember*", 
        (void*)hook_ShouldBlockAccessToMember, 
        (void**)&orig_ShouldBlockAccessToMember
    );

    if (art_hook != nullptr) {
        LOGI("vMeer Core: Native Hidden API Bypass Terkunci Sukses (Android 15 Guard).");
    } else {
        LOGE("vMeer Core: CRITICAL ERROR - Gagal mengunci Native Hidden API Bypass!");
    }
}

// ====================================================================
// JNI INTERACTION 1: MANAGER ENGINE UTAMA (VMEERCORE.JAVA)
// ====================================================================
extern "C" JNIEXPORT void JNICALL
Java_com_vmeer_io_VMeerCore_nativeLaunch(JNIEnv *env, jobject thiz) {
    (void)env; (void)thiz;
    LOGI("vMeer: === Orchestrating Stealth Virtualization (Android 15 Edition) ===");

    // Tahap 0: Jebol sistem keamanan restriksi SDK Android 15 tingkat native
    init_vmeer_native_guards();

    // Tahap 1: Cloaking Stealth (Menghapus jejak maps injeksi library berkas native)
    init_vmeer_stealth(); 

    // Tahap 2: Binder Virtualization & Isolation (Pengalihan transaksi driver Binder IPC)
    init_binder_isolation();

    // Tahap 3: System Services (Memalsukan context aplikasi & daftarkan stub internal)
    start_virtual_system_services();

    // Tahap 4: Graphics Proxy (Aktifkan pemantauan alokasi buffer grafis jika dipasang)
    start_graphics_proxy();

    LOGI("vMeer: === All Native Core Systems Operational & Invisible ===");
}

// ====================================================================
// JNI INTERACTION 2: PIPELINE GRAFIS (GRAPHICSENGINE.JAVA)
// ====================================================================
/**
 * Menerima pelimpahan objek permukaan (Surface View) dari UI dashboard Java.
 * Mengonversinya menjadi objek native window dan mengirimkannya ke thread rendering EGL.
 */
extern "C" JNIEXPORT void JNICALL
Java_com_vmeer_io_GraphicsEngine_nativeSurfaceCreated(JNIEnv* env, jobject thiz, jobject surface) {
    (void)thiz;
    if (surface != nullptr) {
        // Konversi objek Surface Java ke pointer struktur ANativeWindow C++ secara legal
        ANativeWindow* window = ANativeWindow_fromSurface(env, surface);
        if (window != nullptr) {
            LOGI("[Core Graphics] Mengikat Surface ke Pipeline GLES Render Loop...");
            // Memicu pembuatan EGLContext & loop eglSwapBuffers (Menghilangkan Layar Hitam)
            start_egl_bridge(window);
        } else {
            LOGE("[Core Graphics] Gagal melakukan konversi Java Surface ke ANativeWindow!");
        }
    }
}

/**
 * Menghentikan siklus kerja thread rendering secara aman ketika SurfaceView dihancurkan host.
 */
extern "C" JNIEXPORT void JNICALL
Java_com_vmeer_io_GraphicsEngine_nativeSurfaceDestroyed(JNIEnv* env, jobject thiz) {
    (void)env; (void)thiz;
    LOGI("[Core Graphics] Surface dihancurkan. Menghentikan thread pekerja render GLES...");
    // Menghentikan loop eglSwapBuffers di egl_bridge
    stop_egl_bridge();
}
