#include "vm_internal.h"
extern "C" void mount_rom_container(const char* path) {
    LOGI("vMeer ROMFS: Mounting .bin container from %s", path);
}
