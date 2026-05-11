cmake_minimum_required(VERSION 3.22.1)
project(vmeer_engine_modular)

# =============================================================================
# BAGIAN 1: KONFIGURASI PATH & DEPENDENSI
# =============================================================================

set(JNI_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(SHADOWHOOK_DIR ${JNI_SRC_DIR}/3rdparty/shadowhook)

# Cari Library Sistem Android (Wajib untuk Graphics & IPC)
find_library(log-lib log)
find_library(android-lib android)
find_library(egl-lib EGL)
find_library(gles-lib GLESv2)

# Konfigurasi Include Path untuk semua modul
include_directories(
    ${JNI_SRC_DIR}
    ${JNI_SRC_DIR}/include
    ${JNI_SRC_DIR}/bridge
    ${JNI_SRC_DIR}/binder
    ${SHADOWHOOK_DIR}/include
)

# Import ShadowHook (Engine Hooking Utama)
add_library(shadowhook SHARED IMPORTED)
set_target_properties(shadowhook PROPERTIES IMPORTED_LOCATION 
    ${SHADOWHOOK_DIR}/libs/${ANDROID_ABI}/libshadowhook.so)

# =============================================================================
# BAGIAN 2: HELPER & CORE ARCHITECT (Static Libraries)
# =============================================================================

# Helper Module (Dibutuhkan oleh hampir semua library .so)
add_library(vmeer_helper STATIC 
    bridge/hook_bridge.cpp
)

# =============================================================================
# BAGIAN 3: MODUL ENGINE UTAMA (The Orchestra)
# =============================================================================

# A. Module Utama (libvmeer_engine.so) - Menyatukan Core, Stealth, dan VFS
add_library(vmeer_engine SHARED 
    vmeer_engine.cpp 
    vfs_hook.cpp
    vmeer_core.cpp
    vmeer_stealth.cpp   # Update: Tambahan modul Anti-Detect
)
target_link_libraries(vmeer_engine 
    vmeer_helper 
    shadowhook 
    ${log-lib}
)

# B. Module Binder & Hardware (libbinder_vm.so)
add_library(binder_vm SHARED 
    binder_vm.cpp 
    binder/binder_rewriter.cpp
    vmeer_hw_bridge.cpp  # Update: Tambahan Hardware Passthrough
)
target_link_libraries(binder_vm 
    vmeer_helper 
    shadowhook 
    ${log-lib}
)

# C. Module Graphics (libsurface_vm.so)
add_library(surface_vm SHARED 
    surface_vm.cpp
)
target_link_libraries(surface_vm 
    shadowhook          # Update: Dibutuhkan untuk hook dequeueBuffer
    ${log-lib} 
    ${android-lib} 
    ${egl-lib}
)

# D. Module System Server (libvmeer_system.so)
add_library(vmeer_system SHARED 
    vmeer_system.cpp
)
target_link_libraries(vmeer_system 
    ${log-lib}
)

# E. Module ART & Zygote (libart_vm.so)
add_library(art_vm SHARED 
    art_vm.cpp
    vmeer_zygote.cpp    # Update: Integrasi Pseudo Zygote
)
target_link_libraries(art_vm 
    ${log-lib}
)

# =============================================================================
# BAGIAN 4: MODUL EKSPERIMENTAL & BRIDGE
# =============================================================================

add_library(bufferqueue_vm SHARED bufferqueue_vm.cpp)
target_link_libraries(bufferqueue_vm ${log-lib} ${android-lib})

add_library(egl_bridge SHARED egl_bridge.cpp)
target_link_libraries(egl_bridge ${log-lib} ${egl-lib} ${gles-lib})

add_library(romfs_vm SHARED romfs_vm.cpp)
target_link_libraries(romfs_vm ${log-lib})

# =============================================================================
# BAGIAN 5: STANDALONE BINARIES (The Daemon)
# =============================================================================

# Daemon vmeerd (Biner mandiri, bukan .so)
add_executable(vmeerd vmeerd.cpp)
target_link_libraries(vmeerd 
    ${log-lib}
)
