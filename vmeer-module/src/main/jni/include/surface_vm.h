#ifndef VMEER_SURFACE_VM_H
#define VMEER_SURFACE_VM_H

#include <jni.h>

// Fungsi bridge untuk dipanggil dari C++ (vmeer_core)
void start_graphics_proxy();

// Fungsi JNI untuk dipanggil dari Java
extern "C" JNIEXPORT void JNICALL Java_com_vmeer_io_GraphicsEngine_nativeInit(JNIEnv* env, jobject thiz);

#endif
