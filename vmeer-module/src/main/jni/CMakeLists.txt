cmake_minimum_required(VERSION 3.22.1)
project(vmeer_engine_modular)

set(JNI_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(SHADOWHOOK_DIR ${JNI_SRC_DIR}/3rdparty/shadowhook)

# Konfigurasi Include Path
include_directories(
    ${JNI_SRC_DIR}
    ${JNI_SRC_DIR}/bridge
    ${JNI_SRC_DIR}/binder
    ${SHADOWHOOK_DIR}/include
)

# Import ShadowHook Binary
add_library(shadowhook SHARED IMPORTED)
set_target_properties(shadowhook PROPERTIES IMPORTED_LOCATION 
    ${SHADOWHOOK_DIR}/libs/${ANDROID_ABI}/libshadowhook.so)

# --- DAFTAR MODULE ---

# Module Utama
add_library(vmeer_engine SHARED 
    vmeer_engine.cpp 
    bridge/hook_bridge.cpp
    vfs_hook.cpp
)
target_link_libraries(vmeer_engine shadowhook log)

# Module VM & Lainnya
add_library(art_vm SHARED art_vm.cpp)
add_library(binder_vm SHARED binder_vm.cpp binder/binder_rewriter.cpp)
add_library(bufferqueue_vm SHARED bufferqueue_vm.cpp)
add_library(surface_vm SHARED surface_vm.cpp)
add_library(egl_bridge SHARED egl_bridge.cpp)
add_library(romfs_vm SHARED romfs_vm.cpp)

target_link_libraries(art_vm log)
target_link_libraries(binder_vm log)
