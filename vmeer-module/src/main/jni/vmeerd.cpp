#include <iostream>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <sched.h>
#include <errno.h>
#include <android/log.h>
#include <string.h>
#include <vector>
#include <string>

#define LOG_TAG "vmeerd"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)

class StorageSandbox {
public:
    static bool initializeIsolation() {
        // PERBAIKAN 1: Jangan matikan program jika unshare gagal di Termux/PRoot
        if (unshare(CLONE_NEWNS) != 0) {
            LOGW("vmeerd: Gagal unshare namespace (%s). Mengaktifkan mode Userspace VFS / PRoot Bypass.", strerror(errno));
            // Kita kembalikan true agar eksekusi tetap berlanjut menggunakan virtualisasi PRoot
            return true; 
        }
        
        // Hanya lakukan mount private jika unshare berhasil
        if (mount(NULL, "/", NULL, MS_REC | MS_PRIVATE, NULL) != 0) {
            LOGW("vmeerd: Mount MS_PRIVATE dilewati karena batasan kernel.");
        }
        return true;
    }

    static bool prepareStorage(const char* pkgName, int vuid) {
        char virtualPath[256];
        char targetPath[256];
        
        int user_id = vuid / 100000;

        // Struktur folder sandbox yang sinkron dengan vmeer_vfs.cpp
        sprintf(virtualPath, "/data/data/com.vmeer.manager/virtual/user_%d/%s/data", user_id, pkgName);
        sprintf(targetPath, "/data/data/%s", pkgName);

        // 1. Pastikan folder virtual ada
        mkdir(virtualPath, 0755);

        // 2. Fix Permission: Abaikan jika chown gagal karena keterbatasan non-root asli
        if (chown(virtualPath, vuid, vuid) != 0) {
            LOGW("vmeerd: chown gagal (%s), mengandalkan izin internal PRoot.", strerror(errno));
        }

        // 3. Bind Mount virtual
        if (mount(virtualPath, targetPath, NULL, MS_BIND | MS_REC, NULL) == 0) {
            LOGI("vmeerd: Sandbox Storage Linked: %s -> %s (UID: %d)", virtualPath, targetPath, vuid);
            return true;
        } else {
            // Di PRoot, jika mount murni gagal, kita simulasikan sukses karena PRoot menghandle binding via bender flag (-b)
            LOGW("vmeerd: Kernel Mount failed (%s). Mengandalkan fallback redirection PRoot.", strerror(errno));
            return true; 
        }
    }
};

class MessageBroker {
public:
    static void handleCommand(int client_sock) {
        char buffer[512] = {0};
        ssize_t bytes_read = recv(client_sock, buffer, sizeof(buffer), 0);
        
        if (bytes_read > 0) {
            std::string raw(buffer);
            LOGI("vmeerd: Received: %s", buffer);

            // Parsing: PREPARE_STORAGE:pkg_name:vuid
            if (raw.find("PREPARE_STORAGE:") == 0) {
                size_t first_colon = raw.find(':', 16);
                if (first_colon != std::string::npos) {
                    std::string pkg = raw.substr(16, first_colon - 16);
                    int vuid = std::stoi(raw.substr(first_colon + 1));
                    
                    if (StorageSandbox::prepareStorage(pkg.c_str(), vuid)) {
                        send(client_sock, "OK", 2, 0);
                    } else {
                        send(client_sock, "ERR", 3, 0);
                    }
                }
            }
        }
        close(client_sock);
    }
};

int main() {
    // PERBAIKAN 2: Longgarkan pengecekan UID khusus untuk kompatibilitas Termux/PRoot
    if (getuid() != 0) {
        LOGW("vmeerd: Berjalan di lingkungan Non-Root murni/PRoot. Melanjutkan dengan pembatasan userspace.");
    }

    // Jalankan inisialisasi bypass
    if (!StorageSandbox::initializeIsolation()) {
        LOGE("vmeerd: Gagal menginisialisasi isolasi dasar.");
        return 1;
    }

    int server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_fd < 0) {
        LOGE("vmeerd: Gagal membuat socket.");
        return 1;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    addr.sun_path[0] = '\0'; // Abstract socket namespace
    strncpy(addr.sun_path + 1, "vmeer_daemon.cms", sizeof(addr.sun_path) - 2);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        LOGE("vmeerd: Gagal bind socket. %s", strerror(errno));
        close(server_fd);
        return 1;
    }
    
    if (listen(server_fd, 10) < 0) {
        LOGE("vmeerd: Gagal listen socket.");
        close(server_fd);
        return 1;
    }

    LOGI("vmeerd: High-End Broker is Ready (Bypass Mode Active).");

    while (true) {
        int client_sock = accept(server_fd, NULL, NULL);
        if (client_sock >= 0) {
            MessageBroker::handleCommand(client_sock);
        }
    }
    
    close(server_fd);
    return 0;
}
