#include "include/vmeer_pms.h"
#include <android/log.h>
#include <algorithm>

#define LOG_TAG "vMeer_PMS"

namespace vmeer {
namespace pms {

    // Implementasi filter_package_data (Yang dipanggil binder_engine.cpp)
    bool filter_package_data(void* reply_parcel) {
        if (!reply_parcel) return false;
        
        __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "PMS: Filtering Transaction via Parcel...");
        // Di sini nanti kita bedah Parcel-nya
        return true; 
    }

    // Pindahan dari pms_runtime.cpp
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
                        return true;
                    }
                }
                return false;
            }), 
            packages.end()
        );
    }

    std::string get_virtual_installer(const std::string& target_pkg) {
        (void)target_pkg;
        return "com.android.vending";
    }

} // namespace pms
} // namespace vmeer
