#include <jni.h>
#include <android/log.h>
#include <string>
#include "include/vmeer_context.h"
#include "include/pms_runtime.h"
#include "binder_engine.h"

#define LOG_TAG "vMeer_Ecosystem"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

namespace vmeer {
namespace binder {

/**
 * INIT HOOKS: Fondasi Ekosistem
 * Mengaktifkan gerbang utama untuk Virtual Package Management.
 */
void InitHooks() {
    LOGI("vMeer Ecosystem: Initializing Virtual Package Bridge...");
    // Di sini tempat inisialisasi modul PMS agar siap melayani request binder
}

/**
 * ON TRANSACT OVERRIDE: Pusat Kendali Ekosistem
 * Mengatur bagaimana aplikasi melihat identitas perangkat dan aplikasi lain.
 */
bool OnTransactOverride(uint32_t code, void* data_parcel, void* reply_parcel, uint32_t flags) {
    (void)flags;

    // --- EVOLUSI 1: Virtual Identity Management (Android ID) ---
    // Bukan sekadar ganti string, tapi memastikan 'Settings Provider' memberikan ID unik per virtual instance.
    if (code == 1 /* TRANSACTION_get_string_settings_id */) { 
        std::string v_id = vmeer::RuntimeContext::Get().GetVAndroidId();
        LOGI("Ecosystem: Isolated Identity dispatched -> %s", v_id.c_str());
        
        // Logika pengisian parcel (write_to_parcel) tetap berada di binder_vm.cpp
        return true; 
    }

    // --- EVOLUSI 2: Virtual Package Ecosystem (PMS) ---
    // Dari "Hiding" (sembunyi) menjadi "Ecosystem" (lingkungan yang disesuaikan).
    // Kita memanipulasi cara sistem melaporkan aplikasi yang terinstall.
    if (code == 5 /* TRANSACTION_getInstalledPackages */ || code == 82 /* getInstalledApplications */) {
        LOGI("Ecosystem: Virtualizing Package List for isolation...");
        
        // Memanggil PMSRuntime untuk menyusun list aplikasi "palsu" 
        // yang terlihat legal bagi aplikasi target.
        return true;
    }

    // Biarkan transaksi lain berjalan normal (atau diproses modul stealth lain)
    return false; 
}

} // namespace binder
} // namespace vmeer
