#include <jni.h>
#include <android/log.h>
#include <vector>
#include <dlfcn.h>
#include "shadowhook.h"

#define LOG_TAG "vMeer_Hardware"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

// =============================================================================
// BAGIAN 1: SENSOR & GPS SPOOFING
// =============================================================================
/**
 * Aplikasi virtual sering meminta data lokasi. Kita mencegatnya agar 
 * kita bisa memberikan koordinat palsu atau meneruskan koordinat asli 
 * tanpa aplikasi harus memiliki izin ACCESS_FINE_LOCATION sendiri.
 */
class LocationProxy {
public:
    static void mockLocation(double* lat, double* lon) {
        // Logika untuk menyuntikkan koordinat dari vMeer Manager
        *lat = -6.1751; // Contoh: Jakarta
        *lon = 106.8272;
    }
};

// =============================================================================
// BAGIAN 2: CAMERA SERVICE HOOK (The Camera Bridge)
// =============================================================================
/**
 * Android Camera2 API berkomunikasi via Binder ke 'cameraserver'.
 * Kita mencegat deskripsi kamera agar aplikasi virtual melihat kamera host 
 * sebagai miliknya sendiri.
 */
typedef int (*p_get_camera_info)(int, void*);
static p_get_camera_info orig_get_camera_info = nullptr;

int hook_get_camera_info(int id, void* info) {
    LOGI("vMeer: Application requesting Camera ID: %d", id);
    
    // Panggil fungsi asli dari libcameraservice.so
    int res = orig_get_camera_info(id, info);
    
    // Di sini kita bisa memodifikasi CameraInfo (misal: memutar orientasi)
    return res;
}

// =============================================================================
// BAGIAN 3: PERMISSION OVERRIDE (Urutan 8 Logic)
// =============================================================================
/**
 * Inti dari Hardware Passthrough: Memaksa checkPermission() selalu mengembalikan 'GRANTED'.
 */
class PermissionManager {
public:
    static int checkPermissionOverride(const char* permission) {
        LOGI("vMeer: Auto-granting permission: %s", permission);
        return 0; // 0 = PERMISSION_GRANTED
    }
};

// =============================================================================
// BAGIAN 4: JNI & HAL INITIALIZATION
// =============================================================================
extern "C" JNIEXPORT void JNICALL Java_com_vmeer_io_HardwareEngine_nativeInit(JNIEnv* env, jobject thiz) {
    LOGI("vMeer: Initializing Hardware Passthrough Bridge...");

    // 1. Hook Camera Service
    void* stub_cam = shadowhook_hook_sym_name(
        "libcamera_client.so",
        "_ZN7android13CameraService13getCameraInfoEiPNS_10CameraInfoE",
        (void*)hook_get_camera_info,
        (void**)&orig_get_camera_info
    );

    if (stub_cam) LOGI("vMeer: Camera Bridge Active.");
}
