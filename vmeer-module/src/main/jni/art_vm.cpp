#include <jni.h>
#include <android/log.h>
#include <string>
#include <vector>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "shadowhook.h"

#define LOG_TAG "vMeer_ART"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// Pointer untuk menyimpan alamat fungsi asli libart.so yang ditangkap ShadowHook
static bool (*orig_ShouldBlockAccessToMember_11_14)(void*, void*, int, int) = nullptr;
static bool (*orig_ShouldBlockAccessToMember_15)(void*, void*, int, int, bool) = nullptr;

/**
 * Proxy Hook untuk Android 11 hingga Android 14
 * Mengembalikan false secara mutlak = Akses Hidden API tidak pernah diblokir.
 */
static bool proxy_ShouldBlockAccessToMember_11_14(void* member, void* ctx, int access_ctx, int access_method) {
    (void)member; (void)ctx; (void)access_ctx; (void)access_method;
    return false; 
}

/**
 * Proxy Hook khusus Android 15 (Menyesuaikan parameter enkapsulasi baru ART)
 */
static bool proxy_ShouldBlockAccessToMember_15(void* member, void* ctx, int access_ctx, int access_method, bool v) {
    (void)member; (void)ctx; (void)access_ctx; (void)access_method; (void)v;
    return false;
}

namespace vmeer {
namespace art {

/**
 * ARSITEKTUR BARU: Menggunakan ShadowHook untuk memotong Hidden API Enforcement Policy
 * langsung di dalam struktur biner memory ruang kerja libart.so.
 */
void ApplyNativeHiddenApiBypass() {
    LOGI("vMeer ART: Memulai operasi Global Native Hook Bypass (Style: LSPosed/KernelSU)...");

    // 1. Coba pasang hook menggunakan pencarian simbol wildcard untuk Android 11 - 14
    void* hook_11_14 = shadowhook_hook_sym_name(
        "libart.so",
        "_ZN3art9hiddenapi25ShouldBlockAccessToMember*", 
        reinterpret_cast<void*>(proxy_ShouldBlockAccessToMember_11_14),
        reinterpret_cast<void**>(&orig_ShouldBlockAccessToMember_11_14)
    );

    if (hook_11_14 != nullptr) {
        LOGI("vMeer ART: SUCCESS - Global Native Bypass aktif untuk Android 11-14.");
        return;
    }

    // 2. Jika gagal (kemungkinan Android 15), coba pasang hook ke simbol internal Android 15 terbaru
    void* hook_15 = shadowhook_hook_sym_name(
        "libart.so",
        "_ZN3art9hiddenapi29ShouldBlockAccessToMemberImpl*", 
        reinterpret_cast<void*>(proxy_ShouldBlockAccessToMember_15),
        reinterpret_cast<void**>(&orig_ShouldBlockAccessToMember_15)
    );

    if (hook_15 != nullptr) {
        LOGI("vMeer ART: SUCCESS - Global Native Bypass aktif untuk Android 15.");
    } else {
        int err = shadowhook_get_errno();
        LOGE("vMeer ART: CRITICAL - Gagal memotong kebijakan ART! Error: %s", shadowhook_to_errmsg(err));
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
    [span_2](start_span)[span_3](start_span)[span_4](start_span)[span_5](start_span)[span_6](start_span)[span_7](start_span)[span_8](start_span)[span_9](start_span)[span_10](start_span)[span_11](start_span)LOGI("vMeer ART: Menjadwalkan penggabungan classpath -> %s", clean_jar_name.c_str());[span_2](end_span)[span_3](end_span)[span_4](end_span)[span_5](end_span)[span_6](end_span)[span_7](end_span)[span_8](end_span)[span_9](end_span)[span_10](end_span)[span_11](end_span)

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

    [span_12](start_span)jfieldID path_list_fid = env->GetFieldID(loader_clazz, "pathList", "Ldalvik/system/DexPathList;");[span_12](end_span)
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
    [span_13](start_span)jfieldID elements_fid = env->GetFieldID(path_list_clazz, "dexElements", "[Ldalvik/system/DexPathList$Element;");[span_13](end_span)
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
    [span_14](start_span)[span_15](start_span)[span_16](start_span)[span_17](start_span)[span_18](start_span)[span_19](start_span)[span_20](start_span)[span_21](start_span)[span_22](start_span)[span_23](start_span)LOGI("vMeer ART: INTEGRATION LIVE -> Komponen %s sukses disatukan ke runtime.", clean_jar_name.c_str());[span_14](end_span)[span_15](end_span)[span_16](end_span)[span_17](end_span)[span_18](end_span)[span_19](end_span)[span_20](end_span)[span_21](end_span)[span_22](end_span)[span_23](end_span)

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
 * REFACTOR: Handler JNI Bridge kini mengeksekusi Native Hook 
 * alih-alih Java Reflection VMRuntime yang terdeteksi/gagal di Android 15.
 */
JNIEXPORT void JNICALL
Java_com_vmeer_io_EngineLoader_init_1art_1hook(JNIEnv* env, jclass clazz) {
    (void)env;
    (void)clazz;
    vmeer::art::ApplyNativeHiddenApiBypass();
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
