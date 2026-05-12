cmake_minimum_required(VERSION 3.22.1)
project(vmeer_engine_modular)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(JNI_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(SHADOWHOOK_DIR ${JNI_SRC_DIR}/3rdparty/shadowhook)

# Cari Library Sistem NDK
find_library(log-lib log)
find_library(android-lib android)
find_library(egl-lib EGL)
find_library(gles-lib GLESv2)
find_library(z-lib z)
find_library(sqlite-lib sqlite3) # Menemukan libsqlite3.so sistem

# PENTING: Jika OpenSSL tidak ada di Runner, kita bisa abaikan dulu 
# atau arahkan ke prebuilt jika Anda memilikinya.
find_library(crypto-lib crypto)

include_directories(
    ${JNI_SRC_DIR}
    ${JNI_SRC_DIR}/include
    ${SHADOWHOOK_DIR}/include
)

# Import ShadowHook
add_library(shadowhook SHARED IMPORTED)
set_target_properties(shadowhook PROPERTIES IMPORTED_LOCATION 
    ${SHADOWHOOK_DIR}/libs/${ANDROID_ABI}/libshadowhook.so)

add_library(vmeer_engine SHARED 
    vmeer_engine.cpp 
    vmeer_core.cpp
    vmeer_context.cpp
    vmeer_db.cpp
    binder_engine.cpp
    sensor_engine.cpp
    pms_runtime.cpp
    egl_bridge.cpp
    vmeer_stealth.cpp
    surface_vm.cpp
    binder_vm.cpp
    vmeer_system.cpp
)

target_link_libraries(vmeer_engine 
    shadowhook 
    ${log-lib}
    ${android-lib}
    ${egl-lib}
    ${gles-lib}
    ${sqlite-lib} # Gunakan variabel hasil find_library
    ${z-lib}
    ${crypto-lib} # Gunakan variabel, jangan hardcoded 'crypto'
    atomic
)
