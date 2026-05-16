#include <jni.h>
#include <android/log.h>
#include <stdint.h>
#include <dlfcn.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>
#include <string>

// --- System & Memory Handling ---
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/userfaultfd.h>
#include <fcntl.h>
#include <pthread.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/syscall.h>

// --- Core Engine & Stealth ---
#include "shadowhook.h"
#include "vmeer_stealth.h"

// --- Ecosystem Modules ---
#include "include/vmeer_helper.h"
#include "include/vmeer_zygote.h"
#include "include/vmeer_context.h"
#include "include/vmeer_vfs.h"
#include "include/vmeer_system.h" 

// --- Logic Modules ---
#include "binder_engine.h"
#include "sensor_engine.h"
#include "vmeer_pms.h"
#include "egl_bridge.h"

// --- FUSE & ZSTD Headers ---
#include <fuse.h>
#include <zstd.h>

#define LOG_TAG "vMeer_Engine"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// ====================================================================
// NDK COMPATIBILITY LAYER: Hardcoded ioctl untuk NDK jadul (Bypass UFFD_IOC_MAGIC)
// ====================================================================
#ifndef UFFDIO_MOVE
#define UFFDIO_MOVE_MODE_ALLOW_SRC_HOLES ((__u64)1<<0)

struct uffdio_move {
    __u64 dst;
    __u64 src;
    __u64 len;
    __u64 mode;
    __s64 move;
};

// Nilai hex evaluasi dari _IOWR(0xAA, 0x05, struct uffdio_move)
#define UFFDIO_MOVE 0xC028AA05
#endif
// ====================================================================

// --- Fungsi eksternal dari modul lain ---
extern "C" void init_art_hook(JNIEnv* env);
extern "C" void perform_mirror_injection(JNIEnv* env, jobject class_loader, const char* path);
extern "C" void syncJavaProperties(JNIEnv* env);

// Struktur data untuk melemparkan konteks ke thread fault handler
struct FaultHandlerArgs {
    int uffd_fd;
    uintptr_t uffd_vma_start;
    size_t uffd_vma_len;
    int rom_fd;
};

// ====================================================================
// INTERNAL HANDLER (C++ Linkage)
// ====================================================================
static void* UserfaultfdHandlerThread(void* arg) {
    auto* args = static_cast<FaultHandlerArgs*>(arg);
    int uffd = args->uffd_fd;
    size_t page_size = sysconf(_SC_PAGESIZE);
    
    auto* page_buffer = static_cast<char*>(mmap(nullptr, page_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
    if (page_buffer == MAP_FAILED) {
        LOGE("vMeer_UFFD: FATAL - Gagal mengalokasikan page_buffer di user space!");
        close(args->rom_fd);
        delete args;
        return nullptr;
    }

    LOGI("vMeer_Engine: Thread alokasi UFFD subsistem memory manager siap.");

    while (true) {
        struct uffd_msg msg;
        ssize_t nread = read(uffd, &msg, sizeof(msg));
        
        if (nread == 0) {
            LOGI("vMeer_UFFD: Koneksi monitor userfaultfd diputus sistem.");
            break;
        }
        if (nread == -1) {
            if (errno == EAGAIN) continue;
            LOGE("vMeer_UFFD: Error pembacaan pesan descriptor: %s", strerror(errno));
            break;
        }

        if (msg.event == UFFD_EVENT_PAGEFAULT) {
            uintptr_t fault_address = msg.arg.pagefault.address;
            uintptr_t fault_page_aligned = fault_address & ~(page_size - 1);
            
            off_t file_offset = fault_page_aligned - args->uffd_vma_start;

            memset(page_buffer, 0, page_size);
            if (pread(args->rom_fd, page_buffer, page_size, file_offset) < 0) {
                LOGE("vMeer_UFFD: Gagal membaca partisi ROM pada offset %lld", (long long)file_offset);
            }

            // [LAPIS 1]: UFFDIO_MOVE
            struct uffdio_move uffdio_move_args;
            uffdio_move_args.dst = fault_page_aligned;
            uffdio_move_args.src = reinterpret_cast<uintptr_t>(page_buffer);
            uffdio_move_args.len = page_size;
            uffdio_move_args.mode = UFFDIO_MOVE_MODE_ALLOW_SRC_HOLES;

            if (ioctl(uffd, UFFDIO_MOVE, &uffdio_move_args) == -1) {
                LOGW("vMeer_UFFD: UFFDIO_MOVE ditolak kernel Android 15 (%s). Mencoba Lapis 2...", strerror(errno));

                // [LAPIS 2]: UFFDIO_COPY
                struct uffdio_copy uffdio_copy_args;
                uffdio_copy_args.dst = fault_page_aligned;
                uffdio_copy_args.src = reinterpret_cast<uintptr_t>(page_buffer);
                uffdio_copy_args.len = page_size;
                uffdio_copy_args.mode = 0;

                if (ioctl(uffd, UFFDIO_COPY, &uffdio_copy_args) == -1) {
                    LOGE("vMeer_UFFD: UFFDIO_COPY diblokir kernel/SELinux (%s)!", strerror(errno));
                    LOGW("vMeer_UFFD: Mengaktifkan EMERGENCY FALLBACK Lapis 3 (User-space Memcpy)...");

                    // [LAPIS 3]: Manual Memcpy + Forced Wake
                    mprotect(reinterpret_cast<void*>(fault_page_aligned), page_size, PROT_READ | PROT_WRITE);
                    memcpy(reinterpret_cast<void*>(fault_page_aligned), page_buffer, page_size);
                    
                    struct uffdio_range uffdio_wake_args;
                    uffdio_wake_args.start = fault_page_aligned;
                    uffdio_wake_args.len = page_size;
                    ioctl(uffd, UFFDIO_WAKE, &uffdio_wake_args);
                } else {
                    LOGI("vMeer_UFFD: Lapis 2 SUKSES - Memori disuntik via UFFDIO_COPY.");
                }
            }
        }
    }

    munmap(page_buffer, page_size);
    close(args->rom_fd);
    delete args;
    return nullptr;
}

// Helper internal untuk daemon namespace
static bool requestNamespaceSetup(const char* pkgName, int vuid) {
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) return false;

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    addr.sun_path[0] = '\0'; 
    strncpy(addr.sun_path + 1, "vmeer_daemon.cms", sizeof(addr.sun_path) - 2);

    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) == 0) {
        char cmd[512];
        snprintf(cmd, sizeof(cmd), "PREPARE_STORAGE:%s:%d", pkgName, vuid);
        
        if (send(sock, cmd, strlen(cmd), 0) > 0) {
            char response[16] = {0};
            if (recv(sock, response, sizeof(response), 0) > 0) {
                if (strcmp(response, "OK") == 0) {
                    close(sock);
                    return true;
                }
            }
        }
    }
    close(sock);
    return false;
}

