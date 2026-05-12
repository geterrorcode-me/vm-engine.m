#include "include/vmeer_system.h"  // <--- Tambahkan ini agar sinkron
#include <jni.h>
#include <string.h>
#include <sys/system_properties.h>
#include <android/log.h>
#include "shadowhook.h"

#define LOG_TAG "vMeer_System"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

extern "C" {

struct PropFake {
    const char* key;
    const char* value;
};

/**
 * Spoof List:
 * Menyamarkan identitas perangkat agar terlihat seperti 
 * perangkat produksi resmi (Retail/User).
 */
PropFake spoof_props[] = {
    {"ro.debuggable", "0"},         // Mematikan flag debuggable
    {"ro.secure", "1"},             // Memastikan sistem terlihat aman
    {"ro.build.type", "user"},      // Mengubah tipe build dari userdebug ke user
    {"ro.build.tags", "release-keys"}, // Mengganti test-keys (tanda root/custom)
    {"ro.product.model", "VMEER-A14-PRO"}, 
    {"ro.build.selinux", "1"}       // Pura-pura SELinux Enforcing
};

typedef int (*orig_sys_get_t)(const char*, char*);

/**
 * hook_system_property_get:
 * Interceptor untuk memfilter pembacaan system properties.
 */
int hook_system_property_get(const char* name, char* value) {
    if (!name || !value) return 0;

    for (const auto& prop : spoof_props) {
        if (strcmp(name, prop.key) == 0) {
            strcpy(value, prop.value);
            return (int)strlen(value);
        }
    }

    // Jika bukan properti yang kita spoof, panggil fungsi asli libc
    return ((orig_sys_get_t)shadowhook_get_prev_stack(reinterpret_cast<void*>(hook_system_property_get)))(name, value);
}

/**
 * start_virtual_system_services:
 * Fungsi aktivasi yang dipanggil oleh JNI_OnLoad.
 */
void start_virtual_system_services() {
    // Hook __system_property_get di libc.so (Gerbang utama properti sistem)
    shadowhook_hook_symname("libc.so", "__system_property_get", (void*)hook_system_property_get, nullptr);
    LOGI("vMeer System: Property Spoofing LIVE (Stealth Mode Active).");
}

} // extern "C"
