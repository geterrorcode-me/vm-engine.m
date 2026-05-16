#include <jni.h>
#include <string>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <errno.h>
#include <android/log.h>

#define LOG_TAG "vMeer_JNI"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

extern "C"
JNIEXPORT jstring JNICALL
Java_com_vmeer_manager_core_VMeerEngine_sendDaemonCommand(JNIEnv *env, jobject thiz, jstring command_str) {
    // 1. Konversi jstring Java ke std::string C++
    const char *native_cmd = env->GetStringUTFChars(command_str, nullptr);
    std::string cmd(native_cmd);
    env->ReleaseStringUTFChars(command_str, native_cmd);

    // 2. Setup Abstract Unix Domain Socket Client
    int client_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (client_fd < 0) {
        LOGE("JNI: Gagal membuat socket client: %s", strerror(errno));
        return env->NewStringUTF("ERR_SOCKET_CREATION_FAILED");
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    addr.sun_path[0] = '\0'; // Penanda Abstract Namespace Socket
    strncpy(addr.sun_path + 1, "vmeer_daemon.cms", sizeof(addr.sun_path) - 2);

    // 3. Hubungkan ke Daemon vmeerd
    if (connect(client_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        LOGE("JNI: Gagal terhubung ke vmeerd daemon: %s", strerror(errno));
        close(client_fd);
        return env->NewStringUTF("ERR_DAEMON_NOT_RUNNING");
    }

    // 4. Kirim Perintah (PREPARE_STORAGE:pkg_name:vuid)
    if (send(client_fd, cmd.c_str(), cmd.length(), 0) < 0) {
        LOGE("JNI: Gagal mengirim data ke daemon.");
        close(client_fd);
        return env->NewStringUTF("ERR_SEND_FAILED");
    }

    // 5. Baca Respons Balasan dari Daemon (OK / ERR)
    char buffer[32] = {0};
    ssize_t bytes_read = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
    close(client_fd);

    if (bytes_read > 0) {
        return env->NewStringUTF(buffer); // Mengembalikan "OK" atau "ERR" ke Java
    }

    return env->NewStringUTF("ERR_NO_RESPONSE");
}
