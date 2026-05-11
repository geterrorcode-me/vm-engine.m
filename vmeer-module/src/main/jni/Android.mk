LOCAL_PATH := $(call my-dir)
ROOT_JNI := $(LOCAL_PATH)/../../../../jni
ROOT_INCLUDE := $(LOCAL_PATH)/../../../../include

# --- ShadowHook Prebuilt (Asumsi Anda sudah mendownload library-nya) ---
include $(CLEAR_VARS)
LOCAL_MODULE := shadowhook
LOCAL_SRC_FILES := $(ROOT_JNI)/3rdparty/shadowhook/libs/$(TARGET_ARCH_ABI)/libshadowhook.so
include $(PREBUILT_SHARED_LIBRARY)

# --- vMeer Engine Core ---
include $(CLEAR_VARS)
LOCAL_MODULE    := vmeer_engine
LOCAL_C_INCLUDES := $(ROOT_INCLUDE) $(ROOT_JNI)
LOCAL_SRC_FILES := \
    $(ROOT_JNI)/vm_engine.cpp \
    $(ROOT_JNI)/binder/binder_rewriter.cpp \
    $(ROOT_JNI)/vfs_hook.cpp

LOCAL_SHARED_LIBRARIES := shadowhook
LOCAL_LDLIBS    := -llog -landroid -ldl
include $(BUILD_SHARED_LIBRARY)
