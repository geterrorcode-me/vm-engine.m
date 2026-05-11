#ifndef VMEER_STEALTH_H
#define VMEER_STEALTH_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Mengaktifkan mode gaib dengan mencegat pembacaan /proc/self/maps
 * agar library vMeer tidak terdeteksi oleh aplikasi target.
 */
void init_vmeer_stealth();

#ifdef __cplusplus
}
#endif

#endif // VMEER_STEALTH_H
