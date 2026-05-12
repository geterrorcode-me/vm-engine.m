#include <jni.h>
#include <android/log.h>
#include <unistd.h>
#include <string>
#include "shadowhook.h"
#include "include/vmeer_zygote.h"

#define LOG_TAG "vMeer_Zygote"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

namespace vmeer {
namespace zygote {

    // Perbaikan tipe data JNI (jintArray & jobjectArray)
    typedef jint (*nativeForkAndSpecialize_t)(
        JNIEnv*, jclass, jint, jint, jintArray, jint, jobject, jint, 
        jstring, jstring, jintArray, jintArray, jboolean, jstring, 
        jstring, jboolean, jobjectArray, jobjectArray, jboolean, jboolean);

    static nativeForkAndSpecialize_t orig_nativeForkAndSpecialize = nullptr;

    jint proxy_nativeForkAndSpecialize(
        JNIEnv* env, jclass clazz, jint uid, jint gid, jintArray gids, jint runtime_flags,
        jobject rlimits, jint mount_external, jstring se_info, jstring nice_name,
        jintArray fds_to_close, jintArray fds_to_ignore, jboolean is_child_zygote,
        jstring instruction_set, jstring app_data_dir, jboolean is_top_app,
        jobjectArray pkg_data_info_list, jobjectArray whitelisted_data_info_list,
        jboolean bind_mount_app_data_fp, jboolean bind_mount_app_storage_fp) {

        const char* c_nice_name = env->GetStringUTFChars(nice_name, nullptr);
        
        if (c_nice_name && std::string(c_nice_name).find("com.vmeer.virtual") != std::string::npos) {
            LOGI("Zygote: Hooked birth of virtual process -> %s", c_nice_name);
        }

        if (c_nice_name) env->ReleaseStringUTFChars(nice_name, c_nice_name);

        return orig_nativeForkAndSpecialize(
            env, clazz, uid, gid, gids, runtime_flags, rlimits, mount_external, se_info, 
            nice_name, fds_to_close, fds_to_ignore, is_child_zygote, instruction_set, 
            app_data_dir, is_top_app, pkg_data_info_list, whitelisted_data_info_list, 
            bind_mount_app_data_fp, bind_mount_app_storage_fp);
    }

    void HookForkAndSpecialize() {
        void* stub = shadowhook_hook_sym_name(
            "libandroid_runtime.so",
            "com_android_internal_os_Zygote_nativeForkAndSpecialize",
            (void*)proxy_nativeForkAndSpecialize,
            (void**)&orig_nativeForkAndSpecialize
        );

        if (stub) {
            LOGI("Zygote Engine: Hook point fixed and redirected.");
        }
    }

} // namespace zygote
} // namespace vmeer
