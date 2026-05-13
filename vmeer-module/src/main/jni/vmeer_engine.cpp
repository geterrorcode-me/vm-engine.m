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
#include "include/vmeer_helper.h"
#include "include/vmeer_zygote.h"
#include "include/vmeer_context.h"
#include "include/vmeer_vfs.h"
#include "include/vmeer_system.h" // Pastikan header ini ada

// --- Logic Modules ---
#include "binder_engine.h"
#include "sensor_engine.h"
#include "vmeer_pms.h"
#include "egl_bridge.h"

// --- ART VM Bridge ---
extern "C" void init_art_hook(JNIEnv* env);
extern "C" void perform_mirror_injection(JNIEnv* env, jobject class_loader, const char* path);
extern "C" void syncJavaProperties(JNIEnv* env);

#define LOG_TAG "vMeer_Engine"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

extern "C" {

/**
 * requestNamespaceSetup:
 * Menghubungi vmeerd untuk isolasi filesystem.
 * Sekarang mengirimkan vuid agar daemon bisa melakukan chown.
 */
bool requestNamespaceSetup(const char* pkgName, int vuid) {
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) return false;

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    addr.sun_path[0] = '\0'; 
    strncpy(addr.sun_path + 1, "vmeer_daemon.cms", sizeof(addr.sun_path) - 2);

    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) == 0) {
        char cmd[512];
        // Format baru: PREPARE_STORAGE:pkg_name:vuid
        snprintf(cmd, sizeof(cmd), "PREPARE_STORAGE:%s:%d", pkgName, vuid);
        
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
 * Konfigurasi utama saat aplikasi guest akan dijalankan.
 */
JNIEXPORT void JNICALL
Java_com_vmeer_io_VMeerEngine_setupVM(JNIEnv *env, jclass clazz, jobject context, jstring mirrorPath, jint vUid) {
    (void)clazz;
    
    LOGI("====================================================");
    LOGI("   vMeer OS: Configuring High-End Virtual Machine   ");
    LOGI("====================================================");

    const char *path = env->GetStringUTFChars(mirrorPath, nullptr);
    
    // 1. Update Runtime Context (Penting: Set sebelum request storage)
    auto& vContext = vmeer::RuntimeContext::Get();
    vContext.SetVirtualUid(vUid);
    vContext.SetMirrorPath(path);

    // 2. Request Namespace & Storage Setup ke vmeerd
    // Menggunakan package default atau bisa diambil dari context
    requestNamespaceSetup("com.vmeer.guest", vUid);

    // 3. JNI & ART Setup
    init_art_hook(env);
    
    jclass context_clazz = env->GetObjectClass(context);
    jmethodID get_class_loader_mid = env->GetMethodID(context_clazz, "getClassLoader", "()Ljava/lang/ClassLoader;");
    jobject class_loader = env->CallObjectMethod(context, get_class_loader_mid);

    perform_mirror_injection(env, class_loader, path);

    // 4. Sinkronisasi Identitas Java (Build.MODEL, dll)
    syncJavaProperties(env);

    env->ReleaseStringUTFChars(mirrorPath, path);
    LOGI("vMeer Engine: VM Setup for vUID %d is LIVE.", vUid);
}

/**
 * JNI_OnLoad:
 * Inisialisasi awal saat library dimuat.
 */
JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* res) {
    (void)res;
    JNIEnv* env;
    if (vm->GetEnv((void**)&env, JNI_VERSION_1_6) != JNI_OK) return JNI_ERR;

    LOGI("vMeer Engine: Initializing Core Systems...");

    // 1. Stealth Engine Initialization
    init_vmeer_stealth();
    if (shadowhook_init(SHADOWHOOK_MODE_UNIQUE, false) != 0) {
        LOGE("Critical: ShadowHook failed!");
        return JNI_ERR;
    }

    // 2. Synchronization & Core Hooks
    vmeer::helper::ConnectSharedState();
    vmeer::zygote::HookForkAndSpecialize();

    // 3. Services & Spoofing Activation
    start_binder_proxy();
    vmeer::binder::InitHooks();
    start_virtual_system_services(); // Property Spoofing
    start_egl_bridge();              // GPU Spoofing
    vmeer::sensor::InitHooks();      // Sensor Jitter

    // 4. VFS Engine (Redirection & Cloaking)
    vmeer::vfs::StartVFSEngine();

    LOGI("vMeer Engine: Standby - Awaiting setupVM");
    
    return JNI_VERSION_1_6;
}

JNIEXPORT void JNICALL JNI_OnUnload(JavaVM* vm, void* reserved) {
    (void)vm; (void)reserved;
    LOGI("vMeer Engine: Engine Detached.");
}

} // extern "C"
