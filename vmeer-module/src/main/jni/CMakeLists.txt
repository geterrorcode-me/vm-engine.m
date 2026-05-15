cmake_minimum_required(VERSION 3.22.1)
project(vmeer_engine_modular)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(JNI_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(SHADOWHOOK_DIR ${JNI_SRC_DIR}/3rdparty/shadowhook)
# Path ke folder libs tempat libzstd.a berada
set(VMEER_LIBS_DIR ${JNI_SRC_DIR}/libs/${ANDROID_ABI})

# 1. Definisi Library Import (WAJIB untuk file .a / .so eksternal)
# Import ShadowHook
add_library(shadowhook SHARED IMPORTED)
set_target_properties(shadowhook PROPERTIES IMPORTED_LOCATION 
    ${SHADOWHOOK_DIR}/libs/${ANDROID_ABI}/libshadowhook.so)

# Import ZSTD (Hasil build GitHub tadi)
add_library(libzstd STATIC IMPORTED)
set_target_properties(libzstd PROPERTIES IMPORTED_LOCATION 
    ${VMEER_LIBS_DIR}/libzstd.a)

# Import FUSE (Jika kamu sudah punya libfuse3.a nanti)
add_library(libfuse STATIC IMPORTED)
set_target_properties(libfuse PROPERTIES IMPORTED_LOCATION 
    ${VMEER_LIBS_DIR}/libfuse3.a)

# 2. Header Directories
include_directories(
    ${JNI_SRC_DIR}/include        # Tempat zstd.h dan fuse.h berada
    ${JNI_SRC_DIR}
    ${SHADOWHOOK_DIR}/include
)

# 3. List Source Files
set(VMEER_SOURCES
    vmeer_engine.cpp 
    vmeer_core.cpp
    vmeer_context.cpp
    vmeer_db.cpp
    ${JNI_SRC_DIR}/sqlite3.c      # SQLite dikompilasi langsung
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

# 4. Build Library Utama
add_library(vmeer_engine SHARED ${VMEER_SOURCES})

# Flag SQLite & Optimasi
target_compile_definitions(vmeer_engine PRIVATE 
    SQLITE_ENABLE_RTREE
    SQLITE_ENABLE_FTS5
    SQLITE_THREADSAFE=1
)

# 5. Linker (Gunakan nama target yang di-import di atas)
target_link_libraries(vmeer_engine 
    shadowhook 
    libzstd     # Harus sesuai dengan nama di add_library
    libfuse     # Harus sesuai dengan nama di add_library
    log 
    android 
    EGL 
    GLESv2 
    z           # System library (zlib)
    atomic     
)

# Modul Tambahan
add_library(surface_vm_standalone SHARED surface_vm.cpp)
target_link_libraries(surface_vm_standalone shadowhook log android EGL)

add_executable(vmeerd vmeerd.cpp)
target_link_libraries(vmeerd log)
