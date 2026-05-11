#include <jni.h>
#include <android/log.h>
#include <stdint.h>
#include <dlfcn.h>
#include <unistd.h>
#include <string.h>

// Header dari ShadowHook
#include "shadowhook.h"

#define LOG_TAG "vMeer_Engine"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// --- Fungsi Pointer untuk Internal Binder ---
typedef int (*p_size_t)(const void* p);
typedef const uint8_t* (*p_ptr_t)(const void* p);
typedef void (*p_set_pos_t)(void* p, int pos);
typedef int (*p_write_i32_t)(void* p, int32_t v);

static p_size_t p_size = nullptr;
static p_ptr_t p_ptr = nullptr;
static p_set_pos_t p_set_pos = nullptr;
static p_write_i32_t p_write_i32 = nullptr;

// --- Pointer Fungsi Asli (Original) ---
typedef int (*transact_t)(void* instance, uint32_t code, void* data, void* reply, uint32_t flags);
static transact_t original_transact = nullptr;

// --- Inisialisasi Simbol Binder ---
void init_vmeer_internals() {
    void* h = dlopen("libbinder.so", RTLD_NOW);
    if (h) {
        // Simbol ini spesifik untuk Android Parcel
        p_size = (p_size_t)dlsym(h, "_ZNK7android6Parcel8dataSizeEv");
        p_ptr = (p_ptr_t)dlsym(h, "_ZNK7android6Parcel9dataDebugEv");
        p_set_pos = (p_set_pos_t)dlsym(h, "_ZN7android6Parcel15setDataPositionEi");
        p_write_i32 = (p_write_i32_t)dlsym(h, "_ZN7android6Parcel10writeInt32Ei");
        LOGI("Internal Binder symbols initialized.");
    } else {
        LOGE("Failed to dlopen libbinder.so");
    }
}

// --- Handler Hook Transact ---
int vmeer_hook_transact(void* inst, uint32_t code, void* data, void* reply, uint32_t flags) {
    if (p_ptr && p_size && p_set_pos && p_write_i32) {
        int32_t real_uid = (int32_t)getuid();
        int size = p_size(data);
        const uint8_t* raw = p_ptr(data);
        
        if (raw && size > 4) {
            // Logika manipulasi UID (Ubah ke System 1000)
            for (int i = 0; i < (size - 4); i += 4) {
                if (*(const int32_t*)(raw + i) == real_uid) {
                    p_set_pos(data, i);
                    p_write_i32(data, 1000); 
                }
            }
        }
    }
    // Panggil fungsi asli melalui original_transact (hasil dari shadowhook_hook_sym_name)
    return original_transact(inst, code, data, reply, flags);
}

// --- JNI OnLoad (Entry Point) ---
extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* res) {
    LOGI("vMeer Engine loading...");

    // 1. Inisialisasi simbol internal
    init_vmeer_internals();

    // 2. Inisialisasi ShadowHook (Mode Unique)
    if (shadowhook_init(SHADOWHOOK_MODE_UNIQUE, false) != 0) {
        LOGE("ShadowHook init failed!");
        return JNI_ERR;
    }

    // 3. Lakukan Hooking (Fix: shadowhook_hook_sym_name dengan underscore)
    void* stub = shadowhook_hook_sym_name(
        "libbinder.so", 
        "_ZN7android15IPCThreadState8transactEiRKNS_6ParcelEPS1_j", 
        (void*)vmeer_hook_transact, 
        (void**)&original_transact
    );

    if (stub == nullptr) {
        int err_num = shadowhook_get_errno();
        LOGE("Hook failed! Error code: %d", err_num);
    } else {
        LOGI("Hook libbinder.so success!");
    }

    return JNI_VERSION_1_6;
}
