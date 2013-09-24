ifneq ($(TARGET_SIMULATOR),true)

LOCAL_PATH:= $(call my-dir)/../

include $(CLEAR_VARS)

LOCAL_CFLAGS += -Wall

LOCAL_LDLIBS := -L$(LOCAL_PATH)/lib -llog -g

LOCAL_C_INCLUDES += $(LOCAL_PATH)/include

LOCAL_SRC_FILES:= main.c \
                  btio.c \
                  att.c \
                  hcitool.c \

# Taken from BlueZ
LOCAL_SRC_FILES += jni/bluetooth.c\
                   jni/hci.c \


LOCAL_MODULE := amlink

include $(BUILD_EXECUTABLE)

endif  # TARGET_SIMULATOR != true
