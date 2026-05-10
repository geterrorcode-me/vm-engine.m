LOCAL_PATH := $(call my-dir)
ROOT_JNI := $(LOCAL_PATH)/../../../../jni

# --- 1. Core Engine Bootstrap ---
include $(CLEAR_VARS)
LOCAL_MODULE    := vmeer_engine
LOCAL_SRC_FILES := $(ROOT_JNI)/vm_engine.cpp
LOCAL_LDLIBS    := -llog -landroid
include $(BUILD_SHARED_LIBRARY)

# --- 2. Binder Namespace & Proxy ---
include $(CLEAR_VARS)
LOCAL_MODULE    := binder_vm
LOCAL_SRC_FILES := $(ROOT_JNI)/binder_vm.cpp
LOCAL_LDLIBS    := -llog
include $(BUILD_SHARED_LIBRARY)

# --- 3. VFS Bridge & ROM Mounting ---
include $(CLEAR_VARS)
LOCAL_MODULE    := vfs_vm
LOCAL_SRC_FILES := $(ROOT_JNI)/vfs_hook.cpp
LOCAL_LDLIBS    := -llog
include $(BUILD_SHARED_LIBRARY)

# --- 4. Surface & Display (Placeholder untuk Prioritas 1) ---
include $(CLEAR_VARS)
LOCAL_MODULE    := surface_vm
LOCAL_SRC_FILES := $(ROOT_JNI)/surface_vm.cpp
LOCAL_LDLIBS    := -llog -lEGL -lGLESv2
include $(BUILD_SHARED_LIBRARY)
