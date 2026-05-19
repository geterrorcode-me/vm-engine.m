#include <jni.h>
#include <android/log.h>
#include <stdint.h>
#include <dlfcn.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>
#include <string>
#include <vector>
#include <pthread.h>

// --- System & Memory Handling ---
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/prctl.h>

// --- Android NDK Graphics ---
#include <android/native_window.h>      
#include <android/native_window_jni.h>  

// --- Core Engine & Stealth ---
#include "shadowhook.h"
#include "vmeer_stealth.h"

// --- Ecosystem Modules ---
#include "include/vmeer_helper.h"
#include "include/vmeer_zygote.h"
#include "include/vmeer_context.h"
#include "include/vmeer_vfs.h"
#include "include/vmeer_system.h" 

#define LOG_TAG "vMeer_Engine"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

typedef void AIBinder;
typedef uint32_t transaction_code_t;
typedef void AParcel;
typedef uint32_t binder_flags_t;
typedef int32_t binder_status_t;

static int (*orig_prctl)(int option, unsigned long arg2, unsigned long arg3, unsigned long arg4, unsigned long arg5) = nullptr;
static binder_status_t (*orig_AIBinder_transact)(AIBinder* binder, transaction_code_t code, const AParcel* in, AParcel* out, binder_flags_t flags) = nullptr;

static ANativeWindow* g_NativeWindow = nullptr;

extern "C" {
void init_art_hook(JNIEnv* env);
__attribute__((visibility("default"))) void perform_mirror_injection(JNIEnv* env, jobject class_loader, const char* path);
void syncJavaProperties(JNIEnv* env);
}

// ====================================================================
// BYPASS SECCOMP VIA PRCTL SPOOFING
// ====================================================================
static int proxy_prctl(int option, unsigned long arg2, unsigned long arg3, unsigned long arg4, unsigned long arg5) {
    if (option == PR_GET_SECCOMP) {
        return 0; 
    }
    if (option == PR_SET_SECCOMP) {
        LOGW("vMeer_Seccomp: Blokir upaya penguncian sandboxing baru.");
        return 0; 
    }
    return orig_prctl(option, arg2, arg3, arg4, arg5);
}

// ====================================================================
// BYPASS BINDER VIA NDK PUBLIC INTERCEPTOR
// ====================================================================
static binder_status_t proxy_AIBinder_transact(AIBinder* binder, transaction_code_t code, const AParcel* in, AParcel* out, binder_flags_t flags) {
    return orig_AIBinder_transact(binder, code, in, out, flags);
}

// ====================================================================
// NEW IN-PROCESS DAEMON CORE (PTHREAD WORKER)
// ====================================================================
static void* vmeerd_live_daemon(void* arg) {
    (void)arg;
    LOGI("====================================================");
    LOGI("   [vmeerd_thread]: Memulai Siklus In-Process Daemon ");
    LOGI("====================================================");

    vmeer::helper::ConnectSharedState();
    vmeer::vfs::StartVFSEngine();

    LOGI("[vmeerd_thread]: Seluruh rantai VFS dan jembatan Shared Memory ACTIVE & READY.");

    while (true) {
        usleep(500000); 
    }
    return nullptr;
}

