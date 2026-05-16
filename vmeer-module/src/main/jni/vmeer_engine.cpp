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

// --- Logic Modules ---
#include "binder_engine.h"
#include "sensor_engine.h"
#include "vmeer_pms.h"
#include "egl_bridge.h"

// --- FUSE & ZSTD Headers ---
#include <fuse.h>
#include <zstd.h>

#define LOG_TAG "vMeer_Engine"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// Pointer fungsi asli untuk dipanggil kembali di dalam proxy
static int (*orig_prctl)(int option, unsigned long arg2, unsigned long arg3, unsigned long arg4, unsigned long arg5) = nullptr;
static void* (*orig_ipc_transact)(void* thiz, uint32_t code, const void* data, void* reply, uint32_t flags) = nullptr;

extern "C" {
void init_art_hook(JNIEnv* env);
void perform_mirror_injection(JNIEnv* env, jobject class_loader, const char* path);
void syncJavaProperties(JNIEnv* env);
}

// ====================================================================
// ADVANCED INTERCEPTOR: SECCOMP BYPASS VIA PRCTL SPOOFING
// ====================================================================
static int proxy_prctl(int option, unsigned long arg2, unsigned long arg3, unsigned long arg4, unsigned long arg5) {
    // Jika Guest OS (init) mencoba memeriksa status Seccomp kernel
    if (option == PR_GET_SECCOMP) {
        // Tipu Guest OS seolah-olah Seccomp MATI (Mode 0) agar booting jalan terus
        return 0; 
    }
    // Jika ada subsistem yang mencoba memperketat sandbox di tengah jalan
    if (option == PR_SET_SECCOMP) {
        LOGW("vMeer_Seccomp: Menggagalkan upaya penguncian sandboxing tambahan (PR_SET_SECCOMP).");
        return 0; // Kembalikan sukses palsu ke pemanggil
    }
    return orig_prctl(option, arg2, arg3, arg4, arg5);
}

// ====================================================================
// ADVANCED LINKER: SHADOWHOOK HIGH-LEVEL SYMBOL RESOLVER (HYPEROS FIX)
// ====================================================================
static void* FindBinderTransactBySymbolResolver() {
    // Muat libbinder.so ke dalam memory space aplikasi menggunakan loader internal shadowhook
    void* handle = shadowhook_dlopen("libbinder.so");
    if (!handle) {
        LOGE("vMeer_Binder: Gagal membuka objek libbinder.so via ShadowHook Loader.");
        return nullptr;
    }

    void* addr = nullptr;

    // Daftar variasi tanda tangan simbol (Mangled Name) IPCThreadState::transact di Android 14 & 15
    const char* mangled_symbols[] = {
        "_ZN7android15IPCThreadState8transactEijRKNS_14ParcelEPS1_j", // Standard Android 14/15 64-bit
        "_ZN7android15IPCThreadState8transactEijRKNS_6ParcelEPS1_j",  // Alternatif Vendor Custom
        "_ZN7android15IPCThreadState8transactEiRKNS_14ParcelEPS1_j"    // Fallback Legacy
    };

    // Lakukan brute-force pencarian simbol privat di dalam tabel ELF (.symtab / .strtab)
    for (const char* sym : mangled_symbols) {
        addr = shadowhook_dlsym(handle, sym);
        if (addr) {
            LOGI("vMeer_Binder: MATCH SUCCESS! Menemukan simbol %s di alamat: %p", sym, addr);
            break;
        }
    }

    shadowhook_dlclose(handle);
    return addr;
}

static void* proxy_binder_transact(void* thiz, uint32_t code, const void* data, void* reply, uint32_t flags) {
    // Jembatan transaksi IPC Guest OS ke Host OS
    return orig_ipc_transact(thiz, code, data, reply, flags);
}

// Helper internal untuk daemon namespace
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

