#include <jni.h>
#include <android/log.h>
#include <stdint.h>
#include <dlfcn.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>

// Core Hooks & Stealth
#include "shadowhook.h"
#include "vmeer_stealth.h"

// Decoupled Modules
#include "include/vmeer_context.h"
#include "include/binder_engine.h"
#include "include/sensor_engine.h"
#include "include/pms_runtime.h"

// Legacy/Bridge Modules
#include "binder_vm.h"
#include "vmeer_system.h"
#include "egl_bridge.h"

#define LOG_TAG "vMeer_Engine"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

extern "C" {

/**
 * Mengatur isolated storage via Unix Domain Socket ke Daemon.
 */
bool requestNamespaceSetup(const char* pkgName) {
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) return false;

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    addr.sun_path[0] = '\0'; 
    strncpy(addr.sun_path + 1, "vmeer_daemon.cms", sizeof(addr.sun_path) - 2);

    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) == 0) {
        char cmd[512];
        snprintf(cmd, sizeof(cmd), "PREPARE_STORAGE:%s", pkgName);
        if (send(sock, cmd, strlen(cmd), 0) > 0) {
            char response[16] = {0};
            if (recv(sock, response, sizeof(response), 0) > 0) {
                if (strcmp(response, "OK") == 0) {
                    close(sock);
                    return true;
                }
            }
        }
    }
    close(sock);
    return false;
}

/**
 * Entry Point Utama saat Library dimuat ke proses target.
 */
JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* res) {
    LOGI("============================================");
    LOGI("vMeer Engine: Starting Modular Environment...");
    LOGI("============================================");

    // 1. Inisialisasi Stealth (Wajib paling awal agar library menghilang dari /proc/self/maps)
    init_vmeer_stealth();

    // 2. Inisialisasi ShadowHook (Engine dasar untuk interupsi fungsi)
    if (shadowhook_init(SHADOWHOOK_MODE_UNIQUE, false) != 0) {
        LOGI("vMeer Engine: ShadowHook init failed!");
        return JNI_ERR;
    }

    // 3. Konfigurasi Namespace & Storage Isolation
    const char* target_pkg = "com.vmeer.virtual.guest";
    if (!requestNamespaceSetup(target_pkg)) {
        LOGI("vMeer Engine: Namespace setup failed, isolated storage might be unstable.");
    }

    // 4. Bangunkan VRCE (Orchestrator)
    // Memuat Seed, Android ID, dan Aging State dari SQLite
    if (!vmeer::RuntimeContext::Get().Initialize("vm_001", target_pkg)) {
        LOGI("vMeer Engine: Critical Failure - Could not initialize RuntimeContext!");
        return JNI_ERR;
    }

    // 5. Jalankan Semua Modul Logic
    // Modul-modul ini sekarang akan mengambil data deterministik dari Context
    
    start_binder_proxy();              // Legacy Binder Bridge
    vmeer::binder::InitHooks();        // Logic Module: Android ID & Intent Spoofing
    
    start_virtual_system_services();   // System Property Spoofing (ro.build, etc)
    
    start_egl_bridge();                // Graphics/GPU Vendor Spoofing
    
    vmeer::sensor::InitHooks();        // Logic Module: Sensor Noise & Jitter

    LOGI("vMeer Engine: All modules integrated. Environment is LIVE.");
    
    return JNI_VERSION_1_6;
}

/**
 * Cleanup saat library di-unload (Opsional, tergantung penggunaan)
 */
JNIEXPORT void JNICALL JNI_OnUnload(JavaVM* vm, void* reserved) {
    LOGI("vMeer Engine: Shutting down and syncing state...");
    vmeer::RuntimeContext::Get().Heartbeat(); // Final sync ke SQLite
}

} // extern "C"
