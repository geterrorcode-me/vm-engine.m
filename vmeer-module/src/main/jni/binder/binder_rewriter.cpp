#include "include/vm_internal.h"
#include "bridge/hook_bridge.h"

// --- FORWARD DECLARATION ---
// Ini menggantikan #include "Parcel.h"
// Compiler tidak akan komplain lagi soal file missing
namespace android {
    class Parcel; 
}

using namespace android;

// Pointer untuk menyimpan fungsi asli
typedef int (*transact_t)(void*, uint32_t, Parcel*, Parcel*, uint32_t);
static transact_t orig_transact = nullptr;

// Fungsi pengganti (Proxy)
extern "C" int proxy_transact(void* ipc_state, uint32_t code, Parcel* data, Parcel* reply, uint32_t flags) {
    // Di sini kita bisa memantau transaksi binder tanpa perlu tahu isi class Parcel secara detail
    // LOGI("vMeer: Transact intercepted, code: %u", code);

    if (orig_transact) {
        return orig_transact(ipc_state, code, data, reply, flags);
    }
    return -1;
}

void init_binder_isolation() {
    // Melakukan hook menggunakan ShadowHook melalui HookBridge
    // Nama simbol ini untuk Android 14+ (arm64)
    orig_transact = (transact_t)vmeer::HookBridge::hookSymbol(
        "libbinder.so",
        "_ZN7android14IPCThreadState8transactEjRKNS_6ParcelEPS1_j",
        (void*)proxy_transact
    );
    
    if(orig_transact) {
        LOGI("vMeer: Binder isolation active.");
    } else {
        LOGE("vMeer: Failed to hook Binder! Check symbol name.");
    }
}
