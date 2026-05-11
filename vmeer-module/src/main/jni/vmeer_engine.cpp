#include <jni.h>
#include <android/log.h>
#include <stdint.h>
#include <dlfcn.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>

#include "shadowhook.h"
#include "vmeer_stealth.h"    // Tanpa prefix include/
#include "binder_vm.h"       // Tanpa prefix include/
#include "vmeer_system.h"    // Tanpa prefix include/
#include "egl_bridge.h"      // Tanpa prefix include/

#define LOG_TAG "vMeer_Engine"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

// Bungkus dalam extern "C" agar Linker tidak bingung
extern "C" {

bool requestNamespaceSetup(const char* pkgName) {
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) return false;

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    addr.sun_path[0] = '\0'; 
    strncpy(addr.sun_path + 1, "vmeer_daemon.cms", sizeof(addr.sun_path) - 2);

    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) == 0) {
        char cmd[512];
        snprintf(cmd, sizeof(cmd), "PREPARE_STORAGE:%s", pkgName);
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

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* res) {
    LOGI("vMeer Engine: Booting...");

    if (!requestNamespaceSetup("com.vmeer.virtual.guest")) {
        LOGI("vMeer Engine: Namespace setup failed, but continuing...");
    }

    if (shadowhook_init(SHADOWHOOK_MODE_UNIQUE, false) != 0) return JNI_ERR;

    // Panggil semua modul
    init_vmeer_stealth();
    start_binder_proxy();
    start_virtual_system_services();
    start_egl_bridge();

    return JNI_VERSION_1_6;
}

} // extern "C"
