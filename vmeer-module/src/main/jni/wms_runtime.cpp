#include "include/wms_runtime.h"
#include <android/log.h>

#define LOG_TAG "vMeer_WMS"

namespace vmeer {
namespace wms {

/**
 * patch_window_layout:
 * Memodifikasi WindowManager.LayoutParams di dalam Parcel.
 */
void patch_window_layout(void* layout_params_parcel) {
    if (!layout_params_parcel) return;

    // Di sini kita akan melakukan manipulasi bit pada flags:
    // 1. Menghapus FLAG_SECURE jika diperlukan
    // 2. Memastikan token jendela mengarah ke Zygote-injected context
    
    __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "WMS: Patching Window LayoutParams for sandbox isolation.");
}

/**
 * sanitize_window_title:
 * Menyamarkan nama aplikasi asli dari judul jendela di level sistem.
 */
void sanitize_window_title(std::string& title) {
    if (title.empty()) return;
    
    // Sembunyikan identitas vMeer dari sistem asli
    size_t pos = title.find("vmeer");
    if (pos != std::string::npos) {
        title.replace(pos, 5, "system");
    }
}

} // namespace wms
} // namespace vmeer
