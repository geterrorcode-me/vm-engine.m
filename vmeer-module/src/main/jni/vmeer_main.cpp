#include <jni.h>
#include <android/log.h>
#include "shadowhook.h"

#define LOG_TAG "vMeer_Core"

// Deklarasi fungsi eksternal
extern void install_binder_hooks();
extern void vmeer_fuse_init(const char* rom, const char* mnt); // Dari file FUSE kita sebelumnya

extern "C"
JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
    __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "vMeer Engine: Initializing Core...");

    // 1. Inisialisasi ShadowHook (Mode Shared agar bisa dipakai bareng library lain)
    if (shadowhook_init(SHADOWHOOK_MODE_SHARED, false) != 0) {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "ShadowHook init failed!");
        return JNI_ERR;
    }

    // 2. Pasang Binder Hooks untuk memalsukan identitas container
    install_binder_hooks();

    // 3. (Opsional) Hook fungsi-fungsi pendeteksi root standar
    // shadowhook_hook_symname("libc.so", "fopen", ...);
    // shadowhook_hook_symname("libc.so", "access", ...);

    __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "vMeer Engine: Shield Active.");
    return JNI_VERSION_1_6;
}

// JNI Method untuk menjalankan FUSE dari Java/Kotlin
extern "C"
JNIEXPORT void JNICALL
Java_com_vmeer_engine_Core_nativeStartEngine(JNIEnv *env, jobject thiz, jstring rom_path, jstring mount_path) {
    const char *rom = env->GetStringUTFChars(rom_path, nullptr);
    const char *mnt = env->GetStringUTFChars(mount_path, nullptr);

    __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "Starting FUSE Mount: %s -> %s", rom, mnt);
    
    // Panggil fungsi init FUSE (Blocking)
    vmeer_fuse_init(rom, mnt);

    env->ReleaseStringUTFChars(rom_path, rom);
    env->ReleaseStringUTFChars(mount_path, mnt);
}
