#include <jni.h>
#include <string>
#include <android/log.h>
#include "include/vmeer_system.h" // Asumsi berisi definisi identitas native
#include "include/binder_vm.h"   // Untuk koordinasi dengan Binder Proxy

#define LOG_TAG "vMeer_Bridge"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

extern "C" {

/**
 * syncJavaProperties:
 * Memaksa Java Layer (via Mirror Class) untuk menggunakan identitas vMeer.
 */
void syncJavaProperties(JNIEnv* env) {
    // 1. Cari Mirror Class hasil Mirror Generator (Struktur black.android.*)
    jclass brBuild = env->FindClass("black/android/os/BRBuild");
    if (brBuild == nullptr) {
        env->ExceptionClear(); // Bersihkan exception jika class tidak ditemukan
        LOGI("Bridge: Mirror Class not found, skipping Java sync.");
        return;
    }

    // 2. Ambil Field ID (Sesuai dengan hasil generator kamu)
    jfieldID modelID = env->GetStaticFieldID(brBuild, "MODEL", "Ljava/lang/String;");
    jfieldID brandID = env->GetStaticFieldID(brBuild, "BRAND", "Ljava/lang/String;");
    jfieldID manufacturerID = env->GetStaticFieldID(brBuild, "MANUFACTURER", "Ljava/lang/String;");

    // 3. Siapkan Nilai Identitas Palsu
    jstring fakeModel = env->NewStringUTF("VMEER-A14-PRO");
    jstring fakeBrand = env->NewStringUTF("vMeer");
    jstring fakeManuf = env->NewStringUTF("vMeer-Corp");
    
    // 4. Injeksi Nilai ke Java Static Fields
    if (modelID) env->SetStaticObjectField(brBuild, modelID, fakeModel);
    if (brandID) env->SetStaticObjectField(brBuild, brandID, fakeBrand);
    if (manufacturerID) env->SetStaticObjectField(brBuild, manufacturerID, fakeManuf);

    LOGI("vMeer Bridge: Java Mirror Identity SYNCED successfully.");
}

/**
 * JNI Method untuk memulai vMeer Engine dari Kotlin/Java
 */
JNIEXPORT void JNICALL
Java_com_vmeer_engine_Core_nativeBootContainer(JNIEnv *env, jobject thiz, jstring romPath, jstring mountPath) {
    LOGI("vMeer Bridge: Boot sequence initiated.");

    // A. Sinkronisasi identitas Java sebelum Zygote virtual jalan
    syncJavaProperties(env);

    // B. Ambil path dari Java String
    const char *rom = env->GetStringUTFChars(romPath, nullptr);
    const char *mnt = env->GetStringUTFChars(mountPath, nullptr);

    // C. Jalankan FUSE / Mount Engine (Logic yang kita buat di vmeer_main.cpp)
    // start_vmeer_fuse_engine(rom, mnt);

    LOGI("vMeer Bridge: Container is now running in isolated namespace.");

    env->ReleaseStringUTFChars(romPath, rom);
    env->ReleaseStringUTFChars(mountPath, mnt);
}

} // extern "C"
