#include <jni.h>
#include <android/log.h>
#include <stdint.h>
#include <dlfcn.h>
#include "shadowhook.h"

// Include modul-modul vMeer yang sudah kita buat
#include "include/vmeer_stealth.h"
#include "include/binder_vm.h"
#include "include/vmeer_system.h"
#include "include/egl_bridge.h"

#define LOG_TAG "vMeer_Engine"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// Forward declaration untuk fungsi requestNamespaceSetup yang sudah kamu buat
bool requestNamespaceSetup(const char* pkgName);

extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* res) {
    LOGI("vMeer Engine: === GLOBAL BOOT SEQUENCE START ===");

    // 1. DAEMON & NAMESPACE (Urutan #1)
    // Meminta vmeerd menyiapkan sandbox filesystem
    if (!requestNamespaceSetup("com.target.app")) { 
        LOGE("vMeer Engine: Namespace Isolation Failed!");
        // return JNI_ERR; // Opsional: tetap lanjut atau stop jika gagal
    }

    // 2. SHADOWHOOK INIT (Urutan #2)
    // Dasar dari semua sistem interceptor kita
    if (shadowhook_init(SHADOWHOOK_MODE_UNIQUE, false) != 0) {
        LOGE("vMeer Engine: ShadowHook init failed!");
        return JNI_ERR;
    }

    // 3. STEALTH MODE (Urutan #3)
    // Langsung menghilang dari /proc/self/maps sebelum discan
    init_vmeer_stealth();

    // 4. BINDER VIRTUALIZATION & UID SPOOFING (Urutan #4)
    // Menjalankan Handle Registry dan IPC Interceptor
    start_binder_proxy();

    // 5. VIRTUAL SYSTEM SERVICES (Urutan #5)
    // Menjalankan VAMS/VPMS palsu
    start_virtual_system_services();

    // 6. GRAPHICS PRESENTATION BRIDGE (Urutan #6)
    // Interupsi pada eglSwapBuffers
    start_egl_bridge();

    LOGI("vMeer Engine: === ALL SYSTEMS NOMINAL [100%%] ===");
    return JNI_VERSION_1_6;
}
