#include <jni.h>
#include <android/log.h>
#include <stdint.h>
#include <dlfcn.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>

// --- Core Engine & Stealth ---
#include "shadowhook.h"
#include "vmeer_stealth.h"

// --- Ecosystem Modules (Pilar Baru) ---
#include "include/vmeer_helper.h"    // Multi-process Sync & Shared Memory
#include "include/vmeer_zygote.h"    // Zygote: nativeForkAndSpecialize
#include "include/vmeer_context.h"   // Runtime State (Android ID, Seed, etc)

// --- Logic Modules (Virtual Services) ---
#include "binder_engine.h"           // Virtual AMS & Identity Spoofing
#include "sensor_engine.h"           // Sensor Noise & Jitter
#include "vmeer_pms.h"             // Virtual Package Ecosystem
#include "egl_bridge.h"              // Graphics/GPU Vendor Virtualization

// --- Legacy & Bridge ---
#include "binder_vm.h"               // Low-level Binder Hook Bridge
#include "vmeer_system.h"            // System Property Spoofing

#define LOG_TAG "vMeer_Engine"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

extern "C" {

/**
 * requestNamespaceSetup:
 * Mengatur isolated storage dan sandboxing melalui Unix Domain Socket ke Daemon vmeerd.
 */
bool requestNamespaceSetup(const char* pkgName) {
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) return false;

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    // Abstract socket address
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
 * JNI_OnLoad:
 * Entry point utama. Di sini seluruh komponen ekosistem dibangunkan.
 */
JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* res) {
    LOGI("====================================================");
    LOGI("   vMeer OS Engine: Initializing Virtual Ecosystem  ");
    LOGI("====================================================");

    // 1. STEALTH PHASE: 
    // Menyembunyikan jejak library dari memory maps sebelum engine aktif.
    init_vmeer_stealth();

    // 2. CORE HOOKING ENGINE:
    // Inisialisasi ShadowHook untuk memanipulasi fungsi sistem.
    if (shadowhook_init(SHADOWHOOK_MODE_UNIQUE, false) != 0) {
        LOGE("Critical: ShadowHook initialization failed!");
        return JNI_ERR;
    }

    // 3. HELPER & SYNC PHASE (Pilar Baru):
    // Menghubungkan ke Shared Memory agar data antar proses virtual tetap sinkron 
    // tanpa menyebabkan SQLite database locked.
    if (!vmeer::helper::ConnectSharedState()) {
        LOGI("Warning: Shared Memory sync unavailable. Falling back to direct DB.");
    }

    // 4. ENVIRONMENT SETUP:
    const char* target_pkg = "com.vmeer.virtual.guest"; // Default isolated package
    if (!requestNamespaceSetup(target_pkg)) {
        LOGI("Warning: Namespace setup failed. Storage isolation might be limited.");
    }

    // 5. RUNTIME CONTEXT (VRCE):
    // Memuat identitas virtual (Android ID, Serial, IMEI) dari database.
    if (!vmeer::RuntimeContext::Get().Initialize("vm_001", target_pkg)) {
        LOGE("Critical: Failed to load Virtual Context!");
        return JNI_ERR;
    }

    // 6. ZYGOTE EVOLUTION (Pilar Baru):
    // Melakukan hook pada proses kelahiran (fork) agar sandbox aktif sejak awal.
    vmeer::zygote::HookForkAndSpecialize();

    // 7. MODULES ACTIVATION (Virtual Services):
    LOGI("vMeer Engine: Activating Virtual Service Layers...");
    
    // Virtual AMS & Binder Ecosystem
    start_binder_proxy();              
    vmeer::binder::InitHooks();        
    
    // System & Graphics Virtualization
    start_virtual_system_services();   
    start_egl_bridge();                
    
    // Hardware Jitter & Sensors
    vmeer::sensor::InitHooks();        

    LOGI("====================================================");
    LOGI("   vMeer Ecosystem is now LIVE and Isolated.        ");
    LOGI("====================================================");
    
    return JNI_VERSION_1_6;
}

/**
 * JNI_OnUnload:
 * Sinkronisasi terakhir sebelum library dilepas.
 */
JNIEXPORT void JNICALL JNI_OnUnload(JavaVM* vm, void* reserved) {
    LOGI("vMeer Engine: Finalizing state synchronization...");
    vmeer::RuntimeContext::Get().Heartbeat(); 
}

} // extern "C"
