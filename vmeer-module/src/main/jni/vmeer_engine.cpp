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

// Alamat asli fungsi yang di-hook
static int (*orig_prctl)(int option, unsigned long arg2, unsigned long arg3, unsigned long arg4, unsigned long arg5) = nullptr;
static void* (*orig_ipc_transact)(void* thiz, uint32_t code, const void* data, void* reply, uint32_t flags) = nullptr;

// ====================================================================
// ADVANCED INTERCEPTOR: SECCOMP BYPASS VIA PRCTL SPOOFING
// ====================================================================
static int proxy_prctl(int option, unsigned long arg2, unsigned long arg3, unsigned long arg4, unsigned long arg5) {
    // Jika Guest OS (init) mencoba memeriksa status atau menyetel batasan seccomp lebih ketat
    if (option == PR_GET_SECCOMP) {
        // Tipu Guest OS seolah-olah Seccomp MATI (Mode 0) agar dia tidak panik dan mogok booting
        return 0; 
    }
    if (option == PR_SET_SECCOMP) {
        LOGW("vMeer_Seccomp: Menggagalkan upaya penguncian sandboxing tambahan (PR_SET_SECCOMP).");
        return 0; // Kembalikan sukses palsu
    }
    return orig_prctl(option, arg2, arg3, arg4, arg5);
}

// ====================================================================
// ADVANCED LINKER: PATTERN SCANNER UNTUK BINDER HYPEROS (ANDROID 15)
// ====================================================================
static void* FindBinderTransactByPattern() {
    void* handle = dlopen("libbinder.so", RTLD_NOW);
    if (!handle) return nullptr;

    // Skenario 1: Coba cari simbol standar fallback dulu
    const char* fallbacks[] = {
        "_ZN7android15IPCThreadState8transactEijRKNS_14ParcelEPS1_j",
        "_ZN7android15IPCThreadState8transactEijRKNS_6ParcelEPS1_j"
    };
    for (const char* sym : fallbacks) {
        void* addr = dlsym(handle, sym);
        if (addr) return addr;
    }

    // Skenario 2: PATTERN SCANNING (Jika simbol disembunyikan total oleh Xiaomi)
    // Memindai opcode standar dari fungsi IPCThreadState::transact pada arsitektur ARM64
    // Ciri khas ARM64 Prologue: pacibsp / stp x29, x30, [sp, ...]
    LOGW("vMeer_Binder: Simbol ELF disembunyikan. Memulai pemindaian biner kasar di memori...");
    
    FILE* maps = fopen("/proc/self/maps", "r");
    if (!maps) return nullptr;

    char line[512];
    uintptr_t start_addr = 0, end_addr = 0;
    while (fgets(line, sizeof(line), maps)) {
        if (strstr(line, "libbinder.so") && strstr(line, "r-xp")) { // Cari segmen executable
            sscanf(line, "%lx-%lx", &start_addr, &end_addr);
            break;
        }
    }
    fclose(maps);

    if (start_addr && end_addr) {
        // Nyari instruksi assembly khas transaksi binder ARM64 secara brutal
        for (uintptr_t i = start_addr; i < end_addr - 16; i += 4) {
            uint32_t* insn = reinterpret_cast<uint32_t*>(i);
            // Contoh kecocokan register internal transaksi binder (AArch64)
            if (insn[0] == 0xa9bf7bfd && insn[1] == 0x910003fd) { // stp x29, x30, [sp, #-16]!; mov x29, sp
                LOGI("vMeer_Binder: PATTERN MATCH! Menemukan fungsi kandidat transact di: %p", (void*)i);
                return reinterpret_cast<void*>(i);
            }
        }
    }
    return nullptr;
}

static void* proxy_binder_transact(void* thiz, uint32_t code, const void* data, void* reply, uint32_t flags) {
    // Di sini tempat kamu menyaring atau memanipulasi transaksi antar-proses Guest OS
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
void init_art_hook(JNIEnv* env);
void perform_mirror_injection(JNIEnv* env, jobject class_loader, const char* path);
void syncJavaProperties(JNIEnv* env);

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

    int rom_fd = open(path, O_RDONLY);
    if (rom_fd >= 0) {
        struct stat st;
        if (fstat(rom_fd, &st) == 0) {
            size_t rom_size = st.st_size;
            void* vma_addr = mmap(nullptr, rom_size, PROT_READ, MAP_PRIVATE, rom_fd, 0);
            if (vma_addr != MAP_FAILED) {
                LOGI("vMeer_Engine: ROM sukses dipetakan via Zero-Copy Static Map di alamat %p", vma_addr);
            } else {
                LOGE("vMeer_Engine: Gagal melakukan mmap ROM: %s", strerror(errno));
            }
        }
        close(rom_fd);
    }

    // --- SUNTIKAN HOOK SECCOMP & PRCTL TEPAT SEBELUM BOOTING ---
    void* libc_handle = dlopen("libc.so", RTLD_NOW);
    if (libc_handle) {
        shadowhook_hook_sym_name("libc.so", "prctl", (void*)proxy_prctl, (void**)&orig_prctl);
        LOGI("vMeer_Seccomp: Interceptor prctl aktif. Pengelabuan Seccomp Mode 2 siap.");
    }

    // --- RE-HOOK BINDER VIA PATTERN SCANNER ---
    void* binder_transact_addr = FindBinderTransactByPattern();
    if (binder_transact_addr) {
        shadowhook_hook_func_addr(binder_transact_addr, (void*)proxy_binder_transact, (void**)&orig_ipc_transact);
        LOGI("vMeer_Binder: SUCCESS - Binder Bridge berhasil dikunci via Pattern Scanning!");
    } else {
        LOGE("vMeer_Binder: CRITICAL - Gagal menemukan basis alamat transaksi IPC!");
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

    return JNI_VERSION_1_6;
}

} // extern "C"
