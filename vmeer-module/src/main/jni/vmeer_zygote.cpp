#include <jni.h>
#include <android/log.h>
#include <unistd.h>
#include "shadowhook.h"
#include "include/vmeer_zygote.h"
#include "include/vmeer_context.h"

#define LOG_TAG "vMeer_Zygote"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

namespace vmeer {
namespace zygote {

    // Pointer ke fungsi asli di libandroid_runtime.so
    typedef jint (*nativeForkAndSpecialize_t)(
        JNIEnv*, jclass, jint, jint, jint_array, jint, jobject, jint, 
        jstring, jstring, jint_array, jint_array, jboolean, jstring, 
        jstring, jboolean, jstring_array, jstring_array, jboolean, jboolean);

    static nativeForkAndSpecialize_t orig_nativeForkAndSpecialize = nullptr;

    /**
     * PROXY: Fungsi ini yang akan dieksekusi setiap kali Android menjalankan aplikasi baru.
     */
    jint proxy_nativeForkAndSpecialize(
        JNIEnv* env, jclass clazz, jint uid, jint gid, jint_array gids, jint runtime_flags,
        jobject rlimits, jint mount_external, jstring se_info, jstring nice_name,
        jint_array fds_to_close, jint_array fds_to_ignore, jboolean is_child_zygote,
        jstring instruction_set, jstring app_data_dir, jboolean is_top_app,
        jstring_array pkg_data_info_list, jstring_array whitelisted_data_info_list,
        jboolean bind_mount_app_data_fp, jboolean bind_mount_app_storage_fp) {

        // 1. Identifikasi proses: Apakah ini paket yang harus masuk vMeer?
        const char* c_nice_name = env->GetStringUTFChars(nice_name, nullptr);
        
        if (c_nice_name && std::string(c_nice_name).find("com.vmeer.virtual") != std::string::npos) {
            LOGI("Zygote: Hooked birth of virtual process -> %s", c_nice_name);
            
            // 2. Manipulasi UID/GID untuk isolasi (Evolusi Virtual UID)
            // uid = 20000 + (uid % 10000); // Contoh offset UID virtual
        }

        if (c_nice_name) env->ReleaseStringUTFChars(nice_name, c_nice_name);

        // 3. Panggil fungsi asli agar proses benar-benar tercipta
        return orig_nativeForkAndSpecialize(
            env, clazz, uid, gid, gids, runtime_flags, rlimits, mount_external, se_info, 
            nice_name, fds_to_close, fds_to_ignore, is_child_zygote, instruction_set, 
            app_data_dir, is_top_app, pkg_data_info_list, whitelisted_data_info_list, 
            bind_mount_app_data_fp, bind_mount_app_storage_fp);
    }

    void HookForkAndSpecialize() {
        // Mendekati Zygote64 pada libandroid_runtime.so
        void* stub = shadowhook_hook_sym_name(
            "libandroid_runtime.so",
            "com_android_internal_os_Zygote_nativeForkAndSpecialize",
            (void*)proxy_nativeForkAndSpecialize,
            (void**)&orig_nativeForkAndSpecialize
        );

        if (stub) {
            LOGI("Zygote Engine: nativeForkAndSpecialize successfully redirected.");
        }
    }

} // namespace zygote
} // namespace vmeer
