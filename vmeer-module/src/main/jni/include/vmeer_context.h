#ifndef VMEER_CONTEXT_H
#define VMEER_CONTEXT_H

#include <string>
#include <map>
#include <mutex>

// Melindungi akses ke Virtual Package Manager
#include "vmeer_pms.h" 

namespace vmeer {

/**
 * VirtualIdentity:
 * Menyimpan metadata identitas setiap proses yang berjalan di dalam sandbox.
 */
struct VirtualIdentity {
    std::string package_name;
    int virtual_uid;
    bool is_isolated;
    bool use_virtual_storage;
};

class RuntimeContext {
public:
    /**
     * Singleton Pattern:
     * Memastikan hanya ada satu instance context di seluruh lifecycle engine.
     */
    static RuntimeContext& Get() {
        static RuntimeContext instance;
        return instance;
    }

    // --- Core Methods ---
    bool Initialize(const std::string& vm_id, const std::string& target_pkg);
    void Heartbeat(); 
    
    /**
     * Package:
     * Mengembalikan referensi ke Virtual PMS untuk manajemen manifest & permissions.
     */
    pms::PMSRuntime& Package();

    // --- Registry & Zygote Support ---
    void RegisterVirtualApp(const std::string& pkg, int v_uid);
    VirtualIdentity* GetIdentity(const std::string& proc_name);

    // --- High-End VM Setters (Digunakan oleh setupVM & prepareStorageSandbox) ---
    void SetVirtualUid(int uid) { 
        std::lock_guard<std::mutex> lock(registry_mutex_);
        m_vuid = uid; 
    }
    
    void SetMirrorPath(const std::string& path) { 
        std::lock_guard<std::mutex> lock(registry_mutex_);
        m_mirror_path = path; 
    }

    // PERBAIKAN KRUSIAL: Menambahkan setter untuk Target Package Name
    void SetTargetPackage(const std::string& pkg) {
        std::lock_guard<std::mutex> lock(registry_mutex_);
        m_target_package = pkg;
    }

    // --- Getters ---
    int GetVirtualUid() const { return m_vuid; }
    std::string GetMirrorPath() const { return m_mirror_path; }
    std::string GetTargetPackage() const { return m_target_package; }
    std::string GetVAndroidId() const { return m_v_android_id; }
    std::string GetMasterSeed() const { return m_master_seed; }

private:
    /**
     * Constructor:
     * Urutan inisialisasi disesuaikan dengan urutan deklarasi variabel di bawah
     * untuk mencegah warning -Wreorder-ctor.
     */
    RuntimeContext() 
        : m_master_seed("vmeer_default_seed_8888"),
          m_vuid(0) {}

    // Variabel Identitas (Sesuai urutan inisialisasi)
    std::string m_vm_id;
    std::string m_target_package;
    std::string m_v_android_id;
    std::string m_master_seed;
    std::string m_mirror_path;
    int m_vuid;

    // Registry Management
    std::map<std::string, VirtualIdentity> registry_;
    std::mutex registry_mutex_;
};

} // namespace vmeer

#endif // VMEER_CONTEXT_H
