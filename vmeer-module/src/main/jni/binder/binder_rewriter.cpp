#include "include/vm_internal.h"
#include "bridge/hook_bridge.h"

// Forward Declaration untuk Parcel
namespace android {
    class Parcel;
}

using namespace android;

typedef int (*transact_t)(void*, uint32_t, Parcel*, Parcel*, uint32_t);
static transact_t orig_transact = nullptr;

// Proxy untuk membelokkan komunikasi AMS/PMS
extern "C" int proxy_transact(void* ipc_state, uint32_t code, Parcel* data, Parcel* reply, uint32_t flags) {
    
    // Urutan 3: Binder Interception
    // Code 1 biasanya adalah GET_SERVICE_TRANSACTION pada IServiceManager
    if (code == 1) {
        // Di sini kita bisa mengintip Parcel 'data' untuk melihat nama service.
        // Jika request "activity" atau "package", kita bisa memalsukan 'reply'.
        // LOGI("vMeer: Mencegat request ServiceManager");
    }

    if (orig_transact) {
        return orig_transact(ipc_state, code, data, reply, flags);
    }
    return -1;
}

void init_binder_isolation() {
    orig_transact = (transact_t)vmeer::HookBridge::hookSymbol(
        "libbinder.so",
        "_ZN7android14IPCThreadState8transactEjRKNS_6ParcelEPS1_j",
        (void*)proxy_transact
    );
    
    if(orig_transact) {
        LOGI("vMeer: Binder Isolation Active (Urutan 3)");
    }
}
