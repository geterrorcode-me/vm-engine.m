#ifndef VMEER_DB_H
#define VMEER_DB_H

#ifdef __cplusplus
extern "C" {
#endif

bool init_vmeer_database(const char* db_path);
const char* get_v_android_id_c(const char* pkg_name);

#ifdef __cplusplus
}
#endif

#endif
