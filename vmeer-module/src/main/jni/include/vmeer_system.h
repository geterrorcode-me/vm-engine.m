#ifndef VMEER_SYSTEM_H
#define VMEER_SYSTEM_H

#include <string>

// Memulai Virtual System Services (Urutan 6 & 7)
void start_virtual_system_services();

// Mendaftarkan aplikasi ke dalam Virtual PMS
void register_virtual_package(const char* pkgName, int uid);

#endif