JNIEXPORT jboolean JNICALL
Java_com_vmeer_io_VMeerEngine_isInitialized(JNIEnv *env, jclass clazz) {
    (void)env; (void)clazz;
    return JNI_TRUE;
}

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

JNIEXPORT jboolean JNICALL
Java_com_vmeer_io_VMeerEngine_prepareStorageSandbox(JNIEnv *env, jclass clazz, jstring pkgName, jint vUid) {
    (void)clazz;
    const char *native_pkg = env->GetStringUTFChars(pkgName, nullptr);
    bool result = requestNamespaceSetup(native_pkg, vUid);
    env->ReleaseStringUTFChars(pkgName, native_pkg);
    return result ? JNI_TRUE : JNI_FALSE;
}

/**
 * setupVM: PROSES UTAMA KONFIGURASI ENGINE DAN MEMORY VIRTUAL
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

    // Buka file ROM secara langsung untuk dipetakan ke memori virtual space proses
    int rom_fd = open(path, O_RDONLY);
    if (rom_fd >= 0) {
        struct stat st;
        if (fstat(rom_fd, &st) == 0) {
            size_t rom_size = st.st_size;
            
            // Lakukan Direct Memory Mapping dari berkas ROM SquashFS secara sinkron.
            void* vma_addr = mmap(nullptr, rom_size, PROT_READ, MAP_PRIVATE, rom_fd, 0);
            if (vma_addr != MAP_FAILED) {
                LOGI("vMeer_Engine: ROM sukses dipetakan via Zero-Copy Static Map di alamat %p", vma_addr);
            } else {
                LOGE("vMeer_Engine: Gagal melakukan mmap ROM secara statis: %s", strerror(errno));
            }
        }
        close(rom_fd);
    } else {
        LOGE("vMeer_Engine: Gagal membuka berkas ROM SquashFS: %s", strerror(errno));
    }

    // --- HOOK SECCOMP & PRCTL UNTUK MEMBUKA BLOKIR SYSCALL ---
    void* libc_handle = dlopen("libc.so", RTLD_NOW);
    if (libc_handle) {
        shadowhook_hook_sym_name("libc.so", "prctl", (void*)proxy_prctl, (void**)&orig_prctl);
        LOGI("vMeer_Seccomp: Interceptor prctl aktif. Pengelabuan Seccomp Mode 2 siap.");
    }

    // --- RE-HOOK BINDER VIA SHADOWHOOK INTERNAL RESOLVER ---
    void* binder_transact_addr = FindBinderTransactBySymbolResolver();
    if (binder_transact_addr) {
        shadowhook_hook_func_addr(binder_transact_addr, (void*)proxy_binder_transact, (void**)&orig_ipc_transact);
        LOGI("vMeer_Binder: SUCCESS - Binder Bridge berhasil dikunci mapan!");
    } else {
        LOGE("vMeer_Binder: CRITICAL - Gagal menemukan simbol biner privat. Menjalankan fallback standar...");
        void* libc_fallback = dlopen("libbinder.so", RTLD_NOW);
        if (libc_fallback) {
             void* fallback_addr = dlsym(libc_fallback, "_ZN7android15IPCThreadState8transactEijRKNS_14ParcelEPS1_j");
             if (fallback_addr) {
                 shadowhook_hook_func_addr(fallback_addr, (void*)proxy_binder_transact, (void**)&orig_ipc_transact);
                 LOGI("vMeer_Binder: Fallback sukses mengunci simbol standar.");
             }
        }
    }

    // Jalankan jembatan ART Java runtime milikmu
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
    
    int sh_status = shadowhook_init(SHADOWHOOK_MODE_SHARED, false);
    if (sh_status != 0) {
        LOGW("WARNING: ShadowHook gagal inisialisasi (Code: %d). Berjalan dalam Fallback Mode.", sh_status);
    } else {
        LOGI("vMeer: ShadowHook Shared Engine successfully engaged.");
    }

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
