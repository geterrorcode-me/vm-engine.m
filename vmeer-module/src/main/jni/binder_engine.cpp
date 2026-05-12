#include "include/vmeer_context.h"
#include "include/pms_runtime.h" // Modul PMS
#include "binder_engine.h"
#include <jni.h>
#include <android/log.h>
#include <string>

#define LOG_TAG "vMeer_BinderEngine"

namespace vmeer {
namespace binder {

/**
 * Fungsi ini dipanggil oleh hook 'onTransact' di binder_vm.cpp
 */
bool OnTransactOverride(uint32_t code, void* data_parcel, void* reply_parcel, uint32_t flags) {
    // 1. Identifikasi Interface (misal: "android.os.IServiceManager")
    // std::u16string descriptor = get_descriptor(data_parcel);

    // 2. Logika untuk Android ID (Settings Provider)
    if (code == 1 /* TRANSACTION_get_string_settings_id */) { 
        std::string v_id = RuntimeContext::Get().GetAndroidId();
        // write_to_parcel(reply_parcel, v_id);
        __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "Redirected Android ID to: %s", v_id.c_str());
        return true; 
    }

    // 3. Logika untuk PMS (Hiding Apps)
    if (code == 5 /* TRANSACTION_getInstalledPackages */) {
        // Biarkan transaksi asli jalan dulu, lalu kita filter reply-nya
        // vmeer::pms::filter_package_list(reply_parcel);
        __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "Filtering Package List via PMSRuntime");
        return true;
    }

    return false; // Lanjutkan ke transaksi asli jika tidak di-override
}

} // namespace binder
} // namespace vmeer
