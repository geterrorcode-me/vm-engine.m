#include "include/vmeer_context.h"
#include "vmeer_db.h" // WAJIB: Agar fungsi init_vmeer_database dikenali
#include "vmeer_pms.h"
#include <android/log.h>

namespace vmeer {
bool RuntimeContext::Initialize(const char* vm_id, const char* target_pkg) {
    if (!vm_id || !target_pkg) return false;
    m_vm_id = vm_id;
    m_target_package = target_pkg;

    std::string db_path = "/data/data/" + m_target_package + "/vmeer_core.db";
    if (!init_vmeer_database(db_path.c_str())) return false;

    m_v_android_id = get_v_android_id_c(target_pkg);
    pms::PMSRuntime::Get().RegisterPackage(target_pkg, 10000);
    return true;
}

void RuntimeContext::Heartbeat() { /* Logic */ }
pms::PMSRuntime& RuntimeContext::Package() { return pms::PMSRuntime::Get(); }
}
