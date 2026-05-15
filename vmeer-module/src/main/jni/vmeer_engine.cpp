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

// --- FUSE & ZSTD Headers ---
#include <fuse.h>
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
 * FIX SHADOWHOOK: Helper untuk mencegah error "declared here"
 * Memastikan semua parameter di-cast ke tipe yang diminta oleh shadowhook.h
 */
static void* do_hook(const char* lib, const char* sym, void* proxy, void** orig) {
    // shadowhook_hook_sym_name(const char*, const char*, void*, void**)
    void* stub = shadowhook_hook_sym_name(lib, sym, proxy, orig);
    if (!stub) {
        int err_num = shadowhook_get_errno();
        LOGE("Hook Failed: %s:%s | %s", lib, sym, shadowhook_to_errmsg(err_num));
    }
    return stub;
}

/**
 * requestNamespaceSetup:
 * Berkomunikasi dengan daemon (vmeerd) untuk isolasi storage.
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
 * setupVM:
 * Konfigurasi Virtual Machine yang dipanggil dari Java level.
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

    // 2. Request Namespace & Storage Setup
    requestNamespaceSetup("com.vmeer.guest", vUid);

    // 3. JNI & ART Setup
    init_art_hook(env);
    
    jclass context_clazz = env->GetObjectClass(context);
    jmethodID get_class_loader_mid = env->GetMethodID(context_clazz, "getClassLoader", "()Ljava/lang/ClassLoader;");
    jobject class_loader = env->CallObjectMethod(context, get_class_loader_mid);

    perform_mirror_injection(env, class_loader, path);

    // 4. Sinkronisasi Identitas (Spoofing)
    syncJavaProperties(env);

    env->ReleaseStringUTFChars(mirrorPath, path);
    LOGI("vMeer Engine: VM Setup for vUID %d is LIVE.", vUid);
}

/**
 * JNI_OnLoad:
 * Inisialisasi awal sistem saat library dimuat.
 */
JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* res) {
    (void)res;
    JNIEnv* env;
    if (vm->GetEnv((void**)&env, JNI_VERSION_1_6) != JNI_OK) return JNI_ERR;

    LOGI("vMeer Engine: Booting Core Systems...");

    // 1. Stealth Engine Initialization
    init_vmeer_stealth();
    
    // Gunakan MODE_UNIQUE untuk Dimensity 8300 (ARMv9)
    if (shadowhook_init(SHADOWHOOK_MODE_UNIQUE, false) != 0) {
        LOGE("CRITICAL: ShadowHook failed to initialize!");
        return JNI_ERR;
    }

    // 2. Core Hooks & Shared State
    vmeer::helper::ConnectSharedState();
    vmeer::zygote::HookForkAndSpecialize();

    // 3. Services Activation
    start_binder_proxy();
    vmeer::binder::InitHooks();
    start_virtual_system_services(); 
    start_egl_bridge();              
    vmeer::sensor::InitHooks();      

    // 4. VFS Engine (libfuse3 + libzstd)
    vmeer::vfs::StartVFSEngine();

    LOGI("vMeer Engine: Status READY - Engine v1.0.0-STABLE");
    
    return JNI_VERSION_1_6;
}

JNIEXPORT void JNICALL JNI_OnUnload(JavaVM* vm, void* reserved) {
    (void)vm; (void)reserved;
    LOGI("vMeer Engine: Engine Detached.");
}

} // extern "C"
