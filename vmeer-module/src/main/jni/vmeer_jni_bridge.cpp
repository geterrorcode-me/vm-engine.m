#include <jni.h>
#include <string>
#include "include/vmeer_system.h"
#include <android/log.h>

#define LOG_TAG "vMeer_Bridge"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

extern "C" {

/**
 * syncJavaProperties:
 * Sinkronisasi identitas antara Native Hook dan Java Build Class.
 */
void syncJavaProperties(JNIEnv* env) {
    // 1. Cari Mirror Class hasil Mirror Generator kamu
    jclass brBuild = env->FindClass("black/android/os/BRBuild");
    if (brBuild == nullptr) {
        LOGI("Bridge: Mirror Class not found, skipping Java sync.");
        return;
    }

    // 2. Ambil Field ID dari Mirror Class (Reflective Bridge)
    jfieldID modelID = env->GetStaticFieldID(brBuild, "MODEL", "Ljava/lang/String;");
    jfieldID manufacturerID = env->GetStaticFieldID(brBuild, "MANUFACTURER", "Ljava/lang/String;");
    jfieldID brandID = env->GetStaticFieldID(brBuild, "BRAND", "Ljava/lang/String;");

    // 3. Suntikkan nilai yang sama dengan vmeer_system.cpp
    jstring fakeModel = env->NewStringUTF("VMEER-A14-PRO");
    jstring fakeBrand = env->NewStringUTF("vMeer");
    
    if (modelID) env->SetStaticObjectField(brBuild, modelID, fakeModel);
    if (brandID) env->SetStaticObjectField(brBuild, brandID, fakeBrand);

    LOGI("vMeer Bridge: Java & Native identities are now SYNCED.");
}

} // extern "C"
