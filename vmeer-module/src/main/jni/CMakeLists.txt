cmake_minimum_required(VERSION 3.22.1)
project(vmeer_engine_modular)

set(JNI_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(SHADOWHOOK_DIR ${JNI_SRC_DIR}/3rdparty/shadowhook)

# Library Publik NDK
find_library(log-lib log)
find_library(android-lib android)
find_library(egl-lib EGL)

include_directories(
    ${JNI_SRC_DIR}
    ${JNI_SRC_DIR}/include
    ${SHADOWHOOK_DIR}/include
)

# Import ShadowHook
add_library(shadowhook SHARED IMPORTED)
set_target_properties(shadowhook PROPERTIES IMPORTED_LOCATION 
    ${SHADOWHOOK_DIR}/libs/${ANDROID_ABI}/libshadowhook.so)

# =============================================================================
# CORE ENGINE (Menyatukan semua modul agar tidak ada Undefined Symbol)
# =============================================================================
add_library(vmeer_engine SHARED 
    vmeer_engine.cpp 
    vmeer_core.cpp
    egl_bridge.cpp
    vmeer_stealth.cpp
    surface_vm.cpp
    binder_vm.cpp
    vmeer_system.cpp
    vmeer_stealth.cpp
)

target_link_libraries(vmeer_engine 
    shadowhook 
    ${log-lib}
    ${android-lib}
    ${egl-lib}           # egl-lib biasanya dari find_library(egl-lib EGL)
    GLESv2               # Tambahkan library OpenGL ES 2.0
    atomic               # Penting untuk arsitektur arm64
)

# Modul Standalone untuk akses langsung dari Java (Opsional)
add_library(surface_vm SHARED surface_vm.cpp)
target_link_libraries(surface_vm shadowhook ${log-lib} ${android-lib} ${egl-lib})

add_executable(vmeerd vmeerd.cpp)
target_link_libraries(vmeerd ${log-lib})
