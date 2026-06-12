LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_RENDERSCRIPT_COMPATIBILITY :=
LOCAL_MODULE := xenia-kernel
LOCAL_CPP_EXTENSION := .cc .cp .cxx .cpp .CPP .c++ .C
LOCAL_RS_EXTENSION := .rs .fs
ifeq ($(filter-out ABI=$(TARGET_ARCH_ABI) $(PREMAKE_ANDROIDNDK_CONFIGURATIONS_PREFIXED) $(PREMAKE_ANDROIDNDK_PLATFORMS_PREFIXED),ABI=arm64-v8a CONFIGURATION=Checked PLATFORM=Android-ARM64),)
  LOCAL_ARM_NEON := true
  LOCAL_CPPFLAGS := \
    -D_UNICODE \
    -DUNICODE \
    -D_LIB \
    -DDEBUG \
    -fvisibility=hidden \
    -std=c++17
  LOCAL_CPP_FEATURES := exceptions rtti
  LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/.. \
    $(LOCAL_PATH)/../src \
    $(LOCAL_PATH)/../third_party \
    $(LOCAL_PATH)/../third_party/glslang
  LOCAL_C_INCLUDES := $(call PREMAKE_ANDROIDNDK_SHELL_ESCAPE,$(LOCAL_C_INCLUDES))
  LOCAL_MODULE_FILENAME := libxenia-kernel
  LOCAL_SRC_FILES := \
    ../src/xenia/kernel/kernel_flags.cc \
    ../src/xenia/kernel/kernel_module.cc \
    ../src/xenia/kernel/kernel_state.cc \
    ../src/xenia/kernel/user_module.cc \
    ../src/xenia/kernel/util/gameinfo_utils.cc \
    ../src/xenia/kernel/util/native_list.cc \
    ../src/xenia/kernel/util/object_table.cc \
    ../src/xenia/kernel/util/shim_utils.cc \
    ../src/xenia/kernel/util/xdbf_utils.cc \
    ../src/xenia/kernel/xam/app_manager.cc \
    ../src/xenia/kernel/xam/apps/xam_app.cc \
    ../src/xenia/kernel/xam/apps/xgi_app.cc \
    ../src/xenia/kernel/xam/apps/xlivebase_app.cc \
    ../src/xenia/kernel/xam/apps/xmp_app.cc \
    ../src/xenia/kernel/xam/content_manager.cc \
    ../src/xenia/kernel/xam/user_profile.cc \
    ../src/xenia/kernel/xam/xam_avatar.cc \
    ../src/xenia/kernel/xam/xam_content.cc \
    ../src/xenia/kernel/xam/xam_content_aggregate.cc \
    ../src/xenia/kernel/xam/xam_content_device.cc \
    ../src/xenia/kernel/xam/xam_enum.cc \
    ../src/xenia/kernel/xam/xam_info.cc \
    ../src/xenia/kernel/xam/xam_input.cc \
    ../src/xenia/kernel/xam/xam_locale.cc \
    ../src/xenia/kernel/xam/xam_module.cc \
    ../src/xenia/kernel/xam/xam_msg.cc \
    ../src/xenia/kernel/xam/xam_net.cc \
    ../src/xenia/kernel/xam/xam_notify.cc \
    ../src/xenia/kernel/xam/xam_nui.cc \
    ../src/xenia/kernel/xam/xam_party.cc \
    ../src/xenia/kernel/xam/xam_task.cc \
    ../src/xenia/kernel/xam/xam_ui.cc \
    ../src/xenia/kernel/xam/xam_user.cc \
    ../src/xenia/kernel/xam/xam_video.cc \
    ../src/xenia/kernel/xam/xam_voice.cc \
    ../src/xenia/kernel/xbdm/xbdm_misc.cc \
    ../src/xenia/kernel/xbdm/xbdm_module.cc \
    ../src/xenia/kernel/xboxkrnl/cert_monitor.cc \
    ../src/xenia/kernel/xboxkrnl/debug_monitor.cc \
    ../src/xenia/kernel/xboxkrnl/xboxkrnl_audio.cc \
    ../src/xenia/kernel/xboxkrnl/xboxkrnl_audio_xma.cc \
    ../src/xenia/kernel/xboxkrnl/xboxkrnl_crypt.cc \
    ../src/xenia/kernel/xboxkrnl/xboxkrnl_debug.cc \
    ../src/xenia/kernel/xboxkrnl/xboxkrnl_error.cc \
    ../src/xenia/kernel/xboxkrnl/xboxkrnl_hal.cc \
    ../src/xenia/kernel/xboxkrnl/xboxkrnl_hid.cc \
    ../src/xenia/kernel/xboxkrnl/xboxkrnl_io.cc \
    ../src/xenia/kernel/xboxkrnl/xboxkrnl_io_info.cc \
    ../src/xenia/kernel/xboxkrnl/xboxkrnl_memory.cc \
    ../src/xenia/kernel/xboxkrnl/xboxkrnl_misc.cc \
    ../src/xenia/kernel/xboxkrnl/xboxkrnl_module.cc \
    ../src/xenia/kernel/xboxkrnl/xboxkrnl_modules.cc \
    ../src/xenia/kernel/xboxkrnl/xboxkrnl_ob.cc \
    ../src/xenia/kernel/xboxkrnl/xboxkrnl_rtl.cc \
    ../src/xenia/kernel/xboxkrnl/xboxkrnl_strings.cc \
    ../src/xenia/kernel/xboxkrnl/xboxkrnl_threading.cc \
    ../src/xenia/kernel/xboxkrnl/xboxkrnl_usbcam.cc \
    ../src/xenia/kernel/xboxkrnl/xboxkrnl_video.cc \
    ../src/xenia/kernel/xboxkrnl/xboxkrnl_xconfig.cc \
    ../src/xenia/kernel/xenumerator.cc \
    ../src/xenia/kernel/xevent.cc \
    ../src/xenia/kernel/xfile.cc \
    ../src/xenia/kernel/xiocompletion.cc \
    ../src/xenia/kernel/xmodule.cc \
    ../src/xenia/kernel/xmutant.cc \
    ../src/xenia/kernel/xnotifylistener.cc \
    ../src/xenia/kernel/xobject.cc \
    ../src/xenia/kernel/xsemaphore.cc \
    ../src/xenia/kernel/xsocket.cc \
    ../src/xenia/kernel/xsymboliclink.cc \
    ../src/xenia/kernel/xthread.cc \
    ../src/xenia/kernel/xtimer.cc
  LOCAL_STATIC_LIBRARIES := \
    aes_128 \
    fmt \
    xenia-apu \
    xenia-base \
    xenia-cpu \
    xenia-hid \
    xenia-vfs
  include $(BUILD_STATIC_LIBRARY)
