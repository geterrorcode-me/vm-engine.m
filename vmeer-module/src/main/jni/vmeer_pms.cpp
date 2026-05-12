#include "include/vmeer_pms.h"
#include <android/log.h>
#include <algorithm>

#define LOG_TAG "vMeer_PMS"

namespace vmeer {
namespace pms {

/**
 * Singleton Get():
 * Mengembalikan instance statis dari PMSRuntime.
 */
PMSRuntime& PMSRuntime::Get() {
    static PMSRuntime instance;
    return instance;
}

void PMSRuntime::RegisterPackage(const std::string& pkg_name, uint32_t v_uid) {
    auto pkg = std::make_shared<VirtualPackage>();
    pkg->package_name = pkg_name;
    pkg->virtual_uid = v_uid;
    pkg->is_visible = true;

    m_registry[pkg_name] = pkg;
    m_uid_to_pkg[v_uid] = pkg_name;
    
    __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "Registered: %s (vUID: %u)", pkg_name.c_str(), v_uid);
}

std::shared_ptr<VirtualPackage> PMSRuntime::GetPackage(const std::string& pkg_name) {
    auto it = m_registry.find(pkg_name);
    if (it != m_registry.end()) {
        return it->second;
    }
    return nullptr;
}

uint32_t PMSRuntime::GetVirtualUid(const std::string& pkg_name) {
    auto pkg = GetPackage(pkg_name);
    return pkg ? pkg->virtual_uid : 0;
}

std::string PMSRuntime::GetPackageFromUid(uint32_t v_uid) {
    auto it = m_uid_to_pkg.find(v_uid);
    return (it != m_uid_to_pkg.end()) ? it->second : "";
}

bool PMSRuntime::IsVisible(const std::string& target_pkg) {
    // Default visibility logic
    if (target_pkg.find("com.vmeer") != std::string::npos) return false;
    return true;
}

std::string PMSRuntime::ResolveVirtualPath(const std::string& pkg_name) {
    return "/data/data/com.vmeer.manager/virtual/storage/" + pkg_name;
}

/**
 * Bridge function untuk binder_engine.cpp
 */
bool filter_package_data(void* reply_parcel) {
    if (!reply_parcel) return false;
    // TODO: Implementasi Parcel stripping menggunakan libbinder internal
    __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, "PMS: Intercepted Package Data Reply.");
    return true;
}

void filter_package_list(std::vector<std::string>& packages) {
    std::vector<std::string> blacklisted = {
        "com.topjohnwu.magisk",
        "com.vmeer.manager",
        "org.meowcat.edxposed.manager"
    };

    packages.erase(
        std::remove_if(packages.begin(), packages.end(), [&](const std::string& pkg) {
            for (const auto& b : blacklisted) {
                if (pkg.find(b) != std::string::npos) {
                    __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "Hiding: %s", pkg.c_str());
                    return true;
                }
            }
            return false;
        }), 
        packages.end()
    );
}

} // namespace pms
} // namespace vmeer
