#ifndef VMEER_BINDER_ENGINE_H
#define VMEER_BINDER_ENGINE_H

extern "C" {
    // Pastikan nama fungsi ini sama persis dengan yang ada di binder_engine.cpp
    void start_binder_proxy();
}

namespace vmeer {
namespace binder {
    void InitHooks();
}
}

#endif
