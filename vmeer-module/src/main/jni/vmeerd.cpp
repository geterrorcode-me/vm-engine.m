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

#define LOG_TAG "vmeerd"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

class StorageSandbox {
public:
    static bool initializeIsolation() {
        // Master isolation agar host tidak "kotor" oleh mount virtual
        if (unshare(CLONE_NEWNS) != 0) {
            LOGE("vmeerd: Gagal unshare namespace: %s", strerror(errno));
            return false;
        }
        mount(NULL, "/", NULL, MS_REC | MS_PRIVATE, NULL);
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

        // 2. Fix Permission: Ini krusial!
        // Aplikasi guest harus merasa memiliki folder tersebut (UID/GID match)
        chown(virtualPath, vuid, vuid);

        // 3. Bind Mount dengan MS_REC untuk support OBB/Data subfolders
        if (mount(virtualPath, targetPath, NULL, MS_BIND | MS_REC, NULL) == 0) {
            LOGI("vmeerd: Sandbox Storage Linked: %s -> %s (UID: %d)", virtualPath, targetPath, vuid);
            return true;
        } else {
            LOGE("vmeerd: Mount failed: %s", strerror(errno));
            return false;
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

            // Parsing baru: PREPARE_STORAGE:pkg_name:vuid
            if (raw.find("PREPARE_STORAGE:") == 0) {
                size_t first_colon = raw.find(':', 16);
                std::string pkg = raw.substr(16, first_colon - 16);
                int vuid = std::stoi(raw.substr(first_colon + 1));
                
                if (StorageSandbox::prepareStorage(pkg.c_str(), vuid)) {
                    send(client_sock, "OK", 2, 0);
                } else {
                    send(client_sock, "ERR", 3, 0);
                }
            }
        }
        close(client_sock);
    }
};

int main() {
    // Memastikan daemon berjalan sebagai root untuk akses mount
    if (getuid() != 0) {
        LOGE("vmeerd must run as root!");
        return 1;
    }

    if (!StorageSandbox::initializeIsolation()) return 1;

    int server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    addr.sun_path[0] = '\0';
    strncpy(addr.sun_path + 1, "vmeer_daemon.cms", sizeof(addr.sun_path) - 2);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) return 1;
    if (listen(server_fd, 10) < 0) return 1;

    LOGI("vmeerd: High-End Broker is Ready.");

    while (true) {
        int client_sock = accept(server_fd, NULL, NULL);
        if (client_sock >= 0) {
            MessageBroker::handleCommand(client_sock);
        }
    }
    return 0;
}
