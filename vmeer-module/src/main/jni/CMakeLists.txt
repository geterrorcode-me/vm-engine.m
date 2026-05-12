cmake_minimum_required(VERSION 3.22.1)
project(vmeer_engine_modular)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -lsqlite3")

set(JNI_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(SHADOWHOOK_DIR ${JNI_SRC_DIR}/3rdparty/shadowhook)

# Gunakan INTERFACE untuk memastikan semua target bisa melihat folder include
include_directories(
    ${JNI_SRC_DIR}
    ${JNI_SRC_DIR}/include
    ${SHADOWHOOK_DIR}/include
)

# Import ShadowHook
add_library(shadowhook SHARED IMPORTED)
set_target_properties(shadowhook PROPERTIES IMPORTED_LOCATION 
    ${SHADOWHOOK_DIR}/libs/${ANDROID_ABI}/libshadowhook.so)

# List semua file source (Pastikan tidak ada typo pada nama file)
set(VMEER_SOURCES
    vmeer_engine.cpp 
    vmeer_core.cpp
    vmeer_context.cpp
    vmeer_db.cpp
    vmeer_pms.cpp
    binder_engine.cpp
    sensor_engine.cpp
    pms_runtime.cpp
    egl_bridge.cpp
    vmeer_stealth.cpp
    surface_vm.cpp
    binder_vm.cpp
    vmeer_system.cpp
)

# Build Library Utama
add_library(vmeer_engine SHARED ${VMEER_SOURCES})

# Tambahkan compile flag agar compiler lebih teliti terhadap include
target_compile_options(vmeer_engine PRIVATE -Wall -Wextra)

target_link_libraries(vmeer_engine 
    shadowhook 
    log 
    android 
    EGL 
    GLESv2 
    sqlite3    # Penting: Mencari libsqlite3.so di sysroot NDK
    z          
    atomic     
)

# Modul tambahan
add_library(surface_vm_standalone SHARED surface_vm.cpp)
target_link_libraries(surface_vm_standalone shadowhook log android EGL)

add_executable(vmeerd vmeerd.cpp)
target_link_libraries(vmeerd log)
