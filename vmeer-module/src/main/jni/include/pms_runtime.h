#ifndef VMEER_PMS_RUNTIME_H
#define VMEER_PMS_RUNTIME_H

#include <string>
#include <vector>

namespace vmeer {
namespace pms {

/**
 * Interface untuk memfilter daftar paket yang terdeteksi oleh aplikasi target.
 * Digunakan oleh BinderEngine saat mencegat transaksi getInstalledPackages.
 */
void FilterPackageList(std::vector<std::string>& packages);

/**
 * Memberikan nama installer palsu (misal: "com.android.vending") 
 * agar aplikasi merasa diinstal dari sumber resmi.
 */
std::string GetVirtualInstaller(const std::string& target_pkg);

/**
 * Mengecek apakah sebuah paket harus disembunyikan secara individual.
 */
bool IsPackageBlacklisted(const std::string& pkg_name);

} // namespace pms
} // namespace vmeer

#endif // VMEER_PMS_RUNTIME_H