else ifeq ($(filter-out ABI=$(TARGET_ARCH_ABI) $(PREMAKE_ANDROIDNDK_CONFIGURATIONS_PREFIXED) $(PREMAKE_ANDROIDNDK_PLATFORMS_PREFIXED),ABI=arm64-v8a CONFIGURATION=Debug PLATFORM=Android-ARM64),)
  LOCAL_ARM_NEON := true
  LOCAL_CPPFLAGS := \
    -D_UNICODE \
    -DUNICODE \
    -D_LIB \
    -DDEBUG \
    -D_NO_DEBUG_HEAP=1 \
    -fvisibility=hidden \
    -std=c++17
  LOCAL_CPP_FEATURES := exceptions rtti
  LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/.. \
    $(LOCAL_PATH)/../src \
    $(LOCAL_PATH)/../third_party \
    $(LOCAL_PATH)/../third_party/glslang
  LOCAL_C_INCLUDES := $(call PREMAKE_ANDROIDNDK_SHELL_ESCAPE,$(LOCAL_C_INCLUDES))
  LOCAL_MODULE_FILENAME := libxenia-kernel
  LOCAL_SRC_FILES := \
    ../src/xenia/kernel/kernel_flags.cc \
    ../src/xenia/kernel/kernel_module.cc \
    ../src/xenia/kernel/kernel_state.cc \
    ../src/xenia/kernel/user_module.cc \
    ../src/xenia/kernel/util/gameinfo_utils.cc \
    ../src/xenia/kernel/util/native_list.cc \
    ../src/xenia/kernel/util/object_table.cc \
    ../src/xenia/kernel/util/shim_utils.cc \
    ../src/xenia/kernel/util/xdbf_utils.cc \
    ../src/xenia/kernel/xam/app_manager.cc \
    ../src/xenia/kernel/xam/apps/xam_app.cc \
    ../src/xenia/kernel/xam/apps/xgi_app.cc \
    ../src/xenia/kernel/xam/apps/xlivebase_app.cc \
    ../src/xenia/kernel/xam/apps/xmp_app.cc \
    ../src/xenia/kernel/xam/content_manager.cc \
    ../src/xenia/kernel/xam/user_profile.cc \
    ../src/xenia/kernel/xam/xam_avatar.cc \
    ../src/xenia/kernel/xam/xam_content.cc \
    ../src/xenia/kernel/xam/xam_content_aggregate.cc \
    ../src/xenia/kernel/xam/xam_content_device.cc \
    ../src/xenia/kernel/xam/xam_enum.cc \
    ../src/xenia/kernel/xam/xam_info.cc \
    ../src/xenia/kernel/xam/xam_input.cc \
    ../src/xenia/kernel/xam/xam_locale.cc \
    ../src/xenia/kernel/xam/xam_module.cc \
    ../src/xenia/kernel/xam/xam_msg.cc \
    ../src/xenia/kernel/xam/xam_net.cc \
    ../src/xenia/kernel/xam/xam_notify.cc \
    ../src/xenia/kernel/xam/xam_nui.cc \
    ../src/xenia/kernel/xam/xam_party.cc \
    ../src/xenia/kernel/xam/xam_task.cc \
    ../src/xenia/kernel/xam/xam_ui.cc \
    ../src/xenia/kernel/xam/xam_user.cc \
    ../src/xenia/kernel/xam/xam_video.cc \
    ../src/xenia/kernel/xam/xam_voice.cc \
    ../src/xenia/kernel/xbdm/xbdm_misc.cc \
    ../src/xenia/kernel/xbdm/xbdm_module.cc \
    ../src/xenia/kernel/xboxkrnl/cert_monitor.cc \
    ../src/xenia/kernel/xboxkrnl/debug_monitor.cc \
    ../src/xenia/kernel/xboxkrnl/xboxkrnl_audio.cc \
    ../src/xenia/kernel/xboxkrnl/xboxkrnl_audio_xma.cc \
    ../src/xenia/kernel/xboxkrnl/xboxkrnl_crypt.cc \
    ../src/xenia/kernel/xboxkrnl/xboxkrnl_debug.cc \
    ../src/xenia/kernel/xboxkrnl/xboxkrnl_error.cc \
    ../src/xenia/kernel/xboxkrnl/xboxkrnl_hal.cc \
    ../src/xenia/kernel/xboxkrnl/xboxkrnl_hid.cc \
    ../src/xenia/kernel/xboxkrnl/xboxkrnl_io.cc \
    ../src/xenia/kernel/xboxkrnl/xboxkrnl_io_info.cc \
    ../src/xenia/kernel/xboxkrnl/xboxkrnl_memory.cc \
    ../src/xenia/kernel/xboxkrnl/xboxkrnl_misc.cc \
    ../src/xenia/kernel/xboxkrnl/xboxkrnl_module.cc \
    ../src/xenia/kernel/xboxkrnl/xboxkrnl_modules.cc \
    ../src/xenia/kernel/xboxkrnl/xboxkrnl_ob.cc \
    ../src/xenia/kernel/xboxkrnl/xboxkrnl_rtl.cc \
    ../src/xenia/kernel/xboxkrnl/xboxkrnl_strings.cc \
    ../src/xenia/kernel/xboxkrnl/xboxkrnl_threading.cc \
    ../src/xenia/kernel/xboxkrnl/xboxkrnl_usbcam.cc \
    ../src/xenia/kernel/xboxkrnl/xboxkrnl_video.cc \
    ../src/xenia/kernel/xboxkrnl/xboxkrnl_xconfig.cc \
    ../src/xenia/kernel/xenumerator.cc \
    ../src/xenia/kernel/xevent.cc \
    ../src/xenia/kernel/xfile.cc \
    ../src/xenia/kernel/xiocompletion.cc \
    ../src/xenia/kernel/xmodule.cc \
    ../src/xenia/kernel/xmutant.cc \
    ../src/xenia/kernel/xnotifylistener.cc \
    ../src/xenia/kernel/xobject.cc \
    ../src/xenia/kernel/xsemaphore.cc \
    ../src/xenia/kernel/xsocket.cc \
    ../src/xenia/kernel/xsymboliclink.cc \
    ../src/xenia/kernel/xthread.cc \
    ../src/xenia/kernel/xtimer.cc
  LOCAL_STATIC_LIBRARIES := \
    aes_128 \
    fmt \
    xenia-apu \
    xenia-base \
    xenia-cpu \
    xenia-hid \
    xenia-vfs
  include $(BUILD_STATIC_LIBRARY)
