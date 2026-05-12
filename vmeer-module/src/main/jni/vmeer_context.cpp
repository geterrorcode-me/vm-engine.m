#include "include/vmeer_context.h"
#include <android/log.h>

#define LOG_TAG "vMeer_Context"

namespace vmeer {

bool RuntimeContext::Initialize(const std::string& vm_id, const std::string& target_pkg) {
    std::lock_guard<std::mutex> lock(registry_mutex_);
    
    m_vm_id = vm_id;
    m_target_package = target_pkg;
    m_v_android_id = "v_aid_" + vm_id;

    // PERBAIKAN: Tambahkan 'true' di akhir untuk use_virtual_storage
    VirtualIdentity ident = {target_pkg, 20000, true, true};
    registry_[target_pkg] = ident;

    __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "vMeer Context Reborn: %s", target_pkg.c_str());
    return true;
}

void RuntimeContext::RegisterVirtualApp(const std::string& pkg, int v_uid) {
    std::lock_guard<std::mutex> lock(registry_mutex_);
    // PERBAIKAN: Tambahkan 'true' di akhir untuk use_virtual_storage
    registry_[pkg] = {pkg, v_uid, true, true};
}

VirtualIdentity* RuntimeContext::GetIdentity(const std::string& proc_name) {
    std::lock_guard<std::mutex> lock(registry_mutex_);
    auto it = registry_.find(proc_name);
    if (it != registry_.end()) {
        return &(it->second);
    }
    return nullptr;
}

void RuntimeContext::Heartbeat() {
    __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "Heartbeat: VM Process is Alive.");
}

pms::PMSRuntime& RuntimeContext::Package() { 
    return pms::PMSRuntime::Get(); 
}

} // namespace vmeer
