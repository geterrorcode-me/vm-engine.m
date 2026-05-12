#include "include/pms_runtime.h"
#include <android/log.h>
#include <vector>
#include <string>
#include <algorithm>

#define LOG_TAG "vMeer_PMS"

namespace vmeer {
namespace pms {

    /**
     * Evolusi: Filter daftar paket agar aplikasi target merasa 
     * berada di lingkungan yang bersih (Virtual Ecosystem).
     */
    void filter_package_list(std::vector<std::string>& packages) {
        // Daftar paket yang dilarang terlihat oleh guest app
        std::vector<std::string> blacklisted = {
            "com.topjohnwu.magisk",
            "com.vmeer.manager",
            "org.meowcat.edxposed.manager"
        };

        packages.erase(
            std::remove_if(packages.begin(), packages.end(), [&](const std::string& pkg) {
                for (const auto& b : blacklisted) {
                    if (pkg.find(b) != std::string::npos) {
                        __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "Hiding package: %s", pkg.c_str());
                        return true;
                    }
                }
                return false;
            }), 
            packages.end()
        );
    }

    std::string get_virtual_installer(const std::string& target_pkg) {
        // Berpura-pura diinstall oleh Play Store untuk semua aplikasi virtual
        (void)target_pkg;
        return "com.android.vending";
    }

} // namespace pms
} // namespace vmeer
