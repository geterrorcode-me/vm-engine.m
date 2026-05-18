#include <jni.h>
#include <android/log.h>
#include <string>
#include <vector>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#define LOG_TAG "vMeer_ART"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace vmeer {
namespace art {

/**
 * Mem-bypass kebijakan Hidden API Policy pada Android 9 hingga Android 15.
 * Menggunakan refleksi tingkat internal untuk membebaskan pengecualian (exemptions)
 * sehingga komponen VM dapat memanggil API tersembunyi milik sistem host tanpa diblokir.
 */
void ApplyHiddenApiBypass(JNIEnv* env) {
    LOGI("vMeer ART: Menginisialisasi Hidden API Bypass...");
    
    jclass vm_runtime_clazz = env->FindClass("dalvik/system/VMRuntime");
    if (!vm_runtime_clazz) {
        LOGE("vMeer ART: Gagal menemukan kelas dalvik.system.VMRuntime");
        return;
    }

    jmethodID get_runtime_mid = env->GetStaticMethodID(vm_runtime_clazz, "getRuntime", "()Ldalvik/system/VMRuntime;");
    if (!get_runtime_mid) {
        LOGE("vMeer ART: Gagal menemukan method static VMRuntime.getRuntime()");
        return;
    }
    
    jobject vm_runtime_obj = env->CallStaticObjectMethod(vm_runtime_clazz, get_runtime_mid);
    if (!vm_runtime_obj) {
        LOGE("vMeer ART: Objek VMRuntime bernilai NULL.");
        return;
    }

    jmethodID set_exemptions_mid = env->GetMethodID(vm_runtime_clazz, "setHiddenApiExemptions", "([Ljava/lang/String;)V");
    if (!set_exemptions_mid) {
        LOGE("vMeer ART: Gagal menemukan method setHiddenApiExemptions()");
        return;
    }
    
    // Memberikan pengecualian string "L" untuk membebaskan seluruh struktur paket API internal Android
    jobjectArray str_array = env->NewObjectArray(1, env->FindClass("java/lang/String"), env->NewStringUTF("L"));
    env->CallVoidMethod(vm_runtime_obj, set_exemptions_mid, str_array);

    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        LOGE("vMeer ART: Eksekusi Hidden API Bypass ditolak oleh runtime Android 15.");
    } else {
        LOGI("vMeer ART: SUCCESS - Kebijakan Hidden API berhasil dilonggarkan.");
    }
}

/**
 * Menyuntikkan berkas .jar virtual dari VFS secara dinamis ke dalam ClassLoader aktif.
 * Menggunakan manipulasi runtime di tingkat struktur DexPathList$Element.
 */
