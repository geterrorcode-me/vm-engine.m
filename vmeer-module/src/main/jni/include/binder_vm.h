#ifndef VMEER_BINDER_VM_H
#define VMEER_BINDER_VM_H

#ifdef __cplusplus
extern "C" {
#endif

void start_binder_proxy();
const char* resolve_virtual_service(const char* original_name);

#ifdef __cplusplus
}
#endif

#endif
