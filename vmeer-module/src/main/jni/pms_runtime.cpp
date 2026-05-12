#include "include/vmeer_context.h"
#include <android/log.h>
#include <vector>
#include <string>

#define LOG_TAG "vMeer_PMS"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

namespace vmeer {
namespace pms {

/**
 * Filter daftar paket yang diinstal.
 * Menghapus paket vMeer dari list agar aplikasi target merasa dia di lingkungan asli.
 */
void filter_package_list(std::vector<std::string>& packages) {
    std::vector<std::string> blacklisted = {
        "com.vmeer.virtual", 
        "com.vmeer.manager",
        "io.github.vmeer"
    };

    for (auto it = packages.begin(); it != packages.end(); ) {
        bool is_blacklisted = false;
        for (const auto& b : blacklisted) {
            if (*it == b) {
                is_blacklisted = true;
                break;
            }
        }
        if (is_blacklisted) {
            LOGI("vMeer_PMS: Hiding package: %s", it->c_str());
            it = packages.erase(it);
        } else {
            ++it;
        }
    }
}

/**
 * Mocking Installer Package Name.
 * Banyak app cek apakah mereka diinstall dari Play Store atau bukan.
 */
std::string get_virtual_installer(const std::string& target_pkg) {
    // Kita bisa buat deterministik: Selalu terlihat diinstall dari Play Store
    return "com.android.vending";
}

} // namespace pms
} // namespace vmeer
