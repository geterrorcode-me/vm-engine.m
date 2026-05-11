#include <jni.h>
#include <android/log.h>
#include "include/surface_vm.h"
#include "include/binder_vm.h"
#include "include/vmeer_system.h"

// Deklarasi fungsi dari egl_bridge.cpp
extern "C" void start_egl_bridge();

#define LOG_TAG "vMeer_Core"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

extern "C" JNIEXPORT void JNICALL
Java_com_vmeer_io_VMeerCore_nativeLaunch(JNIEnv *env, jobject thiz) {
    LOGI("vMeer: === Engine Launch Sequence Initiated ===");

    // 1. Jalankan Stealth Mode (Anti-Detection)
    // init_vmeer_stealth(); 

    // 2. Transisi Grafis: Pindah dari Internal ke Presentation Boundary
    // Kita panggil bridge baru yang menggunakan eglSwapBuffers (Stable)
    start_egl_bridge(); 

    // NOTE: start_graphics_proxy() di surface_vm.cpp tetap bisa dipanggil 
    // jika hanya untuk metadata, tapi pastikan logika internal libgui-nya
    // sudah mulai dipindahkan ke egl_bridge.
    // start_graphics_proxy(); 

    // 3. Jalankan Binder Redirection (dari binder_vm.cpp)
    // Pastikan binder_vm.h sudah menggunakan extern "C"
    start_binder_proxy();

    // 4. Jalankan Virtual System Services (dari vmeer_system.cpp)
    start_virtual_system_services();

    LOGI("vMeer: === All Systems Nominal. Engine is Live. ===");
}
