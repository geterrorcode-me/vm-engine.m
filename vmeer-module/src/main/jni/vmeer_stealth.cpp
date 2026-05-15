#include "include/vmeer_stealth.h"
#include "shadowhook.h"
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <string>
#include <android/log.h>

#define LOG_TAG "vMeer_Stealth"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// Definisi tipe fungsi untuk original call
typedef ssize_t (*p_read_t)(int, void *, size_t);
typedef int (*p_open_t)(const char *, int, mode_t);

static p_read_t orig_read = nullptr;
static p_open_t orig_open = nullptr;

// Thread-local atau global tracker untuk maps
// Catatan: Menggunakan global FD bisa berbahaya jika banyak thread, 
// tapi untuk keperluan stealth dasar ini masih fungsional.
static int maps_fd = -1;

extern "C" {

/**
 * Hook read: 
 * Memotong baris /proc/self/maps yang mengandung jejak vMeer atau ShadowHook.
 */
ssize_t hook_read(int fd, void *buf, size_t count) {
    if (orig_read == nullptr) return -1;

    ssize_t ret = orig_read(fd, buf, count);
    
    // Hanya proses jika ini adalah FD dari maps yang kita tandai
    if (ret > 0 && fd == maps_fd && maps_fd != -1) {
        std::string content((char*)buf, (size_t)ret);
        bool modified = false;
        
        // Cari dan hapus baris yang mengandung library kita
        const char* keywords[] = {"vmeer", "shadowhook", "libfuse3", "libzstd"};
        
        for (const char* key : keywords) {
            size_t pos;
            while ((pos = content.find(key)) != std::string::npos) {
                size_t line_start = content.rfind('\n', pos);
                size_t line_end = content.find('\n', pos);
                
                if (line_start == std::string::npos) line_start = 0;
                else line_start += 1;

                if (line_end == std::string::npos) line_end = content.length();
                else line_end += 1;
                
                content.erase(line_start, line_end - line_start);
                modified = true;
            }
        }
        
        if (modified) {
            size_t new_len = content.length();
            if (new_len <= (size_t)count) {
                memcpy(buf, content.c_str(), new_len);
                return (ssize_t)new_len;
            }
        }
    }
    return ret;
}

/**
 * Hook open: 
 * Menandai file descriptor jika aplikasi (terutama Anti-Cheat) membaca maps.
 */
int hook_open(const char *pathname, int flags, mode_t mode) {
    if (orig_open == nullptr) return -1;

    int fd = orig_open(pathname, flags, mode);
    
    if (fd != -1 && pathname != nullptr) {
        if (strstr(pathname, "/proc/self/maps") != nullptr || strstr(pathname, "/proc/maps") != nullptr) {
            maps_fd = fd;
            LOGI("vMeer: [Stealth] Cloaking maps on FD: %d", fd);
        }
    }
    return fd;
}

/**
 * init_vmeer_stealth:
 * Mengaktifkan sistem kamuflase memori.
 */
void init_vmeer_stealth() {
    LOGI("vMeer: [Stealth] Engaging Cloaking Device...");
    
    // FIX: Gunakan casting (void*) dan (void**) secara eksplisit 
    // untuk menghindari error "declared here" pada shadowhook_hook_sym_name
    
    // Hook 'open'
    shadowhook_hook_sym_name(
        "libc.so", 
        "open", 
        (void*)hook_open, 
        (void**)&orig_open
    );

    // Hook 'read'
    shadowhook_hook_sym_name(
        "libc.so", 
        "read", 
        (void*)hook_read, 
        (void**)&orig_read
    );
    
    LOGI("vMeer: [Stealth] Engine is now invisible in memory maps.");
}

} // extern "C"
