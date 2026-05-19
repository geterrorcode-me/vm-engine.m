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
 * Mekanisme Fallback Alternatif: Memaksa target SDK version di tingkat internal VM
 * turun ke tingkat di mana pemeriksaan Hidden API dilewati secara bawaan (SDK 27 / Android 8.1).
 */
bool ForceSetTargetSdkVersion(JNIEnv* env) {
    LOGI("vMeer ART: Mengeksekusi Fallback Lapis 2: targetSdkVersion Spoofing...");
    
    jclass vm_runtime_clazz = env->FindClass("dalvik/system/VMRuntime");
    if (env->ExceptionCheck() || !vm_runtime_clazz) {
        env->ExceptionClear();
        return false;
    }

    jmethodID get_runtime_mid = env->GetStaticMethodID(vm_runtime_clazz, "getRuntime", "()Ldalvik/system/VMRuntime;");
    jmethodID set_target_sdk_mid = env->GetMethodID(vm_runtime_clazz, "setTargetSdkVersion", "(I)V");

    if (get_runtime_mid && set_target_sdk_mid) {
        jobject vm_runtime_obj = env->CallStaticObjectMethod(vm_runtime_clazz, get_runtime_mid);
        if (vm_runtime_obj) {
            // Set target SDK ke 27 (Android Oreo) secara paksa pada runtime instance
            env->CallVoidMethod(vm_runtime_obj, set_target_sdk_mid, 27);
            if (!env->ExceptionCheck()) {
                LOGI("vMeer ART: SUCCESS - targetSdkVersion sukses diturunkan ke API 27 secara internal.");
                return true;
            }
            env->ExceptionClear();
        }
    }
    return false;
}

/**
 * ARSITEKTUR BARU: Menggunakan pencarian simbol berlapis (Multi-Signature & Multi-Library)
 * serta fallback manipulasi target SDK untuk performa bypass mutlak.
 */
void ApplyNativeHiddenApiBypass(JNIEnv* env) {
    LOGI("vMeer ART: Memulai operasi Global Native Hook Bypass (Robust Mode)...");

    // Daftar mangled name presisi untuk ShouldBlockAccessToMember (Android 11 - 14)
    const char* art_symbols_11_14[] = {
        "_ZN3art9hiddenapi25ShouldBlockAccessToMemberEPNS_9ArtMethodENS0_12AccessMethodENS0_12ActionPolicyE",
        "_ZN3art9hiddenapi25ShouldBlockAccessToMemberEPNS_9ArtMethodENS0_12AccessMethodE",
        "_ZN3art9hiddenapi25ShouldBlockAccessToMember*" 
    };

    void* hook_11_14 = nullptr;
    for (const char* sym : art_symbols_11_14) {
        hook_11_14 = shadowhook_hook_sym_name(
            "libart.so",
            sym, 
            reinterpret_cast<void*>(proxy_ShouldBlockAccessToMember_11_14),
            reinterpret_cast<void**>(&orig_ShouldBlockAccessToMember_11_14)
        );
        if (hook_11_14 != nullptr) {
            LOGI("vMeer ART: SUCCESS - Global Native Bypass aktif untuk Android 11-14 via simbol: %s", sym);
            return;
        }
    }

    // Daftar mangled name presisi untuk Android 15 (ShouldBlockAccessToMemberImpl / Enforcement Policy)
    const char* art_symbols_15[] = {
        "_ZN3art9hiddenapi29ShouldBlockAccessToMemberImplEPNS_9ArtMethodENS0_12AccessMethodENS0_12ActionPolicyEb",
        "_ZN3art9hiddenapi29ShouldBlockAccessToMemberImplEPNS_9ArtMethodENS0_12AccessMethodEb",
        "_ZN3art9hiddenapi29ShouldBlockAccessToMemberImpl*" 
    };

    void* hook_15 = nullptr;
    const char* art_libs[] = {"libart.so", "libartbase.so"};

    for (const char* lib : art_libs) {
        for (const char* sym : art_symbols_15) {
            hook_15 = shadowhook_hook_sym_name(
                lib,
                sym, 
                reinterpret_cast<void*>(proxy_ShouldBlockAccessToMember_15),
                reinterpret_cast<void**>(&orig_ShouldBlockAccessToMember_15)
            );
            if (hook_15 != nullptr) {
                LOGI("vMeer ART: SUCCESS - Global Native Bypass aktif untuk Android 15 [%s] -> %s", lib, sym);
                return;
            }
        }
    }

    // JIKA SELURUH HOOK BINER GAGAL: Jalankan Fallback Lapis 2 ( targetSdkVersion Spoofing )
    if (hook_11_14 == nullptr && hook_15 == nullptr) {
        int err = shadowhook_get_errno();
        LOGW("vMeer ART: WARNING - Hook ELF dilewati (%s). Mengalihkan ke sistem targetSdkVersion Spoofing...", shadowhook_to_errmsg(err));
        if (ForceSetTargetSdkVersion(env)) {
            LOGI("vMeer ART: SUCCESS - Bypass dijamin aktif melalui regulasi targetSdkVersion < 28.");
        } else {
            LOGE("vMeer ART: FATAL - Seluruh sistem pertahanan ART gagal ditembus!");
        }
    }
}

/**
 * Menyuntikkan berkas .jar virtual dari VFS secara dinamis ke dalam ClassLoader aktif.
 */
