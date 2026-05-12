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
        bool is_visible;
    };

    class PMSRuntime {
    public:
        static PMSRuntime& Get();
        void RegisterPackage(const std::string& pkg_name, uint32_t v_uid);
        bool IsVisible(const std::string& target_pkg);
        
    private:
        PMSRuntime() = default;
        std::map<std::string, std::shared_ptr<VirtualPackage>> m_registry;
    };

    // --- FUNGSI YANG DICARI BINDER ENGINE ---
    bool filter_package_data(void* reply_parcel);
    void filter_package_list(std::vector<std::string>& packages);
    std::string get_virtual_installer(const std::string& target_pkg);

} // namespace pms
} // namespace vmeer

#endif
