#ifndef VMEER_BINDER_VM_H
#define VMEER_BINDER_VM_H

// Inisialisasi pengalihan Binder (Urutan 3)
void init_vmeer_internals();

// Fungsi untuk membelokkan transaksi binder tertentu
void redirect_binder_transaction(int code);

#endif
