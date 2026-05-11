cmake_minimum_required(VERSION 3.22.1)
project(vmeer_engine_modular)

set(JNI_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(SHADOWHOOK_DIR ${JNI_SRC_DIR}/3rdparty/shadowhook)

# 1. Cari Library Sistem Android yang dibutuhkan
find_library(log-lib log)
find_library(android-lib android)
find_library(egl-lib EGL)

# 2. Konfigurasi Include Path (Semua folder masuk sini)
include_directories(
    ${JNI_SRC_DIR}
    ${JNI_SRC_DIR}/include
    ${JNI_SRC_DIR}/bridge
    ${JNI_SRC_DIR}/binder
    ${SHADOWHOOK_DIR}/include
)

# 3. Import ShadowHook Binary (Library pihak ketiga)
add_library(shadowhook SHARED IMPORTED)
set_target_properties(shadowhook PROPERTIES IMPORTED_LOCATION 
    ${SHADOWHOOK_DIR}/libs/${ANDROID_ABI}/libshadowhook.so)

# --- KONFIGURASI MODUL (MODULAR SYSTEM) ---

# A. Helper Module (Static)
# Kita buat bridge/hook_bridge.cpp jadi library statis agar bisa dipinjam modul lain
add_library(vmeer_helper STATIC 
    bridge/hook_bridge.cpp
)

# B. Module Utama (libvmeer_engine.so)
add_library(vmeer_engine SHARED 
    vmeer_engine.cpp 
    vfs_hook.cpp
)
target_link_libraries(vmeer_engine 
    vmeer_helper 
    shadowhook 
    ${log-lib}
)

# C. Module Binder (libbinder_vm.so)
add_library(binder_vm SHARED 
    binder_vm.cpp 
    binder/binder_rewriter.cpp
)
target_link_libraries(binder_vm 
    vmeer_helper 
    shadowhook 
    ${log-lib}
)

# D. Module Graphics & Surface (libsurface_vm.so)
add_library(surface_vm SHARED surface_vm.cpp)
target_link_libraries(surface_vm 
    ${log-lib} 
    ${android-lib} 
    ${egl-lib}
)

# E. Module BufferQueue (libbufferqueue_vm.so)
add_library(bufferqueue_vm SHARED bufferqueue_vm.cpp)
target_link_libraries(bufferqueue_vm 
    ${log-lib} 
    ${android-lib}
)

# F. Module ART VM (libart_vm.so)
add_library(art_vm SHARED art_vm.cpp)
target_link_libraries(art_vm ${log-lib})

# G. Module Lainnya
add_library(egl_bridge SHARED egl_bridge.cpp)
target_link_libraries(egl_bridge ${log-lib} ${egl-lib})

add_library(romfs_vm SHARED romfs_vm.cpp)
target_link_libraries(romfs_vm ${log-lib})
