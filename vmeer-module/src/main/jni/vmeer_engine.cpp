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
void perform_mirror_injection(JNIEnv* env, jobject class_loader, const char* path);
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

static bool requestNamespaceSetup(const char* pkgName, int vuid) {
    int sock = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (sock < 0) return false;

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    addr.sun_path[0] = '\0'; 
    strncpy(addr.sun_path + 1, "vmeer_daemon.cms", sizeof(addr.sun_path) - 2);

    int res = connect(sock, (struct sockaddr *)&addr, sizeof(addr));
    if (res < 0 && errno != EINPROGRESS) {
        LOGW("vMeer_Engine: Daemon vmeerd belum aktif atau tidak merespons. Melewati setup namespace.");
        close(sock);
        return true; 
    }

    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    
    fd_set fdset;
    FD_ZERO(&fdset);
    FD_SET(sock, &fdset);

    if (select(sock + 1, nullptr, &fdset, nullptr, &tv) > 0) {
        char cmd[512];
        snprintf(cmd, sizeof(cmd), "PREPARE_STORAGE:%s:%d", pkgName, vuid);
        send(sock, cmd, strlen(cmd), 0);
        LOGI("vMeer_Engine: Perintah PREPARE_STORAGE dikirim ke daemon.");
    }

    close(sock);
    return true; 
}

extern "C" {

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

    bool daemon_status = requestNamespaceSetup(pkg, vUid);
    
    env->ReleaseStringUTFChars(pkgName, pkg);
    return daemon_status ? JNI_TRUE : JNI_FALSE;
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
    } else {
        LOGE("vMeer_Binder: Kritis! Sistem host tidak memiliki libbinder_ndk.so.");
    }

    // ====================================================================
    // 🔍 JALUR TRACER PINTO KEMATIAN NATIVE
    // ====================================================================
    LOGI("vMeer_Engine: [TRACER 1] Menuju pemanggilan init_art_hook...");
    init_art_hook(env);
    
    LOGI("vMeer_Engine: [TRACER 2] init_art_hook lolos! Memeriksa objek context...");
    if (context == nullptr) {
        LOGE("vMeer_Engine: CRITICAL - Parameter 'context' dari Java bernilai NULL!");
        env->ReleaseStringUTFChars(mirrorPath, path);
        return;
    }

    LOGI("vMeer_Engine: [TRACER 3] Membuka kelas context...");
    jclass context_clazz = env->GetObjectClass(context);
    if (context_clazz == nullptr) {
        LOGE("vMeer_Engine: Gagal mendapatkan jclass dari objek context.");
        env->ReleaseStringUTFChars(mirrorPath, path);
        return;
    }

    LOGI("vMeer_Engine: [TRACER 4] Mencari method getClassLoader...");
    jmethodID get_class_loader_mid = env->GetMethodID(context_clazz, "getClassLoader", "()Ljava/lang/ClassLoader;");
    if (get_class_loader_mid == nullptr) {
        LOGE("vMeer_Engine: Method getClassLoader() tidak ditemukan!");
        env->ReleaseStringUTFChars(mirrorPath, path);
        return;
    }

    LOGI("vMeer_Engine: [TRACER 5] Memanggil getClassLoader via JNI...");
    jobject class_loader = env->CallObjectMethod(context, get_class_loader_mid);
    if (class_loader == nullptr) {
        LOGE("vMeer_Engine: Hasil pemanggilan getClassLoader() bernilai NULL!");
    }

    LOGI("vMeer_Engine: [TRACER 6] Masuk ke perform_mirror_injection...");
    perform_mirror_injection(env, class_loader, path);
    
    LOGI("vMeer_Engine: [TRACER 7] Masuk ke syncJavaProperties...");
    syncJavaProperties(env);

    env->ReleaseStringUTFChars(mirrorPath, path);
    LOGI("vMeer Engine: VM Setup for vUID %d is LIVE.", vUid);
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
        LOGW("vMeer_Graphics: Jembatan grafik lama dilepas.");
    }

    if (surface != nullptr) {
        g_NativeWindow = ANativeWindow_fromSurface(env, surface);
        if (g_NativeWindow != nullptr) {
            LOGI("vMeer_Graphics: SUCCESS - Jembatan ANativeWindow berhasil dikunci ke memori GPU!");
        } else {
            LOGE("vMeer_Graphics: Gagal mengonversi objek Surface ke Native Window!");
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
        LOGI("vMeer_Graphics: Geometri buffer disesuaikan ke resolusi virtual: %dx%d", width, height);
    }
}

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* res) {
    (void)res;
    JNIEnv* env;
    if (vm->GetEnv((void**)&env, JNI_VERSION_1_6) != JNI_OK) return JNI_ERR;

    LOGI("vMeer Engine: Booting Core Systems...");
    init_vmeer_stealth();
    shadowhook_init(SHADOWHOOK_MODE_SHARED, false);

    vmeer::helper::ConnectSharedState();
    vmeer::vfs::StartVFSEngine();

    LOGI("vMeer Engine: Status READY - Engine v1.0.0-STABLE");
    return JNI_VERSION_1_6;
}

} // extern "C"
