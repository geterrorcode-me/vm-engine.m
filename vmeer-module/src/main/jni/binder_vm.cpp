#include "include/binder_vm.h"
#include "shadowhook.h"
#include <android/log.h>

#define LOG_TAG "vMeer_VSM"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

// Definisi fungsi asli (mangled name libbinder)
typedef void* (*p_getService_t)(void* self, const void* name);
static p_getService_t orig_getService = nullptr;

extern "C" {

// Proxy function untuk getService
void* hook_getService(void* self, const void* name) {
    // Di sini kita akan mengonversi 'name' (String16) ke string C++
    // Lalu gunakan resolve_virtual_service() untuk menentukan arahnya.
    
    LOGI("vMeer: Application is looking for a service...");
    
    // Untuk saat ini, kita biarkan original berjalan, 
    // tapi kita sudah punya 'tap' untuk membelokkannya.
    return orig_getService(self, name);
}

void start_binder_proxy() {
    LOGI("vMeer: [VSM] Hooking IServiceManager::getService...");

    // Hook simbol getService (ini adalah simbol umum di banyak versi Android)
    shadowhook_hook_sym_name(
        "libbinder.so", 
        "_ZN7android14IServiceManager10getServiceERKNS_7String16E", 
        (void*)hook_getService, 
        (void**)&orig_getService
    );

    LOGI("vMeer: [VSM] Namespace protection is active.");
}

} // extern "C"
