#include <jni.h>
#include <android/log.h>
#include "shadowhook.h"
#include "vmeer_stealth.h"
#include "binder_vm.h"
#include "vmeer_system.h"
#include "egl_bridge.h"
// TAMBAHKAN INI
#include "vmeer_context.h" 

#define LOG_TAG "vMeer_Engine"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

extern "C" {

// ... fungsi requestNamespaceSetup tetap sama ...

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* res) {
    LOGI("vMeer Engine: Booting with VRCE Architecture...");

    // 1. Stealth Mode Harus Paling Awal
    // Kita harus menghilang sebelum sistem sempat melakukan scanning
    init_vmeer_stealth();

    // 2. Inisialisasi ShadowHook
    if (shadowhook_init(SHADOWHOOK_MODE_UNIQUE, false) != 0) return JNI_ERR;

    // 3. Bangunkan Orchestrator (VRCE)
    // Di sini kita menentukan identitas VM ini. 
    // Untuk tahap awal, kita hardcode "vm_001", kedepannya bisa dinamis dari JNI.
    if (!vmeer::RuntimeContext::Get().Initialize("vm_001", "com.vmeer.virtual.guest")) {
        LOGI("vMeer Engine: VRCE Critical Failure!");
        return JNI_ERR; 
    }

    // 4. Namespace Setup (Storage Isolation)
    if (!requestNamespaceSetup("com.vmeer.virtual.guest")) {
        LOGI("vMeer Engine: Namespace setup failed.");
    }

    // 5. Jalankan Modul-Modul Logic
    // Sekarang modul-modul ini bisa memanggil RuntimeContext::Get() secara aman
    start_binder_proxy();
    start_virtual_system_services();
    start_egl_bridge();

    LOGI("vMeer Engine: VRCE Environment Ready.");
    return JNI_VERSION_1_6;
}

} // extern "C"
