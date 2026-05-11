#include "include/binder_vm.h"
#include <android/log.h>
#include <map>
#include <string>

#define LOG_TAG "vMeer_VSM"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

// Tabel Mapping Layanan (Virtual Namespace)
static std::map<std::string, std::string> service_map = {
    {"activity", "vmeer.virtual.activity_manager"},
    {"package",  "vmeer.virtual.package_manager"},
    {"device_policy", "none"} // Sembunyikan layanan ini
};

extern "C" {

/**
 * Logika inti Virtual Service Manager.
 * Memeriksa apakah layanan yang diminta harus di-redirect atau disembunyikan.
 */
const char* resolve_virtual_service(const char* original_name) {
    if (service_map.count(original_name)) {
        LOGI("vMeer: Redirecting service [%s] -> [%s]", 
             original_name, service_map[original_name].c_str());
        return service_map[original_name].c_str();
    }
    return original_name; // Biarkan layanan lain lewat (untuk sementara)
}

void start_binder_proxy() {
    LOGI("vMeer: [VSM] Virtual Service Manager is standing guard.");
    // Hooking BpBinder::transact atau ServiceManager::getService di sini
}

}
