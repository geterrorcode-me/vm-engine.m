#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/syscall.h>
#include <sys/mman.h>
#include <string.h>
#include <android/log.h>

#define TAG "vMeer_Daemon_Core"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

// Alamat Abstract Socket TIOSeccomp Bridge (\0 di depan wajib agar tidak membuat file fisik)
const char* VMEER_SOCKET_NAME = "\0vmeer_tiospace_socket";
int global_virtual_binder_fd = -1;

// Mengalokasikan virtual device binder di RAM anonim (Bypass SELinux untrusted_app)
void setup_memfd_binder() {
    if (global_virtual_binder_fd == -1) {
        // MFD_CLOEXEC memastikan descriptor ini terkunci rapat di dalam runtime daemon
        global_virtual_binder_fd = syscall(__NR_memfd_create, "vmeer_v_binder", MFD_CLOEXEC);
        if (global_virtual_binder_fd < 0) {
            LOGE("[!] Daemon: Gagal alokasi memfd_create untuk virtual binder!");
            return;
        }
        // Potong ukuran memori sebesar 1MB (Ukuran standar transaksi IPC Binder Android)
        ftruncate(global_virtual_binder_fd, 1024 * 1024);
        LOGI("[+] Daemon: MemFD Virtual Binder sukses terkunci di RAM.");
    }
}

// Menghidupkan Abstract Socket Server pendengar instruksi dari Host Java
void run_vmeerd_socket_server() {
    int server_fd = socket(AF_LOCAL, SOCK_STREAM, 0);
    if (server_fd < 0) {
        LOGE("[!] Daemon: Gagal menginisialisasi AF_LOCAL socket.");
        return;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_LOCAL;
    
    // Copy 21 karakter nama socket (termasuk null-byte di awal)
    memcpy(addr.sun_path, VMEER_SOCKET_NAME, 21);

    // Bind socket ke level kernel tanpa menyentuh storage fisik
    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr.sun_family) + 21) < 0) {
        LOGE("[!] Daemon: Gagal bind Abstract Socket. Port kemungkinan tersumbat.");
        close(server_fd);
        return;
    }

    listen(server_fd, 5);
    LOGI("[===] vMeerd Engine Daemon ACTIVE: Menunggu komando dari Host...");

    // Loop abadi di background thread untuk melayani IPC request
    while (true) {
        int client_fd = accept(server_fd, nullptr, nullptr);
        if (client_fd >= 0) {
            char rx_buffer[128] = {0};
            read(client_fd, rx_buffer, sizeof(rx_buffer) - 1);
            
            LOGI("[IPC] Perintah masuk: %s", rx_buffer);

            // Jika menerima sinyal PREPARE_STORAGE dari VMeerEngine Java, siapkan sandbox memfd
            if (strncmp(rx_buffer, "PREPARE_STORAGE", 15) == 0) {
                setup_memfd_binder();
                
                // Kirim balik status "OK" sebagai konfirmasi jabat tangan sukses
                write(client_fd, "OK", 2);
            }
            close(client_fd);
        }
    }
}

// Fungsi utama/main entry-point bawaan daemon vMeer yang asli
int main(int argc, char** argv) {
    LOGI("[*] vmeerd (TIOSeccomp Enhanced) diluncurkan.");
    
    // TODO: Jalankan fungsi bawaan vmeer_main_init() asli jika ada
    
    // Jalankan server socket interseptor kita
    run_vmeerd_socket_server();
    
    return 0;
}
