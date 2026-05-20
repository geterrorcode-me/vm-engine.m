#include <jni.h>
#include <android/log.h>
#include <unordered_map>
#include <string>
#include <mutex>

#define LOG_TAG "vMeer_ServiceMgr"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace vmeer {
namespace service {

class VirtualServiceManager {
private:
    std::unordered_map<std::string, jobject> virtual_services;
    std::mutex service_mtx;

    VirtualServiceManager() = default;

public:
    static VirtualServiceManager& GetInstance() {
        static VirtualServiceManager instance;
        return instance;
    }

    /**
     * Mendaftarkan Virtual Service palsu yang di-mock ke dalam registry
     */
    void AddService(JNIEnv* env, const std::string& name, jobject service_binder) {
        std::lock_guard<std::mutex> lock(service_mtx);
        
        // Hapus instansi lama jika ada untuk mencegah kebocoran referensi global JNI
        auto it = virtual_services.find(name);
        if (it != virtual_services.end()) {
            env->DeleteGlobalRef(it->second);
        }

        virtual_services[name] = env->NewGlobalRef(service_binder);
        LOGI("vMeer ServiceMgr: Terdaftar Virtual Service baru -> [%s]", name.c_str());
    }

    /**
     * Mengambil Virtual Service palsu berdasarkan nama layanan
     */
    jobject GetService(JNIEnv* env, const std::string& name) {
        std::lock_guard<std::mutex> lock(service_mtx);
        auto it = virtual_services.find(name);
        if (it != virtual_services.end()) {
            LOGI("vMeer ServiceMgr: Pembelokan sukses! Mengembalikan stub virtual untuk layanan -> [%s]", name.c_str());
            return env->NewLocalRef(it->second); // Mengembalikan local ref aman bagi thread pemanggil
        }
        return nullptr;
    }

    /**
     * Membersihkan seluruh layanan virtual saat VM dihentikan
     */
    void ClearAllServices(JNIEnv* env) {
        std::lock_guard<std::mutex> lock(service_mtx);
        for (auto& pair : virtual_services) {
            env->DeleteGlobalRef(pair.second);
        }
        virtual_services.clear();
        LOGI("vMeer ServiceMgr: Seluruh registry virtual services dibersihkan secara aman.");
    }
};

} // namespace service
} // namespace vmeer

// =============================================================================
// GERBANG JNI UNTUK INTERAKSI UTAS JAVA (VMeerServiceManager.java)
// =============================================================================

extern "C" {

JNIEXPORT void JNICALL
Java_com_vmeer_io_VMeerServiceManager_nativeAddService(JNIEnv* env, jclass clazz, jstring name, jobject binder) {
    (void)clazz;
    if (!name || !binder) return;

    const char* native_name = env->GetStringUTFChars(name, nullptr);
    if (native_name) {
        vmeer::service::VirtualServiceManager::GetInstance().AddService(env, std::string(native_name), binder);
        env->ReleaseStringUTFChars(name, native_name);
    }
}

JNIEXPORT jobject JNICALL
Java_com_vmeer_io_VMeerServiceManager_nativeGetService(JNIEnv* env, jclass clazz, jstring name) {
    (void)clazz;
    if (!name) return nullptr;

    jobject result = nullptr;
    const char* native_name = env->GetStringUTFChars(name, nullptr);
    if (native_name) {
        result = vmeer::service::VirtualServiceManager::GetInstance().GetService(env, std::string(native_name));
        env->ReleaseStringUTFChars(name, native_name);
    }
    return result;
}

JNIEXPORT void JNICALL
Java_com_vmeer_io_VMeerServiceManager_nativeClearServices(JNIEnv* env, jclass clazz) {
    (void)clazz;
    vmeer::service::VirtualServiceManager::GetInstance().ClearAllServices(env);
}

} // extern "C"