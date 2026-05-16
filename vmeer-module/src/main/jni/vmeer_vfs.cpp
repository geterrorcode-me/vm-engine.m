#include <jni.h>
#include <string>
#include <sys/stat.h>
#include <dirent.h> 
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/syscall.h>
#include <android/log.h>
#include <dlfcn.h>

#ifndef DT_DIR
#define DT_DIR 4
#endif

#include "shadowhook.h"
#include "include/vmeer_vfs.h"
#include "include/vmeer_context.h"

#define LOG_TAG "vMeer_VFS"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace vmeer {
namespace vfs {

// Deklarasi fungsi penampung biner asli (Jauh lebih stabil untuk Android 15)
static int (*orig_openat)(int, const char*, int, mode_t) = nullptr;
static int (*orig_getdents64)(unsigned int, void*, unsigned int) = nullptr;
static ssize_t (*orig_read)(int, void*, size_t) = nullptr;
static char* (*orig_getenv)(const char*) = nullptr;

// --- Helper: Transformasi Path Asli -> Path Virtual Dinamis ---
std::string RedirectPath(const char* original) {
    if (!original || strlen(original) == 0) return "";
    
    std::string path(original);
    auto& ctx = RuntimeContext::Get();
    
    // Ambil package name dinamis dari context (tidak di-hardcode lagi)
    std::string pkg = ctx.GetTargetPackage();
    if (pkg.empty()) {
        pkg = "com.vmeer.guest"; // Fallback aman
    }
    
    int vuid = ctx.GetVirtualUid();
    int user_id = vuid / 100000;

    // SINKRONISASI: Diarahkan ke folder internal sandbox com.vmeer.io
    if (path.find("/data/data/" + pkg) == 0) {
        return "/data/user/0/com.vmeer.io/app_app_bin/rootfs/user_" + std::to_string(user_id) + "/" + pkg + path.substr(11 + pkg.length());
    }

    if (path.find("/storage/emulated/0") == 0) {
        return "/data/user/0/com.vmeer.io/app_app_bin/rootfs/user_" + std::to_string(user_id) + "/sdcard" + path.substr(19);
    }

    return path;
}

// --- [HOOK] OpenAt ---
int hook_openat(int dirfd, const char* pathname, int flags, mode_t mode) {
    if (pathname != nullptr) {
        std::string v_path = RedirectPath(pathname);
        if (v_path != pathname) {
            return orig_openat(dirfd, v_path.c_str(), flags, mode);
        }
    }
    return orig_openat(dirfd, pathname, flags, mode);
}

// --- [HOOK] GetDents64 (Menyembunyikan jejak Engine di dalam direktori) ---
int hook_getdents64(unsigned int fd, void* dirp, unsigned int count) {
    int nread = orig_getdents64(fd, dirp, count);
    if (nread <= 0) return nread;

    int bpos = 0;
    while (bpos < nread) {
        struct linux_dirent64 {
            uint64_t        d_ino;
            int64_t         d_off;
            unsigned short  d_reclen;
            unsigned char   d_type;
            char            d_name[];
        };

        struct linux_dirent64* d = (struct linux_dirent64*)((char*)dirp + bpos);
        
        // SINKRONISASI: Sembunyikan identitas paket host com.vmeer.io & vmeer_engine
        if (strstr(d->d_name, "com.vmeer.io") || strstr(d->d_name, "vmeer_engine") || strstr(d->d_name, "shadowhook")) {
            int reclen = d->d_reclen;
            int copy_len = nread - (bpos + reclen);
            if (copy_len > 0) memmove(d, (char*)d + reclen, copy_len);
            nread -= reclen;
            continue;
        }
        bpos += d->d_reclen;
    }
    return nread;
}

// --- [HOOK] Read (Maps Scrubber - Menghapus jejak injeksi memori) ---
ssize_t hook_read(int fd, void* buf, size_t count) {
    ssize_t res = orig_read(fd, buf, count);
    if (res <= 0) return res;

    char path[1024]; 
    char fd_path[64];
    snprintf(fd_path, sizeof(fd_path), "/proc/self/fd/%d", fd);
    if (readlink(fd_path, path, sizeof(path)) > 0 && strstr(path, "/maps")) {
        std::string content((char*)buf, res);
        std::string cleaned = "";
        size_t pos = 0, next;
        while ((next = content.find('\n', pos)) != std::string::npos) {
            std::string line = content.substr(pos, next - pos);
            if (line.find("vmeer") == std::string::npos && line.find("shadowhook") == std::string::npos) {
                cleaned += line + "\n";
            }
            pos = next + 1;
        }
        if (cleaned.length() < count) {
            memcpy(buf, cleaned.c_str(), cleaned.length());
            return (ssize_t)cleaned.length();
        }
    }
    return res;
}

// --- [HOOK] GetEnv (Sembunyikan status Debugger) ---
char* hook_getenv(const char* name) {
    if (!name) return nullptr;
    if (strcmp(name, "DEBUG") == 0 || strcmp(name, "DEBUGGABLE") == 0) return nullptr;
    if (strcmp(name, "ADB_ENABLED") == 0) return (char*)"0";
    return orig_getenv(name);
}

void StartVFSEngine() {
    // Gunakan penyimpanan alamat fungsi asli instan agar eksekusi hook stabil di Android 15
    shadowhook_hook_sym_name("libc.so", "openat", (void*)hook_openat, (void**)&orig_openat);
    shadowhook_hook_sym_name("libc.so", "__getdents64", (void*)hook_getdents64, (void**)&orig_getdents64);
    shadowhook_hook_sym_name("libc.so", "read", (void*)hook_read, (void**)&orig_read);
    shadowhook_hook_sym_name("libc.so", "getenv", (void*)hook_getenv, (void**)&orig_getenv);
    
    LOGI("vMeer VFS: Engine Activated. Seluruh jalur I/O virtual berhasil dialihkan.");
}

} // namespace vfs
} // namespace vmeer
