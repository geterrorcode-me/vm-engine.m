cmake_minimum_required(VERSION 3.22.1)
project(vmeer_engine_modular)

# WAJIB: Gunakan C++17 agar std::unique_ptr dan fitur modern lainnya berjalan lancar
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(JNI_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(SHADOWHOOK_DIR ${JNI_SRC_DIR}/3rdparty/shadowhook)

# Library Publik NDK
find_library(log-lib log)
find_library(android-lib android)
find_library(egl-lib EGL)
find_library(gles-lib GLESv2)
find_library(z-lib z) # Dibutuhkan oleh beberapa versi crypto/sqlite

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
# CORE ENGINE (Modul Terintegrasi)
# =============================================================================
add_library(vmeer_engine SHARED 
    vmeer_engine.cpp 
    vmeer_core.cpp
    vmeer_context.cpp    # Orchestrator
    vmeer_db.cpp         # Persistence Layer
    binder_engine.cpp    # Logic Module: Binder
    sensor_engine.cpp    # Logic Module: Sensor
    pms_runtime.cpp
    egl_bridge.cpp
    vmeer_stealth.cpp
    surface_vm.cpp
    binder_vm.cpp        # Pastikan tidak tumpang tindih dengan binder_engine
    vmeer_system.cpp
)

target_link_libraries(vmeer_engine 
    shadowhook 
    ${log-lib}
    ${android-lib}
    ${egl-lib}
    ${gles-lib}
    ${sqlite3-lib}
    sqlite3              # Database Engine
    crypto               # OpenSSL untuk HMAC/SHA256 (Deterministic Seed)
    atomic               # Penting untuk operasi thread-safe di ARM
    z                    # Compression (biasanya dependensi crypto)
)

# Modul Standalone (Jika masih dibutuhkan secara terpisah)
add_library(surface_vm_standalone SHARED surface_vm.cpp)
target_link_libraries(surface_vm_standalone shadowhook ${log-lib} ${android-lib} ${egl-lib})

add_executable(vmeerd vmeerd.cpp)
target_link_libraries(vmeerd ${log-lib})
