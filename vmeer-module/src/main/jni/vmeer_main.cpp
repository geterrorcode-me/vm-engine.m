#include <jni.h>
#include <android/log.h>

#define LOG_TAG "vMeer_MainGate"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

extern "C" {
    // Rujukan ke sistem penyimpanan virtual standalone FUSE Anda
    void vmeer_fuse_init(const char* rom, const char* mnt);
}

// SINKRONISASI JNI: Diselaraskan ke com.vmeer.io.VMeerEngine agar terbaca legal oleh Java Host
extern "C" JNIEXPORT void JNICALL
Java_com_vmeer_io_VMeerEngine_nativeStartEngine(JNIEnv *env, jobject thiz, jstring rom_path, jstring mount_path) {
    (void)thiz;
    if (!rom_path || !mount_path) return;
    
    LOGI("[VFS Gate] Memulai inisialisasi ruang penyimpanan terisolasi FUSE...");
    
    const char *rom = env->GetStringUTFChars(rom_path, nullptr);
    const char *mnt = env->GetStringUTFChars(mount_path, nullptr);
    
    // Mount ROM tamu ke dalam folder mount virtual sandbox
    vmeer_fuse_init(rom, mnt);
    
    env->ReleaseStringUTFChars(rom_path, rom);
    env->ReleaseStringUTFChars(mount_path, mnt);
    
    LOGI("[VFS Gate] Virtual Storage System FUSE berhasil dimuat.");
}
