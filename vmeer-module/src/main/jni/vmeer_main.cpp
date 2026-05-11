#include <jni.h>
#include <android/log.h>
#include "shadowhook.h"

#define LOG_TAG "vMeer_Core"

// Deklarasi fungsi dari file lain
extern void install_binder_hooks();

extern "C"
JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
    __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "Engine starting...");

    // 1. Inisialisasi ShadowHook
    if (shadowhook_init(SHADOWHOOK_MODE_SHARED, false) != 0) {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "ShadowHook init failed!");
        return JNI_ERR;
    }

    // 2. Pasang hooks
    install_binder_hooks();

    __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "Engine Ready.");
    return JNI_VERSION_1_6;
}
