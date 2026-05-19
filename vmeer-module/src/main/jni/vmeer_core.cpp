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
// DEKLARASI EKSTERNAL (Membungkam Error Header Lama)
// ====================================================================
extern "C" {
    void init_binder_isolation(); 
    void start_graphics_proxy();   
    void start_virtual_system_services(); 
}

// ====================================================================
// BYPASS NATIVE: ATURAN HIDDEN API ART ANDROID 15
// ====================================================================
static bool (*orig_ShouldBlockAccessToMember)(void*, void*, int, int) = nullptr;

/**
 * Hook Proxy untuk memotong pemeriksaan pembatasan API internal Android.
 */
bool hook_ShouldBlockAccessToMember(void* member, void* ctx, int access_ctx, int access_method) {
    (void)member; 
    (void)ctx; 
    (void)access_ctx; 
    (void)access_method; // FIX: Pemanggilan void casting yang valid
    return false; 
}

/**
 * Mengunci inisialisasi ShadowHook dalam mode SHARED
 */
void init_vmeer_native_guards() {
    int sh_status = shadowhook_init(SHADOWHOOK_MODE_SHARED, false);
    if (sh_status != 0) {
        LOGI("vMeer Core: ShadowHook Shared Instance terdeteksi aktif di modul lain. Melanjutkan...");
    }

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

    init_vmeer_native_guards();
    init_vmeer_stealth(); 
    init_binder_isolation();
    start_virtual_system_services();
    start_graphics_proxy();

    LOGI("vMeer: === All Native Core Systems Operational & Invisible ===");
}
