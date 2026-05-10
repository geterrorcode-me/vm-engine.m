LOCAL_PATH := $(call my-dir)
ROOT_JNI := $(LOCAL_PATH)/../../../../jni
ROOT_INCLUDE := $(LOCAL_PATH)/../../../../include

# --- 1. Komponen Independen ---
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

# --- 2. Orchestrator Engine ---
include $(CLEAR_VARS)
LOCAL_MODULE    := vmeer_engine
LOCAL_C_INCLUDES := $(ROOT_INCLUDE)
LOCAL_SRC_FILES := $(ROOT_JNI)/vm_engine.cpp $(ROOT_JNI)/vfs_hook.cpp

# Beri tahu linker bahwa modul ini butuh binder_vm agar symbol ditemukan
LOCAL_SHARED_LIBRARIES := binder_vm
LOCAL_LDLIBS    := -llog -landroid -ldl
include $(BUILD_SHARED_LIBRARY)
