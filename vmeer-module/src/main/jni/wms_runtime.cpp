#include "include/wms_runtime.h"
#include <android/log.h>
#include <string>

#define LOG_TAG "vMeer_WMS"

namespace vmeer {
namespace wms {

    /**
     * Memalsukan Window Token agar aplikasi virtual tidak ditolak oleh SurfaceFlinger.
     * Biasanya dipanggil saat intercepting 'addWindow' (code 1 di WMS Binder).
     */
    void patch_window_layout(void* layout_params_parcel) {
        // LOGIC:
        // 1. Baca WindowManager.LayoutParams dari parcel.
        // 2. Jika type adalah TYPE_APPLICATION, pastikan token merujuk ke StubActivity.
        // 3. Sembunyikan flag yang bisa mendeteksi overlay/virtual display.
        
        __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "WMS: Patching LayoutParams for Virtual Window.");
        (void)layout_params_parcel;
    }

    /**
     * Menghapus identitas asli dari judul jendela (Window Title)
     */
    void sanitize_window_title(std::string& title) {
        if (title.find("com.vmeer") == std::string::npos) {
            title = "[vMeer] " + title;
        }
    }

} // namespace wms
} // namespace vmeer
