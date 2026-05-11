#ifndef VMEER_SURFACE_VM_H
#define VMEER_SURFACE_VM_H

#include <jni.h>

#ifdef __cplusplus
extern "C" {
#endif

void start_graphics_proxy();
JNIEXPORT void JNICALL Java_com_vmeer_io_GraphicsEngine_nativeInit(JNIEnv* env, jobject thiz);

#ifdef __cplusplus
}
#endif

#endif
