LOCAL_PATH := $(call my-dir)
# Jalur ke folder jni utama (4 tingkat ke atas dari vmeer-module/src/main/jni)
ROOT_JNI := $(LOCAL_PATH)/../../../../jni
# Jalur ke folder include utama
ROOT_INCLUDE := $(LOCAL_PATH)/../../../../include

# --- COMMON SETTINGS ---
define vmeer-module-defaults
    LOCAL_C_INCLUDES := $(ROOT_INCLUDE)
    LOCAL_LDLIBS := -llog -landroid
endef

# --- 1. EGL Bridge ---
include $(CLEAR_VARS)
LOCAL_MODULE    := egl_bridge
LOCAL_SRC_FILES := $(ROOT_JNI)/egl_bridge.cpp
LOCAL_C_INCLUDES := $(ROOT_INCLUDE)
LOCAL_LDLIBS    := -llog -lEGL
include $(BUILD_SHARED_LIBRARY)

# --- 2. BufferQueue VM ---
include $(CLEAR_VARS)
LOCAL_MODULE    := bufferqueue_vm
LOCAL_SRC_FILES := $(ROOT_JNI)/bufferqueue_vm.cpp
LOCAL_C_INCLUDES := $(ROOT_INCLUDE)
LOCAL_LDLIBS    := -llog
include $(BUILD_SHARED_LIBRARY)

# --- 3. Surface VM ---
include $(CLEAR_VARS)
LOCAL_MODULE    := surface_vm
LOCAL_SRC_FILES := $(ROOT_JNI)/surface_vm.cpp
LOCAL_C_INCLUDES := $(ROOT_INCLUDE)
LOCAL_LDLIBS    := -llog -lEGL -lGLESv2
include $(BUILD_SHARED_LIBRARY)

# --- 4. Core vMeer Engine ---
include $(CLEAR_VARS)
LOCAL_MODULE    := vmeer_engine
LOCAL_SRC_FILES := $(ROOT_JNI)/vm_engine.cpp
LOCAL_C_INCLUDES := $(ROOT_INCLUDE)
LOCAL_LDLIBS    := -llog
include $(BUILD_SHARED_LIBRARY)
