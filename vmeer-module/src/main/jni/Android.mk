LOCAL_PATH := $(call my-dir)
ROOT_JNI := $(LOCAL_PATH)/../../../../jni

# --- 1. Surface Virtualization (The Renderer) ---
include $(CLEAR_VARS)
LOCAL_MODULE    := surface_vm
LOCAL_SRC_FILES := $(ROOT_JNI)/surface_vm.cpp
LOCAL_LDLIBS    := -llog -lEGL -lGLESv2 -landroid
include $(BUILD_SHARED_LIBRARY)

# --- 2. BufferQueue Manager (The Bridge) ---
include $(CLEAR_VARS)
LOCAL_MODULE    := bufferqueue_vm
LOCAL_SRC_FILES := $(ROOT_JNI)/bufferqueue_vm.cpp
LOCAL_LDLIBS    := -llog -lui -lgui
include $(BUILD_SHARED_LIBRARY)

# --- 3. EGL Bridge (Hardware Passthrough) ---
include $(CLEAR_VARS)
LOCAL_MODULE    := egl_bridge
LOCAL_SRC_FILES := $(ROOT_JNI)/egl_bridge.cpp
LOCAL_LDLIBS    := -llog -lEGL
include $(BUILD_SHARED_LIBRARY)

# --- 4. Core Bootstrap (The Orchestrator) ---
include $(CLEAR_VARS)
LOCAL_MODULE    := vmeer_engine
LOCAL_SRC_FILES := $(ROOT_JNI)/vm_engine.cpp
LOCAL_LDLIBS    := -llog -landroid
include $(BUILD_SHARED_LIBRARY)