bool InjectMirrorFramework(JNIEnv* env, jobject class_loader, const std::string& jar_path) {
    // 🛡️ SECURITY PROTECTOR: Menolak mutlak pemuatan berkas .bin mentah ke dalam ART
    if (jar_path.find(".bin") != std::string::npos) {
        LOGE("vMeer ART: REJECTED! %s dilarang keras dibaca langsung oleh ART.", jar_path.c_str());
        LOGE("vMeer ART: Gunakan daemon vmeerd untuk memetakan kontainer SquashFS ke berkas .jar virtual!");
        return false;
    }

    // Ambil nama file bersih untuk mempermudah pelacakan debugging logcat
    std::string clean_jar_name = jar_path.substr(jar_path.find_last_of("/\\") + 1);
    LOGI("vMeer ART: Menjadwalkan penggabungan classpath -> %s", clean_jar_name.c_str());

    if (class_loader == nullptr) {
        LOGE("vMeer ART: Operasi dibatalkan, ClassLoader host bernilai NULL!");
        return false;
    }

    // Validasi integritas berkas di tingkat kernel sebelum diserahkan ke ClassLoader
    int jar_fd = open(jar_path.c_str(), O_RDONLY);
    if (jar_fd < 0) {
        LOGE("vMeer ART: File %s tidak dapat dibaca secara fisik di VFS.", clean_jar_name.c_str());
        return false;
    }
    close(jar_fd);

    // 1. Dapatkan referensi dalvik.system.BaseDexClassLoader dan field pathList
    jclass loader_clazz = env->FindClass("dalvik/system/BaseDexClassLoader");
    if (env->ExceptionCheck() || !loader_clazz) { 
        env->ExceptionClear(); 
        LOGE("vMeer ART: Gagal memuat kelas BaseDexClassLoader.");
        return false; 
    }

    jfieldID path_list_fid = env->GetFieldID(loader_clazz, "pathList", "Ldalvik/system/DexPathList;");
    if (!path_list_fid) { 
        LOGE("vMeer ART: Field 'pathList' tidak ditemukan pada ClassLoader."); 
        return false; 
    }

    jobject path_list_obj = env->GetObjectField(class_loader, path_list_fid);
    if (!path_list_obj) { 
        LOGE("vMeer ART: Instance objek 'pathList' bernilai NULL."); 
        return false; 
    }

    jclass path_list_clazz = env->GetObjectClass(path_list_obj);

    // 2. Ambil struktur array dexElements lama yang sedang berjalan pada aplikasi host
    jfieldID elements_fid = env->GetFieldID(path_list_clazz, "dexElements", "[Ldalvik/system/DexPathList$Element;");
    if (!elements_fid) { 
        LOGE("vMeer ART: Field 'dexElements' sistem operasi tidak ditemukan."); 
        return false; 
    }

    jobjectArray old_elements = (jobjectArray)env->GetObjectField(path_list_obj, elements_fid);
    jsize old_len = (old_elements != nullptr) ? env->GetArrayLength(old_elements) : 0;

    // 3. Resolusi method pembuat elemen internal Android (Mendukung Android 9 hingga Android 15)
    jmethodID make_elements_mid = env->GetStaticMethodID(
        path_list_clazz, 
        "makePathElements", 
        "(Ljava/util/List;Ljava/io/File;Ljava/util/List;)[Ldalvik/system/DexPathList$Element;"
    );

    if (!make_elements_mid) {
        env->ExceptionClear();
        // Fallback signature untuk beberapa modifikasi vendor ROM tertentu (makeDexElements)
        make_elements_mid = env->GetStaticMethodID(
            path_list_clazz, 
            "makeDexElements", 
            "(Ljava/util/List;Ljava/io/File;Ljava/util/List;Ljava/lang/ClassLoader;)[Ldalvik/system/DexPathList$Element;"
        );
    }

    if (!make_elements_mid) {
        LOGE("vMeer ART: Fatal - Gagal menemukan fungsi generator DexPathList$Element.");
        return false;
    }

    // 4. Instansiasi berkas .jar ke dalam struktur java.util.ArrayList<File> via JNI
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

    // 5. Panggil fungsi pabrikasi elemen internal Android Runtime untuk mengurai berkas .jar virtual
    jobjectArray new_element_array = (jobjectArray)env->CallStaticObjectMethod(
        path_list_clazz, 
        make_elements_mid, 
        files_list, 
        nullptr, 
        suppressed_exceptions_list
    );

    if (env->ExceptionCheck() || !new_element_array) {
        env->ExceptionClear();
        LOGE("vMeer ART: Android Runtime menolak mengurai kode DEX di dalam berkas: %s", clean_jar_name.c_str());
        env->DeleteLocalRef(j_path); env->DeleteLocalRef(files_list);
        env->DeleteLocalRef(file_obj); env->DeleteLocalRef(suppressed_exceptions_list);
        return false;
    }

    jsize new_elements_len = env->GetArrayLength(new_element_array);
    if (new_elements_len == 0) {
        LOGE("vMeer ART: Hasil dekapsulasi elemen %s kosong. Berkas corrupt atau dilindungi W^X.", clean_jar_name.c_str());
        env->DeleteLocalRef(j_path); env->DeleteLocalRef(files_list);
        env->DeleteLocalRef(file_obj); env->DeleteLocalRef(suppressed_exceptions_list);
        return false;
    }

    // 6. Alokasikan array gabungan baru (Menggabungkan Lingkungan Host dan Lingkungan Tamu)
    jclass element_clazz = env->FindClass("dalvik/system/DexPathList$Element");
    jobjectArray combined_elements = env->NewObjectArray(old_len + new_elements_len, element_clazz, nullptr);

    // Salin elemen aplikasi host asli
    for (jsize i = 0; i < old_len; i++) {
        jobject element = env->GetObjectArrayElement(old_elements, i);
        env->SetObjectArrayElement(combined_elements, i, element);
        env->DeleteLocalRef(element);
    }

    // Sisipkan elemen baru dari Guest OS virtual framework .jar hasil ekstraksi VFS
    for (jsize i = 0; i < new_elements_len; i++) {
        jobject element = env->GetObjectArrayElement(new_element_array, i);
        env->SetObjectArrayElement(combined_elements, old_len + i, element);
        env->DeleteLocalRef(element);
    }

    // 7. Kunci kembali struktur array baru ke dalam ClassLoader host aktif
    env->SetObjectField(path_list_obj, elements_fid, combined_elements);
    LOGI("vMeer ART: INTEGRATION LIVE -> Komponen %s sukses disatukan ke runtime.", clean_jar_name.c_str());

    // Bersihkan seluruh referensi memori lokal JNI lokal untuk mencegah kebocoran tabel referensi
    env->DeleteLocalRef(j_path);
    env->DeleteLocalRef(files_list);
    env->DeleteLocalRef(file_obj);
    env->DeleteLocalRef(suppressed_exceptions_list);

    return true;
}

} // namespace art
} // namespace vmeer

// =============================================================================
// GERBANG EKSPOR SIMBOL JNI (BRIDGING INTER-MODULE & JAVA LAYER)
// =============================================================================

extern "C" {

/**
 * Ekspor simbol murni tingkat C (C-Binding) agar dapat diselesaikan secara dinamis 
 * saat dipanggil menggunakan dlsym() atau pautan langsung dari 'libvmeer_engine.so'.
 */
__attribute__((visibility("default"))) void perform_mirror_injection(JNIEnv* env, jobject class_loader, const char* path) {
    if (path != nullptr && env != nullptr) {
        vmeer::art::InjectMirrorFramework(env, class_loader, path);
    }
}

/**
 * Handler JNI Bridge untuk pemicuan manual Hidden API Bypass langsung dari Java (EngineLoader.java)
 */
JNIEXPORT void JNICALL
Java_com_vmeer_io_EngineLoader_init_1art_1hook(JNIEnv* env, jclass clazz) {
    (void)clazz;
    vmeer::art::ApplyHiddenApiBypass(env);
}

/**
 * Handler JNI Bridge untuk penyuntikan berkas .jar individual langsung dari Java (EngineLoader.java)
 */
JNIEXPORT void JNICALL
Java_com_vmeer_io_EngineLoader_perform_1mirror_1injection(JNIEnv* env, jclass clazz, jobject class_loader, jstring path) {
    (void)clazz;
    if (!path || !class_loader) return;
    
    const char* native_path = env->GetStringUTFChars(path, nullptr);
    if (native_path) {
        vmeer::art::InjectMirrorFramework(env, class_loader, native_path);
        env->ReleaseStringUTFChars(path, native_path);
    }
}

} // extern "C"
