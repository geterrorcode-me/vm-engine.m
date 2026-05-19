cmake_minimum_required(VERSION 3.22.1)
project(vmeer_engine_modular)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(JNI_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(SHADOWHOOK_DIR ${JNI_SRC_DIR}/3rdparty/shadowhook)
set(VMEER_LIBS_DIR ${JNI_SRC_DIR}/libs/${ANDROID_ABI})

# ==========================================================
# 1. REGISTER NDK SYSTEM LIBRARIES (Memicu Auto-Inject Sysroot)
# ==========================================================
find_library(LOG_LIB log)
find_library(ANDROID_LIB android)
find_library(EGL_LIB EGL)
find_library(GLESV2_LIB GLESv2) # Ini akan mengaktifkan path <GLES2/gl2.h>

# ==========================================================
# 2. DEFINISI LIBRARY IMPORT (THIRD PARTY)
# ==========================================================
add_library(shadowhook SHARED IMPORTED)
set_target_properties(shadowhook PROPERTIES IMPORTED_LOCATION 
    ${SHADOWHOOK_DIR}/libs/${ANDROID_ABI}/libshadowhook.so)

add_library(libzstd STATIC IMPORTED)
set_target_properties(libzstd PROPERTIES IMPORTED_LOCATION 
    ${VMEER_LIBS_DIR}/libzstd.a)

add_library(libfuse3 STATIC IMPORTED) 
set_target_properties(libfuse3 PROPERTIES IMPORTED_LOCATION 
    ${VMEER_LIBS_DIR}/libfuse3.a)

# ==========================================================
# 3. HEADER DIRECTORIES
# ==========================================================
include_directories(
    ${JNI_SRC_DIR}/include
    ${JNI_SRC_DIR}/include/fuse3    
    ${JNI_SRC_DIR}
    ${SHADOWHOOK_DIR}/include
)

# ==========================================================
# 4. LIST SOURCE FILES TARGET UTAMA (VMEER_ENGINE)
# ==========================================================
set(VMEER_SOURCES
    vmeer_engine.cpp
    vmeer_jni.cpp
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
# 5. BUILD TARGET UTAMA & LINKING VIA VARIABLE NDK
# ==========================================================
add_library(vmeer_engine SHARED ${VMEER_SOURCES})

target_compile_definitions(vmeer_engine PRIVATE 
    SQLITE_ENABLE_RTREE
    SQLITE_ENABLE_FTS5
    SQLITE_THREADSAFE=1
    FUSE_USE_VERSION=31  
    SHADOWHOOK_IS_SHARED_LIB
)

target_link_libraries(vmeer_engine 
    shadowhook 
    libzstd     
    libfuse3    
    ${LOG_LIB}
    ${ANDROID_LIB}
    ${EGL_LIB}
    ${GLESV2_LIB}
    z           
    atomic     
    dl          
    m
)

# ==========================================================
# 6. MODUL STANDALONE, DAEMON & COMPONENT ISOLATION
# ==========================================================
add_library(binder_vm SHARED binder_vm.cpp)
target_link_libraries(binder_vm shadowhook ${LOG_LIB} ${ANDROID_LIB})

add_library(art_vm SHARED art_vm.cpp)
target_link_libraries(art_vm shadowhook ${LOG_LIB} ${ANDROID_LIB})

add_library(romfs_vm SHARED vmeer_squashfs_bridge.cpp)
target_link_libraries(romfs_vm ${LOG_LIB} ${ANDROID_LIB} libzstd)

# Modul Isolasi Grafis Standalone
add_library(surface_vm_standalone SHARED surface_vm.cpp)
target_compile_definitions(surface_vm_standalone PRIVATE SHADOWHOOK_IS_SHARED_LIB)
set_target_properties(surface_vm_standalone PROPERTIES LINK_FLAGS "-Wl,--allow-shlib-undefined")
target_link_libraries(surface_vm_standalone 
    shadowhook 
    ${LOG_LIB} 
    ${ANDROID_LIB} 
    ${EGL_LIB} 
    dl   
)

# VMeer Daemon Binary
add_executable(vmeerd vmeerd.cpp)
target_link_libraries(vmeerd ${LOG_LIB} ${ANDROID_LIB} libfuse3 libzstd)
