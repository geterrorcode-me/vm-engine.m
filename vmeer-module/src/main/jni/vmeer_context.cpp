#include "include/vmeer_context.h"
#include "vmeer_db.h" // Sangat penting agar fungsi DB dikenali
#include "vmeer_pms.h"
#include <android/log.h>

#define LOG_TAG "vMeer_Context"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

namespace vmeer {

bool RuntimeContext::Initialize(const char* vm_id, const char* target_pkg) {
    if (!vm_id || !target_pkg) return false;

    m_vm_id = vm_id;
    m_target_package = target_pkg;

    // Sekarang init_vmeer_database akan dikenali karena ada di vmeer_db.h
    std::string db_path = "/data/data/" + m_target_package + "/vmeer_core.db";
    if (!init_vmeer_database(db_path.c_str())) {
        return false;
    }

    m_v_android_id = get_v_android_id_c(target_pkg);
    pms::PMSRuntime::Get().RegisterPackage(target_pkg, 10000);

    return true;
}

// Tambahkan definisi Heartbeat agar sensor_engine.cpp tidak error
void RuntimeContext::Heartbeat() {
    // Logic heartbeat
}

pms::PMSRuntime& RuntimeContext::Package() {
    return pms::PMSRuntime::Get();
}

} // namespace vmeer
