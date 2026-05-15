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

typedef ssize_t (*p_read_t)(int, void *, size_t);
typedef int (*p_open_t)(const char *, int, mode_t);

static p_read_t orig_read = nullptr;
static p_open_t orig_open = nullptr;
static int maps_fd = -1;

extern "C" {

ssize_t hook_read(int fd, void *buf, size_t count) {
    if (!orig_read) return -1;
    ssize_t ret = orig_read(fd, buf, count);
    
    if (ret > 0 && fd == maps_fd && maps_fd != -1) {
        std::string content(static_cast<char*>(buf), static_cast<size_t>(ret));
        const char* targets[] = {"vmeer", "shadowhook", "libfuse3", "libzstd"};
        bool modified = false;

        for (const char* key : targets) {
            size_t pos;
            while ((pos = content.find(key)) != std::string::npos) {
                size_t start = content.rfind('\n', pos);
                size_t end = content.find('\n', pos);
                start = (start == std::string::npos) ? 0 : start + 1;
                end = (end == std::string::npos) ? content.length() : end + 1;
                content.erase(start, end - start);
                modified = true;
            }
        }
        if (modified && content.length() <= static_cast<size_t>(count)) {
            memcpy(buf, content.c_str(), content.length());
            return static_cast<ssize_t>(content.length());
        }
    }
    return ret;
}

int hook_open(const char *pathname, int flags, mode_t mode) {
    if (!orig_open) return -1;
    int fd = orig_open(pathname, flags, mode);
    if (fd != -1 && pathname && strstr(pathname, "maps")) {
        maps_fd = fd;
        LOGI("vMeer: [Stealth] Intercepted maps FD: %d", fd);
    }
    return fd;
}

void init_vmeer_stealth() {
    LOGI("vMeer: [Stealth] Engaging Cloaking Device...");
    
    // GUNAKAN reinterpret_cast UNTUK MENGHINDARI ERROR "DECLARED HERE"
    shadowhook_hook_sym_name(
        "libc.so", "open", 
        reinterpret_cast<void*>(hook_open), 
        reinterpret_cast<void**>(&orig_open)
    );

    shadowhook_hook_sym_name(
        "libc.so", "read", 
        reinterpret_cast<void*>(hook_read), 
        reinterpret_cast<void**>(&orig_read)
    );
    
    LOGI("vMeer: [Stealth] Cloaking ACTIVE.");
}

}
