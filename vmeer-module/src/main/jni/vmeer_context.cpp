#include "vmeer_context.h"
#include "vmeer_db.h"
#include "vmeer_pms.h" // Include baru
#include <android/log.h>

#define LOG_TAG "vMeer_Context"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

namespace vmeer {

bool RuntimeContext::Initialize(const char* vm_id, const char* target_pkg) {
    m_vm_id = vm_id;
    m_target_package = target_pkg;

    // 1. Inisialisasi Database
    std::string db_path = "/data/data/" + m_target_package + "/vmeer_core.db";
    if (!init_vmeer_database(db_path.c_str())) {
        return false;
    }

    // 2. Load Identity
    m_v_android_id = get_v_android_id_c(target_pkg);

    // 3. Bangun Virtual Package Ecosystem
    // Simulasi: Daftarkan paket target ke dalam ekosistem virtual
    // Di masa depan, ini akan memuat daftar dari DB
    pms::PMSRuntime::Get().RegisterPackage(target_pkg, 10000 + (rand() % 1000));

    LOGI("vMeer: [Context] System initialized for %s", target_pkg);
    return true;
}

// Tambahkan getter untuk akses mudah ke PMS
pms::PMSRuntime& RuntimeContext::Package() {
    return pms::PMSRuntime::Get();
}

} // namespace vmeer
