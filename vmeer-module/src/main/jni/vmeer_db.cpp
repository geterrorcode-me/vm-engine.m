#include <sqlite3.h>
#include <string>
#include <android/log.h>
#include "vmeer_db.h"

#define LOG_TAG "vMeer_DB"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

static sqlite3* db = nullptr;

extern "C" {

bool init_vmeer_database(const char* db_path) {
    if (sqlite3_open(db_path, &db) != SQLITE_OK) {
        LOGI("vMeer: [DB] Failed to open database at %s", db_path);
        return false;
    }

    // Buat tabel profil jika belum ada
    const char* sql = "CREATE TABLE IF NOT EXISTS v_device_profile ("
                      "pkg_name TEXT PRIMARY KEY, "
                      "android_id TEXT, "
                      "imei TEXT, "
                      "model TEXT);";
    
    char* err_msg = nullptr;
    if (sqlite3_exec(db, sql, 0, 0, &err_msg) != SQLITE_OK) {
        LOGI("vMeer: [DB] SQL Error: %s", err_msg);
        sqlite3_free(err_msg);
        return false;
    }

    LOGI("vMeer: [DB] Persistent Storage is Ready.");
    return true;
}

/**
 * Mengambil Android ID virtual dari database berdasarkan nama paket.
 */
std::string get_v_android_id(const char* pkg_name) {
    const char* sql = "SELECT android_id FROM v_device_profile WHERE pkg_name = ?;";
    sqlite3_stmt* stmt;
    std::string result = "default_id";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, pkg_name, -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            result = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        }
        sqlite3_finalize(stmt);
    }
    return result;
}

}
