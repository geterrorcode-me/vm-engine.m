#include <jni.h>
#include <unistd.h>
#include <sys/wait.h>
#include <android/log.h>
#include <string>
#include <vector>

#define LOG_TAG "vMeer_Zygote"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// =============================================================================
// BAGIAN 1: PROCESS FORKING LOGIC (The Birth)
// =============================================================================

class PseudoZygote {
public:
    /**
     * Melakukan fork untuk melahirkan aplikasi virtual baru.
     * Proses anak akan mewarisi Namespace dari Pseudo Zygote ini.
     */
    static pid_t forkAndSpecialize(const char* packageName, const char* entryPoint) {
        pid_t pid = fork();

        if (pid == 0) {
            // --- PROSES ANAK (GUEST APP) ---
            LOGI("vMeer: Anak proses lahir untuk %s", packageName);
            
            // 1. Masuk ke spesifikasi aplikasi (Urutan 2 & 3 otomatis terbawa)
            specializeProcess(packageName, entryPoint);
            
            // 2. Tidak boleh kembali ke loop Zygote
            _exit(0);
        } else if (pid > 0) {
            // --- PROSES INDUK (ZYGOTE) ---
            LOGI("vMeer: Zygote memantau PID %d", pid);
            return pid;
        } else {
            LOGE("vMeer: Gagal melakukan fork!");
            return -1;
        }
    }

private:
    static void specializeProcess(const char* pkg, const char* entry) {
        // Di sini kita memanggil JNI_OnLoad milik engine secara internal
        // atau melakukan execve() jika kita menggunakan binary wrapper.
        LOGI("vMeer: Menspesialisasi proses ke %s...", entry);
    }
};

// =============================================================================
// BAGIAN 2: RUNTIME INTEGRATION (ART Hooking)
// =============================================================================

/**
 * Kita mencegat fungsi forkAndSpecialize milik com.android.internal.os.Zygote
 * agar dialihkan ke PseudoZygote kita.
 */
extern "C" JNIEXPORT jint JNICALL Java_com_vmeer_io_VMeerZygote_nativeFork(JNIEnv* env, jclass clazz, jstring pkg) {
    const char* nativePkg = env->GetStringUTFChars(pkg, nullptr);
    
    pid_t newPid = PseudoZygote::forkAndSpecialize(nativePkg, "android.app.ActivityThread");
    
    env->ReleaseStringUTFChars(pkg, nativePkg);
    return (jint)newPid;
}

// =============================================================================
// BAGIAN 3: PROCESS REAPER (Stability)
// =============================================================================

void setup_sigchld_handler() {
    // Menangani 'zombie process' agar resource tetap ijo dan tidak leak.
    struct sigaction sa;
    sa.sa_handler = [](int sig) {
        while (waitpid(-1, nullptr, WNOHANG) > 0);
    };
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    sigaction(SIGCHLD, &sa, nullptr);
}
