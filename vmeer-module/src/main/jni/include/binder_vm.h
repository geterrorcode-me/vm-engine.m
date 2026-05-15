#ifndef VMEER_BINDER_VM_H
#define VMEER_BINDER_VM_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Memulai sistem interceptor Binder
void start_binder_proxy();

// Membelokkan nama service asli ke service virtual vMeer
const char* resolve_virtual_service(const char* original_name);

// Melakukan remapping handle binder untuk isolasi
int32_t remap_to_virtual(int32_t real_handle);

#ifdef __cplusplus
}
#endif

#endif
