cmake_minimum_required(VERSION 3.22.1)
project(vmeer_engine_modular)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(JNI_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(SHADOWHOOK_DIR ${JNI_SRC_DIR}/3rdparty/shadowhook)
# Path ke folder libs sesuai arsitektur (arm64-v8a)
set(VMEER_LIBS_DIR ${JNI_SRC_DIR}/libs/${ANDROID_ABI})

# ==========================================================
# 1. DEFINISI LIBRARY IMPORT
# ==========================================================

# ShadowHook (Shared)
add_library(shadowhook SHARED IMPORTED)
set_target_properties(shadowhook PROPERTIES IMPORTED_LOCATION 
    ${SHADOWHOOK_DIR}/libs/${ANDROID_ABI}/libshadowhook.so)

# ZSTD (Static)
add_library(libzstd STATIC IMPORTED)
set_target_properties(libzstd PROPERTIES IMPORTED_LOCATION 
    ${VMEER_LIBS_DIR}/libzstd.a)

# FUSE 3 (Static - Hasil build sukses tadi)
add_library(libfuse3 STATIC IMPORTED) # Nama disamakan jadi libfuse3
set_target_properties(libfuse3 PROPERTIES IMPORTED_LOCATION 
    ${VMEER_LIBS_DIR}/libfuse3.a)

# ==========================================================
# 2. HEADER DIRECTORIES
# ==========================================================
include_directories(
    ${JNI_SRC_DIR}/include
    ${JNI_SRC_DIR}/include/fuse3    # Path header fuse hasil build
    ${JNI_SRC_DIR}
    ${SHADOWHOOK_DIR}/include
)

# ==========================================================
# 3. LIST SOURCE FILES
# ==========================================================
set(VMEER_SOURCES
    vmeer_engine.cpp 
    vmeer_core.cpp
    vmeer_context.cpp
    vmeer_db.cpp
    ${JNI_SRC_DIR}/sqlite3.c
    vmeer_pms.cpp
    wms_runtime.cpp
    binder_engine.cpp
    sensor_engine.cpp
    ams_runtime.cpp
    egl_bridge.cpp
    vmeer_stealth.cpp
    surface_vm.cpp
    binder_vm.cpp
    vmeer_system.cpp
    vmeer_helper.cpp
    vmeer_zygote.cpp
    art_vm.cpp
    vmeer_vfs.cpp
    vmeer_jni_bridge.cpp
    vmeer_main.cpp
    vmeer_fuse_ops.cpp
    vmeer_squashfs_bridge.cpp
)

# ==========================================================
# 4. BUILD TARGET UTAMA
# ==========================================================
add_library(vmeer_engine SHARED ${VMEER_SOURCES})

# Flag SQLite & Engine Optimization
target_compile_definitions(vmeer_engine PRIVATE 
    SQLITE_ENABLE_RTREE
    SQLITE_ENABLE_FTS5
    SQLITE_THREADSAFE=1
    FUSE_USE_VERSION=31  # Versi API FUSE yang digunakan
    SHADOWHOOK_IS_SHARED_LIB
)

# ==========================================================
# 5. LINKER (Menghubungkan semua komponen)
# ==========================================================
target_link_libraries(vmeer_engine 
    shadowhook 
    libzstd     
    libfuse3    # Sekarang sudah cocok dengan add_library di atas
    log 
    android 
    EGL 
    GLESv2 
    z           # System library (zlib)
    atomic     
    dl          # Diperlukan untuk dynamic loading
)

# ==========================================================
# 6. MODUL STANDALONE & DAEMON
# ==========================================================

# Surface VM Standalone
add_library(surface_vm_standalone SHARED surface_vm.cpp)
target_link_libraries(surface_vm_standalone shadowhook log android EGL)

# VMeer Daemon (Untuk kontrol root/shell)
add_executable(vmeerd vmeerd.cpp)
target_link_libraries(vmeerd log android libfuse3 libzstd)
