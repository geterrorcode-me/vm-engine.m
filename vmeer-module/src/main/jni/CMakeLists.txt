cmake_minimum_required(VERSION 3.22.1)
project(vmeer_engine_modular)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(JNI_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(SHADOWHOOK_DIR ${JNI_SRC_DIR}/3rdparty/shadowhook)

# Pastikan folder include lokal diprioritaskan
include_directories(
    ${JNI_SRC_DIR}/include
    ${JNI_SRC_DIR}
    ${SHADOWHOOK_DIR}/include
)

# Import ShadowHook
add_library(shadowhook SHARED IMPORTED)
set_target_properties(shadowhook PROPERTIES IMPORTED_LOCATION 
    ${SHADOWHOOK_DIR}/libs/${ANDROID_ABI}/libshadowhook.so)

# List semua file source
set(VMEER_SOURCES
    vmeer_engine.cpp 
    vmeer_core.cpp
    vmeer_context.cpp
    vmeer_db.cpp
    # SQLite Amalgamation: Ini akan dikompilasi langsung ke vmeer_engine
    ${JNI_SRC_DIR}/sqlite3.c
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
)

# Build Library Utama
add_library(vmeer_engine SHARED ${VMEER_SOURCES})

# Tambahkan flag khusus agar SQLite bekerja maksimal di Android
target_compile_definitions(vmeer_engine PRIVATE 
    SQLITE_ENABLE_RTREE
    SQLITE_ENABLE_FTS5
    SQLITE_THREADSAFE=1
)

target_compile_options(vmeer_engine PRIVATE -Wall -Wextra)

# Bagian Linker
target_link_libraries(vmeer_engine 
    shadowhook 
    log 
    android 
    EGL 
    GLESv2 
    # sqlite3 DIHAPUS dari sini karena sudah ada di VMEER_SOURCES sebagai .c
    z          
    atomic     
)

# Modul tambahan
add_library(surface_vm_standalone SHARED surface_vm.cpp)
target_link_libraries(surface_vm_standalone shadowhook log android EGL)

add_executable(vmeerd vmeerd.cpp)
target_link_libraries(vmeerd log)
