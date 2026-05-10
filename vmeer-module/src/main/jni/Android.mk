LOCAL_PATH := $(call my-dir)
ROOT_JNI := $(LOCAL_PATH)/../../../../jni

# --- 1. EGL Bridge ---
include $(CLEAR_VARS)
LOCAL_MODULE    := egl_bridge
LOCAL_SRC_FILES := $(ROOT_JNI)/egl_bridge.cpp
LOCAL_LDLIBS    := -llog -lEGL
include $(BUILD_SHARED_LIBRARY)

# --- 2. BufferQueue VM ---
include $(CLEAR_VARS)
LOCAL_MODULE    := bufferqueue_vm
LOCAL_SRC_FILES := $(ROOT_JNI)/bufferqueue_vm.cpp
# Hapus -lui -lgui dari sini karena mereka private API
LOCAL_LDLIBS    := -llog -landroid
include $(BUILD_SHARED_LIBRARY)

# --- 3. Surface VM ---
include $(CLEAR_VARS)
LOCAL_MODULE    := surface_vm
LOCAL_SRC_FILES := $(ROOT_JNI)/surface_vm.cpp
LOCAL_LDLIBS    := -llog -lEGL -lGLESv2 -landroid
include $(BUILD_SHARED_LIBRARY)

# --- 4. Core vMeer Engine ---
include $(CLEAR_VARS)
LOCAL_MODULE    := vmeer_engine
LOCAL_SRC_FILES := $(ROOT_JNI)/vm_engine.cpp
LOCAL_LDLIBS    := -llog -landroid
include $(BUILD_SHARED_LIBRARY)
