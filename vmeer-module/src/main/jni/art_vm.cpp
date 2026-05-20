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
 */
static bool proxy_ShouldBlockAccessToMember_11_14(void* member, void* ctx, int access_ctx, int access_method) {
    return false; 
}

/**
 * Proxy Hook khusus Android 15
 */
static bool proxy_ShouldBlockAccessToMember_15(void* member, void* ctx, int access_ctx, int access_method, bool v) {
    return false;
}

namespace vmeer {
namespace art {

// Helper untuk membersihkan exception JNI
static void ClearJniException(JNIEnv* env) {
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
    }
}

/**
 * Mendapatkan instansi VMRuntime secara aman
 */
jobject GetVMRuntimeInstance(JNIEnv* env) {
    jclass vm_runtime_clazz = env->FindClass("dalvik/system/VMRuntime");
    if (!vm_runtime_clazz) {
        ClearJniException(env);
        return nullptr;
    }

    jmethodID get_runtime_mid = env->GetStaticMethodID(vm_runtime_clazz, "getRuntime", "()Ldalvik/system/VMRuntime;");
    if (!get_runtime_mid) {
        ClearJniException(env);
        env->DeleteLocalRef(vm_runtime_clazz);
        return nullptr;
    }

    jobject obj = env->CallStaticObjectMethod(vm_runtime_clazz, get_runtime_mid);
    env->DeleteLocalRef(vm_runtime_clazz);
    
    if (!obj) {
        ClearJniException(env);
        return nullptr;
    }
    return obj;
}

/**
 * BYPASS STRATEGY ANDROID 15: Menggunakan Meta-Refleksi Unsafe tingkat JNI
 * untuk mengeksekusi metode Java yang dilindungi tanpa melalui verifikasi JNI klasik.
 */
bool InvokeUnsafeMethod(JNIEnv* env, jobject target_obj, const char* method_name, const char* sig, ...) {
    jclass clazz = env->GetObjectClass(target_obj);
    jclass class_clazz = env->FindClass("java/lang/Class");
    
    // Gunakan getDeclaredMethod untuk menghindari restriksi JNI GetMethodID
    jmethodID get_declared_method_id = env->GetMethodID(
        class_clazz, 
        "getDeclaredMethod", 
        "(Ljava/lang/String;[Ljava/lang/Class;)Ljava/lang/reflect/Method;"
    );

    if (!get_declared_method_id) {
        ClearJniException(env);
        env->DeleteLocalRef(clazz);
        env->DeleteLocalRef(class_clazz);
        return false;
    }

    jstring j_method_name = env->NewStringUTF(method_name);
    
    // Deteksi tipe parameter berdasarkan nama fungsi (untuk kestabilan runtime JNI)
    jobjectArray param_types = nullptr;
    if (strcmp(method_name, "setTargetSdkVersion") == 0) {
        param_types = env->NewObjectArray(1, class_clazz, nullptr);
        jclass int_class = env->FindClass("java/lang/Integer");
        jfieldID type_fid = env->GetStaticFieldID(int_class, "TYPE", "Ljava/lang/Class;");
        jobject int_type_obj = env->GetStaticObjectField(int_class, type_fid);
        env->SetObjectArrayElement(param_types, 0, int_type_obj);
        env->DeleteLocalRef(int_class);
        env->DeleteLocalRef(int_type_obj);
    } else if (strcmp(method_name, "setHiddenApiExemptions") == 0) {
        param_types = env->NewObjectArray(1, class_clazz, nullptr);
        jclass string_array_class = env->FindClass("[Ljava/lang/String;");
        env->SetObjectArrayElement(param_types, 0, string_array_class);
        env->DeleteLocalRef(string_array_class);
    }

    jobject method_obj = env->CallObjectMethod(target_obj, get_declared_method_id, j_method_name, param_types);
    env->DeleteLocalRef(j_method_name);
    env->DeleteLocalRef(param_types);

    if (env->ExceptionCheck() || !method_obj) {
        ClearJniException(env);
        env->DeleteLocalRef(clazz);
        env->DeleteLocalRef(class_clazz);
        if (method_obj) env->DeleteLocalRef(method_obj);
        return false;
    }

    // Set accessible ke true secara dinamis
    jclass accessible_object_clazz = env->FindClass("java/lang/reflect/AccessibleObject");
    jmethodID set_accessible_mid = env->GetMethodID(accessible_object_clazz, "setAccessible", "(Z)V");
    env->CallVoidMethod(method_obj, set_accessible_mid, JNI_TRUE);
    ClearJniException(env);

    // Ambil Method ID dari Java Method Object untuk pemanggilan Unsafe
    jmethodID target_mid = env->FromReflectedMethod(method_obj);
    
    // Eksekusi pemanggilan nyata
    va_list args;
    va_start(args, sig);
    if (strcmp(method_name, "setTargetSdkVersion") == 0) {
        int sdk = va_arg(args, int);
        env->CallVoidMethod(target_obj, target_mid, sdk);
    } else if (strcmp(method_name, "setHiddenApiExemptions") == 0) {
        jobjectArray exemptions = va_arg(args, jobjectArray);
        env->CallVoidMethod(target_obj, target_mid, exemptions);
    }
    va_end(args);

    bool success = !env->ExceptionCheck();
    ClearJniException(env);

    env->DeleteLocalRef(method_obj);
    env->DeleteLocalRef(accessible_object_clazz);
    env->DeleteLocalRef(clazz);
    env->DeleteLocalRef(class_clazz);

    return success;
}

bool ForceSetTargetSdkVersion(JNIEnv* env) {
    LOGI("vMeer ART: Fallback Lapis 2: Spoofing Target SDK...");
    
    jobject vm_runtime_obj = GetVMRuntimeInstance(env);
    if (!vm_runtime_obj) {
        LOGE("vMeer ART: Gagal mendapatkan instance VMRuntime.");
        return false;
    }

    // Eksekusi bypass meta-refleksi
    bool success = InvokeUnsafeMethod(env, vm_runtime_obj, "setTargetSdkVersion", "(I)V", 28);
    env->DeleteLocalRef(vm_runtime_obj);

    if (success) {
        LOGI("vMeer ART: SUCCESS - Target SDK diset ulang ke API 28.");
    } else {
        LOGE("vMeer ART: Gagal menyetel Target SDK.");
    }
    return success;
}

bool ApplyExemptionsBypass(JNIEnv* env) {
    LOGI("vMeer ART: Fallback Lapis 3: setHiddenApiExemptions...");
    
    jobject vm_runtime_obj = GetVMRuntimeInstance(env);
    if (!vm_runtime_obj) {
        LOGE("vMeer ART: Gagal mendapatkan instance VMRuntime.");
        return false;
    }

    jclass string_clazz = env->FindClass("java/lang/String");
    jstring wildcard = env->NewStringUTF("L");
    jobjectArray str_array = env->NewObjectArray(1, string_clazz, wildcard);

    // Eksekusi bypass meta-refleksi
    bool success = InvokeUnsafeMethod(env, vm_runtime_obj, "setHiddenApiExemptions", "([Ljava/lang/String;)V", str_array);

    env->DeleteLocalRef(wildcard);
    env->DeleteLocalRef(str_array);
    env->DeleteLocalRef(string_clazz);
    env->DeleteLocalRef(vm_runtime_obj);

    if (success) {
        LOGI("vMeer ART: SUCCESS - Exemptions diterapkan.");
    } else {
        LOGE("vMeer ART: Gagal menerapkan Exemptions.");
    }
    return success;
}

void ApplyNativeHiddenApiBypass(JNIEnv* env) {
    LOGI("vMeer ART: Memulai Global Native Hook Bypass (Style: LSPosed/Unsafe)...");

    // Coba Hook Android 11-14
    const char* art_symbols_11_14[] = {
        "_ZN3art9hiddenapi25ShouldBlockAccessToMemberEPNS_9ArtMethodENS0_12AccessMethodENS0_12ActionPolicyE",
        "_ZN3art9hiddenapi25ShouldBlockAccessToMemberEPNS_9ArtMethodENS0_12AccessMethodE",
        nullptr
    };

    for (int i = 0; art_symbols_11_14[i] != nullptr; ++i) {
        void* hook = shadowhook_hook_sym_name(
            "libart.so",
            art_symbols_11_14[i], 
            reinterpret_cast<void*>(proxy_ShouldBlockAccessToMember_11_14),
            reinterpret_cast<void**>(&orig_ShouldBlockAccessToMember_11_14)
        );
        if (hook != nullptr) {
            LOGI("vMeer ART: Hook Sukses (Android 11-14): %s", art_symbols_11_14[i]);
            return;
        }
    }

    // Coba Hook Android 15+
    const char* art_symbols_15[] = {
        "_ZN3art9hiddenapi29ShouldBlockAccessToMemberImplEPNS_9ArtMethodENS0_12AccessMethodENS0_12ActionPolicyEb",
        "_ZN3art9hiddenapi29ShouldBlockAccessToMemberImplEPNS_9ArtMethodENS0_12AccessMethodEb",
        nullptr
    };
    
    const char* libs_15[] = {"libart.so", "libartbase.so", nullptr};

    for (int l = 0; libs_15[l] != nullptr; ++l) {
        for (int s = 0; art_symbols_15[s] != nullptr; ++s) {
            void* hook = shadowhook_hook_sym_name(
                libs_15[l],
                art_symbols_15[s], 
                reinterpret_cast<void*>(proxy_ShouldBlockAccessToMember_15),
                reinterpret_cast<void**>(&orig_ShouldBlockAccessToMember_15)
            );
            if (hook != nullptr) {
                LOGI("vMeer ART: Hook Sukses (Android 15+): %s in %s", art_symbols_15[s], libs_15[l]);
                return;
            }
        }
    }

    // Fallback jika Hook Gagal (Android 15/Xiaomi Custom Namespace Protection)
    LOGW("vMeer ART: Native Hook Gagal. Menggunakan Robust JNI Fallback...");
    
    // Eksekusi Lapis 3 (Exemptions) & Lapis 2 (targetSDK) secara berurutan
    if (!ApplyExemptionsBypass(env)) {
        ForceSetTargetSdkVersion(env);
    }
}

bool InjectMirrorFramework(JNIEnv* env, jobject class_loader, const std::string& jar_path) {
    if (jar_path.empty() || class_loader == nullptr) {
        LOGE("vMeer ART: Parameter tidak valid.");
        return false;
    }

    if (access(jar_path.c_str(), R_OK) != 0) {
        LOGE("vMeer ART: File tidak dapat diakses: %s", jar_path.c_str());
        return false;
    }

    LOGI("vMeer ART: Menyuntikkan JAR: %s", jar_path.c_str());

    if (env->PushLocalFrame(32) != 0) {
        LOGE("vMeer ART: Gagal push local frame.");
        return false;
    }

    bool result = false;
    do {
        jclass loader_clazz = env->FindClass("dalvik/system/BaseDexClassLoader");
        if (!loader_clazz) break;

        jfieldID path_list_fid = env->GetFieldID(loader_clazz, "pathList", "Ldalvik/system/DexPathList;");
        if (!path_list_fid) break;

        jobject path_list_obj = env->GetObjectField(class_loader, path_list_fid);
        if (!path_list_obj) break;

        jclass path_list_clazz = env->GetObjectClass(path_list_obj);
        
        jfieldID elements_fid = env->GetFieldID(path_list_clazz, "dexElements", "[Ldalvik/system/DexPathList$Element;");
        if (!elements_fid) break;

        jobjectArray old_elements = (jobjectArray)env->GetObjectField(path_list_obj, elements_fid);
        jsize old_len = old_elements ? env->GetArrayLength(old_elements) : 0;

        jmethodID make_elements_mid = env->GetStaticMethodID(
            path_list_clazz, "makePathElements", 
            "(Ljava/util/List;Ljava/io/File;Ljava/util/List;)[Ldalvik/system/DexPathList$Element;"
        );

        if (!make_elements_mid) {
            ClearJniException(env);
            make_elements_mid = env->GetStaticMethodID(
                path_list_clazz, "makeDexElements", 
                "(Ljava/util/List;Ljava/io/File;Ljava/util/List;Ljava/lang/ClassLoader;)[Ldalvik/system/DexPathList$Element;"
            );
        }
        if (!make_elements_mid) break;

        jclass array_list_clazz = env->FindClass("java/util/ArrayList");
        jmethodID array_list_init = env->GetMethodID(array_list_clazz, "<init>", "()V");
        jmethodID array_list_add = env->GetMethodID(array_list_clazz, "add", "(Ljava/lang/Object;)Z");

        jobject files_list = env->NewObject(array_list_clazz, array_list_init);
        
        jclass file_clazz = env->FindClass("java/io/File");
        jmethodID file_init = env->GetMethodID(file_clazz, "<init>", "(Ljava/lang/String;)V");
        jstring j_path = env->NewStringUTF(jar_path.c_str());
        jobject file_obj = env->NewObject(file_clazz, file_init, j_path);
        
        env->CallBooleanMethod(files_list, array_list_add, file_obj);

        jobject suppressed_list = env->NewObject(array_list_clazz, array_list_init);

        jobjectArray new_elements = (jobjectArray)env->CallStaticObjectMethod(
            path_list_clazz, make_elements_mid, files_list, nullptr, suppressed_list
        );

        if (!new_elements || env->ExceptionCheck()) {
            ClearJniException(env);
            LOGE("vMeer ART: Gagal membuat Dex Elements.");
            break;
        }

        jsize new_len = env->GetArrayLength(new_elements);
        if (new_len == 0) {
            LOGE("vMeer ART: Elemen DEX kosong.");
            break;
        }

        jclass element_clazz = env->FindClass("dalvik/system/DexPathList$Element");
        jobjectArray combined_elements = env->NewObjectArray(old_len + new_len, element_clazz, nullptr);

        for (jsize i = 0; i < old_len; i++) {
            jobject el = env->GetObjectArrayElement(old_elements, i);
            env->SetObjectArrayElement(combined_elements, i, el);
            env->DeleteLocalRef(el);
        }

        for (jsize i = 0; i < new_len; i++) {
            jobject el = env->GetObjectArrayElement(new_elements, i);
            env->SetObjectArrayElement(combined_elements, old_len + i, el);
            env->DeleteLocalRef(el);
        }

        env->SetObjectField(path_list_obj, elements_fid, combined_elements);
        
        LOGI("vMeer ART: Injektasi Berhasil.");
        result = true;

    } while (false);

    env->PopLocalFrame(nullptr);
    return result;
}

} // namespace art
} // namespace vmeer

extern "C" {

__attribute__((visibility("default"))) void perform_mirror_injection(JNIEnv* env, jobject class_loader, const char* path) {
    if (path && env && class_loader) {
        vmeer::art::InjectMirrorFramework(env, class_loader, std::string(path));
    }
}

JNIEXPORT void JNICALL
Java_com_vmeer_io_EngineLoader_init_1art_1hook(JNIEnv* env, jclass clazz) {
    (void)clazz;
    vmeer::art::ApplyNativeHiddenApiBypass(env);
}

JNIEXPORT void JNICALL
Java_com_vmeer_io_EngineLoader_perform_1mirror_1injection(JNIEnv* env, jclass clazz, jobject class_loader, jstring path) {
    (void)clazz;
    if (!path || !class_loader) return;
    
    const char* native_path = env->GetStringUTFChars(path, nullptr);
    if (native_path) {
        vmeer::art::InjectMirrorFramework(env, class_loader, std::string(native_path));
        env->ReleaseStringUTFChars(path, native_path);
    }
}

} // extern "C"