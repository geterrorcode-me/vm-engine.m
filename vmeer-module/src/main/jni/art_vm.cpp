#include <jni.h>
#include <android/log.h>
#include <string>
#include <vector>

#define LOG_TAG "vMeer_ART"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace vmeer {
namespace art {

void ApplyHiddenApiBypass(JNIEnv* env) {
    jclass vm_runtime_clazz = env->FindClass("dalvik/system/VMRuntime");
    if (!vm_runtime_clazz) return;

    jmethodID get_runtime_mid = env->GetStaticMethodID(vm_runtime_clazz, "getRuntime", "()Ldalvik/system/VMRuntime;");
    if (!get_runtime_mid) return;
    jobject vm_runtime_obj = env->CallStaticObjectMethod(vm_runtime_clazz, get_runtime_mid);

    jmethodID set_exemptions_mid = env->GetMethodID(vm_runtime_clazz, "setHiddenApiExemptions", "([Ljava/lang/String;)V");
    if (!set_exemptions_mid) return;
    
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
    LOGI("vMeer ART: Injecting %s via Native Path Elements Maker", mirror_path.c_str());

    if (class_loader == nullptr) {
        LOGE("vMeer ART: ClassLoader is NULL!");
        return false;
    }

    // 1. Dapatkan dalvik.system.BaseDexClassLoader & pathList
    jclass loader_clazz = env->FindClass("dalvik/system/BaseDexClassLoader");
    if (env->ExceptionCheck()) { env->ExceptionClear(); return false; }

    jfieldID path_list_fid = env->GetFieldID(loader_clazz, "pathList", "Ldalvik/system/DexPathList;");
    if (!path_list_fid) { LOGE("vMeer ART: Field pathList not found."); return false; }

    jobject path_list_obj = env->GetObjectField(class_loader, path_list_fid);
    if (!path_list_obj) { LOGE("vMeer ART: pathList is NULL."); return false; }

    jclass path_list_clazz = env->GetObjectClass(path_list_obj);

    // 2. Ambil array dexElements yang lama
    jfieldID elements_fid = env->GetFieldID(path_list_clazz, "dexElements", "[Ldalvik/system/DexPathList$Element;");
    if (!elements_fid) { LOGE("vMeer ART: Field dexElements not found."); return false; }

    jobjectArray old_elements = (jobjectArray)env->GetObjectField(path_list_obj, elements_fid);
    jsize old_len = (old_elements != nullptr) ? env->GetArrayLength(old_elements) : 0;

    // 3. Cari static method 'makePathElements' di dalam DexPathList
    jmethodID make_elements_mid = env->GetStaticMethodID(
        path_list_clazz, 
        "makePathElements", 
        "(Ljava/util/List;Ljava/io/File;Ljava/util/List;)[Ldalvik/system/DexPathList$Element;"
    );

    if (!make_elements_mid) {
        env->ExceptionClear();
        // Fallback untuk variasi signature Android tertentu (makeDexElements)
        make_elements_mid = env->GetStaticMethodID(
            path_list_clazz, 
            "makeDexElements", 
            "(Ljava/util/List;Ljava/io/File;Ljava/util/List;Ljava/lang/ClassLoader;)[Ldalvik/system/DexPathList$Element;"
        );
    }

    if (!make_elements_mid) {
        LOGE("vMeer ART: Gagal menemukan method pembuat elemen (makePathElements/makeDexElements).");
        return false;
    }

    // 4. Siapkan parameter untuk pemanggilan method: Buat ArrayList berisi file .bin kita
    jclass array_list_clazz = env->FindClass("java/util/ArrayList");
    jmethodID array_list_init = env->GetMethodID(array_list_clazz, "<init>", "()V");
    jmethodID array_list_add = env->GetMethodID(array_list_clazz, "add", "(Ljava/lang/Object;)Z");

    jobject files_list = env->NewObject(array_list_clazz, array_list_init);
    jclass file_clazz = env->FindClass("java/io/File");
    jmethodID file_init = env->GetMethodID(file_clazz, "<init>", "(Ljava/lang/String;)V");
    
    jstring j_path = env->NewStringUTF(mirror_path.c_str());
    jobject file_obj = env->NewObject(file_clazz, file_init, j_path);
    env->CallBooleanMethod(files_list, array_list_add, file_obj);

    // List kosong untuk menampung IOException jika terjadi error kompilasi internal
    jobject suppressed_exceptions_list = env->NewObject(array_list_clazz, array_list_init);

    // 5. Eksekusi pembuatan Element baru khusus untuk file .bin tersebut
    jobjectArray new_element_array = (jobjectArray)env->CallStaticObjectMethod(
        path_list_clazz, 
        make_elements_mid, 
        files_list, 
        nullptr, 
        suppressed_exceptions_list
    );

    if (env->ExceptionCheck() || !new_element_array) {
        env->ExceptionClear();
        LOGE("vMeer ART: Gagal mengekstrak elemen lewat makePathElements murni.");
        env->DeleteLocalRef(j_path);
        env->DeleteLocalRef(files_list);
        env->DeleteLocalRef(file_obj);
        env->DeleteLocalRef(suppressed_exceptions_list);
        return false;
    }

    jsize new_elements_len = env->GetArrayLength(new_element_array);
    if (new_elements_len == 0) {
        LOGE("vMeer ART: Hasil ekstraksi elemen bernilai kosong (0).");
        env->DeleteLocalRef(j_path);
        env->DeleteLocalRef(files_list);
        env->DeleteLocalRef(file_obj);
        env->DeleteLocalRef(suppressed_exceptions_list);
        return false;
    }

    // 6. Alokasikan array gabungan final
    jclass element_clazz = env->FindClass("dalvik/system/DexPathList$Element");
    jobjectArray combined_elements = env->NewObjectArray(old_len + new_elements_len, element_clazz, nullptr);

    // Salin elemen lama
    for (jsize i = 0; i < old_len; i++) {
        jobject element = env->GetObjectArrayElement(old_elements, i);
        env->SetObjectArrayElement(combined_elements, i, element);
        env->DeleteLocalRef(element);
    }

    // Gabungkan elemen baru hasil ekstraksi berkas .bin
    for (jsize i = 0; i < new_elements_len; i++) {
        jobject element = env->GetObjectArrayElement(new_element_array, i);
        env->SetObjectArrayElement(combined_elements, old_len + i, element);
        env->DeleteLocalRef(element);
    }

    // 7. Kunci kembali ke ClassLoader host
    env->SetObjectField(path_list_obj, elements_fid, combined_elements);
    LOGI("vMeer ART: SUCCESS! Elemen berkas .bin berhasil disatukan. Total: %d -> %d", old_len, old_len + new_elements_len);

    // Cleanup refs
    env->DeleteLocalRef(j_path);
    env->DeleteLocalRef(files_list);
    env->DeleteLocalRef(file_obj);
    env->DeleteLocalRef(suppressed_exceptions_list);

    return true;
}

} // namespace art
} // namespace vmeer

// Gerbang JNI Luar (Berada di luar lingkup namespace vmeer::art)
extern "C" void init_art_hook(JNIEnv* env) {
    vmeer::art::ApplyHiddenApiBypass(env);
}

extern "C" void perform_mirror_injection(JNIEnv* env, jobject class_loader, const char* path) {
    vmeer::art::InjectMirrorFramework(env, class_loader, path);
}
