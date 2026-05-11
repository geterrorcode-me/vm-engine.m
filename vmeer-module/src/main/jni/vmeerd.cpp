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

#define LOG_TAG "vmeerd"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// =============================================================================
// BAGIAN 1: NAMESPACE & STORAGE LOGIC (Urutan 1 & 2)
// =============================================================================

class StorageSandbox {
public:
    static bool initializeIsolation() {
        // Unshare Mount Namespace agar perubahan mount tidak terlihat oleh host
        if (unshare(CLONE_NEWNS) != 0) {
            LOGE("vmeerd: Gagal unshare namespace: %s", strerror(errno));
            return false;
        }
        // Pastikan propagasi mount diatur ke private
        mount(NULL, "/", NULL, MS_REC | MS_PRIVATE, NULL);
        LOGI("vmeerd: Global Namespace Isolation aktif.");
        return true;
    }

    static bool prepareStorage(const char* pkgName) {
        // Simulasi pembuatan jalur (Path harus disesuaikan dengan struktur folder virtual Anda)
        char virtualPath[256];
        char targetPath[256];
        
        sprintf(virtualPath, "/data/data/com.vmeer.io/virtual/%s", pkgName);
        sprintf(targetPath, "/data/data/%s", pkgName);

        // Melakukan Bind Mount (Inti dari Urutan 2)
        if (mount(virtualPath, targetPath, NULL, MS_BIND, NULL) == 0) {
            LOGI("vmeerd: Bind mount sukses untuk %s", pkgName);
            return true;
        } else {
            LOGE("vmeerd: Bind mount gagal untuk %s: %s", pkgName, strerror(errno));
            return false;
        }
    }
};

// =============================================================================
// BAGIAN 2: BROKER IPC (COMMAND HANDLER)
// =============================================================================

class MessageBroker {
public:
    static void handleCommand(int client_sock) {
        char buffer[512] = {0};
        ssize_t bytes_read = recv(client_sock, buffer, sizeof(buffer), 0);
        
        if (bytes_read > 0) {
            LOGI("vmeerd: Menerima perintah: %s", buffer);

            // Parsing perintah: PREPARE_STORAGE:com.package.name
            if (strncmp(buffer, "PREPARE_STORAGE:", 16) == 0) {
                const char* pkgName = buffer + 16;
                
                if (StorageSandbox::prepareStorage(pkgName)) {
                    send(client_sock, "OK", 2, 0);
                } else {
                    send(client_sock, "ERR", 3, 0);
                }
            }
        }
        close(client_sock);
    }
};

// =============================================================================
// BAGIAN 3: DAEMON ENTRY POINT & SERVER LOOP
// =============================================================================

int main() {
    LOGI("vmeerd: Daemon memulai siklus hidup...");

    // 1. Inisialisasi Isolasi Namespace Dasar
    if (!StorageSandbox::initializeIsolation()) return 1;

    // 2. Setup Abstract Socket Server
    int server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    
    // Abstract socket name: \0vmeer_daemon.cms
    addr.sun_path[0] = '\0';
    strncpy(addr.sun_path + 1, "vmeer_daemon.cms", sizeof(addr.sun_path) - 2);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        LOGE("vmeerd: Gagal bind socket: %s", strerror(errno));
        return 1;
    }

    if (listen(server_fd, 10) < 0) return 1;
    LOGI("vmeerd: Broker siap melayani Engine.");

    // 3. Main Loop
    while (true) {
        int client_sock = accept(server_fd, NULL, NULL);
        if (client_sock >= 0) {
            MessageBroker::handleCommand(client_sock);
        }
    }

    return 0;
}
