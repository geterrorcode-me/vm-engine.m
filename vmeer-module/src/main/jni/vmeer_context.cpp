#include "include/vmeer_context.h"
#include <android/log.h>

#define LOG_TAG "vMeer_Context"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

namespace vmeer {

/**
 * Initialize:
 * Menyiapkan identitas virtual dasar saat engine pertama kali dimuat.
 */
bool RuntimeContext::Initialize(const std::string& vm_id, const std::string& target_pkg) {
    std::lock_guard<std::mutex> lock(registry_mutex_);
    
    m_vm_id = vm_id;
    m_target_package = target_pkg;
    
    // Sederhana: Generate Android ID palsu berbasis VM ID
    m_v_android_id = "v_aid_" + vm_id;

    // Registrasi paket utama ke dalam registry internal
    VirtualIdentity ident = {target_pkg, 20000, true, true};
    registry_[target_pkg] = ident;

    LOGI("vMeer Context: Initialized for %s (Virtual AID: %s)", 
         target_pkg.c_str(), m_v_android_id.c_str());
    
    return true;
}

/**
 * Package:
 * Bridge menuju Virtual Package Manager.
 */
pms::PMSRuntime& RuntimeContext::Package() { 
    // Pastikan namespace 'pms' sudah sesuai dengan deklarasi di vmeer_pms.h
    return pms::PMSRuntime::Get(); 
}

/**
 * RegisterVirtualApp:
 * Digunakan oleh Zygote Hook atau setupVM untuk mendaftarkan proses baru.
 */
void RuntimeContext::RegisterVirtualApp(const std::string& pkg, int v_uid) {
    std::lock_guard<std::mutex> lock(registry_mutex_);
    
    // Simpan identitas ke map untuk dilacak saat fork()
    registry_[pkg] = {pkg, v_uid, true, true};
    
    LOGI("vMeer Context: Registered Virtual App %s with vUID %d", pkg.c_str(), v_uid);
}

/**
 * GetIdentity:
 * Mencari data identitas berdasarkan nama proses (penting untuk Stealth/Spoofing).
 */
VirtualIdentity* RuntimeContext::GetIdentity(const std::string& proc_name) {
    std::lock_guard<std::mutex> lock(registry_mutex_);
    
    auto it = registry_.find(proc_name);
    if (it != registry_.end()) {
        return &(it->second);
    }
    return nullptr;
}

/**
 * Heartbeat:
 * Sinkronisasi state sebelum proses dimatikan (OnUnload).
 */
void RuntimeContext::Heartbeat() {
    LOGI("vMeer Context: State synchronization heartbeat executed.");
    // Di sini kamu bisa menambahkan sinkronisasi Shared Memory atau Database.
}

} // namespace vmeer