else ifeq ($(filter-out ABI=$(TARGET_ARCH_ABI) $(PREMAKE_ANDROIDNDK_CONFIGURATIONS_PREFIXED) $(PREMAKE_ANDROIDNDK_PLATFORMS_PREFIXED),ABI=arm64-v8a CONFIGURATION=Release PLATFORM=Android-ARM64),)
  LOCAL_ARM_NEON := true
  LOCAL_CPPFLAGS := \
    -D_UNICODE \
    -DUNICODE \
    -D_LIB \
    -DNDEBUG \
    -D_NO_DEBUG_HEAP=1 \
    -fvisibility=hidden \
    -std=c++17
  LOCAL_CPP_FEATURES := exceptions rtti
  LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/.. \
    $(LOCAL_PATH)/../src \
    $(LOCAL_PATH)/../third_party \
    $(LOCAL_PATH)/../third_party/glslang
  LOCAL_C_INCLUDES := $(call PREMAKE_ANDROIDNDK_SHELL_ESCAPE,$(LOCAL_C_INCLUDES))
  LOCAL_MODULE_FILENAME := libxenia-kernel
  LOCAL_SRC_FILES := \
    ../src/xenia/kernel/kernel_flags.cc \
    ../src/xenia/kernel/kernel_module.cc \
    ../src/xenia/kernel/kernel_state.cc \
    ../src/xenia/kernel/user_module.cc \
    ../src/xenia/kernel/util/gameinfo_utils.cc \
    ../src/xenia/kernel/util/native_list.cc \
    ../src/xenia/kernel/util/object_table.cc \
    ../src/xenia/kernel/util/shim_utils.cc \
    ../src/xenia/kernel/util/xdbf_utils.cc \
    ../src/xenia/kernel/xam/app_manager.cc \
    ../src/xenia/kernel/xam/apps/xam_app.cc \
    ../src/xenia/kernel/xam/apps/xgi_app.cc \
    ../src/xenia/kernel/xam/apps/xlivebase_app.cc \
    ../src/xenia/kernel/xam/apps/xmp_app.cc \
    ../src/xenia/kernel/xam/content_manager.cc \
    ../src/xenia/kernel/xam/user_profile.cc \
    ../src/xenia/kernel/xam/xam_avatar.cc \
    ../src/xenia/kernel/xam/xam_content.cc \
    ../src/xenia/kernel/xam/xam_content_aggregate.cc \
    ../src/xenia/kernel/xam/xam_content_device.cc \
    ../src/xenia/kernel/xam/xam_enum.cc \
    ../src/xenia/kernel/xam/xam_info.cc \
    ../src/xenia/kernel/xam/xam_input.cc \
    ../src/xenia/kernel/xam/xam_locale.cc \
    ../src/xenia/kernel/xam/xam_module.cc \
    ../src/xenia/kernel/xam/xam_msg.cc \
    ../src/xenia/kernel/xam/xam_net.cc \
    ../src/xenia/kernel/xam/xam_notify.cc \
    ../src/xenia/kernel/xam/xam_nui.cc \
    ../src/xenia/kernel/xam/xam_party.cc \
    ../src/xenia/kernel/xam/xam_task.cc \
    ../src/xenia/kernel/xam/xam_ui.cc \
    ../src/xenia/kernel/xam/xam_user.cc \
    ../src/xenia/kernel/xam/xam_video.cc \
    ../src/xenia/kernel/xam/xam_voice.cc \
    ../src/xenia/kernel/xbdm/xbdm_misc.cc \
    ../src/xenia/kernel/xbdm/xbdm_module.cc \
    ../src/xenia/kernel/xboxkrnl/cert_monitor.cc \
    ../src/xenia/kernel/xboxkrnl/debug_monitor.cc \
    ../src/xenia/kernel/xboxkrnl/xboxkrnl_audio.cc \
    ../src/xenia/kernel/xboxkrnl/xboxkrnl_audio_xma.cc \
    ../src/xenia/kernel/xboxkrnl/xboxkrnl_crypt.cc \
    ../src/xenia/kernel/xboxkrnl/xboxkrnl_debug.cc \
    ../src/xenia/kernel/xboxkrnl/xboxkrnl_error.cc \
    ../src/xenia/kernel/xboxkrnl/xboxkrnl_hal.cc \
    ../src/xenia/kernel/xboxkrnl/xboxkrnl_hid.cc \
    ../src/xenia/kernel/xboxkrnl/xboxkrnl_io.cc \
    ../src/xenia/kernel/xboxkrnl/xboxkrnl_io_info.cc \
    ../src/xenia/kernel/xboxkrnl/xboxkrnl_memory.cc \
    ../src/xenia/kernel/xboxkrnl/xboxkrnl_misc.cc \
    ../src/xenia/kernel/xboxkrnl/xboxkrnl_module.cc \
    ../src/xenia/kernel/xboxkrnl/xboxkrnl_modules.cc \
    ../src/xenia/kernel/xboxkrnl/xboxkrnl_ob.cc \
    ../src/xenia/kernel/xboxkrnl/xboxkrnl_rtl.cc \
    ../src/xenia/kernel/xboxkrnl/xboxkrnl_strings.cc \
    ../src/xenia/kernel/xboxkrnl/xboxkrnl_threading.cc \
    ../src/xenia/kernel/xboxkrnl/xboxkrnl_usbcam.cc \
    ../src/xenia/kernel/xboxkrnl/xboxkrnl_video.cc \
    ../src/xenia/kernel/xboxkrnl/xboxkrnl_xconfig.cc \
    ../src/xenia/kernel/xenumerator.cc \
    ../src/xenia/kernel/xevent.cc \
    ../src/xenia/kernel/xfile.cc \
    ../src/xenia/kernel/xiocompletion.cc \
    ../src/xenia/kernel/xmodule.cc \
    ../src/xenia/kernel/xmutant.cc \
    ../src/xenia/kernel/xnotifylistener.cc \
    ../src/xenia/kernel/xobject.cc \
    ../src/xenia/kernel/xsemaphore.cc \
    ../src/xenia/kernel/xsocket.cc \
    ../src/xenia/kernel/xsymboliclink.cc \
    ../src/xenia/kernel/xthread.cc \
    ../src/xenia/kernel/xtimer.cc
  LOCAL_STATIC_LIBRARIES := \
    aes_128 \
    fmt \
    xenia-apu \
    xenia-base \
    xenia-cpu \
    xenia-hid \
    xenia-vfs
  include $(BUILD_STATIC_LIBRARY)
