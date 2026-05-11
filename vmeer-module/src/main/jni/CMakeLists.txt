cmake_minimum_required(VERSION 3.22.1)
project(vmeer_engine_modular)

# --- 1. KONFIGURASI PATH & INCLUDE ---

# Folder JNI saat ini
set(JNI_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR})

# Folder ShadowHook (Pastikan path ini ada: vmeer-module/src/main/jni/3rdparty/shadowhook)
set(SHADOWHOOK_DIR ${JNI_SRC_DIR}/3rdparty/shadowhook)

# Daftarkan semua lokasi header agar bisa dipanggil dengan #include
include_directories(
    ${JNI_SRC_DIR}
    ${JNI_SRC_DIR}/bridge
    ${JNI_SRC_DIR}/binder
    ${SHADOWHOOK_DIR}/include
)

# --- 2. IMPORT LIBRARY EXTERNAL ---

# Import ShadowHook Binary (.so)
add_library(shadowhook SHARED IMPORTED)
set_target_properties(shadowhook PROPERTIES IMPORTED_LOCATION 
    ${SHADOWHOOK_DIR}/libs/${ANDROID_ABI}/libshadowhook.so)

# --- 3. DEFINISI MODULE MODULAR (Sesuai isi .aar Anda) ---

# A. Module Utama: vmeer_engine (Menangani Hooking & Bridge)
add_library(vmeer_engine SHARED 
    vmeer_engine.cpp 
    bridge/hook_bridge.cpp
    vfs_hook.cpp
)
target_link_libraries(vmeer_engine shadowhook log)

# B. Module VM: art_vm
add_library(art_vm SHARED art_vm.cpp)
target_link_libraries(art_vm log)

# C. Module Binder: binder_vm
add_library(binder_vm SHARED 
    binder_vm.cpp 
    binder/binder_rewriter.cpp
)
target_link_libraries(binder_vm log)

# D. Module Grafis & Buffer
add_library(bufferqueue_vm SHARED bufferqueue_vm.cpp)
add_library(surface_vm SHARED surface_vm.cpp)
add_library(egl_bridge SHARED egl_bridge.cpp)

# E. Module File System
add_library(romfs_vm SHARED romfs_vm.cpp)

# --- 4. OPTIMASI BUILD ---

# Menambahkan flag C++17 agar fitur modern bisa digunakan
set_target_properties(
    vmeer_engine art_vm binder_vm bufferqueue_vm surface_vm egl_bridge romfs_vm 
    PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON
)
