/**
 * vMeer Engine - Core Implementation (Production Grade)
 * Path: vmeer-module/src/main/jni/vmeer_engine.cpp
 * * Includes: Stealth Hooking, UID Scanning, and Binder Interception.
 */

#include <jni.h>
#include <android/log.h>
#include <stdint.h>
#include <dlfcn.h>
#include <unistd.h>
#include "shadowhook.h"

#define LOG_TAG "vMeer_Engine"

// --- Binder Function Types ---
typedef int (*transact_t)(void* instance, uint32_t code, void* data, void* reply, uint32_t flags);
static transact_t original_transact = nullptr;

typedef int (*parcel_data_size_t)(const void* parcel);
typedef const uint8_t* (*parcel_data_ptr_t)(const void* parcel);
typedef void (*parcel_set_pos_t)(void* parcel, int pos);
typedef int (*parcel_write_int32_t)(void* parcel, int32_t val);

// --- Internal Pointers ---
static parcel_data_size_t p_size = nullptr;
static parcel_data_ptr_t p_ptr = nullptr;
static parcel_set_pos_t p_set_pos = nullptr;
static parcel_write_i32_t p_write_i32 = nullptr;

/**
 * Mendapatkan akses ke fungsi internal libbinder.so secara dinamis
 */
void init_vmeer_internals() {
    void* handle = dlopen("libbinder.so", RTLD_NOW);
    if (handle) {
        p_size = (parcel_data_size_t)dlsym(handle, "_ZNK7android6Parcel8dataSizeEv");
        p_ptr = (parcel_data_ptr_t)dlsym(handle, "_ZNK7android6Parcel9dataDebugEv");
        p_set_pos = (parcel_set_pos_t)dlsym(handle, "_ZN7android6Parcel15setDataPositionEi");
        p_write_i32 = (parcel_write_i32_t)dlsym(handle, "_ZN7android6Parcel10writeInt32Ei");
    }
}

/**
 * Proxy Transact: Inti dari manipulasi identitas vMeer
 */
int vmeer_hook_transact(void* inst, uint32_t code, void* data, void* reply, uint32_t flags) {
    if (p_ptr && p_size && p_set_pos && p_write_i32) {
        int32_t real_uid = getuid();
        int size = p_size(data);
        const uint8_t* raw = p_ptr(data);
        
        // Memindai Parcel untuk memalsukan UID aplikasi menjadi System UID (1000)
        if (raw && size > 4) {
            for (int i = 0; i < (size - 4); i += 4) {
                if (*(const int32_t*)(raw + i) == real_uid) {
                    int old_pos = i; // Sederhananya kita asumsikan kursor perlu kembali
                    p_set_pos(data, i);
                    p_write_i32(data, 1000); 
                }
            }
        }
    }
    // Teruskan ke Binder Driver asli
    return original_transact(inst, code, data, reply, flags);
}

extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* res) {
    __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "vMeer Engine: Stealth Booting...");
    
    init_vmeer_internals();
    
    // Inisialisasi ShadowHook dalam mode unik agar sulit dideteksi
    if (shadowhook_init(SHADOWHOOK_MODE_UNIQUE, false) != 0) {
        return JNI_ERR;
    }
    
    // Melakukan Hook pada fungsi transact di libbinder.so
    shadowhook_hook_symname(
        "libbinder.so",
        "_ZN7android15IPCThreadState8transactEiRKNS_6ParcelEPS1_j",
        (void*)vmeer_hook_transact,
        (void**)&original_transact
    );
    
    __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "vMeer Engine: Core Activated.");
    return JNI_VERSION_1_6;
}
