#include <jni.h>
#include <android/log.h>
#include <string.h>
#include <dirent.h>
#include <dlfcn.h>
#include "shadowhook.h"

#define LOG_TAG "vMeer_Stealth"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

// =============================================================================
// BAGIAN 1: FILE CLOAKING (Menyembunyikan Folder vMeer)
// =============================================================================
/**
 * Aplikasi sering memindai direktori untuk mencari tanda-tanda Virtual App.
 * Kita mencegat fungsi 'readdir' agar tidak menampilkan folder vMeer.
 */
typedef struct dirent* (*p_readdir)(DIR*);
static p_readdir orig_readdir = nullptr;

struct dirent* hook_readdir(DIR* dirp) {
    struct dirent* entry;
    while ((entry = orig_readdir(dirp)) != nullptr) {
        // Jika menemukan nama paket vMeer, skip dan cari entri berikutnya
        if (strstr(entry->d_name, "com.vmeer.io") || strstr(entry->d_name, "vmeerd")) {
            continue; 
        }
        break;
    }
    return entry;
}

// =============================================================================
// BAGIAN 2: MAPS FILTERING (Menyembunyikan Hook Library)
// =============================================================================
/**
 * Aplikasi canggih akan membaca /proc/self/maps untuk mencari 'shadowhook' 
 * atau library kita. Kita harus memfilter output pembacaan file tersebut.
 */
typedef ssize_t (*p_read)(int, void*, size_t);
static p_read orig_read = nullptr;

ssize_t hook_read(int fd, void* buf, size_t count) {
    ssize_t bytes = orig_read(fd, buf, count);
    if (bytes > 0) {
        // Jika data mengandung kata kunci vmeer atau shadowhook, kita sensor
        char* p = (char*)buf;
        if (strstr(p, "vmeer") || strstr(p, "shadowhook")) {
            memset(buf, 0, bytes); // Hapus jejak di buffer memori
            return 0; 
        }
    }
    return bytes;
}

// =============================================================================
// BAGIAN 3: STEALTH INITIALIZATION (Integration)
// =============================================================================

void init_stealth_module() {
    LOGI("vMeer: Activating Stealth Mode...");

    // 1. Hook readdir untuk manipulasi hasil list file
    shadowhook_hook_sym_name(
        "libc.so", "readdir", 
        (void*)hook_readdir, (void**)&orig_readdir
    );

    // 2. Hook read untuk memfilter pembacaan /proc/self/maps
    shadowhook_hook_sym_name(
        "libc.so", "read", 
        (void*)hook_read, (void**)&orig_read
    );
}
