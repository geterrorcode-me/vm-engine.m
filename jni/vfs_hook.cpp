#include "vm_internal.h"

extern "C" void init_vfs_bridge(const char* path) {
    LOGI("vMeer VFS: Mapping ROM from %s", path);
}
