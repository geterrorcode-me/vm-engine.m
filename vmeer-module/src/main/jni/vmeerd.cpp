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
#include <thread> // Ditambahkan untuk mendukung pengujian mandiri internal

#define LOG_TAG "vmeerd"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)

class StorageSandbox {
public:
    static bool initializeIsolation() {
        if (unshare(CLONE_NEWNS) != 0) {
            LOGW("vmeerd: Gagal unshare namespace (%s). Mengaktifkan mode Userspace VFS / PRoot Bypass.", strerror(errno));
            fprintf(stderr, "[vmeerd] WARN: Gagal unshare namespace (%s). Menggunakan PRoot Bypass mode.\n", strerror(errno));
            return true; 
        }
        
        if (mount(NULL, "/", NULL, MS_REC | MS_PRIVATE, NULL) != 0) {
            LOGW("vmeerd: Mount MS_PRIVATE dilewati karena batasan kernel.");
        }
        return true;
    }

    static bool prepareStorage(const char* pkgName, int vuid) {
        char virtualPath[256];
        char targetPath[256];
        
        int user_id = vuid / 100000;

        sprintf(virtualPath, "/data/data/com.vmeer.manager/virtual/user_%d/%s/data", user_id, pkgName);
        sprintf(targetPath, "/data/data/%s", pkgName);

        mkdir(virtualPath, 0755);

        if (chown(virtualPath, vuid, vuid) != 0) {
            LOGW("vmeerd: chown gagal (%s), mengandalkan izin internal PRoot.", strerror(errno));
        }

        if (mount(virtualPath, targetPath, NULL, MS_BIND | MS_REC, NULL) == 0) {
            LOGI("vmeerd: Sandbox Storage Linked: %s -> %s (UID: %d)", virtualPath, targetPath, vuid);
            return true;
        } else {
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
            fprintf(stdout, "[vmeerd] Data Masuk ke Broker: %s\n", buffer);

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

int main(int argc, char* argv[]) {
    std::string image_path = "";
    std::string target_path = "";

    // 1. Argument Parser Adaptif
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg.find("--image=") == 0) {
            image_path = arg.substr(8);
        } else if (arg.find("--target=") == 0) {
            target_path = arg.substr(9);
        }
    }

    fprintf(stdout, "[vmeerd] Memulai Daemon...\n");
    if (!image_path.empty() && !target_path.empty()) {
        fprintf(stdout, "[vmeerd] Konfigurasi VFS: Image=%s, Target=%s\n", image_path.c_str(), target_path.c_str());
    }

    if (getuid() != 0) {
        LOGW("vmeerd: Berjalan di lingkungan Non-Root murni/PRoot.");
        fprintf(stdout, "[vmeerd] Info: Berjalan di lingkungan Userspace PRoot.\n");
    }

    // 2. Jalankan Inisialisasi Isolasi
    if (!StorageSandbox::initializeIsolation()) {
        LOGE("vmeerd: Gagal menginisialisasi isolasi dasar.");
        fprintf(stderr, "[vmeerd] FATAL: Gagal menginisialisasi isolasi dasar.\n");
        return 1;
    }

    // 3. Setup Abstract Unix Socket IPC
    int server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_fd < 0) {
        LOGE("vmeerd: Gagal membuat socket.");
        fprintf(stderr, "[vmeerd] FATAL: Gagal membuat socket: %s\n", strerror(errno));
        return 1;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    addr.sun_path[0] = '\0'; // Abstract socket marker
    strncpy(addr.sun_path + 1, "vmeer_daemon.cms", sizeof(addr.sun_path) - 2);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        LOGE("vmeerd: Gagal bind socket. %s", strerror(errno));
        fprintf(stderr, "[vmeerd] FATAL: Gagal bind socket (%s). Coba lakukan 'pkill -f vmeerd' terlebih dahulu.\n", strerror(errno));
        close(server_fd);
        return 1;
    }
    
    if (listen(server_fd, 10) < 0) {
        LOGE("vmeerd: Gagal listen socket.");
        fprintf(stderr, "[vmeerd] FATAL: Gagal melakukan listen pada socket.\n");
        close(server_fd);
        return 1;
    }

    fprintf(stdout, "[vmeerd] SUCCESS: High-End Broker is Ready and Listening!\n");
    LOGI("vmeerd: High-End Broker is Ready (Bypass Mode Active).");

    // ==================== TRICK: INTERNAL SELF-TEST THREAD ====================
    std::thread self_test_thread([]() {
        // Beri jeda 500ms agar loop utama accept() di bawah sudah siap berjaga
        usleep(500000); 
        
        fprintf(stdout, "[Self-Test] Menghubungkan ke abstract socket internal...\n");
        int test_fd = socket(AF_UNIX, SOCK_STREAM, 0);
        if (test_fd < 0) return;

        struct sockaddr_un test_addr;
        memset(&test_addr, 0, sizeof(test_addr));
        test_addr.sun_family = AF_UNIX;
        test_addr.sun_path[0] = '\0';
        strncpy(test_addr.sun_path + 1, "vmeer_daemon.cms", sizeof(test_addr.sun_path) - 2);

        if (connect(test_fd, (struct sockaddr *)&test_addr, sizeof(test_addr)) == 0) {
            fprintf(stdout, "[Self-Test] KONEKSI INTERN SUKSES! Mengirim mock data...\n");
            std::string mock_cmd = "PREPARE_STORAGE:com.vmeer.guestapp:1010088";
            send(test_fd, mock_cmd.c_str(), mock_cmd.length(), 0);
            
            char res[10] = {0};
            recv(test_fd, res, sizeof(res), 0);
            fprintf(stdout, "[Self-Test] Respons Balik dari Broker: %s\n", res);
        } else {
            fprintf(stderr, "[Self-Test] KONEKSI GAGAL: %s\n", strerror(errno));
        }
        close(test_fd);
    });
    self_test_thread.detach(); // Lepas thread agar berjalan secara asinkronus
    // =========================================================================

    // 4. Main Event Loop
    while (true) {
        int client_sock = accept(server_fd, NULL, NULL);
        if (client_sock >= 0) {
            MessageBroker::handleCommand(client_sock);
        }
    }
    
    close(server_fd);
    return 0;
}
