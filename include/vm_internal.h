#ifndef VM_INTERNAL_H
#define VM_INTERNAL_H
#include <jni.h>
#include <android/log.h>
#define LOG_TAG "vMeer_Engine"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
int init_vfs_bridge(const char* path);
void start_virtual_binder();
#endif
