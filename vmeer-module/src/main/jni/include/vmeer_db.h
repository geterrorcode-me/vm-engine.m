#ifndef VMEER_DB_H
#define VMEER_DB_H

#include <string>
#include <vector>
#include <stdint.h>

namespace vmeer {
namespace db {

/**
 * Membuka database vMeer.
 * Lokasi biasanya di /data/data/com.vmeer.virtual/vmeer.db
 */
bool OpenDatabase(const std::string& db_path);

/**
 * Memuat state instance berdasarkan VM ID.
 * Mengisi seed, uptime, dan cycles.
 */
bool LoadInstanceContext(const std::string& vm_id, 
                         std::vector<uint8_t>* out_seed, 
                         int64_t* out_uptime,
                         int* out_cycles);

/**
 * Inisialisasi instance baru jika belum ada di database.
 */
bool CreateInstance(const std::string& vm_id, 
                    const std::string& pkg_name, 
                    const std::vector<uint8_t>& seed);

/**
 * Menyimpan data aging (uptime dan battery cycles).
 */
bool UpdateHealth(const std::string& vm_id, int64_t uptime, int cycles);

/**
 * Menutup database saat engine unload.
 */
void CloseDatabase();

} // namespace db
} // namespace vmeer

#endif // VMEER_DB_H
