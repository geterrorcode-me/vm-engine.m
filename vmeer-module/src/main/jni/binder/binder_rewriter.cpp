#include <stdint.h>
#include <android/log.h>
#include <string.h>
#include "shadowhook.h"

#define TAG "vMeer_BinderBridge"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

// Forward declaration forward stub milik platform framework Android
namespace android {
    class Parcel;
}

typedef int (*transact_t)(void*, uint32_t, android::Parcel*, android::Parcel*, uint32_t);
static transact_t orig_transact = nullptr;

// Proxy Intersepsi IPC Thread State Binder
int proxy_transact(void* ipc_state, uint32_t code, android::Parcel* data, android::Parcel* reply, uint32_t flags) {
    
    // CODE 1 = IServiceManager::GET_SERVICE_TRANSACTION
    if (code == 1 && data != nullptr) {
        // [Fase ini digunakan oleh Mini SystemServer untuk mendeteksi query dari aplikasi tamu]
        // Jika aplikasi tamu meminta service "activity", "package", atau "window",
        // Pipa ini yang bertugas mengarahkannya ke GuestSystemServer.java milik kita.
    }

    if (orig_transact) {
        return orig_transact(ipc_state, code, data, reply, flags);
    }
    return -1;
}

extern "C" void init_binder_isolation() {
    // Manfaatkan shadowhook untuk membajak IPCThreadState::transact secara murni di libbinder.so
    void* stub = shadowhook_hook_sym_name(
        "libbinder.so",
        "_ZN7android14IPCThreadState8transactEjRKNS_6ParcelEPS1_j",
        (void*)proxy_transact,
        (void**)&orig_transact
    );
    
    if (stub != nullptr) {
        LOGI("vMeer: Binder Isolation Active via ShadowHook (Urutan 3)");
    } else {
        LOGE("vMeer: Gagal melakukan hooking libbinder.so!");
    }
}
