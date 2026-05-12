#include "include/vmeer_context.h" // Gunakan path yang benar
#include "vmeer_db.h"
#include "vmeer_pms.h"
#include <android/log.h>

#define LOG_TAG "vMeer_Context"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

namespace vmeer {

bool RuntimeContext::Initialize(const char* vm_id, const char* target_pkg) {
    if (!vm_id || !target_pkg) return false;

    m_vm_id = vm_id;
    m_target_package = target_pkg;

    // Inisialisasi Database
    std::string db_path = "/data/data/" + m_target_package + "/vmeer_core.db";
    if (!init_vmeer_database(db_path.c_str())) {
        LOGI("vMeer: Database init failed");
        return false;
    }

    // Ambil Android ID Virtual
    m_v_android_id = get_v_android_id_c(target_pkg);

    // Registrasi ke Virtual Ecosystem
    pms::PMSRuntime::Get().RegisterPackage(target_pkg, 10000);

    LOGI("vMeer: [Context] System initialized for %s", target_pkg);
    return true;
}

pms::PMSRuntime& RuntimeContext::Package() {
    return pms::PMSRuntime::Get();
}

} // namespace vmeer
