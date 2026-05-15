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
#include "include/vmeer_system.h" 

// --- Logic Modules ---
#include "binder_engine.h"
#include "sensor_engine.h"
#include "vmeer_pms.h"
#include "egl_bridge.h"

// --- FUSE & ZSTD Integration ---
#include <fuse.h>       // Sekarang aman karena include/fuse3 sudah di-path
#include <zstd.h>

// --- ART VM Bridge ---
extern "C" void init_art_hook(JNIEnv* env);
extern "C" void perform_mirror_injection(JNIEnv* env, jobject class_loader, const char* path);
extern "C" void syncJavaProperties(JNIEnv* env);

#define LOG_TAG "vMeer_Engine"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

extern "C" {

/**
 * FIX SHADOWHOOK: Helper untuk melakukan hook dengan casting yang benar
 * Ini mencegah error "declared here" karena type mismatch.
 */
static void* do_hook(const char* lib, const char* sym, void* proxy, void** orig) {
    void* stub = shadowhook_hook_sym_name(lib, sym, proxy, orig);
    if (!stub) {
        LOGE("Hook Failed: %s -> %s | Error: %s", lib, sym, shadowhook_to_errmsg(shadowhook_get_errno()));
    }
    return stub;
}

/**
 * requestNamespaceSetup:
 * Menghubungi vmeerd untuk isolasi filesystem.
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
 * setupVM: Konfigurasi utama aplikasi guest.
 */
JNIEXPORT void JNICALL
Java_com_vmeer_io_VMeerEngine_setupVM(JNIEnv *env, jclass clazz, jobject context, jstring mirrorPath, jint vUid) {
    (void)clazz;
    
    LOGI("====================================================");
    LOGI("   vMeer OS: Configuring High-End Virtual Machine   ");
    LOGI("====================================================");

    const char *path = env->GetStringUTFChars(mirrorPath, nullptr);
    
    // 1. Update Runtime Context
    auto& vContext = vmeer::RuntimeContext::Get();
    vContext.SetVirtualUid(vUid);
    vContext.SetMirrorPath(path);

    // 2. Setup VFS dengan FUSE & ZSTD (PENTING!)
    // Pastikan daemon sudah siap menerima perintah mount squashfs
    requestNamespaceSetup("com.vmeer.guest", vUid);

    // 3. JNI & ART Setup
    init_art_hook(env);
    
    jclass context_clazz = env->GetObjectClass(context);
    jmethodID get_class_loader_mid = env->GetMethodID(context_clazz, "getClassLoader", "()Ljava/lang/ClassLoader;");
    jobject class_loader = env->CallObjectMethod(context, get_class_loader_mid);

    perform_mirror_injection(env, class_loader, path);

    // 4. Sinkronisasi Identitas Java
    syncJavaProperties(env);

    env->ReleaseStringUTFChars(mirrorPath, path);
    LOGI("vMeer Engine: VM Setup for vUID %d is LIVE (FUSE+ZSTD Enabled).", vUid);
}

/**
 * JNI_OnLoad: Inisialisasi awal.
 */
JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* res) {
    (void)res;
    JNIEnv* env;
    if (vm->GetEnv((void**)&env, JNI_VERSION_1_6) != JNI_OK) return JNI_ERR;

    LOGI("vMeer Engine: Initializing Core Systems...");

    // 1. Stealth & ShadowHook Initialization
    init_vmeer_stealth();
    
    // Gunakan SHADOWHOOK_MODE_UNIQUE agar lebih stabil di Dimensity 8300
    if (shadowhook_init(SHADOWHOOK_MODE_UNIQUE, false) != 0) {
        LOGE("Critical: ShadowHook failed to init!");
        return JNI_ERR;
    }

    // 2. Ecosystem Connect
    vmeer::helper::ConnectSharedState();
    vmeer::zygote::HookForkAndSpecialize();

    // 3. Services Activation
    start_binder_proxy();
    vmeer::binder::InitHooks();
    start_virtual_system_services(); 
    start_egl_bridge();              
    vmeer::sensor::InitHooks();      

    // 4. VFS Engine Start (Ini akan menggunakan libfuse3 yang baru kita build)
    vmeer::vfs::StartVFSEngine();

    LOGI("vMeer Engine: Standby - Engine v1.0.0-STABLE");
    
    return JNI_VERSION_1_6;
}

JNIEXPORT void JNICALL JNI_OnUnload(JavaVM* vm, void* reserved) {
    (void)vm; (void)reserved;
    LOGI("vMeer Engine: Engine Detached.");
}

} // extern "C"
