#include <jni.h>
#include <android/log.h>
#include <string>
#include "include/vmeer_context.h"
#include "include/pms_runtime.h"
#include "include/vmeer_helper.h" // Modul baru untuk Shared Memory
#include "binder_engine.h"

#define LOG_TAG "vMeer_Ecosystem"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

namespace vmeer {
namespace binder {

void InitHooks() {
    LOGI("vMeer Ecosystem: Virtualizing Service Manager...");
    // Inisialisasi cache shared memory untuk akses cepat data AMS virtual
    vmeer::helper::ConnectSharedState();
}

bool OnTransactOverride(uint32_t code, void* data_parcel, void* reply_parcel, uint32_t flags) {
    (void)flags;

    // --- EVOLUSI 1: Virtual AMS (Task & Process Isolation) ---
    // Intersep startActivity (Code 15) untuk membungkus intent asli ke dalam Stub
    if (code == 15 /* START_ACTIVITY_TRANSACTION */) {
        LOGI("vMeer AMS: Intercepting StartActivity. Redirecting to StubContainer...");
        // Logic: Ubah Target Component menjadi com.vmeer.stub.StubActivity
        return true; 
    }

    // --- EVOLUSI 2: Virtual Identity Management ---
    if (code == 1 /* TRANSACTION_get_string_settings_id */) { 
        std::string v_id = vmeer::RuntimeContext::Get().GetVAndroidId();
        LOGI("vMeer Identity: Dispatched Isolated ID -> %s", v_id.c_str());
        return true; 
    }

    // --- EVOLUSI 3: Virtual Package Ecosystem ---
    if (code == 5 || code == 82) {
        LOGI("vMeer PMS: Virtualizing Environment for Sandbox isolation...");
        return true;
    }

    return false; 
}

} // namespace binder
} // namespace vmeer
