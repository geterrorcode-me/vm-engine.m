LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_MODULE    := vmeer_engine
LOCAL_C_INCLUDES := $(LOCAL_PATH)/../../../../include
LOCAL_SRC_FILES := \
    ../../../../jni/vm_engine.cpp \
    ../../../../jni/binder_vm.cpp \
    ../../../../jni/vfs_hook.cpp
LOCAL_LDLIBS    := -llog -landroid
include $(BUILD_SHARED_LIBRARY)
