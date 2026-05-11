#ifndef VMEER_HOOK_BRIDGE_H
#define VMEER_HOOK_BRIDGE_H

#include <vector>
#include <string>

namespace vmeer {
    class HookBridge {
    public:
        static void* hookSymbol(const char* libName, const char* symbol, void* proxy);
        static void unhook(void* stub);
    };
}

#endif
