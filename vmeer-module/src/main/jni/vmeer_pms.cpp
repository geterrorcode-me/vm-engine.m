#include "vmeer_pms.h"
#include <android/log.h>

#define LOG_TAG "vMeer_PMS"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

namespace vmeer {
namespace pms {

PMSRuntime& PMSRuntime::Get() {
    static PMSRuntime instance;
    return instance;
}

void PMSRuntime::RegisterPackage(const std::string& pkg_name, uint32_t v_uid) {
    auto pkg = std::make_shared<VirtualPackage>();
    pkg->package_name = pkg_name;
    pkg->virtual_uid = v_uid;
    pkg->data_dir = "/data/data/com.vmeer.virtual/v_user/" + pkg_name;
    pkg->is_visible = true;

    m_registry[pkg_name] = pkg;
    m_uid_to_pkg[v_uid] = pkg_name;
    
    LOGI("vMeer: [PMS] Registered %s with Virtual UID: %u", pkg_name.c_str(), v_uid);
}

std::shared_ptr<VirtualPackage> PMSRuntime::GetPackage(const std::string& pkg_name) {
    if (m_registry.find(pkg_name) != m_registry.end()) {
        return m_registry[pkg_name];
    }
    return nullptr;
}

uint32_t PMSRuntime::GetVirtualUid(const std::string& pkg_name) {
    auto pkg = GetPackage(pkg_name);
    return pkg ? pkg->virtual_uid : 0;
}

std::string PMSRuntime::GetPackageFromUid(uint32_t v_uid) {
    if (m_uid_to_pkg.find(v_uid) != m_uid_to_pkg.end()) {
        return m_uid_to_pkg[v_uid];
    }
    return "";
}

bool PMSRuntime::IsVisible(const std::string& target_pkg) {
    // Logika Visibility Graph: 
    // Contoh: Hanya tampilkan jika paket terdaftar di registry virtual kita
    return m_registry.find(target_pkg) != m_registry.end();
}

std::string PMSRuntime::ResolveVirtualPath(const std::string& pkg_name) {
    auto pkg = GetPackage(pkg_name);
    return pkg ? pkg->data_dir : "";
}

} // namespace pms
} // namespace vmeer
