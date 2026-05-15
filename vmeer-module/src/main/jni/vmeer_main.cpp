#include <jni.h>
#include <android/log.h>
#include "shadowhook.h"

#define LOG_TAG "vMeer_Core"

/**
 * PENTING: JNI_OnLoad DIHAPUS dari file ini karena sudah ada di vmeer_engine.cpp.
 * Linker (ld) akan error "duplicate symbol" jika ada dua JNI_OnLoad dalam satu library.
 */

// Deklarasi fungsi eksternal
extern void install_binder_hooks();
extern "C" void vmeer_fuse_init(const char* rom, const char* mnt); 

/**
 * vmeer_main_init:
 * Fungsi helper untuk menjalankan logika inisialisasi yang sebelumnya ada di JNI_OnLoad.
 * Fungsi ini bisa dipanggil dari JNI_OnLoad milik vmeer_engine.cpp jika diperlukan.
 */
extern "C" void vmeer_main_init() {
    __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "vMeer Main Module: Initializing...");
    
    // Panggil hooks yang spesifik untuk module ini
    install_binder_hooks();
    
    __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "vMeer Main Module: Ready.");
}

// JNI Method untuk menjalankan FUSE dari Java/Kotlin tetap dipertahankan
extern "C"
JNIEXPORT void JNICALL
Java_com_vmeer_engine_Core_nativeStartEngine(JNIEnv *env, jobject thiz, jstring rom_path, jstring mount_path) {
    (void)thiz;
    if (!rom_path || !mount_path) return;

    const char *rom = env->GetStringUTFChars(rom_path, nullptr);
    const char *mnt = env->GetStringUTFChars(mount_path, nullptr);

    __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "Starting FUSE Mount: %s -> %s", rom, mnt);
    
    // Panggil fungsi init FUSE
    vmeer_fuse_init(rom, mnt);

    env->ReleaseStringUTFChars(rom_path, rom);
    env->ReleaseStringUTFChars(mount_path, mnt);
}
