#include "include/sqlite3.h"
#include <string>
#include <cstring> // Untuk strcpy
#include <cstdlib> // Untuk malloc
#include <android/log.h>
#include "vmeer_db.h"

#define LOG_TAG "vMeer_DB"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

static sqlite3* g_db = nullptr;

// Internal Helper
std::string query_android_id(const char* pkg_name) {
    if (!g_db || !pkg_name) return "default_id";

    const char* sql = "SELECT android_id FROM v_device_profile WHERE pkg_name = ?;";
    sqlite3_stmt* stmt;
    std::string result = "default_id";

    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, pkg_name, -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            const unsigned char* val = sqlite3_column_text(stmt, 0);
            if (val) result = reinterpret_cast<const char*>(val);
        }
        sqlite3_finalize(stmt);
    }
    return result;
}

extern "C" {

bool init_vmeer_database(const char* db_path) {
    if (sqlite3_open(db_path, &g_db) != SQLITE_OK) {
        return false;
    }
    
    const char* sql = "CREATE TABLE IF NOT EXISTS v_device_profile ("
                      "pkg_name TEXT PRIMARY KEY, android_id TEXT, imei TEXT, model TEXT);";
    
    char* err_msg = nullptr;
    if (sqlite3_exec(g_db, sql, nullptr, nullptr, &err_msg) != SQLITE_OK) {
        if (err_msg) sqlite3_free(err_msg);
        return false;
    }
    return true;
}

const char* get_v_android_id_c(const char* pkg_name) {
    std::string id = query_android_id(pkg_name);
    char* out = (char*)malloc(id.length() + 1);
    if (out) strcpy(out, id.c_str());
    return out;
}

}