bool InjectMirrorFramework(JNIEnv* env, jobject class_loader, const std::string& jar_path) {
    if (jar_path.find(".bin") != std::string::npos) {
        LOGE("vMeer ART: REJECTED! %s dilarang keras dibaca langsung oleh ART.", jar_path.c_str());
        return false;
    }

    std::string clean_jar_name = jar_path.substr(jar_path.find_last_of("/\\") + 1);
    LOGI("vMeer ART: Menjadwalkan penggabungan classpath -> %s", clean_jar_name.c_str());

    if (class_loader == nullptr) {
        LOGE("vMeer ART: ClassLoader host bernilai NULL!");
        return false;
    }

    int jar_fd = open(jar_path.c_str(), O_RDONLY);
    if (jar_fd < 0) {
        LOGE("vMeer ART: File %s tidak dapat dibaca di VFS.", clean_jar_name.c_str());
        return false;
    }
    close(jar_fd);

    jclass loader_clazz = env->FindClass("dalvik/system/BaseDexClassLoader");
    if (env->ExceptionCheck() || !loader_clazz) { 
        env->ExceptionClear(); 
        LOGE("vMeer ART: Gagal memuat BaseDexClassLoader.");
        return false; 
    }

    jfieldID path_list_fid = env->GetFieldID(loader_clazz, "pathList", "Ldalvik/system/DexPathList;");
    if (!path_list_fid) { 
        LOGE("vMeer ART: Field 'pathList' tidak ditemukan."); 
        return false; 
    }

    jobject path_list_obj = env->GetObjectField(class_loader, path_list_fid);
    if (!path_list_obj) { 
        LOGE("vMeer ART: Instance 'pathList' bernilai NULL."); 
        return false; 
    }

    jclass path_list_clazz = env->GetObjectClass(path_list_obj);

    jfieldID elements_fid = env->GetFieldID(path_list_clazz, "dexElements", "[Ldalvik/system/DexPathList$Element;");
    if (!elements_fid) { 
        LOGE("vMeer ART: Field 'dexElements' tidak ditemukan."); 
        return false; 
    }

    jobjectArray old_elements = (jobjectArray)env->GetObjectField(path_list_obj, elements_fid);
    jsize old_len = (old_elements != nullptr) ? env->GetArrayLength(old_elements) : 0;

    jmethodID make_elements_mid = env->GetStaticMethodID(
        path_list_clazz, 
        "makePathElements", 
        "(Ljava/util/List;Ljava/io/File;Ljava/util/List;)[Ldalvik/system/DexPathList$Element;"
    );

    if (!make_elements_mid) {
        env->ExceptionClear();
        make_elements_mid = env->GetStaticMethodID(
            path_list_clazz, 
            "makeDexElements", 
            "(Ljava/util/List;Ljava/io/File;Ljava/util/List;Ljava/lang/ClassLoader;)[Ldalvik/system/DexPathList$Element;"
        );
    }

    if (!make_elements_mid) {
        LOGE("vMeer ART: Gagal menemukan fungsi generator DexPathList$Element.");
        return false;
    }

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

    jobjectArray new_element_array = (jobjectArray)env->CallStaticObjectMethod(
        path_list_clazz, 
        make_elements_mid, 
        files_list, 
        nullptr, 
        suppressed_exceptions_list
    );

    if (env->ExceptionCheck() || !new_element_array) {
        env->ExceptionClear();
        LOGE("vMeer ART: Gagal mengurai biner DEX dari berkas: %s", clean_jar_name.c_str());
        env->DeleteLocalRef(j_path); env->DeleteLocalRef(files_list);
        env->DeleteLocalRef(file_obj); env->DeleteLocalRef(suppressed_exceptions_list);
        return false;
    }

    jsize new_elements_len = env->GetArrayLength(new_element_array);
    if (new_elements_len == 0) {
        LOGE("vMeer ART: Hasil dekapsulasi elemen %s kosong.", clean_jar_name.c_str());
        env->DeleteLocalRef(j_path); env->DeleteLocalRef(files_list);
        env->DeleteLocalRef(file_obj); env->DeleteLocalRef(suppressed_exceptions_list);
        return false;
    }

    jclass element_clazz = env->FindClass("dalvik/system/DexPathList$Element");
    jobjectArray combined_elements = env->NewObjectArray(old_len + new_elements_len, element_clazz, nullptr);

    for (jsize i = 0; i < old_len; i++) {
        jobject element = env->GetObjectArrayElement(old_elements, i);
        env->SetObjectArrayElement(combined_elements, i, element);
        env->DeleteLocalRef(element);
    }

    for (jsize i = 0; i < new_elements_len; i++) {
        jobject element = env->GetObjectArrayElement(new_element_array, i);
        env->SetObjectArrayElement(combined_elements, old_len + i, element);
        env->DeleteLocalRef(element);
    }

    env->SetObjectField(path_list_obj, elements_fid, combined_elements);
    LOGI("vMeer ART: INTEGRATION LIVE -> Komponen %s sukses disatukan ke runtime.", clean_jar_name.c_str());

    env->DeleteLocalRef(j_path);
    env->DeleteLocalRef(files_list);
    env->DeleteLocalRef(file_obj);
    env->DeleteLocalRef(suppressed_exceptions_list);

    return true;
}

} // namespace art
} // namespace vmeer

// =============================================================================
// GERBANG EKSPOR SIMBOL JNI
// =============================================================================

extern "C" {

__attribute__((visibility("default"))) void perform_mirror_injection(JNIEnv* env, jobject class_loader, const char* path) {
    if (path != nullptr && env != nullptr) {
        vmeer::art::InjectMirrorFramework(env, class_loader, path);
    }
}

JNIEXPORT void JNICALL
Java_com_vmeer_io_EngineLoader_init_1art_1hook(JNIEnv* env, jclass clazz) {
    (void)clazz;
    vmeer::art::ApplyNativeHiddenApiBypass(env); // Mengirim pointer JNIEnv untuk Lapis 2 Fallback
}

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