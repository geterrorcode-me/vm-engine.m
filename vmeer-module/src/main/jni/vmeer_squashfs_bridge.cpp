#include <jni.h>
#include <string>
#include <android/log.h>
#include "zstd.h" // Library yang kita build tadi

#define LOG_TAG "vMeer_SquashFS"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

extern "C" {

// Fungsi untuk inisialisasi SquashFS menggunakan Zstd
jboolean Java_com_vmeer_core_NativeEngine_initSquashFS(JNIEnv* env, jobject thiz, jstring path) {
    const char* nativePath = env->GetStringUTFChars(path, nullptr);
    
    LOGI("Memuat Virtual File System dari: %s", nativePath);
    
    // Logic untuk mount squashfs via fuse akan diletakkan di sini
    // Untuk sekarang kita return true agar engine bisa lanjut booting
    
    env->ReleaseStringUTFChars(path, nativePath);
    return JNI_TRUE;
}

// Helper untuk mengecek versi Zstd yang terhubung
const char* get_zstd_version() {
    return ZSTD_versionString();
}

}