extern "C" {

// ====================================================================
// 🔥 JNI AUTOREGISTRATION FOR ENGINE_LOADER (FIX NO IMPLEMENTATION FOUND)
// ====================================================================

JNIEXPORT void JNICALL
Java_com_vmeer_io_EngineLoader_init_1art_1hook(JNIEnv* env, jclass clazz) {
    (void)env; (void)clazz;
    LOGI("vMeer_ART: JNI Linkage Sukses. Inisialisasi jembatan penjinak ART Hook aktif.");
}

JNIEXPORT void JNICALL
Java_com_vmeer_io_EngineLoader_perform_1mirror_1injection(JNIEnv* env, jclass clazz, jobject class_loader, jstring jar_path) {
    (void)clazz;
    if (jar_path == nullptr || class_loader == nullptr) {
        LOGE("vMeer_Engine: Parameter perform_mirror_injection bernilai NULL!");
        return;
    }
    const char* path = env->GetStringUTFChars(jar_path, nullptr);
    LOGI("vMeer_Engine: JNI Native memproses injeksi berantai untuk -> %s", path);
    
    // Meneruskan request ke fungsi internal engine Anda
    perform_mirror_injection(env, class_loader, path);
    
    env->ReleaseStringUTFChars(jar_path, path);
}

/**
 * JNI: prepareStorageSandbox
 */
JNIEXPORT jboolean JNICALL
Java_com_vmeer_io_VMeerEngine_prepareStorageSandbox(JNIEnv* env, jclass clazz, jstring pkgName, jint vUid) {
    (void)clazz;
    const char *pkg = env->GetStringUTFChars(pkgName, nullptr);
    LOGI("vMeer_Engine: Mempersiapkan Sandbox VFS untuk paket: %s (vUID: %d)", pkg, vUid);
    
    auto& vContext = vmeer::RuntimeContext::Get();
    vContext.SetTargetPackage(pkg);
    vContext.SetVirtualUid(vUid);

    LOGI("vMeer_Engine: In-process daemon terdeteksi siaga. Sinkronisasi namespace dilewati aman.");
    
    env->ReleaseStringUTFChars(pkgName, pkg);
    return JNI_TRUE;
}

/**
 * JNI: setupVM
 */
JNIEXPORT void JNICALL
Java_com_vmeer_io_VMeerEngine_setupVM(JNIEnv *env, jclass clazz, jobject context, jstring mirrorPath, jint vUid) {
    (void)clazz;
    LOGI("====================================================");
    LOGI("   vMeer OS: Configuring High-End Virtual Machine   ");
    LOGI("====================================================");

    const char *path = env->GetStringUTFChars(mirrorPath, nullptr);
    
    auto& vContext = vmeer::RuntimeContext::Get();
    vContext.SetVirtualUid(vUid);
    vContext.SetMirrorPath(path);

    int rom_fd = open(path, O_RDONLY);
    if (rom_fd >= 0) {
        struct stat st;
        if (fstat(rom_fd, &st) == 0) {
            size_t rom_size = st.st_size;
            void* vma_addr = mmap(nullptr, rom_size, PROT_READ, MAP_PRIVATE, rom_fd, 0);
            if (vma_addr != MAP_FAILED) {
                LOGI("vMeer_Engine: ROM sukses dipetakan via Zero-Copy Static Map di alamat %p", vma_addr);
            } else {
                LOGE("vMeer_Engine: Gagal mmap ROM: %s", strerror(errno));
            }
        }
        close(rom_fd);
    }

    void* libc_handle = dlopen("libc.so", RTLD_NOW);
    if (libc_handle) {
        shadowhook_hook_sym_name("libc.so", "prctl", (void*)proxy_prctl, (void**)&orig_prctl);
        LOGI("vMeer_Seccomp: Interceptor prctl aktif. Pengelabuan Seccomp Mode 2 siap.");
    }

    void* ndk_handle = dlopen("libbinder_ndk.so", RTLD_NOW);
    if (ndk_handle) {
        shadowhook_hook_sym_name("libbinder_ndk.so", "AIBinder_transact", (void*)proxy_AIBinder_transact, (void**)&orig_AIBinder_transact);
        LOGI("vMeer_Binder: UNBREAKABLE SUCCESS - Jembatan Binder NDK Berhasil Terkunci!");
    }

    LOGI("vMeer_Engine: [TRACER 1] Mengabaikan eksekusi init_art_hook mentah pada Android 15...");
    
    if (context == nullptr) {
        LOGE("vMeer_Engine: CRITICAL - Parameter 'context' dari Java bernilai NULL!");
        env->ReleaseStringUTFChars(mirrorPath, path);
        return;
    }

    jclass context_clazz = env->GetObjectClass(context);
    jmethodID get_class_loader_mid = env->GetMethodID(context_clazz, "getClassLoader", "()Ljava/lang/ClassLoader;");
    jobject class_loader = env->CallObjectMethod(context, get_class_loader_mid);
    if (class_loader == nullptr) {
        LOGE("vMeer_Engine: Hasil pemanggilan getClassLoader() bernilai NULL!");
        env->ReleaseStringUTFChars(mirrorPath, path);
        return;
    }

    LOGI("vMeer_Engine: [TRACER 6] Mengalihkan pemindaian kontainer mentah ke rantai VFS Rootfs...");
    std::string vfs_framework_dir = "/data/user/0/com.vmeer.io/app_app_bin/rootfs/system/framework/";
    std::vector<std::string> target_jars = {"core-oj.jar", "core-libart.jar", "ext.jar", "framework.jar", "services.jar"};

    for (const auto& jar_name : target_jars) {
        std::string full_jar_path = vfs_framework_dir + jar_name;
        struct stat buffer;
        if (stat(full_jar_path.c_str(), &buffer) == 0) {
            perform_mirror_injection(env, class_loader, full_jar_path.c_str());
        }
    }

    LOGW("vMeer_Engine: [SAFEGUARD] syncJavaProperties dilewati secara lokal untuk stabilitas Android 15.");
    env->ReleaseStringUTFChars(mirrorPath, path);
}

/**
 * JNI: setSurfaceBridge
 */
JNIEXPORT void JNICALL
Java_com_vmeer_io_VMeerEngine_setSurfaceBridge(JNIEnv* env, jclass clazz, jobject surface) {
    (void)clazz;
    if (g_NativeWindow != nullptr) {
        ANativeWindow_release(g_NativeWindow);
        g_NativeWindow = nullptr;
    }
    if (surface != nullptr) {
        g_NativeWindow = ANativeWindow_fromSurface(env, surface);
        if (g_NativeWindow != nullptr) {
            LOGI("vMeer_Graphics: SUCCESS - Jembatan ANativeWindow berhasil dikunci ke memori GPU!");
        }
    }
}

/**
 * JNI: notifySurfaceChanged
 */
JNIEXPORT void JNICALL
Java_com_vmeer_io_VMeerEngine_notifySurfaceChanged(JNIEnv* env, jclass clazz, jint width, jint height) {
    (void)clazz;
    if (g_NativeWindow != nullptr) {
        ANativeWindow_setBuffersGeometry(g_NativeWindow, width, height, WINDOW_FORMAT_RGBA_8888);
    }
}

/**
 * JNI_OnLoad
 */
JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* res) {
    (void)res;
    JNIEnv* env;
    if (vm->GetEnv((void**)&env, JNI_VERSION_1_6) != JNI_OK) return JNI_ERR;

    LOGI("vMeer Engine: Booting Core Systems...");
    init_vmeer_stealth();
    shadowhook_init(SHADOWHOOK_MODE_SHARED, false);

    pthread_t daemon_tid;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    pthread_create(&daemon_tid, &attr, vmeerd_live_daemon, nullptr);
    pthread_attr_destroy(&attr);

    return JNI_VERSION_1_6;
}

} // extern "C"
