#include "include/vm_internal.h"

extern "C" JNIEXPORT jboolean JNICALL
Java_com_vmeer_io_EngineLoader_startEngine(JNIEnv* env, jobject thiz, jstring romPath) {
    const char* path = env->GetStringUTFChars(romPath, nullptr);
    LOGI("vMeer Engine Starting...");
    init_vfs_bridge(path);
    start_virtual_binder();
    env->ReleaseStringUTFChars(romPath, path);
    return JNI_TRUE;
}