else ifeq ($(filter-out ABI=$(TARGET_ARCH_ABI) $(PREMAKE_ANDROIDNDK_CONFIGURATIONS_PREFIXED) $(PREMAKE_ANDROIDNDK_PLATFORMS_PREFIXED),ABI=x86_64 CONFIGURATION=Checked PLATFORM=Android-x86_64),)
  LOCAL_ARM_NEON := true
  LOCAL_CPPFLAGS := \
    -D_UNICODE \
    -DUNICODE \
    -D_LIB \
    -DDEBUG \
    -fvisibility=hidden \
    -std=c++17
  LOCAL_CONLYFLAGS += \
    -mavx
  LOCAL_CPPFLAGS += \
    -mavx
  LOCAL_CPP_FEATURES := exceptions rtti
  LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/.. \
    $(LOCAL_PATH)/../src \
    $(LOCAL_PATH)/../third_party \
    $(LOCAL_PATH)/../third_party/glslang
  LOCAL_C_INCLUDES := $(call PREMAKE_ANDROIDNDK_SHELL_ESCAPE,$(LOCAL_C_INCLUDES))
  LOCAL_MODULE_FILENAME := libxenia-kernel
  LOCAL_SRC_FILES := \
    ../src/xenia/kernel/kernel_flags.cc \
    ../src/xenia/kernel/kernel_module.cc \
    ../src/xenia/kernel/kernel_state.cc \
    ../src/xenia/kernel/user_module.cc \
    ../src/xenia/kernel/util/gameinfo_utils.cc \
    ../src/xenia/kernel/util/native_list.cc \
    ../src/xenia/kernel/util/object_table.cc \
    ../src/xenia/kernel/util/shim_utils.cc \
    ../src/xenia/kernel/util/xdbf_utils.cc \
    ../src/xenia/kernel/xam/app_manager.cc \
    ../src/xenia/kernel/xam/apps/xam_app.cc \
    ../src/xenia/kernel/xam/apps/xgi_app.cc \
    ../src/xenia/kernel/xam/apps/xlivebase_app.cc \
    ../src/xenia/kernel/xam/apps/xmp_app.cc \
    ../src/xenia/kernel/xam/content_manager.cc \
    ../src/xenia/kernel/xam/user_profile.cc \
    ../src/xenia/kernel/xam/xam_avatar.cc \
    ../src/xenia/kernel/xam/xam_content.cc \
    ../src/xenia/kernel/xam/xam_content_aggregate.cc \
    ../src/xenia/kernel/xam/xam_content_device.cc \
    ../src/xenia/kernel/xam/xam_enum.cc \
    ../src/xenia/kernel/xam/xam_info.cc \
    ../src/xenia/kernel/xam/xam_input.cc \
    ../src/xenia/kernel/xam/xam_locale.cc \
    ../src/xenia/kernel/xam/xam_module.cc \
    ../src/xenia/kernel/xam/xam_msg.cc \
    ../src/xenia/kernel/xam/xam_net.cc \
    ../src/xenia/kernel/xam/xam_notify.cc \
    ../src/xenia/kernel/xam/xam_nui.cc \
    ../src/xenia/kernel/xam/xam_party.cc \
    ../src/xenia/kernel/xam/xam_task.cc \
    ../src/xenia/kernel/xam/xam_ui.cc \
    ../src/xenia/kernel/xam/xam_user.cc \
    ../src/xenia/kernel/xam/xam_video.cc \
    ../src/xenia/kernel/xam/xam_voice.cc \
    ../src/xenia/kernel/xbdm/xbdm_misc.cc \
    ../src/xenia/kernel/xbdm/xbdm_module.cc \
    ../src/xenia/kernel/xboxkrnl/cert_monitor.cc \
    ../src/xenia/kernel/xboxkrnl/debug_monitor.cc \
    ../src/xenia/kernel/xboxkrnl/xboxkrnl_audio.cc \
    ../src/xenia/kernel/xboxkrnl/xboxkrnl_audio_xma.cc \
    ../src/xenia/kernel/xboxkrnl/xboxkrnl_crypt.cc \
    ../src/xenia/kernel/xboxkrnl/xboxkrnl_debug.cc \
    ../src/xenia/kernel/xboxkrnl/xboxkrnl_error.cc \
    ../src/xenia/kernel/xboxkrnl/xboxkrnl_hal.cc \
    ../src/xenia/kernel/xboxkrnl/xboxkrnl_hid.cc \
    ../src/xenia/kernel/xboxkrnl/xboxkrnl_io.cc \
    ../src/xenia/kernel/xboxkrnl/xboxkrnl_io_info.cc \
    ../src/xenia/kernel/xboxkrnl/xboxkrnl_memory.cc \
    ../src/xenia/kernel/xboxkrnl/xboxkrnl_misc.cc \
    ../src/xenia/kernel/xboxkrnl/xboxkrnl_module.cc \
    ../src/xenia/kernel/xboxkrnl/xboxkrnl_modules.cc \
    ../src/xenia/kernel/xboxkrnl/xboxkrnl_ob.cc \
    ../src/xenia/kernel/xboxkrnl/xboxkrnl_rtl.cc \
    ../src/xenia/kernel/xboxkrnl/xboxkrnl_strings.cc \
    ../src/xenia/kernel/xboxkrnl/xboxkrnl_threading.cc \
    ../src/xenia/kernel/xboxkrnl/xboxkrnl_usbcam.cc \
    ../src/xenia/kernel/xboxkrnl/xboxkrnl_video.cc \
    ../src/xenia/kernel/xboxkrnl/xboxkrnl_xconfig.cc \
    ../src/xenia/kernel/xenumerator.cc \
    ../src/xenia/kernel/xevent.cc \
    ../src/xenia/kernel/xfile.cc \
    ../src/xenia/kernel/xiocompletion.cc \
    ../src/xenia/kernel/xmodule.cc \
    ../src/xenia/kernel/xmutant.cc \
    ../src/xenia/kernel/xnotifylistener.cc \
    ../src/xenia/kernel/xobject.cc \
    ../src/xenia/kernel/xsemaphore.cc \
    ../src/xenia/kernel/xsocket.cc \
    ../src/xenia/kernel/xsymboliclink.cc \
    ../src/xenia/kernel/xthread.cc \
    ../src/xenia/kernel/xtimer.cc
  LOCAL_STATIC_LIBRARIES := \
    aes_128 \
    fmt \
    xenia-apu \
    xenia-base \
    xenia-cpu \
    xenia-hid \
    xenia-vfs
  include $(BUILD_STATIC_LIBRARY)
