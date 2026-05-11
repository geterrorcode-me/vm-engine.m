cmake_minimum_required(VERSION 3.22.1)
project(vmeer_engine_modular)

# =============================================================================
# 1. PATH & NDK LIBRARIES
# =============================================================================
set(JNI_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(SHADOWHOOK_DIR ${JNI_SRC_DIR}/3rdparty/shadowhook)

# Hanya cari library yang PASTI ADA di NDK
find_library(log-lib log)
find_library(android-lib android)
find_library(egl-lib EGL)

# JANGAN gunakan find_library untuk gui atau ui karena akan NOTFOUND di Runner/CI
# =============================================================================

include_directories(
    ${JNI_SRC_DIR}
    ${JNI_SRC_DIR}/include
    ${JNI_SRC_DIR}/bridge
    ${SHADOWHOOK_DIR}/include
)

# Import ShadowHook
add_library(shadowhook SHARED IMPORTED)
set_target_properties(shadowhook PROPERTIES IMPORTED_LOCATION 
    ${SHADOWHOOK_DIR}/libs/${ANDROID_ABI}/libshadowhook.so)

# =============================================================================
# 2. MODUL GRAPHICS (surface_vm)
# =============================================================================
add_library(surface_vm SHARED 
    surface_vm.cpp
)

# KUNCI: Hapus ${gui-lib} dan ${ui-lib}. 
# Kita sudah pakai dlopen("libgui.so") di dalam kodenya.
target_link_libraries(surface_vm 
    shadowhook
    ${log-lib} 
    ${android-lib} 
    ${egl-lib}
)

# =============================================================================
# 3. MODUL ENGINE UTAMA
# =============================================================================
add_library(vmeer_engine SHARED 
    vmeer_engine.cpp 
    vmeer_core.cpp
    vmeer_stealth.cpp
)
target_link_libraries(vmeer_engine 
    shadowhook 
    ${log-lib}
)

# =============================================================================
# 4. MODUL LAINNYA
# =============================================================================
add_library(binder_vm SHARED binder_vm.cpp)
target_link_libraries(binder_vm shadowhook ${log-lib})

add_library(vmeer_system SHARED vmeer_system.cpp)
target_link_libraries(vmeer_system ${log-lib})

add_executable(vmeerd vmeerd.cpp)
target_link_libraries(vmeerd ${log-lib})
