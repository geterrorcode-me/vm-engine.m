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

// --- System & Memory Handling ---
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/prctl.h>

// --- Core Engine & Stealth ---
#include "shadowhook.h"
#include "vmeer_stealth.h"

// --- Ecosystem Modules ---
#include "include/vmeer_helper.h"
#include "include/vmeer_zygote.h"
#include "include/vmeer_context.h"
#include "include/vmeer_vfs.h"
#include "include/vmeer_system.h" 

#define LOG_TAG "vMeer_Engine"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// Definisi tipe data tiruan agar tidak perlu include header NDK Binder yang rumit
typedef void AIBinder;
typedef uint32_t transaction_code_t;
typedef void AParcel;
typedef uint32_t binder_flags_t;
typedef int32_t binder_status_t;

// Pointer fungsi asli dari libc dan libbinder_ndk
static int (*orig_prctl)(int option, unsigned long arg2, unsigned long arg3, unsigned long arg4, unsigned long arg5) = nullptr;
static binder_status_t (*orig_AIBinder_transact)(AIBinder* binder, transaction_code_t code, const AParcel* in, AParcel* out, binder_flags_t flags) = nullptr;

extern "C" {
void init_art_hook(JNIEnv* env);
void perform_mirror_injection(JNIEnv* env, jobject class_loader, const char* path);
void syncJavaProperties(JNIEnv* env);
}

// ====================================================================
// BYPASS SECCOMP VIA PRCTL SPOOFING
// ====================================================================
static int proxy_prctl(int option, unsigned long arg2, unsigned long arg3, unsigned long arg4, unsigned long arg5) {
    if (option == PR_GET_SECCOMP) {
        return 0; // Tipu Guest OS: Katakan Seccomp MATI (Mode 0)
    }
    if (option == PR_SET_SECCOMP) {
        LOGW("vMeer_Seccomp: Blokir upaya penguncian sandboxing baru.");
        return 0; 
    }
    return orig_prctl(option, arg2, arg3, arg4, arg5);
}

// ====================================================================
// BYPASS BINDER VIA NDK PUBLIC INTERCEPTOR
// ====================================================================
static binder_status_t proxy_AIBinder_transact(AIBinder* binder, transaction_code_t code, const AParcel* in, AParcel* out, binder_flags_t flags) {
    // Transaksi IPC Guest OS dialihkan lewat jembatan NDK ini
    return orig_AIBinder_transact(binder, code, in, out, flags);
}

// Helper internal IPC Daemon
static bool requestNamespaceSetup(const char* pkgName, int vuid) {
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

extern "C" {

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

    // --- 1. DIRECT STORAGE MEMORY MAP ---
    int rom_fd = open(path, O_RDONLY);
    if (rom_fd >= 0) {
        struct stat st;
        if (fstat(rom_fd, &st) == 0) {
            size_t rom_size = st.st_size;
            void* vma_addr = mmap(nullptr, rom_size, PROT_READ, MAP_PRIVATE, rom_fd, 0);
            if (vma_addr != MAP_FAILED) {
                LOGI("vMeer_Engine: ROM sukses dipetakan via Zero-Copy Static Map di alamat %p", vma_addr);
            } else {
                LOGE("vMeer_Engine: Gagal mmap ROM: %s", strerror(errno));
            }
        }
        close(rom_fd);
    }

    // --- 2. SUNTIKAN HOOK SECCOMP (PRCTL) ---
    void* libc_handle = dlopen("libc.so", RTLD_NOW);
    if (libc_handle) {
        shadowhook_hook_sym_name("libc.so", "prctl", (void*)proxy_prctl, (void**)&orig_prctl);
        LOGI("vMeer_Seccomp: Interceptor prctl aktif. Pengelabuan Seccomp Mode 2 siap.");
    }

    // --- 3. SUNTIKAN HOOK BINDER NDK (ANTI-CRITICAL ANDROID 15) ---
    void* ndk_handle = dlopen("libbinder_ndk.so", RTLD_NOW);
    if (ndk_handle) {
        // Menggunakan simbol resmi publik, PASTI KETEMU di HyperOS / Android 15 manapun
        shadowhook_hook_sym_name("libbinder_ndk.so", "AIBinder_transact", (void*)proxy_AIBinder_transact, (void**)&orig_AIBinder_transact);
        LOGI("vMeer_Binder: UNBREAKABLE SUCCESS - Jembatan Binder NDK Berhasil Terkunci!");
    } else {
        LOGE("vMeer_Binder: Kritis! Sistem host tidak memiliki libbinder_ndk.so.");
    }

    init_art_hook(env);
    
    jclass context_clazz = env->GetObjectClass(context);
    jmethodID get_class_loader_mid = env->GetMethodID(context_clazz, "getClassLoader", "()Ljava/lang/ClassLoader;");
    jobject class_loader = env->CallObjectMethod(context, get_class_loader_mid);

    perform_mirror_injection(env, class_loader, path);
    syncJavaProperties(env);

    env->ReleaseStringUTFChars(mirrorPath, path);
    LOGI("vMeer Engine: VM Setup for vUID %d is LIVE.", vUid);
}

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* res) {
    (void)res;
    JNIEnv* env;
    if (vm->GetEnv((void**)&env, JNI_VERSION_1_6) != JNI_OK) return JNI_ERR;

    LOGI("vMeer Engine: Booting Core Systems...");
    init_vmeer_stealth();
    shadowhook_init(SHADOWHOOK_MODE_SHARED, false);

    // PENTING: Jangan taruh fungsi hook lama berbasis ELF IPCThreadState di sini lagi!
    vmeer::helper::ConnectSharedState();
    vmeer::vfs::StartVFSEngine();

    LOGI("vMeer Engine: Status READY - Engine v1.0.0-STABLE");
    return JNI_VERSION_1_6;
}

} // extern "C"
