#ifndef VMEER_CONTEXT_H
#define VMEER_CONTEXT_H

#include <string>
#include <map>
#include <mutex>
#include "vmeer_pms.h"

namespace vmeer {

// Struktur Identitas untuk Registry
struct VirtualIdentity {
    std::string package_name;
    int virtual_uid;
    bool is_isolated;
};

class RuntimeContext {
public:
    static RuntimeContext& Get() {
        static RuntimeContext instance;
        return instance;
    }

    // Fungsi Utama (Tipe data diselaraskan dengan .cpp)
    bool Initialize(const std::string& vm_id, const std::string& target_pkg);
    void Heartbeat(); 
    pms::PMSRuntime& Package();

    // Registry Access untuk Zygote
    void RegisterVirtualApp(const std::string& pkg, int v_uid);
    VirtualIdentity* GetIdentity(const std::string& proc_name);

    // Getters
    std::string GetTargetPackage() const { return m_target_package; }
    std::string GetVAndroidId() const { return m_v_android_id; }
    std::string GetMasterSeed() const { return m_master_seed; }

private:
    RuntimeContext() : m_master_seed("vmeer_default_seed_8888") {}
    
    std::string m_vm_id;
    std::string m_target_package;
    std::string m_v_android_id;
    std::string m_master_seed;

    // Registry untuk evolusi String Descriptor
    std::map<std::string, VirtualIdentity> registry_;
    std::mutex registry_mutex_;
};

} // namespace vmeer

#endif
