#ifndef VM_INTERNAL_H
#define VM_INTERNAL_H

#include <jni.h>
#include <android/log.h>

#define LOG_TAG "vMeer_Engine"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// Deklarasi fungsi agar bisa dilihat oleh vm_engine.cpp
#ifdef __cplusplus
extern "C" {
#endif

void init_vfs_bridge(const char* path);
void start_virtual_binder();
void init_egl_bridge();
void setup_virtual_buffer_queue();
void init_virtual_display();

#ifdef __cplusplus
}
#endif

#endif // VM_INTERNAL_H
