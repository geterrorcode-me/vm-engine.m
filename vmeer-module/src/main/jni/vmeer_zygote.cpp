#include <jni.h>
#include <android/log.h>
#include <unistd.h>
#include <string>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sched.h>
#include <fcntl.h>

#include "shadowhook.h"
#include "include/vmeer_zygote.h"
#include "include/vmeer_context.h"

#define LOG_TAG "vMeer_Zygote"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace vmeer {
namespace zygote {

    // Tanda tangan fungsi native Zygote sesuai standar NDK/Android Runtime
    typedef jint (*nativeForkAndSpecialize_t)(
        JNIEnv*, jclass, jint, jint, jintArray, jint, jobject, jint, 
        jstring, jstring, jintArray, jintArray, jboolean, jstring, 
        jstring, jboolean, jobjectArray, jobjectArray, jboolean, jboolean);

    static nativeForkAndSpecialize_t orig_nativeForkAndSpecialize = nullptr;

    /**
     * PROXY: Titik cegat kelahiran proses aplikasi.
     */
    jint proxy_nativeForkAndSpecialize(
        JNIEnv* env, jclass clazz, jint uid, jint gid, jintArray gids, jint runtime_flags,
        jobject rlimits, jint mount_external, jstring se_info, jstring nice_name,
        jintArray fds_to_close, jintArray fds_to_ignore, jboolean is_child_zygote,
        jstring instruction_set, jstring app_data_dir, jboolean is_top_app,
        jobjectArray pkg_data_info_list, jobjectArray whitelisted_data_info_list,
        jboolean bind_mount_app_data_fp, jboolean bind_mount_app_storage_fp) {

        // 1. Identifikasi nama paket sebelum fork
        const char* c_nice_name = env->GetStringUTFChars(nice_name, nullptr);
        std::string pkg_name = (c_nice_name ? c_nice_name : "");
        bool is_vmeer_app = (pkg_name.find("com.vmeer.virtual") != std::string::npos);

        // 2. Eksekusi Fork (Panggil fungsi asli)
        jint pid = orig_nativeForkAndSpecialize(
            env, clazz, uid, gid, gids, runtime_flags, rlimits, mount_external, se_info, 
            nice_name, fds_to_close, fds_to_ignore, is_child_zygote, instruction_set, 
            app_data_dir, is_top_app, pkg_data_info_list, whitelisted_data_info_list, 
            bind_mount_app_data_fp, bind_mount_app_storage_fp);

        // 3. LOGIKA ISOLASI (Hanya dieksekusi di proses anak yang baru lahir)
        if (pid == 0 && is_vmeer_app) {
            LOGI("vMeer Zygote: Child process [%s] detected. Orchestrating Sandbox...", pkg_name.c_str());

            /**
             * MOUNT NAMESPACE ISOLATION
             * CLONE_NEWNS memisahkan tabel mount proses ini dari sistem.
             */
            if (unshare(CLONE_NEWNS) == 0) {
                // MS_PRIVATE memastikan perubahan mount di sini tidak "bocor" ke luar (GSI Safety)
                if (mount("none", "/", NULL, MS_REC | MS_PRIVATE, NULL) == 0) {
                    
                    // Contoh: Mount folder virtual khusus untuk menimpa SDCard asli
                    // Kita asumsikan daemon sudah menyiapkan folder di /data/vmeer/virtual_sd
                    // mount("/data/vmeer/virtual_sd", "/sdcard", NULL, MS_BIND, NULL);
                    
                    LOGI("vMeer Zygote: Mount Namespace is now Private & Isolated.");
                }
            } else {
                LOGE("vMeer Zygote: Failed to unshare namespace. Check SELinux permissions!");
            }
        }

        // Cleanup JNI string
        if (c_nice_name) env->ReleaseStringUTFChars(nice_name, c_nice_name);
        
        return pid;
    }

    /**
     * Entry point untuk memasang Hook pada libandroid_runtime.so
     */
    void HookForkAndSpecialize() {
        LOGI("Zygote Engine: Searching for nativeForkAndSpecialize symbol...");

        void* stub = shadowhook_hook_sym_name(
            "libandroid_runtime.so",
            "com_android_internal_os_Zygote_nativeForkAndSpecialize",
            (void*)proxy_nativeForkAndSpecialize,
            (void**)&orig_nativeForkAndSpecialize
        );

        if (stub) {
            LOGI("Zygote Engine: Hook installed successfully. System remains stable.");
        } else {
            int err_num = shadowhook_get_errno();
            const char* err_msg = shadowhook_to_errmsg(err_num);
            LOGE("Zygote Engine: Hook failed! Error %d: %s", err_num, err_msg);
        }
    }

} // namespace zygote
} // namespace vmeer
