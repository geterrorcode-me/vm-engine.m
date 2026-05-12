#include <jni.h>
#include <string>
#include <sys/stat.h>
#include <dirent.h> // Standar Android dirent
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/syscall.h>
#include <android/log.h>

// Gunakan definisi manual jika dirent64 tidak ditemukan di beberapa environment NDK
#ifndef DT_DIR
#define DT_DIR 4
#endif

#include "shadowhook.h"
#include "include/vmeer_vfs.h"
#include "include/vmeer_context.h"

#define LOG_TAG "vMeer_VFS"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

namespace vmeer {
namespace vfs {

// Helper: Transformasi Path Asli -> Path Virtual
std::string RedirectPath(const char* original) {
    if (!original || strlen(original) == 0) return "";
    
    std::string path(original);
    auto& ctx = RuntimeContext::Get();
    std::string pkg = ctx.GetTargetPackage();
    int vuid = ctx.GetVirtualUid();
    int user_id = vuid / 100000;

    if (path.find("/data/data/" + pkg) == 0) {
        return "/data/data/com.vmeer.manager/virtual/user_" + std::to_string(user_id) + "/" + pkg + path.substr(11 + pkg.length());
    }

    if (path.find("/storage/emulated/0") == 0) {
        return "/data/data/com.vmeer.manager/virtual/user_" + std::to_string(user_id) + "/sdcard" + path.substr(19);
    }

    return path;
}

// --- [HOOK] OpenAt ---
typedef int (*orig_openat_t)(int, const char*, int, mode_t);
int hook_openat(int dirfd, const char* pathname, int flags, mode_t mode) {
    std::string v_path = RedirectPath(pathname);
    return ((orig_openat_t)shadowhook_get_prev_func(reinterpret_cast<void*>(hook_openat)))(dirfd, v_path.c_str(), flags, mode);
}

// --- [HOOK] GetDents64 ---
// Perhatikan penggunaan struct dirent64 yang biasanya sudah ada di sys/syscall.h atau dirent.h
typedef int (*orig_getdents64_t)(unsigned int, void*, unsigned int);
int hook_getdents64(unsigned int fd, void* dirp, unsigned int count) {
    int nread = ((orig_getdents64_t)shadowhook_get_prev_func(reinterpret_cast<void*>(hook_getdents64)))(fd, dirp, count);
    if (nread <= 0) return nread;

    int bpos = 0;
    while (bpos < nread) {
        // Kita cast ke struct linux_dirent64 secara manual agar aman di berbagai versi NDK
        struct linux_dirent64 {
            uint64_t        d_ino;
            int64_t         d_off;
            unsigned short  d_reclen;
            unsigned char   d_type;
            char            d_name[];
        };

        struct linux_dirent64* d = (struct linux_dirent64*)((char*)dirp + bpos);
        if (strstr(d->d_name, "com.vmeer.manager") || strstr(d->d_name, "vmeer_engine")) {
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

// --- [HOOK] Read (Maps Scrubber) ---
typedef ssize_t (*orig_read_t)(int, void*, size_t);
ssize_t hook_read(int fd, void* buf, size_t count) {
    ssize_t res = ((orig_read_t)shadowhook_get_prev_func(reinterpret_cast<void*>(hook_read)))(fd, buf, count);
    if (res <= 0) return res;

    char path[1024]; // PATH_MAX
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

// --- [HOOK] GetEnv ---
typedef char* (*orig_getenv_t)(const char*);
char* hook_getenv(const char* name) {
    if (!name) return nullptr;
    if (strcmp(name, "DEBUG") == 0 || strcmp(name, "DEBUGGABLE") == 0) return nullptr;
    if (strcmp(name, "ADB_ENABLED") == 0) return (char*)"0";
    return ((orig_getenv_t)shadowhook_get_prev_func(reinterpret_cast<void*>(hook_getenv)))(name);
}

void StartVFSEngine() {
    // Pastikan menggunakan shadowhook_hook_sym_name (dengan underscore)
    shadowhook_hook_sym_name("libc.so", "openat", (void*)hook_openat, nullptr);
    shadowhook_hook_sym_name("libc.so", "__getdents64", (void*)hook_getdents64, nullptr);
    shadowhook_hook_sym_name("libc.so", "read", (void*)hook_read, nullptr);
    shadowhook_hook_sym_name("libc.so", "getenv", (void*)hook_getenv, nullptr);
    LOGI("vMeer VFS: Engine Activated.");
}

} // namespace vfs
} // namespace vmeer
