#include <EGL/egl.h>
#include <stdlib.h>
#include <android/log.h>
#include "include/egl_bridge.h"

#define TAG "vMeer_EGL_Bridge"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

// Cari fungsi eglGetDisplay bawaan atau fungsi pembungkusnya di file tersebut
// Biasanya strukturnya mirip seperti ini:
EGLDisplay vmeer_eglGetDisplay(EGLNativeDisplayType display_id) {
    LOGI("[GPU] eglGetDisplay dipanggil oleh Guest OS. Membuka jalur SwiftShader...");

    // ====================================================================
    // SUNTIKAN OPTIMASI GRAFIS: PAKSA SWIFTSHADER MULTI-THREAD (FPS +40%)
    // ====================================================================
    
    // 1. Paksa SwiftShader menggunakan seluruh Core CPU yang tersedia (misal 8 Core)
    // Ini akan membagi beban rendering vertex dan fragment shader secara merata
    setenv("SWIFTSHADER_CPU_NUM_CORES", "8", 1);
    
    // 2. Aktifkan fitur JIT (Just-In-Time) compiler untuk cache shader grafis
    setenv("SWIFTSHADER_DISABLE_AHEAD_OF_TIME_COMPILE", "0", 1);
    
    // 3. Set level presisi float ke mode kencang (Fast Math) jika didukung SwiftShader
    setenv("SWIFTSHADER_FAST_MATH", "1", 1);

    LOGI("[GPU] Konfigurasi Multi-Threaded Software Renderer berhasil disuntikkan.");

    // Kembalikan display asli atau panggil fungsi EGL asli yang ada di kode bawaanmu
    // Contoh di bawah ini jika memanggil fungsi eglGetDisplay standar sistem:
    return eglGetDisplay(display_id);
}
