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
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

extern "C" {

/**
 * NEW JNI BRIDGE: isInitialized
 * Dipanggil dari Java (VMeerEngine.isInitialized()) untuk memastikan 
 * status muat library .so sukses tanpa crash.
 */
JNIEXPORT jboolean JNICALL
Java_com_vmeer_io_VMeerEngine_isInitialized(JNIEnv *env, jclass clazz) {
    (void)env; (void)clazz;
    return JNI_TRUE;
}

[[maybe_unused]] static void* do_hook(const char* lib, const char* sym, void* proxy, void** orig) {
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
 * NEW JNI BRIDGE: sendDaemonCommand
 */
JNIEXPORT jstring JNICALL
Java_com_vmeer_io_VMeerEngine_sendDaemonCommand(JNIEnv *env, jobject thiz, jstring command_str) {
    (void)thiz;
    const char *native_cmd = env->GetStringUTFChars(command_str, nullptr);
    std::string cmd(native_cmd);
    env->ReleaseStringUTFChars(command_str, native_cmd);

    int client_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (client_fd < 0) {
        LOGE("vMeer JNI: Gagal membuat socket client.");
        return env->NewStringUTF("ERR_SOCKET_FAILED");
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    addr.sun_path[0] = '\0';
    strncpy(addr.sun_path + 1, "vmeer_daemon.cms", sizeof(addr.sun_path) - 2);

    if (connect(client_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        LOGE("vMeer JNI: Koneksi ke daemon ditolak (vmeerd mati).");
        close(client_fd);
        return env->NewStringUTF("ERR_DAEMON_DEAD");
    }

    send(client_fd, cmd.c_str(), cmd.length(), 0);

    char buffer[32] = {0};
    ssize_t bytes_read = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
    close(client_fd);

    if (bytes_read > 0) {
        return env->NewStringUTF(buffer);
    }
    return env->NewStringUTF("ERR_NO_RESP");
}

/**
 * NEW JNI BRIDGE: prepareStorageSandbox
 * Menyesuaikan dengan pemanggilan JNI dari interface Java yang kamu pakai kemarin.
 */
JNIEXPORT jboolean JNICALL
Java_com_vmeer_io_VMeerEngine_prepareStorageSandbox(JNIEnv *env, jclass clazz, jstring pkgName, jint vUid) {
    (void)clazz;
    const char *native_pkg = env->GetStringUTFChars(pkgName, nullptr);
    bool result = requestNamespaceSetup(native_pkg, vUid);
    env->ReleaseStringUTFChars(pkgName, native_pkg);
    return result ? JNI_TRUE : JNI_FALSE;
}

/**
 * setupVM
 */
JNIEXPORT void JNICALL
Java_com_vmeer_io_VMeerEngine_setupVM(JNIEnv *env, jclass clazz, jobject context, jstring mirrorPath, jint vUid) {
    (void)clazz;
    LOGI("====================================================");
    LOGI("   vMeer OS: Configuring High-End Virtual Machine   ");
    LOGI("====================================================");

    const char *path = env->GetStringUTFChars(mirrorPath, nullptr);
    
    auto& vContext = vmeer::RuntimeContext::Get();
    vContext.SetVirtualUid(vUid);
    vContext.SetMirrorPath(path);

    requestNamespaceSetup("com.vmeer.guest", vUid);

    init_art_hook(env);
    
    jclass context_clazz = env->GetObjectClass(context);
    jmethodID get_class_loader_mid = env->GetMethodID(context_clazz, "getClassLoader", "()Ljava/lang/ClassLoader;");
    jobject class_loader = env->CallObjectMethod(context, get_class_loader_mid);

    perform_mirror_injection(env, class_loader, path);

    syncJavaProperties(env);

    env->ReleaseStringUTFChars(mirrorPath, path);
    LOGI("vMeer Engine: VM Setup for vUID %d is LIVE.", vUid);
}

/**
 * JNI_OnLoad
 */
JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* res) {
    (void)res;
    JNIEnv* env;
    if (vm->GetEnv((void**)&env, JNI_VERSION_1_6) != JNI_OK) return JNI_ERR;

    LOGI("vMeer Engine: Booting Core Systems...");

    init_vmeer_stealth();
    
    // PERUBAHAN UTAMA: Ubah ke SHADOWHOOK_MODE_SHARED agar toleran dengan custom ROM/perangkat modern
    int sh_status = shadowhook_init(SHADOWHOOK_MODE_SHARED, false);
    if (sh_status != 0) {
        // JANGAN RETURN JNI_ERR! Izinkan engine berjalan dengan status bypass agar Java tidak mengalami FC
        LOGW("WARNING: ShadowHook gagal inisialisasi (Code: %d). Berjalan dalam Fallback Mode.", sh_status);
    } else {
        LOGI("vMeer: ShadowHook Shared Engine successfully engaged.");
    }

    // Eksekusi core module pendukung jika ShadowHook aman
    if (sh_status == 0) {
        vmeer::helper::ConnectSharedState();
        vmeer::zygote::HookForkAndSpecialize();
        start_binder_proxy();
        vmeer::binder::InitHooks();
        start_virtual_system_services(); 
        start_egl_bridge();              
        vmeer::sensor::InitHooks();      
        vmeer::vfs::StartVFSEngine();
    }

    LOGI("vMeer Engine: Status READY - Engine v1.0.0-STABLE");
    return JNI_VERSION_1_6;
}

JNIEXPORT void JNICALL JNI_OnUnload(JavaVM* vm, void* reserved) {
    (void)vm; (void)reserved;
    LOGI("vMeer Engine: Engine Detached.");
}

} // extern "C"
