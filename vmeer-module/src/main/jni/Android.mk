LOCAL_PATH := $(call my-dir)
ROOT_JNI := $(LOCAL_PATH)/../../../../jni
ROOT_INCLUDE := $(LOCAL_PATH)/../../../../include

# --- 1. EGL Bridge (Standalone) ---
include $(CLEAR_VARS)
LOCAL_MODULE    := egl_bridge
LOCAL_SRC_FILES := $(ROOT_JNI)/egl_bridge.cpp
LOCAL_C_INCLUDES := $(ROOT_INCLUDE)
LOCAL_LDLIBS    := -llog -lEGL
include $(BUILD_SHARED_LIBRARY)

# --- 2. BufferQueue VM (Standalone) ---
include $(CLEAR_VARS)
LOCAL_MODULE    := bufferqueue_vm
LOCAL_SRC_FILES := $(ROOT_JNI)/bufferqueue_vm.cpp
LOCAL_C_INCLUDES := $(ROOT_INCLUDE)
LOCAL_LDLIBS    := -llog -landroid
include $(BUILD_SHARED_LIBRARY)

# --- 3. Surface VM (Standalone) ---
include $(CLEAR_VARS)
LOCAL_MODULE    := surface_vm
LOCAL_SRC_FILES := $(ROOT_JNI)/surface_vm.cpp
LOCAL_C_INCLUDES := $(ROOT_INCLUDE)
LOCAL_LDLIBS    := -llog -lEGL -lGLESv2 -landroid
include $(BUILD_SHARED_LIBRARY)

# --- 4. Core vMeer Engine (The Orchestrator) ---
# Kita masukkan VFS dan Binder ke sini agar simbolnya 'visible' secara internal
include $(CLEAR_VARS)
LOCAL_MODULE    := vmeer_engine
LOCAL_C_INCLUDES := $(ROOT_INCLUDE)
LOCAL_SRC_FILES := \
    $(ROOT_JNI)/vm_engine.cpp \
    $(ROOT_JNI)/vfs_hook.cpp \
    $(ROOT_JNI)/binder_vm.cpp
LOCAL_LDLIBS    := -llog -landroid
include $(BUILD_SHARED_LIBRARY)