else ifeq ($(filter-out ABI=$(TARGET_ARCH_ABI) $(PREMAKE_ANDROIDNDK_CONFIGURATIONS_PREFIXED) $(PREMAKE_ANDROIDNDK_PLATFORMS_PREFIXED),ABI=x86_64 CONFIGURATION=Debug PLATFORM=Android-x86_64),)
  LOCAL_ARM_NEON := true
  LOCAL_CPPFLAGS := \
    -D_UNICODE \
    -DUNICODE \
    -D_LIB \
    -DDEBUG \
    -D_NO_DEBUG_HEAP=1 \
    -fvisibility=hidden \
    -std=c++17
  LOCAL_CONLYFLAGS += \
    -mavx
  LOCAL_CPPFLAGS += \
    -mavx
  LOCAL_CPP_FEATURES := exceptions rtti
  LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/.. \
    $(LOCAL_PATH)/../src \
    $(LOCAL_PATH)/../third_party \
    $(LOCAL_PATH)/../third_party/glslang
  LOCAL_C_INCLUDES := $(call PREMAKE_ANDROIDNDK_SHELL_ESCAPE,$(LOCAL_C_INCLUDES))
  LOCAL_MODULE_FILENAME := libxenia-kernel
  LOCAL_SRC_FILES := \
    ../src/xenia/kernel/kernel_flags.cc \
    ../src/xenia/kernel/kernel_module.cc \
    ../src/xenia/kernel/kernel_state.cc \
    ../src/xenia/kernel/user_module.cc \
    ../src/xenia/kernel/util/gameinfo_utils.cc \
    ../src/xenia/kernel/util/native_list.cc \
    ../src/xenia/kernel/util/object_table.cc \
    ../src/xenia/kernel/util/shim_utils.cc \
    ../src/xenia/kernel/util/xdbf_utils.cc \
    ../src/xenia/kernel/xam/app_manager.cc \
    ../src/xenia/kernel/xam/apps/xam_app.cc \
    ../src/xenia/kernel/xam/apps/xgi_app.cc \
    ../src/xenia/kernel/xam/apps/xlivebase_app.cc \
    ../src/xenia/kernel/xam/apps/xmp_app.cc \
    ../src/xenia/kernel/xam/content_manager.cc \
    ../src/xenia/kernel/xam/user_profile.cc \
    ../src/xenia/kernel/xam/xam_avatar.cc \
    ../src/xenia/kernel/xam/xam_content.cc \
    ../src/xenia/kernel/xam/xam_content_aggregate.cc \
    ../src/xenia/kernel/xam/xam_content_device.cc \
    ../src/xenia/kernel/xam/xam_enum.cc \
    ../src/xenia/kernel/xam/xam_info.cc \
    ../src/xenia/kernel/xam/xam_input.cc \
    ../src/xenia/kernel/xam/xam_locale.cc \
    ../src/xenia/kernel/xam/xam_module.cc \
    ../src/xenia/kernel/xam/xam_msg.cc \
    ../src/xenia/kernel/xam/xam_net.cc \
    ../src/xenia/kernel/xam/xam_notify.cc \
    ../src/xenia/kernel/xam/xam_nui.cc \
    ../src/xenia/kernel/xam/xam_party.cc \
    ../src/xenia/kernel/xam/xam_task.cc \
    ../src/xenia/kernel/xam/xam_ui.cc \
    ../src/xenia/kernel/xam/xam_user.cc \
    ../src/xenia/kernel/xam/xam_video.cc \
    ../src/xenia/kernel/xam/xam_voice.cc \
    ../src/xenia/kernel/xbdm/xbdm_misc.cc \
    ../src/xenia/kernel/xbdm/xbdm_module.cc \
    ../src/xenia/kernel/xboxkrnl/cert_monitor.cc \
    ../src/xenia/kernel/xboxkrnl/debug_monitor.cc \
    ../src/xenia/kernel/xboxkrnl/xboxkrnl_audio.cc \
    ../src/xenia/kernel/xboxkrnl/xboxkrnl_audio_xma.cc \
    ../src/xenia/kernel/xboxkrnl/xboxkrnl_crypt.cc \
    ../src/xenia/kernel/xboxkrnl/xboxkrnl_debug.cc \
    ../src/xenia/kernel/xboxkrnl/xboxkrnl_error.cc \
    ../src/xenia/kernel/xboxkrnl/xboxkrnl_hal.cc \
    ../src/xenia/kernel/xboxkrnl/xboxkrnl_hid.cc \
    ../src/xenia/kernel/xboxkrnl/xboxkrnl_io.cc \
    ../src/xenia/kernel/xboxkrnl/xboxkrnl_io_info.cc \
    ../src/xenia/kernel/xboxkrnl/xboxkrnl_memory.cc \
    ../src/xenia/kernel/xboxkrnl/xboxkrnl_misc.cc \
    ../src/xenia/kernel/xboxkrnl/xboxkrnl_module.cc \
    ../src/xenia/kernel/xboxkrnl/xboxkrnl_modules.cc \
    ../src/xenia/kernel/xboxkrnl/xboxkrnl_ob.cc \
    ../src/xenia/kernel/xboxkrnl/xboxkrnl_rtl.cc \
    ../src/xenia/kernel/xboxkrnl/xboxkrnl_strings.cc \
    ../src/xenia/kernel/xboxkrnl/xboxkrnl_threading.cc \
    ../src/xenia/kernel/xboxkrnl/xboxkrnl_usbcam.cc \
    ../src/xenia/kernel/xboxkrnl/xboxkrnl_video.cc \
    ../src/xenia/kernel/xboxkrnl/xboxkrnl_xconfig.cc \
    ../src/xenia/kernel/xenumerator.cc \
    ../src/xenia/kernel/xevent.cc \
    ../src/xenia/kernel/xfile.cc \
    ../src/xenia/kernel/xiocompletion.cc \
    ../src/xenia/kernel/xmodule.cc \
    ../src/xenia/kernel/xmutant.cc \
    ../src/xenia/kernel/xnotifylistener.cc \
    ../src/xenia/kernel/xobject.cc \
    ../src/xenia/kernel/xsemaphore.cc \
    ../src/xenia/kernel/xsocket.cc \
    ../src/xenia/kernel/xsymboliclink.cc \
    ../src/xenia/kernel/xthread.cc \
    ../src/xenia/kernel/xtimer.cc
  LOCAL_STATIC_LIBRARIES := \
    aes_128 \
    fmt \
    xenia-apu \
    xenia-base \
    xenia-cpu \
    xenia-hid \
    xenia-vfs
  include $(BUILD_STATIC_LIBRARY)
