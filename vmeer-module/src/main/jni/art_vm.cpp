#include <jni.h>
#include <android/log.h>
#include <string>
#include <vector>

#define LOG_TAG "vMeer_ART"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace vmeer {
namespace art {

void ApplyHiddenApiBypass(JNIEnv* env) {
    jclass vm_runtime_clazz = env->FindClass("dalvik/system/VMRuntime");
    if (!vm_runtime_clazz) return;

    jmethodID get_runtime_mid = env->GetStaticMethodID(vm_runtime_clazz, "getRuntime", "()Ldalvik/system/VMRuntime;");
    jobject vm_runtime_obj = env->CallStaticObjectMethod(vm_runtime_clazz, get_runtime_mid);

    jmethodID set_exemptions_mid = env->GetMethodID(vm_runtime_clazz, "setHiddenApiExemptions", "([Ljava/lang/String;)V");
    
    jobjectArray str_array = env->NewObjectArray(1, env->FindClass("java/lang/String"), env->NewStringUTF("L"));
    env->CallVoidMethod(vm_runtime_obj, set_exemptions_mid, str_array);

    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        LOGE("vMeer ART: Hidden API Bypass failed.");
    } else {
        LOGI("vMeer ART: Hidden API Bypass success.");
    }
}

bool InjectMirrorFramework(JNIEnv* env, jobject class_loader, const std::string& mirror_path) {
    LOGI("vMeer ART: Injecting %s into ClassLoader", mirror_path.c_str());

    jclass loader_clazz = env->FindClass("dalvik/system/BaseDexClassLoader");
    jfieldID path_list_fid = env->GetFieldID(loader_clazz, "pathList", "Ldalvik/system/DexPathList;");
    jobject path_list_obj = env->GetObjectField(class_loader, path_list_fid);

    jclass path_list_clazz = env->GetObjectClass(path_list_obj);
    
    // FIX: Hapus atau bungkam variabel yang tidak terpakai
    jfieldID elements_fid = env->GetFieldID(path_list_clazz, "dexElements", "[Ldalvik/system/DexPathList$Element;");
    jobjectArray old_elements = (jobjectArray)env->GetObjectField(path_list_obj, elements_fid);
    (void)old_elements; // <--- TAMBAHKAN INI agar tidak error 'unused variable'

    jmethodID add_dex_path_mid = env->GetMethodID(path_list_clazz, "addDexPath", "(Ljava/lang/String;Z)V");

    if (add_dex_path_mid) {
        jstring j_path = env->NewStringUTF(mirror_path.c_str());
        env->CallVoidMethod(path_list_obj, add_dex_path_mid, j_path, JNI_FALSE);
        LOGI("vMeer ART: mirror.jar successfully added to pathList.");
        return true;
    }

    LOGE("vMeer ART: addDexPath method not found.");
    return false;
}

} // namespace art
} // namespace vmeer

extern "C" void init_art_hook(JNIEnv* env) {
    vmeer::art::ApplyHiddenApiBypass(env);
}

extern "C" void perform_mirror_injection(JNIEnv* env, jobject class_loader, const char* path) {
    vmeer::art::InjectMirrorFramework(env, class_loader, path);
}
