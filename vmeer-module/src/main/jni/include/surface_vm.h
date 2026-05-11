#ifndef VMEER_SURFACE_VM_H
#define VMEER_SURFACE_VM_H

#include <jni.h>

// Inisialisasi pipeline grafis (Urutan 5)
void start_graphics_proxy();

// Entry point untuk JNI
extern "C" JNIEXPORT void JNICALL Java_com_vmeer_io_GraphicsEngine_nativeInit(JNIEnv* env, jobject thiz);

#endif
