#include <sqlite3.h>
#include <string>
#include <android/log.h>
#include "vmeer_db.h"

#define LOG_TAG "vMeer_DB"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

// Gunakan pointer database yang aman
static sqlite3* g_db = nullptr;

/**
 * Fungsi internal C++ untuk mengambil data.
 * Mengembalikan std::string secara copy agar thread-safe.
 */
std::string query_android_id(const char* pkg_name) {
    if (!g_db || !pkg_name) return "default_id";

    const char* sql = "SELECT android_id FROM v_device_profile WHERE pkg_name = ?;";
    sqlite3_stmt* stmt;
    std::string result = "default_id";

    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, pkg_name, -1, SQLITE_STATIC);
        
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            const unsigned char* val = sqlite3_column_text(stmt, 0);
            if (val) {
                result = reinterpret_cast<const char*>(val);
            }
        }
        sqlite3_finalize(stmt);
    } else {
        LOGI("vMeer: [DB] Prepare failed: %s", sqlite3_errmsg(g_db));
    }
    
    return result;
}

extern "C" {

/**
 * Inisialisasi Database
 */
bool init_vmeer_database(const char* db_path) {
    if (sqlite3_open(db_path, &g_db) != SQLITE_OK) {
        LOGI("vMeer: [DB] Failed to open database at %s", db_path);
        return false;
    }

    // Aktifkan mode WAL untuk performa multithread yang lebih baik di Android
    sqlite3_exec(g_db, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);

    const char* sql = "CREATE TABLE IF NOT EXISTS v_device_profile ("
                      "pkg_name TEXT PRIMARY KEY, "
                      "android_id TEXT, "
                      "imei TEXT, "
                      "model TEXT);";
    
    char* err_msg = nullptr;
    if (sqlite3_exec(g_db, sql, nullptr, nullptr, &err_msg) != SQLITE_OK) {
        LOGI("vMeer: [DB] SQL Error: %s", err_msg);
        if (err_msg) sqlite3_free(err_msg);
        return false;
    }

    LOGI("vMeer: [DB] Persistent Storage is Ready.");
    return true;
}

/**
 * Wrapper C untuk digunakan oleh JNI atau modul luar.
 * CATATAN: Karena C tidak punya std::string, kita mengembalikan string yang 
 * dialokasikan di heap (malloc). PEMANGGIL WAJIB MELAKUKAN free().
 */
const char* get_v_android_id_c(const char* pkg_name) {
    std::string id = query_android_id(pkg_name);
    
    // Alokasikan memori baru agar aman saat dibaca oleh pemanggil
    char* out = (char*)malloc(id.length() + 1);
    if (out) {
        strcpy(out, id.c_str());
    }
    return out;
}

/**
 * Menutup database dengan benar
 */
void close_vmeer_database() {
    if (g_db) {
        sqlite3_close(g_db);
        g_db = nullptr;
    }
}

} // extern "C"
