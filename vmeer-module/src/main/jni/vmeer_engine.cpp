#include <jni.h>
#include <android/log.h>
#include <stdint.h>
#include <dlfcn.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>
#include <string>

// --- Core Engine & Stealth ---
#include "shadowhook.h"
#include "vmeer_stealth.h"

// --- Ecosystem Modules ---
#include "include/vmeer_helper.h"    // Multi-process Sync
#include "include/vmeer_zygote.h"    // Zygote Hooking
#include "include/vmeer_context.h"   // Runtime State
#include "include/vmeer_vfs.h"       // [NEW] Virtual File System & Stealth

// --- Logic Modules ---
#include "binder_engine.h"           // Virtual AMS & WMS
#include "sensor_engine.h"           // Sensor Jitter
#include "vmeer_pms.h"               // Virtual Package Manager
#include "egl_bridge.h"              // GPU Virtualization
#include "vmeer_system.h"            // Property Spoofing

// --- ART VM Bridge ---
extern "C" void init_art_hook(JNIEnv* env);
extern "C" void perform_mirror_injection(JNIEnv* env, jobject class_loader, const char* path);

#define LOG_TAG "vMeer_Engine"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

extern "C" {

/**
 * requestNamespaceSetup:
 * Menghubungi vmeerd untuk isolasi filesystem (Mount Namespace).
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
 * setupVM:
 * JNI Bridge utama yang dipanggil oleh App Host (Java)
 */
JNIEXPORT void JNICALL
Java_com_vmeer_io_VMeerEngine_setupVM(JNIEnv *env, jclass clazz, jobject context, jstring mirrorPath, jint vUid) {
    (void)clazz; // Hilangkan warning unused
    
    LOGI("====================================================");
    LOGI("   vMeer OS: Configuring High-End Virtual Machine   ");
    LOGI("====================================================");

    const char *path = env->GetStringUTFChars(mirrorPath, nullptr);
    
    // 1. Dapatkan ClassLoader dari Context aplikasi guest
    jclass context_clazz = env->GetObjectClass(context);
    jmethodID get_class_loader_mid = env->GetMethodID(context_clazz, "getClassLoader", "()Ljava/lang/ClassLoader;");
    jobject class_loader = env->CallObjectMethod(context, get_class_loader_mid);

    // 2. Bypass Hidden API (Buka gerbang sistem)
    init_art_hook(env);

    // 3. Suntikkan mirror.jar ke ClassLoader (Priority Loading)
    perform_mirror_injection(env, class_loader, path);

    // 4. Update Runtime Context (vUID & Config)
    auto& vContext = vmeer::RuntimeContext::Get();
    vContext.SetVirtualUid(vUid);
    vContext.SetMirrorPath(path);

    env->ReleaseStringUTFChars(mirrorPath, path);
    LOGI("vMeer Engine: VM Setup for vUID %d is LIVE.", vUid);
}

/**
 * JNI_OnLoad:
 * Mempersiapkan mesin sebelum aplikasi guest mengambil kendali penuh.
 */
JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* res) {
    (void)res;
    JNIEnv* env;
    if (vm->GetEnv((void**)&env, JNI_VERSION_1_6) != JNI_OK) return JNI_ERR;

    LOGI("vMeer Engine: Initializing Core Systems...");

    // 1. Stealth & Hooking Engine
    init_vmeer_stealth();
    if (shadowhook_init(SHADOWHOOK_MODE_UNIQUE, false) != 0) {
        LOGE("Critical: ShadowHook failed!");
        return JNI_ERR;
    }

    // 2. Multi-process Synchronization
    vmeer::helper::ConnectSharedState();

    // 3. Storage Namespace Isolation
    const char* default_pkg = "com.vmeer.guest";
    requestNamespaceSetup(default_pkg);

    // 4. Zygote Evolution (Hooking fork untuk sandbox)
    vmeer::zygote::HookForkAndSpecialize();

    // 5. Activation of Service Layers
    start_binder_proxy();              // Virtual AMS/WMS
    vmeer::binder::InitHooks();        // Binder Interceptor
    start_virtual_system_services();   // System Property Spoofing
    start_egl_bridge();                // GPU Vendor Spoofing
    vmeer::sensor::InitHooks();        // Hardware Jitter

    // 6. [NEW] VFS Engine Activation (Redirection + Ghost + Scrubber)
    vmeer::vfs::StartVFSEngine();

    LOGI("vMeer Engine: Standby - Awaiting setupVM(context, path, vuid)");
    
    return JNI_VERSION_1_6;
}

JNIEXPORT void JNICALL JNI_OnUnload(JavaVM* vm, void* reserved) {
    (void)vm; (void)reserved;
    LOGI("vMeer Engine: Engine Detached.");
}

} // extern "C"
