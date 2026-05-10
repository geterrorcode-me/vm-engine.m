#ifndef VM_INTERNAL_H
#define VM_INTERNAL_H

#include <jni.h>
#include <android/log.h>

#define LOG_TAG "vMeer_Engine"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// Deklarasi fungsi antar-modul
void init_egl_bridge();
void setup_virtual_buffer_queue();
void init_virtual_display();

#endif // VM_INTERNAL_H
