LOCAL_PATH := $(call my-dir)

# Jalur relatif yang disesuaikan dengan struktur folder Anda
# Pastikan jalur ini benar-benar mengarah ke folder jni utama
ROOT_JNI := $(LOCAL_PATH)

# --- ShadowHook Prebuilt ---
include $(CLEAR_VARS)
LOCAL_MODULE := shadowhook
# Sesuaikan jalur libshadowhook.so sesuai lokasi aslinya
LOCAL_SRC_FILES := $(ROOT_JNI)/3rdparty/shadowhook/libs/$(TARGET_ARCH_ABI)/libshadowhook.so
include $(PREBUILT_SHARED_LIBRARY)

# --- vMeer Engine Core ---
include $(CLEAR_VARS)
LOCAL_MODULE    := vmeer_engine

# Tambahkan jalur folder 'include' agar vm_internal.h ditemukan
LOCAL_C_INCLUDES := \
    $(ROOT_JNI) \
    $(ROOT_JNI)/include \
    $(ROOT_JNI)/bridge \
    $(ROOT_JNI)/binder \
    $(ROOT_JNI)/3rdparty/shadowhook/include

LOCAL_SRC_FILES := \
    vmeer_engine.cpp \
    vfs_hook.cpp \
    bridge/hook_bridge.cpp \
    binder/binder_rewriter.cpp

# Library yang dibutuhkan untuk linking
LOCAL_SHARED_LIBRARIES := shadowhook
LOCAL_LDLIBS    := -llog -landroid -lEGL -ldl

include $(BUILD_SHARED_LIBRARY)
