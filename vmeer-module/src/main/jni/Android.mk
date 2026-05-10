LOCAL_PATH := $(call my-dir)
ROOT_JNI := $(LOCAL_PATH)/../../../../jni
ROOT_INCLUDE := $(LOCAL_PATH)/../../../../include

# --- 1. Komponen Grafis (Tetap Ada) ---
include $(CLEAR_VARS)
LOCAL_MODULE    := surface_vm
LOCAL_SRC_FILES := $(ROOT_JNI)/surface_vm.cpp
LOCAL_C_INCLUDES := $(ROOT_INCLUDE)
LOCAL_LDLIBS    := -llog -lEGL -lGLESv2 -landroid
include $(BUILD_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE    := bufferqueue_vm
LOCAL_SRC_FILES := $(ROOT_JNI)/bufferqueue_vm.cpp
LOCAL_C_INCLUDES := $(ROOT_INCLUDE)
LOCAL_LDLIBS    := -llog -landroid
include $(BUILD_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE    := egl_bridge
LOCAL_SRC_FILES := $(ROOT_JNI)/egl_bridge.cpp
LOCAL_C_INCLUDES := $(ROOT_INCLUDE)
LOCAL_LDLIBS    := -llog -lEGL
include $(BUILD_SHARED_LIBRARY)

# --- 2. Komponen Sistem (Saran Anda) ---
include $(CLEAR_VARS)
LOCAL_MODULE    := binder_vm
LOCAL_SRC_FILES := $(ROOT_JNI)/binder_vm.cpp
LOCAL_C_INCLUDES := $(ROOT_INCLUDE)
LOCAL_LDLIBS    := -llog
include $(BUILD_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE    := art_vm
LOCAL_SRC_FILES := $(ROOT_JNI)/art_vm.cpp
LOCAL_C_INCLUDES := $(ROOT_INCLUDE)
LOCAL_LDLIBS    := -llog
include $(BUILD_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE    := romfs_vm
LOCAL_SRC_FILES := $(ROOT_JNI)/romfs_vm.cpp
LOCAL_C_INCLUDES := $(ROOT_INCLUDE)
LOCAL_LDLIBS    := -llog
include $(BUILD_SHARED_LIBRARY)

# --- 3. Orchestrator Engine ---
include $(CLEAR_VARS)
LOCAL_MODULE    := vmeer_engine
LOCAL_SRC_FILES := $(ROOT_JNI)/vm_engine.cpp $(ROOT_JNI)/vfs_hook.cpp
LOCAL_C_INCLUDES := $(ROOT_INCLUDE)
LOCAL_LDLIBS    := -llog -landroid
include $(BUILD_SHARED_LIBRARY)
