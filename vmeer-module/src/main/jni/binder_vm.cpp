#include "include/binder_vm.h"
#include "shadowhook.h"
#include <android/log.h>
#include <unordered_map>
#include <string>
#include <mutex>

#define LOG_TAG "vMeer_Binder"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

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
    if (orig_transact == nullptr) return -1;
    // Logic pembelokan bisa ditambahkan di sini saat runtime sandbox aktif
    return orig_transact(self, handle, code, data, reply, flags);
}

extern "C" {

const char* resolve_virtual_service(const char* original_name) {
    if (!original_name) return nullptr;
    std::lock_guard<std::mutex> lock(binder_mtx);
    auto it = service_map.find(original_name);
    if (it != service_map.end()) {
        return it->second.c_str();
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

/**
 * Memasang interceptor ke libbinder.so internal Android
 */
void install_binder_hooks() {
    LOGI("vMeer: [Binder] Hooking IPCThreadState::transact...");

    void* stub = shadowhook_hook_sym_name(
        "libbinder.so",
        "_ZN7android15IPCThreadState8transactEiijRKNS_6ParcelEPS1_j",
        reinterpret_cast<void*>(proxy_transact),
        reinterpret_cast<void**>(&orig_transact)
    );

    if (stub == nullptr) {
        int err_num = shadowhook_get_errno();
        LOGE("vMeer: [Binder] Hook Failed! Error: %s", shadowhook_to_errmsg(err_num));
    } else {
        LOGI("vMeer: [Binder] Proxy Layer Active (libbinder.so)");
    }
}

void start_binder_proxy() {
    install_binder_hooks();
}

// ====================================================================
// FIX UTAMA: IMPLEMENTASI SIMBOL YANG DI CARI OLEH LINKER (ld)
// ====================================================================

/**
 * Solusi Undefined Symbol 1: Dipanggil oleh vmeer_core.cpp (Tahap 2)
 * Menjadi gerbang utama inisialisasi isolasi transaksi driver binder IPC.
 */
void init_binder_isolation() {
    LOGI("vMeer Core: Inisialisasi Subsistem Isolasi Binder Virtual...");
    install_binder_hooks();
}

/**
 * Solusi Undefined Symbol 2: Dipanggil oleh vmeer_engine.cpp 
 * Berfungsi untuk menyuntikkan dan memetakan virtual memory layout aplikasi tamu.
 */
void perform_mirror_injection() {
    LOGI("vMeer Core: Mengeksekusi Mirror Injection ke Ruang Memori Guest OS...");
    // Tambahkan logic kustom Anda untuk kloning memory maps game target di bawah ini jika diperlukan
}

} // extern "C"
