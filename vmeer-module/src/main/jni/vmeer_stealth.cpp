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

// Definisi tipe fungsi agar sinkron dengan libc
typedef ssize_t (*p_read_t)(int, void *, size_t);
typedef int (*p_open_t)(const char *, int, mode_t);

// Pointer untuk menyimpan alamat fungsi asli
static p_read_t orig_read = nullptr;
static p_open_t orig_open = nullptr;

// Tracker sederhana untuk file descriptor /proc/self/maps
static int maps_fd = -1;

extern "C" {

/**
 * hook_read:
 * Memanipulasi hasil pembacaan memori maps untuk menyembunyikan jejak engine.
 */
ssize_t hook_read(int fd, void *buf, size_t count) {
    if (orig_read == nullptr) return -1;

    ssize_t ret = orig_read(fd, buf, count);
    
    // Hanya proses jika FD cocok dengan maps yang kita tandai
    if (ret > 0 && fd == maps_fd && maps_fd != -1) {
        std::string content(static_cast<char*>(buf), static_cast<size_t>(ret));
        bool modified = false;
        
        // Daftar kata kunci yang dilarang terlihat oleh Anti-Cheat
        const char* blacklisted[] = {"vmeer", "shadowhook", "libfuse3", "libzstd"};
        
        for (const char* key : blacklisted) {
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
            if (new_len <= static_cast<size_t>(count)) {
                memcpy(buf, content.c_str(), new_len);
                return static_cast<ssize_t>(new_len);
            }
        }
    }
    return ret;
}

/**
 * hook_open:
 * Mendeteksi kapan aplikasi mencoba mengintip daftar library di memori.
 */
int hook_open(const char *pathname, int flags, mode_t mode) {
    if (orig_open == nullptr) return -1;

    int fd = orig_open(pathname, flags, mode);
    
    if (fd != -1 && pathname != nullptr) {
        if (strstr(pathname, "/proc/self/maps") != nullptr) {
            maps_fd = fd;
            LOGI("vMeer: [Stealth] Targeted maps access intercepted (FD: %d)", fd);
        }
    }
    return fd;
}

/**
 * init_vmeer_stealth:
 * Poin aktivasi sistem stealth.
 */
void init_vmeer_stealth() {
    LOGI("vMeer: [Stealth] Engaging Cloaking Device...");
    
    // PENTING: Lakukan casting ke (void*) dan (void**) secara eksplisit
    // agar sinkron dengan deklarasi di shadowhook.h
    
    void* stub_open = shadowhook_hook_sym_name(
        "libc.so", 
        "open", 
        reinterpret_cast<void*>(hook_open), 
        reinterpret_cast<void**>(&orig_open)
    );

    void* stub_read = shadowhook_hook_sym_name(
        "libc.so", 
        "read", 
        reinterpret_cast<void*>(hook_read), 
        reinterpret_cast<void**>(&orig_read)
    );

    if(!stub_open || !stub_read) {
        LOGE("vMeer: [Stealth] Hooking failed! Stealth is COMPROMISED.");
    } else {
        LOGI("vMeer: [Stealth] Cloaking ACTIVE.");
    }
}

} // extern "C"
