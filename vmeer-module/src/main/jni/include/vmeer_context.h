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

    // Tambahkan deklarasi ini agar match dengan .cpp
    bool Initialize(const char* vm_id, const char* target_pkg);
    pms::PMSRuntime& Package();

    // Member variables
    std::string GetTargetPackage() const { return m_target_package; }
    std::string GetVAndroidId() const { return m_v_android_id; }

private:
    RuntimeContext() = default;
    std::string m_vm_id;
    std::string m_target_package;
    std::string m_v_android_id;
};

} // namespace vmeer

#endif
