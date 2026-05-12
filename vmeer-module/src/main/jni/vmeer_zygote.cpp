#include <jni.h>
#include <android/log.h>
#include "shadowhook.h"
#include "include/vmeer_zygote.h"

#define LOG_TAG "vMeer_Zygote"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

namespace vmeer {
namespace zygote {

// Pointer untuk menyimpan fungsi asli Zygote
static void* g_orig_fork = nullptr;

/**
 * IMPLEMENTASI: HookForkAndSpecialize
 * Ini adalah simbol yang dicari oleh Linker.
 */
void HookForkAndSpecialize() {
    LOGI("Zygote Engine: Attempting to hook nativeForkAndSpecialize...");

    // Nama simbol native Zygote pada libandroid_runtime.so
    const char* symbol = "com_android_internal_os_Zygote_nativeForkAndSpecialize";
    
    // Melakukan hooking menggunakan ShadowHook
    g_orig_fork = shadowhook_hook_sym_name(
        "libandroid_runtime.so", 
        symbol, 
        (void*)nullptr, // Ganti dengan fungsi proxy Anda nanti
        nullptr
    );

    if (g_orig_fork) {
        LOGI("Zygote Engine: Hook successful. Every new life is now isolated.");
    } else {
        LOGI("Zygote Engine: Hook failed. Is the device architecture supported?");
    }
}

} // namespace zygote
} // namespace vmeer