else ifeq ($(filter-out ABI=$(TARGET_ARCH_ABI) $(PREMAKE_ANDROIDNDK_CONFIGURATIONS_PREFIXED) $(PREMAKE_ANDROIDNDK_PLATFORMS_PREFIXED),ABI=x86_64 CONFIGURATION=Release PLATFORM=Android-x86_64),)
  LOCAL_ARM_NEON := true
  LOCAL_CPPFLAGS := \
    -D_UNICODE \
    -DUNICODE \
    -D_LIB \
    -DNDEBUG \
    -D_NO_DEBUG_HEAP=1 \
    -fvisibility=hidden \
    -std=c++17
  LOCAL_CONLYFLAGS += \
    -mavx
  LOCAL_CPPFLAGS += \
    -mavx
  LOCAL_CPP_FEATURES := exceptions rtti
  LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/.. \
    $(LOCAL_PATH)/../src \
    $(LOCAL_PATH)/../third_party \
    $(LOCAL_PATH)/../third_party/glslang
  LOCAL_C_INCLUDES := $(call PREMAKE_ANDROIDNDK_SHELL_ESCAPE,$(LOCAL_C_INCLUDES))
  LOCAL_MODULE_FILENAME := libxenia-kernel
  LOCAL_SRC_FILES := \
    ../src/xenia/kernel/kernel_flags.cc \
    ../src/xenia/kernel/kernel_module.cc \
    ../src/xenia/kernel/kernel_state.cc \
    ../src/xenia/kernel/user_module.cc \
    ../src/xenia/kernel/util/gameinfo_utils.cc \
    ../src/xenia/kernel/util/native_list.cc \
    ../src/xenia/kernel/util/object_table.cc \
    ../src/xenia/kernel/util/shim_utils.cc \
    ../src/xenia/kernel/util/xdbf_utils.cc \
    ../src/xenia/kernel/xam/app_manager.cc \
    ../src/xenia/kernel/xam/apps/xam_app.cc \
    ../src/xenia/kernel/xam/apps/xgi_app.cc \
    ../src/xenia/kernel/xam/apps/xlivebase_app.cc \
    ../src/xenia/kernel/xam/apps/xmp_app.cc \
    ../src/xenia/kernel/xam/content_manager.cc \
    ../src/xenia/kernel/xam/user_profile.cc \
    ../src/xenia/kernel/xam/xam_avatar.cc \
    ../src/xenia/kernel/xam/xam_content.cc \
    ../src/xenia/kernel/xam/xam_content_aggregate.cc \
    ../src/xenia/kernel/xam/xam_content_device.cc \
    ../src/xenia/kernel/xam/xam_enum.cc \
    ../src/xenia/kernel/xam/xam_info.cc \
    ../src/xenia/kernel/xam/xam_input.cc \
    ../src/xenia/kernel/xam/xam_locale.cc \
    ../src/xenia/kernel/xam/xam_module.cc \
    ../src/xenia/kernel/xam/xam_msg.cc \
    ../src/xenia/kernel/xam/xam_net.cc \
    ../src/xenia/kernel/xam/xam_notify.cc \
    ../src/xenia/kernel/xam/xam_nui.cc \
    ../src/xenia/kernel/xam/xam_party.cc \
    ../src/xenia/kernel/xam/xam_task.cc \
    ../src/xenia/kernel/xam/xam_ui.cc \
    ../src/xenia/kernel/xam/xam_user.cc \
    ../src/xenia/kernel/xam/xam_video.cc \
    ../src/xenia/kernel/xam/xam_voice.cc \
    ../src/xenia/kernel/xbdm/xbdm_misc.cc \
    ../src/xenia/kernel/xbdm/xbdm_module.cc \
    ../src/xenia/kernel/xboxkrnl/cert_monitor.cc \
    ../src/xenia/kernel/xboxkrnl/debug_monitor.cc \
    ../src/xenia/kernel/xboxkrnl/xboxkrnl_audio.cc \
    ../src/xenia/kernel/xboxkrnl/xboxkrnl_audio_xma.cc \
    ../src/xenia/kernel/xboxkrnl/xboxkrnl_crypt.cc \
    ../src/xenia/kernel/xboxkrnl/xboxkrnl_debug.cc \
    ../src/xenia/kernel/xboxkrnl/xboxkrnl_error.cc \
    ../src/xenia/kernel/xboxkrnl/xboxkrnl_hal.cc \
    ../src/xenia/kernel/xboxkrnl/xboxkrnl_hid.cc \
    ../src/xenia/kernel/xboxkrnl/xboxkrnl_io.cc \
    ../src/xenia/kernel/xboxkrnl/xboxkrnl_io_info.cc \
    ../src/xenia/kernel/xboxkrnl/xboxkrnl_memory.cc \
    ../src/xenia/kernel/xboxkrnl/xboxkrnl_misc.cc \
    ../src/xenia/kernel/xboxkrnl/xboxkrnl_module.cc \
    ../src/xenia/kernel/xboxkrnl/xboxkrnl_modules.cc \
    ../src/xenia/kernel/xboxkrnl/xboxkrnl_ob.cc \
    ../src/xenia/kernel/xboxkrnl/xboxkrnl_rtl.cc \
    ../src/xenia/kernel/xboxkrnl/xboxkrnl_strings.cc \
    ../src/xenia/kernel/xboxkrnl/xboxkrnl_threading.cc \
    ../src/xenia/kernel/xboxkrnl/xboxkrnl_usbcam.cc \
    ../src/xenia/kernel/xboxkrnl/xboxkrnl_video.cc \
    ../src/xenia/kernel/xboxkrnl/xboxkrnl_xconfig.cc \
    ../src/xenia/kernel/xenumerator.cc \
    ../src/xenia/kernel/xevent.cc \
    ../src/xenia/kernel/xfile.cc \
    ../src/xenia/kernel/xiocompletion.cc \
    ../src/xenia/kernel/xmodule.cc \
    ../src/xenia/kernel/xmutant.cc \
    ../src/xenia/kernel/xnotifylistener.cc \
    ../src/xenia/kernel/xobject.cc \
    ../src/xenia/kernel/xsemaphore.cc \
    ../src/xenia/kernel/xsocket.cc \
    ../src/xenia/kernel/xsymboliclink.cc \
    ../src/xenia/kernel/xthread.cc \
    ../src/xenia/kernel/xtimer.cc
  LOCAL_STATIC_LIBRARIES := \
    aes_128 \
    fmt \
    xenia-apu \
    xenia-base \
    xenia-cpu \
    xenia-hid \
    xenia-vfs
  include $(BUILD_STATIC_LIBRARY)
endif