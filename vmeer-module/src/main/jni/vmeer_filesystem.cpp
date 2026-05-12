#include "vmeer_filesystem.h"
#include "shadowhook.h"
#include <string>
#include <android/log.h>

#define LOG_TAG "vMeer_FS"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

typedef int (*p_open_t)(const char *pathname, int flags, mode_t mode);
static p_open_t orig_open = nullptr;

// Folder asli vs Folder Sandbox
static std::string target_data_path = "/data/user/0/com.target.app";
static std::string sandbox_data_path = "/data/user/0/com.vmeer.virtual/sandbox";

extern "C" {

/**
 * Logika pengalihan path.
 * Jika aplikasi mengakses foldernya sendiri, arahkan ke sandbox.
 */
const char* redirect_path(const char* original_path) {
    if (original_path != nullptr && strstr(original_path, target_data_path.c_str())) {
        static std::string new_path;
        new_path = original_path;
        new_path.replace(0, target_data_path.length(), sandbox_data_path);
        
        LOGI("vMeer: [FS] Redirecting %s -> %s", original_path, new_path.c_str());
        return new_path.c_str();
    }
    return original_path;
}

int vmeer_hook_open(const char *pathname, int flags, mode_t mode) {
    return orig_open(redirect_path(pathname), flags, mode);
}

void init_vmeer_filesystem() {
    LOGI("vMeer: [FS] Activating Storage Isolation...");
    shadowhook_hook_sym_name("libc.so", "open", (void*)vmeer_hook_open, (void**)&orig_open);
}

}
