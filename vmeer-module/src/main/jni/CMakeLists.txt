cmake_minimum_required(VERSION 3.10.2)
project("vmeer_engine")

# Set path ke folder ShadowHook
set(SHADOWHOOK_DIR ${CMAKE_SOURCE_DIR}/3rdparty/shadowhook)

# Daftarkan ShadowHook sebagai library luar
add_library(shadowhook SHARED IMPORTED)
set_target_properties(shadowhook PROPERTIES IMPORTED_LOCATION
    ${SHADOWHOOK_DIR}/libs/${ANDROID_ABI}/libshadowhook.so)

include_directories(${SHADOWHOOK_DIR}/include)

# Build vMeer Engine utama
add_library(vmeer_engine SHARED 
    vmeer_engine.cpp 
    vmeer_binder_guard.cpp)

# Hubungkan dengan log Android dan ShadowHook
target_link_libraries(vmeer_engine shadowhook log)
