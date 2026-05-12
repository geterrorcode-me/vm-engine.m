#ifndef VMEER_CONTEXT_H
#define VMEER_CONTEXT_H

#include <string>
#include "vmeer_pms.h"

namespace vmeer {

class RuntimeContext {
public:
    static RuntimeContext& Get() {
        static RuntimeContext instance;
        return instance;
    }

    // Fungsi Utama yang harus dideklarasikan
    bool Initialize(const char* vm_id, const char* target_pkg);
    void Heartbeat(); 
    pms::PMSRuntime& Package();

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
};

} // namespace vmeer

#endif
