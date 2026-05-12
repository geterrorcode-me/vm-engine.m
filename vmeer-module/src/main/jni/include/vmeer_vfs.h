#ifndef VMEER_VFS_H
#define VMEER_VFS_H

namespace vmeer {
namespace vfs {

/**
 * StartVFSEngine:
 * Mengaktifkan seluruh mekanisme Virtual File System.
 * Ini mencakup:
 * 1. Path Redirection (I/O Sandboxing)
 * 2. Ghost Directory (getdents64 stealth)
 * 3. Maps Scrubber (Proc stealth)
 */
void StartVFSEngine();

} // namespace vfs
} // namespace vmeer

#endif // VMEER_VFS_H
