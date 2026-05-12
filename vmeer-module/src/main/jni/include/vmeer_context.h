#ifndef VMEER_CONTEXT_H
#define VMEER_CONTEXT_H

#include <string>
#include <map>
#include <mutex>

namespace vmeer {

// Definisi struktur yang dicari oleh .cpp
struct VirtualIdentity {
    std::string package_name;
    int virtual_uid;
    bool is_isolated;
    bool use_virtual_storage;
};

class RuntimeContext {
public:
    static RuntimeContext& Get() {
        static RuntimeContext instance;
        return instance;
    }

    // Public Methods
    bool Initialize(const std::string& vm_id, const std::string& target_pkg);
    void RegisterVirtualApp(const std::string& pkg, int v_uid);
    VirtualIdentity* GetIdentity(const std::string& proc_name);
    void Heartbeat();

    // High-End Setters untuk VM
    void SetVirtualUid(int uid) { m_vuid = uid; }
    void SetMirrorPath(const std::string& path) { m_mirror_path = path; }

    // Getters
    std::string GetTargetPackage() const { return m_target_package; }
    std::string GetVAndroidId() const { return m_v_android_id; }
    int GetVirtualUid() const { return m_vuid; }

private:
    RuntimeContext() : m_vuid(0), m_master_seed("vmeer_seed_default") {}

    // Member Variables yang dibutuhkan oleh vmeer_context.cpp
    std::string m_vm_id;
    std::string m_target_package;
    std::string m_v_android_id;
    std::string m_master_seed;
    std::string m_mirror_path;
    int m_vuid;

    // Registry & Mutex (Penyebab utama error build tadi)
    std::map<std::string, VirtualIdentity> registry_;
    std::mutex registry_mutex_;
};

} // namespace vmeer

#endif
