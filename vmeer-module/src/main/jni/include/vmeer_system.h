#ifndef VMEER_SYSTEM_H
#define VMEER_SYSTEM_H

#ifdef __cplusplus
extern "C" {
#endif

// Deklarasi fungsi agar bisa diakses modul lain
void start_virtual_system_services();
void register_virtual_package(const char* pkgName, int uid);

#ifdef __cplusplus
}
#endif

#endif // VMEER_SYSTEM_H
