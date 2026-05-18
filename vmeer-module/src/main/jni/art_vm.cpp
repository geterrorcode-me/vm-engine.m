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

bool InjectMirrorFramework(JNIEnv* env, jobject class_loader, const std::string& jar_path) {
    // 🛡️ PROTEKSI BARU: Validasi agar kontainer .bin tidak nyasar ke ART Layer
    if (jar_path.find(".bin") != std::string::npos) {
        LOGE("vMeer ART: REJECTED! %s adalah container biner mentah.", jar_path.c_str());
        LOGE("vMeer ART: Lapisan ART hanya menerima berkas .jar yang sudah diekstrak/di-mount lewat VFS!");
        return false;
    }

    LOGI("vMeer ART: Menyuntikkan Target Jar -> %s", jar_path.c_str());

    if (class_loader == nullptr) {
        LOGE("vMeer ART: ClassLoader is NULL!");
        return false;
    }

    // 1. Dapatkan dalvik.system.BaseDexClassLoader & pathList
    jclass loader_clazz = env->FindClass("dalvik/system/BaseDexClassLoader");
    if (env->ExceptionCheck()) { env->ExceptionClear(); return false; }

    jfieldID path_list_fid = env->GetFieldID(loader_clazz, "pathList", "Ldalvik/system/DexPathList;");
    if (!path_list_fid) { LOGE("vMeer ART: Field pathList tidak ditemukan."); return false; }

    jobject path_list_obj = env->GetObjectField(class_loader, path_list_fid);
    if (!path_list_obj) { LOGE("vMeer ART: pathList bernilai NULL."); return false; }

    jclass path_list_clazz = env->GetObjectClass(path_list_obj);

    // 2. Ambil array dexElements host
    jfieldID elements_fid = env->GetFieldID(path_list_clazz, "dexElements", "[Ldalvik/system/DexPathList$Element;");
    if (!elements_fid) { LOGE("vMeer ART: Field dexElements tidak ditemukan."); return false; }

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
        // Fallback Android lama/vendor tweak khusus (makeDexElements)
        make_elements_mid = env->GetStaticMethodID(
            path_list_clazz, 
            "makeDexElements", 
            "(Ljava/util/List;Ljava/io/File;Ljava/util/List;Ljava/lang/ClassLoader;)[Ldalvik/system/DexPathList$Element;"
        );
    }

    if (!make_elements_mid) {
        LOGE("vMeer ART: Gagal memetakan method pembuat elemen internal ART.");
        return false;
    }

    // 4. Bungkus berkas framework .jar ke dalam java.util.ArrayList
    jclass array_list_clazz = env->FindClass("java/util/ArrayList");
    jmethodID array_list_init = env->GetMethodID(array_list_clazz, "<init>", "()V");
    jmethodID array_list_add = env->GetMethodID(array_list_clazz, "add", "(Ljava/lang/Object;)Z");

    jobject files_list = env->NewObject(array_list_clazz, array_list_init);
    jclass file_clazz = env->FindClass("java/io/File");
    jmethodID file_init = env->GetMethodID(file_clazz, "<init>", "(Ljava/lang/String;)V");
    
    jstring j_path = env->NewStringUTF(jar_path.c_str());
    jobject file_obj = env->NewObject(file_clazz, file_init, j_path);
    env->CallBooleanMethod(files_list, array_list_add, file_obj);

    jobject suppressed_exceptions_list = env->NewObject(array_list_clazz, array_list_init);

    // 5. Eksekusi parsing dex ke objek Element
    jobjectArray new_element_array = (jobjectArray)env->CallStaticObjectMethod(
        path_list_clazz, 
        make_elements_mid, 
        files_list, 
        nullptr, 
        suppressed_exceptions_list
    );

    if (env->ExceptionCheck() || !new_element_array) {
        env->ExceptionClear();
        LOGE("vMeer ART: Gagal mengekstrak elemen berkas .jar lewat ART core.");
        env->DeleteLocalRef(j_path); env->DeleteLocalRef(files_list);
        env->DeleteLocalRef(file_obj); env->DeleteLocalRef(suppressed_exceptions_list);
        return false;
    }

    jsize new_elements_len = env->GetArrayLength(new_element_array);
    if (new_elements_len == 0) {
        LOGE("vMeer ART: Hasil dekapsulasi .jar kosong (classes.dex tidak terbaca/corrupt).");
        env->DeleteLocalRef(j_path); env->DeleteLocalRef(files_list);
        env->DeleteLocalRef(file_obj); env->DeleteLocalRef(suppressed_exceptions_list);
        return false;
    }

    // 6. Alokasikan array gabungan final
    jclass element_clazz = env->FindClass("dalvik/system/DexPathList$Element");
    jobjectArray combined_elements = env->NewObjectArray(old_len + new_elements_len, element_clazz, nullptr);

    // Salin elemen lama host
    for (jsize i = 0; i < old_len; i++) {
        jobject element = env->GetObjectArrayElement(old_elements, i);
        env->SetObjectArrayElement(combined_elements, i, element);
        env->DeleteLocalRef(element);
    }

    // Gabungkan elemen baru dari virtual framework .jar
    for (jsize i = 0; i < new_elements_len; i++) {
        jobject element = env->GetObjectArrayElement(new_element_array, i);
        env->SetObjectArrayElement(combined_elements, old_len + i, element);
        env->DeleteLocalRef(element);
    }

    // 7. Kunci ke ClassLoader aktif
    env->SetObjectField(path_list_obj, elements_fid, combined_elements);
    LOGI("vMeer ART: SUCCESS! Elemen %s berhasil disisipkan. Total elemen aktif: %d", 
          jar_path.substr(jar_path.find_last_of("/\\") + 1).c_str(), old_len + new_elements_len);

    // Cleanup refs
    env->DeleteLocalRef(j_path);
    env->DeleteLocalRef(files_list);
    env->DeleteLocalRef(file_obj);
    env->DeleteLocalRef(suppressed_exceptions_list);

    return true;
}

} // namespace art
} // namespace vmeer

// Gerbang JNI Eksternal
extern "C" void init_art_hook(JNIEnv* env) {
    vmeer::art::ApplyHiddenApiBypass(env);
}

extern "C" void perform_mirror_injection(JNIEnv* env, jobject class_loader, const char* path) {
    vmeer::art::InjectMirrorFramework(env, class_loader, path);
}
