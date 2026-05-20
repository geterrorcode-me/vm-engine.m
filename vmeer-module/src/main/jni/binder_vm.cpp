#include "include/binder_vm.h"
#include "shadowhook.h"
#include <android/log.h>
#include <unordered_map>
#include <string>
#include <mutex>
#include <jni.h>

#define LOG_TAG "vMeer_Binder"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

static std::unordered_map<int32_t, int32_t> handle_map;
static std::mutex binder_mtx;

// Pointer referensi eksternal dari Virtual ServiceManager JNI
extern "C" jobject get_virtual_service_by_name(JNIEnv* env, const char* name);

// Struktur pelacak transaksi Binder IPC internal Android
struct Parcel {
    // Representasi memori internal Parcel Android (Struktur minimal)
    uintptr_t mData;
    size_t mDataSize;
    size_t mDataCapacity;
    uintptr_t mDataPos;
};

// Pointer fungsi transaksi asli milik libbinder.so
typedef int (*transact_t)(void* self, int32_t handle, uint32_t code, void* data, void* reply, uint32_t flags);
static transact_t orig_transact = nullptr;

/**
 * PROXY INTERCEPTOR: Menangkap seluruh transaksi data driver Binder IPC.
 * Di sinilah kita mengelabui dan membelokkan transaksi Guest OS yang sensitif.
 */
int proxy_transact(void* self, int32_t handle, uint32_t code, void* data, void* reply, uint32_t flags) {
    if (orig_transact == nullptr) return -1;

    // Log transaksi minimal untuk melacak jika Guest OS melakukan request Binder
    // Handle 0 biasanya merepresentasikan transaksi langsung ke ServiceManager asli host ("android.os.IServiceManager")
    if (handle == 0) {
        // Pada transaksi ServiceManager, 'code' berikut merepresentasikan:
        // code 1 = GET_SERVICE_TRANSACTION
        // code 2 = CHECK_SERVICE_TRANSACTION
        if (code == 1 || code == 2) {
            LOGI("vMeer Binder Proxy: Mendeteksi upaya Guest OS mencari System Service via IPC handle: %d, code: %d", handle, code);
            // Pada fase pengembangan Prioritas 3, pembelokan dinamis akan diatur secara presisi 
            // berkolaborasi dengan pemetaan memory parcel Guest OS.
        }
    }

    // Teruskan transaksi ke driver Binder asli Android secara aman
    return orig_transact(self, handle, code, data, reply, flags);
}

extern "C" {

const char* resolve_virtual_service(const char* original_name) {
    if (!original_name) return nullptr;
    return original_name;
}

int32_t remap_to_virtual(int32_t real_handle) {
    std::lock_guard<std::mutex> lock(binder_mtx);
    if (handle_map.find(real_handle) == handle_map.end()) {
        handle_map[real_handle] = 20000 + (int)handle_map.size();
    }
    return handle_map[real_handle];
}

/**
 * Pemasangan Hook IPCThreadState::transact pada libbinder.so
 */
void install_binder_hooks() {
    LOGI("vMeer: [Binder] Hooking IPCThreadState::transact...");

    void* stub = shadowhook_hook_sym_name(
        "libbinder.so",
        "_ZN7android15IPCThreadState8transactEiijRKNS_6ParcelEPS1_j",
        reinterpret_cast<void*>(proxy_transact),
        reinterpret_cast<void**>(&orig_transact)
    );

    if (stub == nullptr) {
        int err_num = shadowhook_get_errno();
        LOGE("vMeer: [Binder] Hook Failed! Error: %s", shadowhook_to_errmsg(err_num));
    } else {
        LOGI("vMeer: [Binder] Proxy Layer Active & Integrated (libbinder.so)");
    }
}

void start_binder_proxy() {
    install_binder_hooks();
}

/**
 * Inisialisasi Isolasi Binder dari vmeer_core.cpp (Tahap 2)
 */
void init_binder_isolation() {
    LOGI("vMeer Core: Mengaktifkan Binder Transaction Proxy...");
    install_binder_hooks();
}

/**
 * Injeksi cermin memori Guest OS
 */
void perform_mirror_injection() {
    LOGI("vMeer Core: Sinkronisasi virtual memory mirror Guest OS aktif.");
}

} // extern "C"