#include "include/vmeer_stealth.h"
#include "shadowhook.h"
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <string>
#include <android/log.h>

#define LOG_TAG "vMeer_Stealth"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

typedef ssize_t (*p_read_t)(int fd, void *buf, size_t count);
static p_read_t orig_read = nullptr;

typedef int (*p_open_t)(const char *pathname, int flags, mode_t mode);
static p_open_t orig_open = nullptr;

static int maps_fd = -1;

extern "C" {

// Hook read: Memotong baris yang mengandung "vmeer" atau "shadowhook"
ssize_t hook_read(int fd, void *buf, size_t count) {
    ssize_t ret = orig_read(fd, buf, count);
    
    if (ret > 0 && fd == maps_fd && maps_fd != -1) {
        std::string content((char*)buf, ret);
        size_t pos;
        
        // Hapus jejak vMeer dari memory maps
        while ((pos = content.find("vmeer")) != std::string::npos || 
               (pos = content.find("shadowhook")) != std::string::npos) {
            
            size_t line_start = content.rfind('\n', pos);
            size_t line_end = content.find('\n', pos);
            
            if (line_start == std::string::npos) line_start = 0;
            else line_start += 1; // Mulai setelah newline sebelumnya

            if (line_end == std::string::npos) line_end = content.length();
            else line_end += 1; // Sertakan newline-nya
            
            content.erase(line_start, line_end - line_start);
        }
        
        memcpy(buf, content.c_str(), content.length());
        return content.length();
    }
    return ret;
}

// Hook open: Menandai FD jika aplikasi membuka /proc/self/maps
int hook_open(const char *pathname, int flags, mode_t mode) {
    int fd = orig_open(pathname, flags, mode);
    if (pathname != nullptr && strstr(pathname, "maps") != nullptr) {
        maps_fd = fd;
        LOGI("vMeer: [Stealth] Targeted maps access detected on FD: %d", fd);
    }
    return fd;
}

void init_vmeer_stealth() {
    LOGI("vMeer: [Stealth] Engaging Cloaking Device...");
    
    // Hook open dan read di libc.so
    shadowhook_hook_sym_name("libc.so", "open", (void*)hook_open, (void**)&orig_open);
    shadowhook_hook_sym_name("libc.so", "read", (void*)hook_read, (void**)&orig_read);
    
    LOGI("vMeer: [Stealth] vMeer is now invisible in memory maps.");
}

}
