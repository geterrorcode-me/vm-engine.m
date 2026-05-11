#include <jni.h>
#include <android/log.h>
#include <stdint.h>
#include <dlfcn.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>

// Header dari ShadowHook
#include "shadowhook.h"

#define LOG_TAG "vMeer_Engine"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// =============================================================================
// BAGIAN 1: INTERNAL BINDER SYMBOLS & STRUCTURES
// =============================================================================

typedef int (*p_size_t)(const void* p);
typedef const uint8_t* (*p_ptr_t)(const void* p);
typedef void (*p_set_pos_t)(void* p, int pos);
typedef int (*p_write_i32_t)(void* p, int32_t v);

static p_size_t p_size = nullptr;
static p_ptr_t p_ptr = nullptr;
static p_set_pos_t p_set_pos = nullptr;
static p_write_i32_t p_write_i32 = nullptr;

typedef int (*transact_t)(void* instance, uint32_t code, void* data, void* reply, uint32_t flags);
static transact_t original_transact = nullptr;

/**
 * Inisialisasi simbol internal libbinder.so untuk manipulasi Parcel.
 */
void init_vmeer_internals() {
    void* h = dlopen("libbinder.so", RTLD_NOW);
    if (h) {
        p_size = (p_size_t)dlsym(h, "_ZNK7android6Parcel8dataSizeEv");
        p_ptr = (p_ptr_t)dlsym(h, "_ZNK7android6Parcel9dataDebugEv");
        p_set_pos = (p_set_pos_t)dlsym(h, "_ZN7android6Parcel15setDataPositionEi");
        p_write_i32 = (p_write_i32_t)dlsym(h, "_ZN7android6Parcel10writeInt32Ei");
        LOGI("vMeer: Internal Binder symbols initialized.");
    } else {
        LOGE("vMeer: Failed to dlopen libbinder.so");
    }
}

// =============================================================================
// BAGIAN 2: DAEMON IPC (NAMESPACE & STORAGE SETUP)
// =============================================================================

/**
 * Meminta vmeerd untuk menyiapkan Mount Namespace dan Storage Sandbox.
 * Menggunakan Abstract Socket (\0vmeer_daemon.cms) agar bypass SELinux.
 */
bool requestNamespaceSetup(const char* pkgName) {
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) return false;

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    
    // Abstract socket name: \0vmeer_daemon.cms
    addr.sun_path[0] = '\0'; 
    strncpy(addr.sun_path + 1, "vmeer_daemon.cms", sizeof(addr.sun_path) - 2);

    // Timeout 2 detik agar tidak hang
    struct timeval tv = {2, 0};
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);

    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) == 0) {
        char cmd[512];
        snprintf(cmd, sizeof(cmd), "PREPARE_STORAGE:%s", pkgName);
        
        if (send(sock, cmd, strlen(cmd), 0) > 0) {
            char response[16] = {0};
            if (recv(sock, response, sizeof(response), 0) > 0) {
                if (strcmp(response, "OK") == 0) {
                    LOGI("vMeer: Namespace setup for %s confirmed by daemon.", pkgName);
                    close(sock);
                    return true;
                }
            }
        }
    }

    LOGE("vMeer: Critical! Daemon connection failed. Sandbox might be compromised.");
    close(sock);
    return false;
}

// =============================================================================
// BAGIAN 3: BINDER HOOK HANDLER (UID SPOOFING)
// =============================================================================

/**
 * Interceptor untuk transact Binder.
 * Tugas: Memalsukan UID asli menjadi System UID (1000) di dalam Parcel.
 */
int vmeer_hook_transact(void* inst, uint32_t code, void* data, void* reply, uint32_t flags) {
    if (p_ptr && p_size && p_set_pos && p_write_i32) {
        int32_t real_uid = (int32_t)getuid();
        int size = p_size(data);
        const uint8_t* raw = p_ptr(data);
        
        if (raw && size > 4) {
            // Scan Parcel untuk menemukan real_uid dan menimpanya dengan 1000
            for (int i = 0; i < (size - 4); i += 4) {
                if (*(const int32_t*)(raw + i) == real_uid) {
                    p_set_pos(data, i);
                    p_write_i32(data, 1000); 
                }
            }
        }
    }
    
    // Kembalikan ke fungsi asli
    if (original_transact) {
        return original_transact(inst, code, data, reply, flags);
    }
    return -1;
}

// =============================================================================
// BAGIAN 4: JNI ENTRY POINT (BOOT LOADER)
// =============================================================================

extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* res) {
    LOGI("vMeer Engine: Booting progress [START]...");

    // 1. URUTAN 1 & 2: Hubungi Daemon untuk Namespace
    // Ganti ini dengan deteksi paket yang dinamis jika perlu
    if (!requestNamespaceSetup("com.vmeer.virtual.guest")) {
        LOGE("vMeer Engine: Failed to initialize Namespace. Aborting.");
        return JNI_ERR;
    }

    // 2. Persiapan simbol internal Binder
    init_vmeer_internals();

    // 3. Inisialisasi ShadowHook
    if (shadowhook_init(SHADOWHOOK_MODE_UNIQUE, false) != 0) {
        LOGE("vMeer Engine: ShadowHook init failed!");
        return JNI_ERR;
    }

    // 4. URUTAN 3: Hooking Binder Transact
    void* stub = shadowhook_hook_sym_name(
        "libbinder.so", 
        "_ZN7android15IPCThreadState8transactEiRKNS_6ParcelEPS1_j", 
        (void*)vmeer_hook_transact, 
        (void**)&original_transact
    );

    if (stub == nullptr) {
        LOGE("vMeer Engine: Hook failed with errno %d", shadowhook_get_errno());
    } else {
        LOGI("vMeer Engine: Hook libbinder.so successful.");
    }

    LOGI("vMeer Engine: Booting progress [100% COMPLETE]");
    return JNI_VERSION_1_6;
}
