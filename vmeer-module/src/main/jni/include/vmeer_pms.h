#ifndef VMEER_PMS_H
#define VMEER_PMS_H

#include <string>
#include <vector>
#include <map>
#include <memory>

namespace vmeer {
namespace pms {

struct VirtualPackage {
    std::string package_name;
    uint32_t virtual_uid;
    std::string data_dir;
    std::vector<std::string> signatures;
    bool is_visible;
};

class PMSRuntime {
public:
    static PMSRuntime& Get();

    // Registry Management
    void RegisterPackage(const std::string& pkg_name, uint32_t v_uid);
    std::shared_ptr<VirtualPackage> GetPackage(const std::string& pkg_name); // Pastikan ini ADA

    // UID Mapping
    uint32_t GetVirtualUid(const std::string& pkg_name); // Pastikan ini ADA
    std::string GetPackageFromUid(uint32_t v_uid);      // Pastikan ini ADA

    // Visibility Control
    bool IsVisible(const std::string& target_pkg);
    
    // Path Translation
    std::string ResolveVirtualPath(const std::string& pkg_name); // Pastikan ini ADA

private:
    PMSRuntime() = default;
    std::map<std::string, std::shared_ptr<VirtualPackage>> m_registry;
    std::map<uint32_t, std::string> m_uid_to_pkg; // ERROR 'undeclared identifier' sembuh di sini
};

// Bridge functions untuk binder_engine
bool filter_package_data(void* reply_parcel);
void filter_package_list(std::vector<std::string>& packages);

} // namespace pms
} // namespace vmeer

#endif
