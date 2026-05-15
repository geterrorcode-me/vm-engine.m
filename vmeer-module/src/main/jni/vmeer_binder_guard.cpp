#include <jni.h>
#include <android/log.h>
#include <stdint.h>
#include <dlfcn.h>
#include <unistd.h>
#include "shadowhook.h"

#define LOG_TAG "vMeer_Stealth"

typedef int (*transact_t)(void* instance, uint32_t code, void* data, void* reply, uint32_t flags);
static transact_t original_transact = nullptr;

// Gunakan fungsi internal tanpa mengekspor simbol ke tabel publik
static int vmeer_proxy_transact(void* instance, uint32_t code, void* data, void* reply, uint32_t flags) {
    // Tambahkan logic proteksi agar tidak terjadi rekursi hook
    return original_transact(instance, code, data, reply, flags);
}

void install_vmeer_binder_hooks() {
    // ShadowHook akan secara otomatis melakukan 'Instruction Refactoring'
    // Ini mengubah kode asli menjadi trampolin yang tidak terlihat seperti hook standar
    void* stub = shadowhook_hook_sym_name(
        "libbinder.so",
        "_ZN7android15IPCThreadState8transactEiRKNS_6ParcelEPS1_j",
        (void*)vmeer_proxy_transact,
        (void**)&original_transact
    );

    if (stub) {
        // Setelah hook terpasang, ShadowHook menyimpan alamat asli di 'stub'
        // Kita tidak perlu mengekspos alamat ini ke variabel global yang bisa dibaca scanner
    }
}
