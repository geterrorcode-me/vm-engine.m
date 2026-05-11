#include <iostream>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <sched.h>
#include <errno.h>
#include <android/log.h>

#define LOG_TAG "vmeerd"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

class VMeerDaemon {
public:
    void start() {
        LOGI("vMeer Daemon: Memulai inisialisasi...");
        
        // Urutan 1: Bereskan Namespace sebelum Zygote lahir
        if (unshare(CLONE_NEWNS) != 0) {
            LOGE("Gagal unshare namespace: %s", strerror(errno));
            return;
        }

        // Urutan 2: Buat Socket Broker
        listenForCommands();
    }

private:
    void setupStorageSandbox(const char* virtualPath, const char* targetPath) {
        // Melakukan Bind Mount agar folder virtual terlihat asli oleh aplikasi
        if (mount(virtualPath, targetPath, NULL, MS_BIND, NULL) == 0) {
            mount(NULL, targetPath, NULL, MS_PRIVATE, NULL);
            LOGI("Storage Sandbox Aktif: %s -> %s", virtualPath, targetPath);
        } else {
            LOGE("Mount Gagal: %s", strerror(errno));
        }
    }

    void listenForCommands() {
        int server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
        struct sockaddr_un address;
        address.sun_family = AF_UNIX;
        strcpy(address.sun_path, "/dev/vmeer_daemon.sock");
        
        unlink(address.sun_path);
        if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
            LOGE("Bind socket gagal!");
            return;
        }

        listen(server_fd, 5);
        LOGI("Daemon siap di /dev/vmeer_daemon.sock");

        while (true) {
            int client_sock = accept(server_fd, NULL, NULL);
            char buffer[256] = {0};
            read(client_sock, buffer, sizeof(buffer));
            
            // Logika Command: PREPARE_STORAGE|/path/virtual|/path/target
            if (strncmp(buffer, "PREPARE_STORAGE", 15) == 0) {
                // Parsing sederhana (asumsi implementasi strtok)
                // setupStorageSandbox(vPath, tPath);
            }
            close(client_sock);
        }
    }
};

int main() {
    VMeerDaemon daemon;
    daemon.start();
    return 0;
}
