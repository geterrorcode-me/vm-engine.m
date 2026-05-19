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

# FUSE 3 (Static)
add_library(libfuse3 STATIC IMPORTED) 
set_target_properties(libfuse3 PROPERTIES IMPORTED_LOCATION 
    ${VMEER_LIBS_DIR}/libfuse3.a)

# ==========================================================
# 2. HEADER DIRECTORIES
# ==========================================================
include_directories(
    ${JNI_SRC_DIR}/include
    ${JNI_SRC_DIR}/include/fuse3    
    ${JNI_SRC_DIR}
    ${SHADOWHOOK_DIR}/include
)

# ==========================================================
# 3. LIST SOURCE FILES TARGET UTAMA (VMEER_ENGINE)
# ==========================================================
set(VMEER_SOURCES
    vmeer_engine.cpp
    vmeer_jni.cpp
    vmeer_core.cpp         # Bersih dari duplikasi simbol JNI grafis
    vmeer_context.cpp
    vmeer_db.cpp
    ${JNI_SRC_DIR}/sqlite3.c
    vmeer_pms.cpp
    wms_runtime.cpp
    binder_engine.cpp
    sensor_engine.cpp
    ams_runtime.cpp
    egl_bridge.cpp         # Mesin render utama vMeerOS
    vmeer_stealth.cpp
    surface_vm.cpp         # Satu-satunya pemilik fungsi JNI GraphicsEngine yang sah
    vmeer_system.cpp
    vmeer_helper.cpp
    vmeer_zygote.cpp
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
    FUSE_USE_VERSION=31  
    SHADOWHOOK_IS_SHARED_LIB
)

# ==========================================================
# 5. LINKER UTAMA
# ==========================================================
target_link_libraries(vmeer_engine 
    shadowhook 
    libzstd     
    libfuse3    
    log 
    android 
    EGL 
    GLESv2 
    z           
    atomic     
    dl          
)

# ==========================================================
# 6. MODUL STANDALONE, DAEMON & COMPONENT ISOLATION
# ==========================================================

# Modul Isolasi 1: Binder VM Standalone
add_library(binder_vm SHARED binder_vm.cpp)
target_link_libraries(binder_vm shadowhook log android)

# Modul Isolasi 2: ART VM Standalone
add_library(art_vm SHARED art_vm.cpp)
target_link_libraries(art_vm shadowhook log android)

# Modul Isolasi 3: RomFS VM Standalone
add_library(romfs_vm SHARED vmeer_squashfs_bridge.cpp)
target_link_libraries(romfs_vm log android libzstd)

# Modul Isolasi 4: Surface VM Standalone (Fix Undefined Symbols via Dynamic Isolation)
add_library(surface_vm_standalone SHARED surface_vm.cpp)
target_compile_definitions(surface_vm_standalone PRIVATE SHADOWHOOK_IS_SHARED_LIB)
set_target_properties(surface_vm_standalone PROPERTIES LINK_FLAGS "-Wl,--allow-shlib-undefined")
target_link_libraries(surface_vm_standalone 
    shadowhook 
    log 
    android 
    EGL 
    dl   
)

# VMeer Daemon Executable Binary
add_executable(vmeerd vmeerd.cpp)
target_link_libraries(vmeerd log android libfuse3 libzstd)
