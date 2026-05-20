#include <jni.h>
#include <android/log.h>
#include <unistd.h>
#include <sys/types.h>
#include <string>
#include <cstring>
#include <fcntl.h>
#include <mutex>
#include "shadowhook.h"

#define LOG_TAG "vMeer_ProcSandbox"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

static std::mutex sandbox_mtx;
static uid_t virtual_uid = 1010871; // Default virtual UID untuk aplikasi tamu
static gid_t virtual_gid = 1010871;
static pid_t virtual_pid = 12050;   // Default virtual PID yang disamarkan
static std::string virtual_package_name = "com.vmeer.guest";

// Pointer untuk menyimpan fungsi asli libc
typedef uid_t (*getuid_t)();
typedef gid_t (*getgid_t)();
typedef uid_t (*geteuid_t)();
typedef gid_t (*getegid_t)();
typedef pid_t (*getpid_t)();
typedef int (*open_t)(const char*, int, ...);
typedef int (*openat_t)(int, const char*, int, ...);

static getuid_t orig_getuid = nullptr;
static getgid_t orig_getgid = nullptr;
static geteuid_t orig_geteuid = nullptr;
static getegid_t orig_getegid = nullptr;
static getpid_t orig_getpid = nullptr;
static open_t orig_open = nullptr;
static openat_t orig_openat = nullptr;

// =============================================================================
// PROXY SYSTEM CALLS (UID / GID / PID SPOOFING)
// =============================================================================

uid_t proxy_getuid() {
    return virtual_uid;
}

gid_t proxy_getgid() {
    return virtual_gid;
}

uid_t proxy_geteuid() {
    return virtual_uid;
}

gid_t proxy_getegid() {
    return virtual_gid;
}

pid_t proxy_getpid() {
    return virtual_pid;
}

// =============================================================================
// PROXY FILE SYSTEM INTERCEPTION (PROC VIRTUALIZATION)
// =============================================================================

/**
 * Memeriksa dan membelokkan jalur pembacaan file sistem sensitif seperti /proc
 */
std::string redirect_proc_path(const char* path) {
    if (!path) return "";
    std::string path_str(path);

    // Belokkan pembacaan cmdline (Package name deteksi)
    if (path_str == "/proc/self/cmdline") {
        LOGI("vMeer ProcSandbox: Mengalihkan akses cmdline -> memori virtual.");
        // Jalur alternatif yang berisi nama package virtual terisolasi
        return "/data/user/0/com.vmeer.io/app_app_bin/rootfs/proc_cmdline_virtual";
    }
    
    // Belokkan pembacaan status/maps proses jika diperlukan
    if (path_str == "/proc/self/status") {
        return "/data/user/0/com.vmeer.io/app_app_bin/rootfs/proc_status_virtual";
    }

    return path_str;
}

int proxy_open(const char* pathname, int flags, mode_t mode) {
    if (orig_open == nullptr) return -1;
    
    std::string redirected = redirect_proc_path(pathname);
    if (!redirected.empty() && redirected != pathname) {
        return orig_open(redirected.c_str(), flags, mode);
    }
    
    return orig_open(pathname, flags, mode);
}

int proxy_openat(int dirfd, const char* pathname, int flags, mode_t mode) {
    if (orig_openat == nullptr) return -1;

    std::string redirected = redirect_proc_path(pathname);
    if (!redirected.empty() && redirected != pathname) {
        return orig_openat(dirfd, redirected.c_str(), flags, mode);
    }

    return orig_openat(dirfd, pathname, flags, mode);
}

// =============================================================================
// GERBANG INISIALISASI ISO-NAMESPACE
// =============================================================================

namespace vmeer {
namespace sandbox {

void InitializeProcessNamespace(uid_t guest_uid, pid_t guest_pid, const std::string& guest_pkg) {
    std::lock_guard<std::mutex> lock(sandbox_mtx);
    virtual_uid = guest_uid;
    virtual_gid = guest_uid;
    virtual_pid = guest_pid;
    virtual_package_name = guest_pkg;

    LOGI("vMeer ProcSandbox: Mengaktifkan konfigurasi Namespace: vUID=%d, vPID=%d, Guest=[%s]", 
         virtual_uid, virtual_pid, virtual_package_name.c_str());

    // 1. Hooking libc UID/GID/PID Accessor
    shadowhook_hook_sym_name("libc.so", "getuid", reinterpret_cast<void*>(proxy_getuid), reinterpret_cast<void**>(&orig_getuid));
    shadowhook_hook_sym_name("libc.so", "getgid", reinterpret_cast<void*>(proxy_getgid), reinterpret_cast<void**>(&orig_getgid));
    shadowhook_hook_sym_name("libc.so", "geteuid", reinterpret_cast<void*>(proxy_geteuid), reinterpret_cast<void**>(&orig_geteuid));
    shadowhook_hook_sym_name("libc.so", "getegid", reinterpret_cast<void*>(proxy_getegid), reinterpret_cast<void**>(&orig_getegid));
    shadowhook_hook_sym_name("libc.so", "getpid", reinterpret_cast<void*>(proxy_getpid), reinterpret_cast<void**>(&orig_getpid));

    // 2. Hooking libc I/O Operations untuk /proc bypass
    shadowhook_hook_sym_name("libc.so", "open", reinterpret_cast<void*>(proxy_open), reinterpret_cast<void**>(&orig_open));
    shadowhook_hook_sym_name("libc.so", "openat", reinterpret_cast<void*>(proxy_openat), reinterpret_cast<void**>(&orig_openat));

    LOGI("vMeer ProcSandbox: Seluruh system call UID/GID/PID & VFS /proc Interceptor ACTIVE!");
}

} // namespace sandbox
} // namespace vmeer

extern "C" {

JNIEXPORT void JNICALL
Java_com_vmeer_io_EngineLoader_nativeInitProcessSandbox(JNIEnv* env, jclass clazz, jint guest_uid, jint guest_pid, jstring guest_pkg) {
    (void)clazz;
    if (!guest_pkg) return;

    const char* pkg_chars = env->GetStringUTFChars(guest_pkg, nullptr);
    if (pkg_chars) {
        vmeer::sandbox::InitializeProcessNamespace(
            static_cast<uid_t>(guest_uid), 
            static_cast<pid_t>(guest_pid), 
            std::string(pkg_chars)
        );
        env->ReleaseStringUTFChars(guest_pkg, pkg_chars);
    }
}

} // extern "C"