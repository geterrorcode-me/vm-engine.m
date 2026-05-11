#include "hook_bridge.h"
#include "shadowhook.h"
#include <android/log.h>

namespace vmeer {
    void* HookBridge::hookSymbol(const char* libName, const char* symbol, void* proxy) {
        void* stub = shadowhook_hook_sym_name(libName, symbol, proxy, nullptr);
        if (stub == nullptr) {
            int err_num = shadowhook_get_errno();
            __android_log_print(ANDROID_LOG_ERROR, "vMeer_Hook", "Gagal hook %s: %d", symbol, err_num);
        }
        return stub;
    }

    void HookBridge::unhook(void* stub) {
        if (stub) shadowhook_unhook(stub);
    }
}
