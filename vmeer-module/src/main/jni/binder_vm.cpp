#include "include/binder_vm.h"
#include "shadowhook.h"
#include <android/log.h>
#include <unordered_map>
#include <string>
#include <mutex>

#define LOG_TAG "vMeer_Binder"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

static std::unordered_map<int32_t, int32_t> handle_map;
static std::mutex binder_mtx;

// Daftar pembelokan service untuk mengelabui anti-cheat
static std::unordered_map<std::string, std::string> service_map = {
    {"iphonesubinfo", "vmeer_iphonesubinfo"}, // Manipulasi IMEI/IMSI
    {"device_policy", "vmeer_null_service"},  // Sembunyikan status admin/root
    {"package", "vmeer_package_manager"},     // PM internal (isolasi app)
    {"user", "vmeer_user_service"}            // Palsukan info user/multi-user
};

// Definisi fungsi asli untuk disimpan shadowhook
typedef int (*transact_t)(void* self, int32_t handle, uint32_t code, void* data, void* reply, uint32_t flags);
static transact_t orig_transact = nullptr;

// Fungsi Proxy yang akan dijalankan setiap kali ada komunikasi Binder
int proxy_transact(void* self, int32_t handle, uint32_t code, void* data, void* reply, uint32_t flags) {
    // Di sini kamu bisa menambahkan logic untuk membedah 'data' (Parcel)
    // dan mengubah isi transaksinya secara real-time.
    return orig_transact(self, handle, code, data, reply, flags);
}

extern "C" {

const char* resolve_virtual_service(const char* original_name) {
    std::lock_guard<std::mutex> lock(binder_mtx);
    if (service_map.count(original_name)) {
        return service_map[original_name].c_str();
    }
    return original_name;
}

int32_t remap_to_virtual(int32_t real_handle) {
    std::lock_guard<std::mutex> lock(binder_mtx);
    if (handle_map.find(real_handle) == handle_map.end()) {
        handle_map[real_handle] = 20000 + (int)handle_map.size();
    }
    return handle_map[real_handle];
}

void start_binder_proxy() {
    LOGI("vMeer: [Binder] Hooking IPCThreadState::transact...");

    // Nama simbol ini biasanya standar di libbinder.so Android 10-14
    void* stub = shadowhook_hook_symname(
        "libbinder.so",
        "_ZN7android15IPCThreadState8transactEiijRKNS_6ParcelEPS1_j",
        (void*)proxy_transact,
        (void**)&orig_transact
    );

    if (stub == nullptr) {
        LOGI("vMeer: [Binder] Hook Failed! Error: %d", shadowhook_get_errno());
    } else {
        LOGI("vMeer: [Binder] Proxy Layer Active and Stealth.");
    }
}

}
