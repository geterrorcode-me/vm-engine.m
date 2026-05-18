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
    // 🛡️ SECURITY BLOCKER: Proteksi agar kontainer biner (.bin) tidak merusak sistem alokasi ART
    if (jar_path.find(".bin") != std::string::npos) {
        LOGE("vMeer ART: REJECTED! %s adalah container biner mentah.", jar_path.c_str());
        LOGE("vMeer ART: Lapisan ART hanya menerima berkas .jar yang sudah diekstrak/di-mount lewat VFS!");
        return false;
    }

    // Ambil nama file asli di ujung path untuk mempermudah pemantauan logcat
    std::string clean_jar_name = jar_path.substr(jar_path.find_last_of("/\\") + 1);
    LOGI("vMeer ART: Memproses penyisipan runtime framework -> %s", clean_jar_name.c_str());

    if (class_loader == nullptr) {
        LOGE("vMeer ART: ClassLoader host bernilai NULL!");
        return false;
    }

    // 1. Dapatkan dalvik.system.BaseDexClassLoader & pathList
    jclass loader_clazz = env->FindClass("dalvik/system/BaseDexClassLoader");
    if (env->ExceptionCheck()) { env->ExceptionClear(); return false; }

    jfieldID path_list_fid = env->GetFieldID(loader_clazz, "pathList", "Ldalvik/system/DexPathList;");
    if (!path_list_fid) { LOGE("vMeer ART: Field pathList tidak ditemukan."); return false; }

    jobject path_list_obj = env->GetObjectField(class_loader, path_list_fid);
    if (!path_list_obj) { LOGE("vMeer ART: Objek pathList bernilai NULL."); return false; }

    jclass path_list_clazz = env->GetObjectClass(path_list_obj);

    // 2. Ambil array dexElements lama yang terdaftar di host
    jfieldID elements_fid = env->GetFieldID(path_list_clazz, "dexElements", "[Ldalvik/system/DexPathList$Element;");
    if (!elements_fid) { LOGE("vMeer ART: Field dexElements tidak ditemukan."); return false; }

    jobjectArray old_elements = (jobjectArray)env->GetObjectField(path_list_obj, elements_fid);
    jsize old_len = (old_elements != nullptr) ? env->GetArrayLength(old_elements) : 0;

    // 3. Cari static method 'makePathElements' di dalam DexPathList milik sistem
    jmethodID make_elements_mid = env->GetStaticMethodID(
        path_list_clazz, 
        "makePathElements", 
        "(Ljava/util/List;Ljava/io/File;Ljava/util/List;)[Ldalvik/system/DexPathList$Element;"
    );

    if (!make_elements_mid) {
        env->ExceptionClear();
        // Fallback untuk variasi internal signatur pada beberapa vendor ROM (makeDexElements)
        make_elements_mid = env->GetStaticMethodID(
            path_list_clazz, 
            "makeDexElements", 
            "(Ljava/util/List;Ljava/io/File;Ljava/util/List;Ljava/lang/ClassLoader;)[Ldalvik/system/DexPathList$Element;"
        );
    }

    if (!make_elements_mid) {
        LOGE("vMeer ART: Gagal menemukan method pembuat elemen internal Android Runtime.");
        return false;
    }

    // 4. Bungkus berkas .jar yang ditargetkan ke dalam java.util.ArrayList via JNI
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

    // 5. Eksekusi pembuatan objek Element (ART akan membedah classes.dex di dalam berkas .jar secara murni)
    jobjectArray new_element_array = (jobjectArray)env->CallStaticObjectMethod(
        path_list_clazz, 
        make_elements_mid, 
        files_list, 
        nullptr, 
        suppressed_exceptions_list
    );

    if (env->ExceptionCheck() || !new_element_array) {
        env->ExceptionClear();
        LOGE("vMeer ART: Gagal mengurai dex internal dari berkas: %s", clean_jar_name.c_str());
        env->DeleteLocalRef(j_path); env->DeleteLocalRef(files_list);
        env->DeleteLocalRef(file_obj); env->DeleteLocalRef(suppressed_exceptions_list);
        return false;
    }

    jsize new_elements_len = env->GetArrayLength(new_element_array);
    if (new_elements_len == 0) {
        LOGE("vMeer ART: Hasil dekapsulasi elemen %s bernilai 0. Berkas corrupt atau dilindungi.", clean_jar_name.c_str());
        env->DeleteLocalRef(j_path); env->DeleteLocalRef(files_list);
        env->DeleteLocalRef(file_obj); env->DeleteLocalRef(suppressed_exceptions_list);
        return false;
    }

    // 6. Alokasikan array gabungan final (Host Elements + Guest Framework Elements)
    jclass element_clazz = env->FindClass("dalvik/system/DexPathList$Element");
    jobjectArray combined_elements = env->NewObjectArray(old_len + new_elements_len, element_clazz, nullptr);

    // Salin elemen aplikasi asli
    for (jsize i = 0; i < old_len; i++) {
        jobject element = env->GetObjectArrayElement(old_elements, i);
        env->SetObjectArrayElement(combined_elements, i, element);
        env->DeleteLocalRef(element);
    }

    // Masukkan elemen baru dari virtual framework .jar hasil mount VFS
    for (jsize i = 0; i < new_elements_len; i++) {
        jobject element = env->GetObjectArrayElement(new_element_array, i);
        env->SetObjectArrayElement(combined_elements, old_len + i, element);
        env->DeleteLocalRef(element);
    }

    // 7. Kunci kembali struktur array baru ke ClassLoader host agar refleksi sistem aktif
    env->SetObjectField(path_list_obj, elements_fid, combined_elements);
    LOGI("vMeer ART: SUCCESS! Elemen %s berhasil disisipkan. Total komponen aktif: %d", 
          clean_jar_name.c_str(), old_len + new_elements_len);

    // Bersihkan referensi lokal JNI
    env->DeleteLocalRef(j_path);
    env->DeleteLocalRef(files_list);
    env->DeleteLocalRef(file_obj);
    env->DeleteLocalRef(suppressed_exceptions_list);

    return true;
}

} // namespace art
} // namespace vmeer

// =============================================================================
// GERBANG JNI & CROSS-MODULE SYMBOL EXPORT
// =============================================================================

extern "C" {

// Ekspor fungsi C murni dengan visibilitas publik untuk menyelesaikan 'undefined symbol' di libvmeer_engine.so
__attribute__((visibility("default"))) void perform_mirror_injection(JNIEnv* env, jobject class_loader, const char* path) {
    if (path != nullptr) {
        vmeer::art::InjectMirrorFramework(env, class_loader, path);
    }
}

// Handler JNI Bridge untuk pemanggilan langsung dari sisi Java (EngineLoader)
JNIEXPORT void JNICALL
Java_com_vmeer_io_EngineLoader_init_1art_1hook(JNIEnv* env, jclass clazz) {
    vmeer::art::ApplyHiddenApiBypass(env);
}

JNIEXPORT void JNICALL
Java_com_vmeer_io_EngineLoader_perform_1mirror_1injection(JNIEnv* env, jclass clazz, jobject class_loader, jstring path) {
    if (!path) return;
    const char* native_path = env->GetStringUTFChars(path, nullptr);
    if (native_path) {
        vmeer::art::InjectMirrorFramework(env, class_loader, native_path);
        env->ReleaseStringUTFChars(path, native_path);
    }
}

} // extern "C"
