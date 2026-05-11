cmake_minimum_required(VERSION 3.22.1)
project(vmeer_engine_modular)

set(JNI_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(SHADOWHOOK_DIR ${JNI_SRC_DIR}/3rdparty/shadowhook)

# Cari Library Sistem Android
find_library(log-lib log)
find_library(android-lib android)
find_library(egl-lib EGL)

# Konfigurasi Include Path
include_directories(
    ${JNI_SRC_DIR}
    ${JNI_SRC_DIR}/include
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
target_link_libraries(vmeer_engine shadowhook ${log-lib})

# Module VM & Graphics (Tambahkan log & android lib di sini)
add_library(art_vm SHARED art_vm.cpp)
target_link_libraries(art_vm ${log-lib})

add_library(binder_vm SHARED binder_vm.cpp binder/binder_rewriter.cpp)
target_link_libraries(binder_vm ${log-lib})

add_library(bufferqueue_vm SHARED bufferqueue_vm.cpp)
target_link_libraries(bufferqueue_vm ${log-lib} ${android-lib})

add_library(surface_vm SHARED surface_vm.cpp)
target_link_libraries(surface_vm ${log-lib} ${android-lib} ${egl-lib})

add_library(egl_bridge SHARED egl_bridge.cpp)
target_link_libraries(egl_bridge ${log-lib} ${egl-lib})

add_library(romfs_vm SHARED romfs_vm.cpp)
target_link_libraries(romfs_vm ${log-lib})
