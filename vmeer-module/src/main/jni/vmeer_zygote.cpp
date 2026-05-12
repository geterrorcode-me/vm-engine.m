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

    // Signature native Zygote (Zygote64 support)
    typedef jint (*nativeForkAndSpecialize_t)(
        JNIEnv*, jclass, jint, jint, jintArray, jint, jobject, jint, 
        jstring, jstring, jintArray, jintArray, jboolean, jstring, 
        jstring, jboolean, jobjectArray, jobjectArray, jboolean, jboolean);

    static nativeForkAndSpecialize_t orig_nativeForkAndSpecialize = nullptr;

    /**
     * PROXY: Interupsi pada nativeForkAndSpecialize
     * Tempat di mana "Virtual Ecosystem" dipaksakan pada proses baru.
     */
    jint proxy_nativeForkAndSpecialize(
        JNIEnv* env, jclass clazz, jint uid, jint gid, jintArray gids, jint runtime_flags,
        jobject rlimits, jint mount_external, jstring se_info, jstring nice_name,
        jintArray fds_to_close, jintArray fds_to_ignore, jboolean is_child_zygote,
        jstring instruction_set, jstring app_data_dir, jboolean is_top_app,
        jobjectArray pkg_data_info_list, jobjectArray whitelisted_data_info_list,
        jboolean bind_mount_app_data_fp, jboolean bind_mount_app_storage_fp) {

        // 1. Dapatkan nama proses yang akan dijalankan
        const char* c_nice_name = env->GetStringUTFChars(nice_name, nullptr);
        std::string current_proc = (c_nice_name ? c_nice_name : "");
        
        // 2. Tanya ke Registry Context apakah ini proses virtual
        auto* v_ident = vmeer::RuntimeContext::Get().GetIdentity(current_proc);

        // 3. Panggil fungsi asli untuk Forking
        jint pid = orig_nativeForkAndSpecialize(
            env, clazz, uid, gid, gids, runtime_flags, rlimits, mount_external, se_info, 
            nice_name, fds_to_close, fds_to_ignore, is_child_zygote, instruction_set, 
            app_data_dir, is_top_app, pkg_data_info_list, whitelisted_data_info_list, 
            bind_mount_app_data_fp, bind_mount_app_storage_fp);

        // 4. Tahap Spesialisasi (Hanya di proses anak / PID == 0)
        if (pid == 0 && v_ident != nullptr) {
            LOGI("vMeer Zygote: Starting Virtualization for [%s]", v_ident->package_name.c_str());

            // A. ISOLASI NAMESPACE (Pilar Storage)
            if (unshare(CLONE_NEWNS) == 0) {
                // MS_PRIVATE agar tidak merusak mount host (GSI Safety)
                mount("none", "/", NULL, MS_REC | MS_PRIVATE, NULL);
                
                if (v_ident->use_virtual_storage) {
                    LOGI("vMeer Zygote: Storage Namespace Detached.");
                }
            }

            // B. ISOLASI IDENTITAS (Evolusi UID)
            // Di sini kita bisa memanipulasi UID jika diperlukan: setuid(v_ident->virtual_uid);
        }

        if (c_nice_name) env->ReleaseStringUTFChars(nice_name, c_nice_name);
        return pid;
    }

    /**
     * Entry Point untuk mengaktifkan Hook Zygote
     */
    void HookForkAndSpecialize() {
        LOGI("Zygote Engine: Hooking into app_process / zygote64...");

        void* stub = shadowhook_hook_sym_name(
            "libandroid_runtime.so",
            "com_android_internal_os_Zygote_nativeForkAndSpecialize",
            (void*)proxy_nativeForkAndSpecialize,
            (void**)&orig_nativeForkAndSpecialize
        );

        if (stub) {
            LOGI("Zygote Engine: [nativeForkAndSpecialize] successfully intercepted.");
        } else {
            LOGE("Zygote Engine: Hook failure! System isolation will be limited.");
        }
    }

} // namespace zygote
} // namespace vmeer
