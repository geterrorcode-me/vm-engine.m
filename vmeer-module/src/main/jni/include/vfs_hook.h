#ifndef VMEER_VFS_HOOK_H
#define VMEER_VFS_HOOK_H

#include <string>

// Fungsi untuk menyiapkan isolasi folder (Namespace)
bool requestNamespaceSetup(const char* packageName);

// Fungsi untuk menyembunyikan file vMeer dari aplikasi target
void init_vfs_stealth();

#endif // VMEER_VFS_HOOK_H
