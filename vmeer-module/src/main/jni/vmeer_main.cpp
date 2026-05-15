#include <jni.h>
#include <android/log.h>
#include "shadowhook.h"

#define LOG_TAG "vMeer_Core"

// Linker mencari simbol dengan nama C murni
extern "C" {
    void install_binder_hooks();
    void vmeer_fuse_init(const char* rom, const char* mnt);
}

extern "C" void vmeer_main_init() {
    __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "vMeer Main Module: Initializing...");
    install_binder_hooks();
    __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "vMeer Main Module: Ready.");
}

extern "C" JNIEXPORT void JNICALL
Java_com_vmeer_engine_Core_nativeStartEngine(JNIEnv *env, jobject thiz, jstring rom_path, jstring mount_path) {
    (void)thiz;
    if (!rom_path || !mount_path) return;
    const char *rom = env->GetStringUTFChars(rom_path, nullptr);
    const char *mnt = env->GetStringUTFChars(mount_path, nullptr);
    vmeer_fuse_init(rom, mnt);
    env->ReleaseStringUTFChars(rom_path, rom);
    env->ReleaseStringUTFChars(mount_path, mnt);
}
