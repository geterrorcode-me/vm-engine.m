#ifndef VMEER_AIDL_TABLE_H
#define VMEER_AIDL_TABLE_H

#include <sys/system_properties.h>
#include <stdlib.h>

// Kode transaksi untuk IActivityManager (Base: FIRST_CALL_TRANSACTION)
struct AMS_Codes {
    uint32_t GET_RUNNING_APP_PROCESSES;
    uint32_t GET_RECENT_TASKS;
    uint32_t GET_SERVICES;
};

// Singleton untuk mendapatkan kode berdasarkan versi Android
static AMS_Codes get_ams_codes() {
    char sdk_ver[PROP_VALUE_MAX];
    __system_property_get("ro.build.version.sdk", sdk_ver);
    int api_level = atoi(sdk_ver);

    if (api_level >= 34) { // Android 14
        return {21, 22, 30}; // Contoh mapping (angka fiktif, harus disesuaikan dump)
    } 
    // Fallback Android 13
    return {20, 21, 29};
}

#endif