// ====================================================================
// EXTERN "C" LINKAGE: Khusus untuk JNI Eksport agar terbaca oleh Java
// ====================================================================
extern "C" {

JNIEXPORT jboolean JNICALL
Java_com_vmeer_io_VMeerEngine_isInitialized(JNIEnv *env, jclass clazz) {
    (void)env; (void)clazz;
    return JNI_TRUE;
}

[[maybe_unused]] static void* do_hook(const char* lib, const char* sym, void* proxy, void** orig) {
    void* stub = shadowhook_hook_sym_name(lib, sym, proxy, orig);
    if (!stub) {
        int err_num = shadowhook_get_errno();
        LOGE("Hook Failed: %s:%s | %s", lib, sym, shadowhook_to_errmsg(err_num));
    }
    return stub;
}

JNIEXPORT jstring JNICALL
Java_com_vmeer_io_VMeerEngine_sendDaemonCommand(JNIEnv *env, jobject thiz, jstring command_str) {
    (void)thiz;
    const char *native_cmd = env->GetStringUTFChars(command_str, nullptr);
    std::string cmd(native_cmd);
    env->ReleaseStringUTFChars(command_str, native_cmd);

    int client_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (client_fd < 0) {
        LOGE("vMeer JNI: Gagal membuat socket client.");
        return env->NewStringUTF("ERR_SOCKET_FAILED");
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    addr.sun_path[0] = '\0';
    strncpy(addr.sun_path + 1, "vmeer_daemon.cms", sizeof(addr.sun_path) - 2);

    if (connect(client_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        LOGE("vMeer JNI: Koneksi ke daemon ditolak (vmeerd mati).");
        close(client_fd);
        return env->NewStringUTF("ERR_DAEMON_DEAD");
    }

    send(client_fd, cmd.c_str(), cmd.length(), 0);

    char buffer[32] = {0};
    ssize_t bytes_read = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
    close(client_fd);

    if (bytes_read > 0) {
        return env->NewStringUTF(buffer);
    }
    return env->NewStringUTF("ERR_NO_RESP");
}

JNIEXPORT jboolean JNICALL
Java_com_vmeer_io_VMeerEngine_prepareStorageSandbox(JNIEnv *env, jclass clazz, jstring pkgName, jint vUid) {
    (void)clazz;
    const char *native_pkg = env->GetStringUTFChars(pkgName, nullptr);
    bool result = requestNamespaceSetup(native_pkg, vUid);
    env->ReleaseStringUTFChars(pkgName, native_pkg);
    return result ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL
Java_com_vmeer_io_VMeerEngine_setupVM(JNIEnv *env, jclass clazz, jobject context, jstring mirrorPath, jint vUid) {
    (void)clazz;
    LOGI("====================================================");
    LOGI("   vMeer OS: Configuring High-End Virtual Machine   ");
    LOGI("====================================================");

    const char *path = env->GetStringUTFChars(mirrorPath, nullptr);
    
    auto& vContext = vmeer::RuntimeContext::Get();
    vContext.SetVirtualUid(vUid);
    vContext.SetMirrorPath(path);

    requestNamespaceSetup("com.vmeer.guest", vUid);

    int rom_fd = open(path, O_RDONLY);
    if (rom_fd >= 0) {
        struct stat st;
        if (fstat(rom_fd, &st) == 0) {
            size_t rom_size = st.st_size;
            
            void* vma_addr = mmap(nullptr, rom_size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            if (vma_addr != MAP_FAILED) {
                
                int uffd = syscall(__NR_userfaultfd, O_CLOEXEC | O_NONBLOCK);
                if (uffd == -1) {
                    LOGW("vMeer_Engine: Kernel memblokir syscall UFFD. Mengalihkan ke Full Static Map...");
                    mprotect(vma_addr, rom_size, PROT_READ | PROT_WRITE);
                    mmap(vma_addr, rom_size, PROT_READ, MAP_PRIVATE | MAP_FIXED, rom_fd, 0);
                    close(rom_fd);
                } else {
                    struct uffdio_api uffdio_api_args;
                    uffdio_api_args.api = UFFD_API;
                    uffdio_api_args.features = 0;
                    
                    if (ioctl(uffd, UFFDIO_API, &uffdio_api_args) != -1) {
                        struct uffdio_register uffdio_register_args;
                        uffdio_register_args.range.start = reinterpret_cast<uintptr_t>(vma_addr);
                        uffdio_register_args.range.len = rom_size;
                        uffdio_register_args.mode = UFFDIO_REGISTER_MODE_MISSING;
                        
                        if (ioctl(uffd, UFFDIO_REGISTER, &uffdio_register_args) != -1) {
                            auto* thread_args = new FaultHandlerArgs();
                            thread_args->uffd_fd = uffd;
                            thread_args->uffd_vma_start = reinterpret_cast<uintptr_t>(vma_addr);
                            thread_args->uffd_vma_len = rom_size;
                            thread_args->rom_fd = rom_fd;

                            pthread_t thr;
                            if (pthread_create(&thr, nullptr, UserfaultfdHandlerThread, thread_args) == 0) {
                                pthread_detach(thr);
                                LOGI("vMeer_Engine: Virtual Memory Management Lapis 3 Sukses Terpasang.");
                            } else {
                                delete thread_args;
                                close(uffd);
                                close(rom_fd);
                            }
                        } else {
                            close(uffd);
                            close(rom_fd);
                        }
                    } else {
                        close(uffd);
                        close(rom_fd);
                    }
                }
            } else {
                close(rom_fd);
            }
        } else {
            close(rom_fd);
        }
    } else {
        LOGE("vMeer_Engine: Gagal membuka file ROM untuk inisialisasi proteksi kernel: %s", strerror(errno));
    }

    init_art_hook(env);
    
    jclass context_clazz = env->GetObjectClass(context);
    jmethodID get_class_loader_mid = env->GetMethodID(context_clazz, "getClassLoader", "()Ljava/lang/ClassLoader;");
    jobject class_loader = env->CallObjectMethod(context, get_class_loader_mid);

    perform_mirror_injection(env, class_loader, path);

    syncJavaProperties(env);

    env->ReleaseStringUTFChars(mirrorPath, path);
    LOGI("vMeer Engine: VM Setup for vUID %d is LIVE.", vUid);
}

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* res) {
    (void)res;
    JNIEnv* env;
    if (vm->GetEnv((void**)&env, JNI_VERSION_1_6) != JNI_OK) return JNI_ERR;

    LOGI("vMeer Engine: Booting Core Systems...");

    init_vmeer_stealth();
    
    int sh_status = shadowhook_init(SHADOWHOOK_MODE_SHARED, false);
    if (sh_status != 0) {
        LOGW("WARNING: ShadowHook gagal inisialisasi (Code: %d). Berjalan dalam Fallback Mode.", sh_status);
    } else {
        LOGI("vMeer: ShadowHook Shared Engine successfully engaged.");
    }

    if (sh_status == 0) {
        vmeer::helper::ConnectSharedState();
        vmeer::zygote::HookForkAndSpecialize();
        start_binder_proxy();
        vmeer::binder::InitHooks();
        start_virtual_system_services(); 
        start_egl_bridge();              
        vmeer::sensor::InitHooks();      
        vmeer::vfs::StartVFSEngine();
    }

    LOGI("vMeer Engine: Status READY - Engine v1.0.0-STABLE");
    return JNI_VERSION_1_6;
}

JNIEXPORT void JNICALL JNI_OnUnload(JavaVM* vm, void* reserved) {
    (void)vm; (void)reserved;
    LOGI("vMeer Engine: Engine Detached.");
}

} // extern "C"
