#ifndef VM_INTERNAL_H
#define VM_INTERNAL_H

#include <jni.h>
#include <android/log.h>

#define LOG_TAG "vMeer_Engine"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

#ifdef __cplusplus
extern "C" {
#endif

    // Binder Guard Init
    void init_binder_isolation();
    
    // VFS Bridge Init (Ini yang tadi menyebabkan error linkage)
    void init_vfs_bridge(const char* path);

    // Virtual Binder
    void start_virtual_binder();

#ifdef __cplusplus
}
#endif

#endif
