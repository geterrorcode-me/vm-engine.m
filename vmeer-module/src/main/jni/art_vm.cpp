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
    if (!set_exemptions_mid) return;
    
    // Eksperimen bypass API tersembunyi dalvik untuk Android 15 modern
    jobjectArray str_array = env->NewObjectArray(1, env->FindClass("java/lang/String"), env->NewStringUTF("L"));
    env->CallVoidMethod(vm_runtime_obj, set_exemptions_mid, str_array);

    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        LOGE("vMeer ART: Hidden API Bypass failed or restricted on Android 15.");
    } else {
        LOGI("vMeer ART: Hidden API Bypass executed.");
    }
}

bool InjectMirrorFramework(JNIEnv* env, jobject class_loader, const std::string& mirror_path) {
    LOGI("vMeer ART: Injecting %s via Manual Element Appending (Android 15 Support)", mirror_path.c_str());

    if (class_loader == nullptr) {
        LOGE("vMeer ART: ClassLoader is NULL!");
        return false;
    }

    // 1. Dapatkan Class dalvik.system.BaseDexClassLoader
    jclass loader_clazz = env->FindClass("dalvik/system/BaseDexClassLoader");
    if (env->ExceptionCheck()) { env->ExceptionClear(); return false; }

    // 2. Ambil field "pathList" dari BaseDexClassLoader
    jfieldID path_list_fid = env->GetFieldID(loader_clazz, "pathList", "Ldalvik/system/DexPathList;");
    if (!path_list_fid) { LOGE("vMeer ART: Field pathList not found."); return false; }
    
    jobject path_list_obj = env->GetObjectField(class_loader, path_list_fid);
    if (!path_list_obj) { LOGE("vMeer ART: pathList object is NULL."); return false; }

    // 3. Dapatkan field "dexElements" dari DexPathList
    jclass path_list_clazz = env->GetObjectClass(path_list_obj);
    jfieldID elements_fid = env->GetFieldID(path_list_clazz, "dexElements", "[Ldalvik/system/DexPathList$Element;");
    if (!elements_fid) { LOGE("vMeer ART: Field dexElements not found."); return false; }

    jobjectArray old_elements = (jobjectArray)env->GetObjectField(path_list_obj, elements_fid);
    jsize old_len = (old_elements != nullptr) ? env->GetArrayLength(old_elements) : 0;

    // 4. Buat DexClassLoader sementara (transient) untuk memuat mirror_path (readonly.bin/jar) secara legal
    jstring j_path = env->NewStringUTF(mirror_path.c_str());
    jstring optimized_dir = env->NewStringUTF("/data/user/0/com.vmeer.io/code_cache"); // Direktori internal aman
    
    jclass dex_loader_clazz = env->FindClass("dalvik/system/DexClassLoader");
    jmethodID dex_loader_init = env->GetMethodID(dex_loader_clazz, "<init>", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/ClassLoader;)V");
    
    jobject transient_loader = env->NewObject(dex_loader_clazz, dex_loader_init, j_path, optimized_dir, nullptr, class_loader);
    if (env->ExceptionCheck() || !transient_loader) {
        env->ExceptionClear();
        LOGE("vMeer ART: Failed to create transient DexClassLoader.");
        env->DeleteLocalRef(j_path);
        env->DeleteLocalRef(optimized_dir);
        return false;
    }

    // 5. Ekstrak "dexElements" dari DexClassLoader sementara tersebut
    jobject transient_path_list = env->GetObjectField(transient_loader, path_list_fid);
    jobjectArray transient_elements = (jobjectArray)env->GetObjectField(transient_path_list, elements_fid);
    jsize transient_len = (transient_elements != nullptr) ? env->GetArrayLength(transient_elements) : 0;

    if (transient_len == 0) {
        LOGE("vMeer ART: Transient elements is empty. Injection failed.");
        env->DeleteLocalRef(j_path);
        env->DeleteLocalRef(optimized_dir);
        return false;
    }

    // 6. Alokasi array baru gabungan (Ukuran = old_len + transient_len)
    jclass element_clazz = env->FindClass("dalvik/system/DexPathList$Element");
    jobjectArray combined_elements = env->NewObjectArray(old_len + transient_len, element_clazz, nullptr);

    // Salin elemen lawas ke array baru
    for (jsize i = 0; i < old_len; i++) {
        jobject element = env->GetObjectArrayElement(old_elements, i);
        env->SetObjectArrayElement(combined_elements, i, element);
        env->DeleteLocalRef(element);
    }

    // Sisipkan elemen baru dari guest ROM di akhir array
    for (jsize i = 0; i < transient_len; i++) {
        jobject element = env->GetObjectArrayElement(transient_elements, i);
        env->SetObjectArrayElement(combined_elements, old_len + i, element);
        env->DeleteLocalRef(element);
    }

    // 7. Suntikkan kembali array gabungan tersebut ke ClassLoader utama host
    env->SetObjectField(path_list_obj, elements_fid, combined_elements);
    LOGI("vMeer ART: SUCCESS! Mirror Framework injected. Elements combined: %d -> %d", old_len, old_len + transient_len);

    // Cleanup local references
    env->DeleteLocalRef(j_path);
    env->DeleteLocalRef(optimized_dir);
    return true;
}

} // namespace art
} // namespace vmeer

extern "C" void init_art_hook(JNIEnv* env) {
    vmeer::art::ApplyHiddenApiBypass(env);
}

extern "C" void perform_mirror_injection(JNIEnv* env, jobject class_loader, const char* path) {
    vmeer::art::InjectMirrorFramework(env, class_loader, path);
}
