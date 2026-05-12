#include <sqlite3.h>
#include <string>
#include <android/log.h>
#include "vmeer_db.h"

#define LOG_TAG "vMeer_DB"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

static sqlite3* db = nullptr;

// Fungsi Helper C++ (Taruh di luar extern "C")
std::string get_v_android_id_cpp(const char* pkg_name) {
    const char* sql = "SELECT android_id FROM v_device_profile WHERE pkg_name = ?;";
    sqlite3_stmt* stmt;
    std::string result = "default_id";

    if (db && sqlite3_prepare_v2(db, sql, -1, &stmt, 0) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, pkg_name, -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            const char* val = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            if(val) result = val;
        }
        sqlite3_finalize(stmt);
    }
    return result;
}

extern "C" {

bool init_vmeer_database(const char* db_path) {
    if (sqlite3_open(db_path, &db) != SQLITE_OK) {
        LOGI("vMeer: [DB] Failed to open database at %s", db_path);
        return false;
    }

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

// Wrapper untuk C jika dibutuhkan oleh modul lain
const char* get_v_android_id_c(const char* pkg_name) {
    static std::string temp_id;
    temp_id = get_v_android_id_cpp(pkg_name);
    return temp_id.c_str();
}

}
