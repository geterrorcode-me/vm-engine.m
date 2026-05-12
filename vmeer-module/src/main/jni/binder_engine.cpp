#include <jni.h>
#include <android/log.h>
#include <string>
#include "include/vmeer_context.h"
#include "include/pms_runtime.h"
#include "include/ams_runtime.h" // Modul AMS diperdalam
#include "include/wms_runtime.h" // Modul WMS baru
#include "include/vmeer_helper.h"
#include "binder_engine.h"

#define LOG_TAG "vMeer_Ecosystem"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

namespace vmeer {
namespace binder {

void InitHooks() {
    LOGI("vMeer Ecosystem: Virtualizing Service Manager...");
    vmeer::helper::ConnectSharedState();
}

/**
 * OnTransactOverride:
 * Pusat kendali lalu lintas Binder. Di sini kita membelokkan dunia nyata ke virtual.
 */
bool OnTransactOverride(uint32_t code, void* data_parcel, void* reply_parcel, uint32_t flags) {
    (void)flags;

    // --- EVOLUSI 1: Virtual AMS (Deep Integration) ---
    // Menggunakan ams::is_ams_transaction untuk deteksi dinamis
    if (code == 15 /* START_ACTIVITY_TRANSACTION */) {
        LOGI("vMeer AMS: Deep Intercepting StartActivity...");
        // Memanggil fungsi dari ams_runtime untuk membedah dan membungkus Intent
        vmeer::ams::wrap_intent(data_parcel);
        return false; // Lanjutkan transaksi yang sudah dimodifikasi
    }

    // --- EVOLUSI 2: Window Manager Isolation (WMS) ---
    // Penting agar aplikasi virtual punya Window Token sendiri
    if (code == 1 /* ADD_WINDOW_TRANSACTION */) {
        LOGI("vMeer WMS: Patching Window Layout for Sandbox...");
        vmeer::wms::patch_window_layout(data_parcel);
        return false;
    }

    // --- EVOLUSI 3: Virtual Identity Management ---
    if (code == 102 /* Get Android ID Transaction */) { 
        std::string v_id = vmeer::RuntimeContext::Get().GetVAndroidId();
        LOGI("vMeer Identity: Dispatched Isolated ID -> %s", v_id.c_str());
        // Logic: Tulis v_id ke reply_parcel di sini
        return true; 
    }

    // --- EVOLUSI 4: Virtual Package Manager (PMS) ---
    if (code == 5 /* getPackageInfo */ || code == 82 /* getInstalledPackages */) {
        LOGI("vMeer PMS: Filtering package list for Sandbox isolation...");
        // Filter agar aplikasi virtual hanya melihat dirinya sendiri dan sistem dasar
        return vmeer::pms::filter_package_data(reply_parcel);
    }

    return false; 
}

} // namespace binder
} // namespace vmeer
