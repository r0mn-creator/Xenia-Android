LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_RENDERSCRIPT_COMPATIBILITY :=
LOCAL_MODULE := libavutil
LOCAL_CPP_EXTENSION := .cc .cp .cxx .cpp .CPP .c++ .C
LOCAL_RS_EXTENSION := .rs .fs
ifeq ($(filter-out ABI=$(TARGET_ARCH_ABI) $(PREMAKE_ANDROIDNDK_CONFIGURATIONS_PREFIXED) $(PREMAKE_ANDROIDNDK_PLATFORMS_PREFIXED),ABI=arm64-v8a CONFIGURATION=Checked PLATFORM=Android-ARM64),)
  LOCAL_ARM_NEON := true
  LOCAL_CONLYFLAGS := \
    -D_UNICODE \
    -DUNICODE \
    -D_LIB \
    -DDEBUG \
    -DHAVE_AV_CONFIG_H \
    -D_USE_MATH_DEFINES \
    -fvisibility=hidden \
    -w
  LOCAL_CPP_FEATURES := exceptions rtti
  LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/.. \
    $(LOCAL_PATH)/../src \
    $(LOCAL_PATH)/../third_party \
    $(LOCAL_PATH)/../third_party/glslang \
    $(LOCAL_PATH)/../third_party/FFmpeg
  LOCAL_C_INCLUDES := $(call PREMAKE_ANDROIDNDK_SHELL_ESCAPE,$(LOCAL_C_INCLUDES))
  LOCAL_MODULE_FILENAME := liblibavutil
  LOCAL_SRC_FILES := \
    ../third_party/FFmpeg/libavutil/aarch64/cpu.c \
    ../third_party/FFmpeg/libavutil/aarch64/float_dsp_init.c \
    ../third_party/FFmpeg/libavutil/aarch64/float_dsp_neon.S \
    ../third_party/FFmpeg/libavutil/adler32.c \
    ../third_party/FFmpeg/libavutil/aes.c \
    ../third_party/FFmpeg/libavutil/aes_ctr.c \
    ../third_party/FFmpeg/libavutil/audio_fifo.c \
    ../third_party/FFmpeg/libavutil/avsscanf.c \
    ../third_party/FFmpeg/libavutil/avstring.c \
    ../third_party/FFmpeg/libavutil/base64.c \
    ../third_party/FFmpeg/libavutil/blowfish.c \
    ../third_party/FFmpeg/libavutil/bprint.c \
    ../third_party/FFmpeg/libavutil/buffer.c \
    ../third_party/FFmpeg/libavutil/camellia.c \
    ../third_party/FFmpeg/libavutil/cast5.c \
    ../third_party/FFmpeg/libavutil/channel_layout.c \
    ../third_party/FFmpeg/libavutil/color_utils.c \
    ../third_party/FFmpeg/libavutil/cpu.c \
    ../third_party/FFmpeg/libavutil/crc.c \
    ../third_party/FFmpeg/libavutil/des.c \
    ../third_party/FFmpeg/libavutil/dict.c \
    ../third_party/FFmpeg/libavutil/display.c \
    ../third_party/FFmpeg/libavutil/dovi_meta.c \
    ../third_party/FFmpeg/libavutil/downmix_info.c \
    ../third_party/FFmpeg/libavutil/encryption_info.c \
    ../third_party/FFmpeg/libavutil/error.c \
    ../third_party/FFmpeg/libavutil/eval.c \
    ../third_party/FFmpeg/libavutil/fifo.c \
    ../third_party/FFmpeg/libavutil/file.c \
    ../third_party/FFmpeg/libavutil/file_open.c \
    ../third_party/FFmpeg/libavutil/film_grain_params.c \
    ../third_party/FFmpeg/libavutil/fixed_dsp.c \
    ../third_party/FFmpeg/libavutil/float_dsp.c \
    ../third_party/FFmpeg/libavutil/frame.c \
    ../third_party/FFmpeg/libavutil/hash.c \
    ../third_party/FFmpeg/libavutil/hdr_dynamic_metadata.c \
    ../third_party/FFmpeg/libavutil/hmac.c \
    ../third_party/FFmpeg/libavutil/hwcontext.c \
    ../third_party/FFmpeg/libavutil/imgutils.c \
    ../third_party/FFmpeg/libavutil/integer.c \
    ../third_party/FFmpeg/libavutil/intmath.c \
    ../third_party/FFmpeg/libavutil/lfg.c \
    ../third_party/FFmpeg/libavutil/lls.c \
    ../third_party/FFmpeg/libavutil/log.c \
    ../third_party/FFmpeg/libavutil/log2_tab.c \
    ../third_party/FFmpeg/libavutil/mastering_display_metadata.c \
    ../third_party/FFmpeg/libavutil/mathematics.c \
    ../third_party/FFmpeg/libavutil/md5.c \
    ../third_party/FFmpeg/libavutil/mem.c \
    ../third_party/FFmpeg/libavutil/murmur3.c \
    ../third_party/FFmpeg/libavutil/opt.c \
    ../third_party/FFmpeg/libavutil/parseutils.c \
    ../third_party/FFmpeg/libavutil/pixdesc.c \
    ../third_party/FFmpeg/libavutil/pixelutils.c \
    ../third_party/FFmpeg/libavutil/random_seed.c \
    ../third_party/FFmpeg/libavutil/rational.c \
    ../third_party/FFmpeg/libavutil/rc4.c \
    ../third_party/FFmpeg/libavutil/reverse.c \
    ../third_party/FFmpeg/libavutil/ripemd.c \
    ../third_party/FFmpeg/libavutil/samplefmt.c \
    ../third_party/FFmpeg/libavutil/sha.c \
    ../third_party/FFmpeg/libavutil/sha512.c \
    ../third_party/FFmpeg/libavutil/slicethread.c \
    ../third_party/FFmpeg/libavutil/spherical.c \
    ../third_party/FFmpeg/libavutil/stereo3d.c \
    ../third_party/FFmpeg/libavutil/tea.c \
    ../third_party/FFmpeg/libavutil/threadmessage.c \
    ../third_party/FFmpeg/libavutil/time.c \
    ../third_party/FFmpeg/libavutil/timecode.c \
    ../third_party/FFmpeg/libavutil/tree.c \
    ../third_party/FFmpeg/libavutil/twofish.c \
    ../third_party/FFmpeg/libavutil/tx.c \
    ../third_party/FFmpeg/libavutil/tx_double.c \
    ../third_party/FFmpeg/libavutil/tx_float.c \
    ../third_party/FFmpeg/libavutil/tx_int32.c \
    ../third_party/FFmpeg/libavutil/utils.c \
    ../third_party/FFmpeg/libavutil/video_enc_params.c \
    ../third_party/FFmpeg/libavutil/xga_font_data.c \
    ../third_party/FFmpeg/libavutil/xtea.c
  include $(BUILD_STATIC_LIBRARY)
else ifeq ($(filter-out ABI=$(TARGET_ARCH_ABI) $(PREMAKE_ANDROIDNDK_CONFIGURATIONS_PREFIXED) $(PREMAKE_ANDROIDNDK_PLATFORMS_PREFIXED),ABI=arm64-v8a CONFIGURATION=Debug PLATFORM=Android-ARM64),)
  LOCAL_ARM_NEON := true
  LOCAL_CONLYFLAGS := \
    -D_UNICODE \
    -DUNICODE \
    -D_LIB \
    -DDEBUG \
    -D_NO_DEBUG_HEAP=1 \
    -DHAVE_AV_CONFIG_H \
    -D_USE_MATH_DEFINES \
    -fvisibility=hidden \
    -w
  LOCAL_CPP_FEATURES := exceptions rtti
  LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/.. \
    $(LOCAL_PATH)/../src \
    $(LOCAL_PATH)/../third_party \
    $(LOCAL_PATH)/../third_party/glslang \
    $(LOCAL_PATH)/../third_party/FFmpeg
  LOCAL_C_INCLUDES := $(call PREMAKE_ANDROIDNDK_SHELL_ESCAPE,$(LOCAL_C_INCLUDES))
  LOCAL_MODULE_FILENAME := liblibavutil
  LOCAL_SRC_FILES := \
    ../third_party/FFmpeg/libavutil/aarch64/cpu.c \
    ../third_party/FFmpeg/libavutil/aarch64/float_dsp_init.c \
    ../third_party/FFmpeg/libavutil/aarch64/float_dsp_neon.S \
    ../third_party/FFmpeg/libavutil/adler32.c \
    ../third_party/FFmpeg/libavutil/aes.c \
    ../third_party/FFmpeg/libavutil/aes_ctr.c \
    ../third_party/FFmpeg/libavutil/audio_fifo.c \
    ../third_party/FFmpeg/libavutil/avsscanf.c \
    ../third_party/FFmpeg/libavutil/avstring.c \
    ../third_party/FFmpeg/libavutil/base64.c \
    ../third_party/FFmpeg/libavutil/blowfish.c \
    ../third_party/FFmpeg/libavutil/bprint.c \
    ../third_party/FFmpeg/libavutil/buffer.c \
    ../third_party/FFmpeg/libavutil/camellia.c \
    ../third_party/FFmpeg/libavutil/cast5.c \
    ../third_party/FFmpeg/libavutil/channel_layout.c \
    ../third_party/FFmpeg/libavutil/color_utils.c \
    ../third_party/FFmpeg/libavutil/cpu.c \
    ../third_party/FFmpeg/libavutil/crc.c \
    ../third_party/FFmpeg/libavutil/des.c \
    ../third_party/FFmpeg/libavutil/dict.c \
    ../third_party/FFmpeg/libavutil/display.c \
    ../third_party/FFmpeg/libavutil/dovi_meta.c \
    ../third_party/FFmpeg/libavutil/downmix_info.c \
    ../third_party/FFmpeg/libavutil/encryption_info.c \
    ../third_party/FFmpeg/libavutil/error.c \
    ../third_party/FFmpeg/libavutil/eval.c \
    ../third_party/FFmpeg/libavutil/fifo.c \
    ../third_party/FFmpeg/libavutil/file.c \
    ../third_party/FFmpeg/libavutil/file_open.c \
    ../third_party/FFmpeg/libavutil/film_grain_params.c \
    ../third_party/FFmpeg/libavutil/fixed_dsp.c \
    ../third_party/FFmpeg/libavutil/float_dsp.c \
    ../third_party/FFmpeg/libavutil/frame.c \
    ../third_party/FFmpeg/libavutil/hash.c \
    ../third_party/FFmpeg/libavutil/hdr_dynamic_metadata.c \
    ../third_party/FFmpeg/libavutil/hmac.c \
    ../third_party/FFmpeg/libavutil/hwcontext.c \
    ../third_party/FFmpeg/libavutil/imgutils.c \
    ../third_party/FFmpeg/libavutil/integer.c \
    ../third_party/FFmpeg/libavutil/intmath.c \
    ../third_party/FFmpeg/libavutil/lfg.c \
    ../third_party/FFmpeg/libavutil/lls.c \
    ../third_party/FFmpeg/libavutil/log.c \
    ../third_party/FFmpeg/libavutil/log2_tab.c \
    ../third_party/FFmpeg/libavutil/mastering_display_metadata.c \
    ../third_party/FFmpeg/libavutil/mathematics.c \
    ../third_party/FFmpeg/libavutil/md5.c \
    ../third_party/FFmpeg/libavutil/mem.c \
    ../third_party/FFmpeg/libavutil/murmur3.c \
    ../third_party/FFmpeg/libavutil/opt.c \
    ../third_party/FFmpeg/libavutil/parseutils.c \
    ../third_party/FFmpeg/libavutil/pixdesc.c \
    ../third_party/FFmpeg/libavutil/pixelutils.c \
    ../third_party/FFmpeg/libavutil/random_seed.c \
    ../third_party/FFmpeg/libavutil/rational.c \
    ../third_party/FFmpeg/libavutil/rc4.c \
    ../third_party/FFmpeg/libavutil/reverse.c \
    ../third_party/FFmpeg/libavutil/ripemd.c \
    ../third_party/FFmpeg/libavutil/samplefmt.c \
    ../third_party/FFmpeg/libavutil/sha.c \
    ../third_party/FFmpeg/libavutil/sha512.c \
    ../third_party/FFmpeg/libavutil/slicethread.c \
    ../third_party/FFmpeg/libavutil/spherical.c \
    ../third_party/FFmpeg/libavutil/stereo3d.c \
    ../third_party/FFmpeg/libavutil/tea.c \
    ../third_party/FFmpeg/libavutil/threadmessage.c \
    ../third_party/FFmpeg/libavutil/time.c \
    ../third_party/FFmpeg/libavutil/timecode.c \
    ../third_party/FFmpeg/libavutil/tree.c \
    ../third_party/FFmpeg/libavutil/twofish.c \
    ../third_party/FFmpeg/libavutil/tx.c \
    ../third_party/FFmpeg/libavutil/tx_double.c \
    ../third_party/FFmpeg/libavutil/tx_float.c \
    ../third_party/FFmpeg/libavutil/tx_int32.c \
    ../third_party/FFmpeg/libavutil/utils.c \
    ../third_party/FFmpeg/libavutil/video_enc_params.c \
    ../third_party/FFmpeg/libavutil/xga_font_data.c \
    ../third_party/FFmpeg/libavutil/xtea.c
  include $(BUILD_STATIC_LIBRARY)
else ifeq ($(filter-out ABI=$(TARGET_ARCH_ABI) $(PREMAKE_ANDROIDNDK_CONFIGURATIONS_PREFIXED) $(PREMAKE_ANDROIDNDK_PLATFORMS_PREFIXED),ABI=arm64-v8a CONFIGURATION=Release PLATFORM=Android-ARM64),)
  LOCAL_ARM_NEON := true
  LOCAL_CONLYFLAGS := \
    -D_UNICODE \
    -DUNICODE \
    -D_LIB \
    -DNDEBUG \
    -D_NO_DEBUG_HEAP=1 \
    -DHAVE_AV_CONFIG_H \
    -D_USE_MATH_DEFINES \
    -fvisibility=hidden \
    -w
  LOCAL_CPP_FEATURES := exceptions rtti
  LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/.. \
    $(LOCAL_PATH)/../src \
    $(LOCAL_PATH)/../third_party \
    $(LOCAL_PATH)/../third_party/glslang \
    $(LOCAL_PATH)/../third_party/FFmpeg
  LOCAL_C_INCLUDES := $(call PREMAKE_ANDROIDNDK_SHELL_ESCAPE,$(LOCAL_C_INCLUDES))
  LOCAL_MODULE_FILENAME := liblibavutil
  LOCAL_SRC_FILES := \
    ../third_party/FFmpeg/libavutil/aarch64/cpu.c \
    ../third_party/FFmpeg/libavutil/aarch64/float_dsp_init.c \
    ../third_party/FFmpeg/libavutil/aarch64/float_dsp_neon.S \
    ../third_party/FFmpeg/libavutil/adler32.c \
    ../third_party/FFmpeg/libavutil/aes.c \
    ../third_party/FFmpeg/libavutil/aes_ctr.c \
    ../third_party/FFmpeg/libavutil/audio_fifo.c \
    ../third_party/FFmpeg/libavutil/avsscanf.c \
    ../third_party/FFmpeg/libavutil/avstring.c \
    ../third_party/FFmpeg/libavutil/base64.c \
    ../third_party/FFmpeg/libavutil/blowfish.c \
    ../third_party/FFmpeg/libavutil/bprint.c \
    ../third_party/FFmpeg/libavutil/buffer.c \
    ../third_party/FFmpeg/libavutil/camellia.c \
    ../third_party/FFmpeg/libavutil/cast5.c \
    ../third_party/FFmpeg/libavutil/channel_layout.c \
    ../third_party/FFmpeg/libavutil/color_utils.c \
    ../third_party/FFmpeg/libavutil/cpu.c \
    ../third_party/FFmpeg/libavutil/crc.c \
    ../third_party/FFmpeg/libavutil/des.c \
    ../third_party/FFmpeg/libavutil/dict.c \
    ../third_party/FFmpeg/libavutil/display.c \
    ../third_party/FFmpeg/libavutil/dovi_meta.c \
    ../third_party/FFmpeg/libavutil/downmix_info.c \
    ../third_party/FFmpeg/libavutil/encryption_info.c \
    ../third_party/FFmpeg/libavutil/error.c \
    ../third_party/FFmpeg/libavutil/eval.c \
    ../third_party/FFmpeg/libavutil/fifo.c \
    ../third_party/FFmpeg/libavutil/file.c \
    ../third_party/FFmpeg/libavutil/file_open.c \
    ../third_party/FFmpeg/libavutil/film_grain_params.c \
    ../third_party/FFmpeg/libavutil/fixed_dsp.c \
    ../third_party/FFmpeg/libavutil/float_dsp.c \
    ../third_party/FFmpeg/libavutil/frame.c \
    ../third_party/FFmpeg/libavutil/hash.c \
    ../third_party/FFmpeg/libavutil/hdr_dynamic_metadata.c \
    ../third_party/FFmpeg/libavutil/hmac.c \
    ../third_party/FFmpeg/libavutil/hwcontext.c \
    ../third_party/FFmpeg/libavutil/imgutils.c \
    ../third_party/FFmpeg/libavutil/integer.c \
    ../third_party/FFmpeg/libavutil/intmath.c \
    ../third_party/FFmpeg/libavutil/lfg.c \
    ../third_party/FFmpeg/libavutil/lls.c \
    ../third_party/FFmpeg/libavutil/log.c \
    ../third_party/FFmpeg/libavutil/log2_tab.c \
    ../third_party/FFmpeg/libavutil/mastering_display_metadata.c \
    ../third_party/FFmpeg/libavutil/mathematics.c \
    ../third_party/FFmpeg/libavutil/md5.c \
    ../third_party/FFmpeg/libavutil/mem.c \
    ../third_party/FFmpeg/libavutil/murmur3.c \
    ../third_party/FFmpeg/libavutil/opt.c \
    ../third_party/FFmpeg/libavutil/parseutils.c \
    ../third_party/FFmpeg/libavutil/pixdesc.c \
    ../third_party/FFmpeg/libavutil/pixelutils.c \
    ../third_party/FFmpeg/libavutil/random_seed.c \
    ../third_party/FFmpeg/libavutil/rational.c \
    ../third_party/FFmpeg/libavutil/rc4.c \
    ../third_party/FFmpeg/libavutil/reverse.c \
    ../third_party/FFmpeg/libavutil/ripemd.c \
    ../third_party/FFmpeg/libavutil/samplefmt.c \
    ../third_party/FFmpeg/libavutil/sha.c \
    ../third_party/FFmpeg/libavutil/sha512.c \
    ../third_party/FFmpeg/libavutil/slicethread.c \
    ../third_party/FFmpeg/libavutil/spherical.c \
    ../third_party/FFmpeg/libavutil/stereo3d.c \
    ../third_party/FFmpeg/libavutil/tea.c \
    ../third_party/FFmpeg/libavutil/threadmessage.c \
    ../third_party/FFmpeg/libavutil/time.c \
    ../third_party/FFmpeg/libavutil/timecode.c \
    ../third_party/FFmpeg/libavutil/tree.c \
    ../third_party/FFmpeg/libavutil/twofish.c \
    ../third_party/FFmpeg/libavutil/tx.c \
    ../third_party/FFmpeg/libavutil/tx_double.c \
    ../third_party/FFmpeg/libavutil/tx_float.c \
    ../third_party/FFmpeg/libavutil/tx_int32.c \
    ../third_party/FFmpeg/libavutil/utils.c \
    ../third_party/FFmpeg/libavutil/video_enc_params.c \
    ../third_party/FFmpeg/libavutil/xga_font_data.c \
    ../third_party/FFmpeg/libavutil/xtea.c
  include $(BUILD_STATIC_LIBRARY)
else ifeq ($(filter-out ABI=$(TARGET_ARCH_ABI) $(PREMAKE_ANDROIDNDK_CONFIGURATIONS_PREFIXED) $(PREMAKE_ANDROIDNDK_PLATFORMS_PREFIXED),ABI=x86_64 CONFIGURATION=Checked PLATFORM=Android-x86_64),)
  LOCAL_ARM_NEON := true
  LOCAL_CONLYFLAGS := \
    -D_UNICODE \
    -DUNICODE \
    -D_LIB \
    -DDEBUG \
    -DHAVE_AV_CONFIG_H \
    -D_USE_MATH_DEFINES \
    -fvisibility=hidden \
    -w
  LOCAL_CONLYFLAGS += \
    -mavx
  LOCAL_CPPFLAGS += \
    -mavx
  LOCAL_CPP_FEATURES := exceptions rtti
  LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/.. \
    $(LOCAL_PATH)/../src \
    $(LOCAL_PATH)/../third_party \
    $(LOCAL_PATH)/../third_party/glslang \
    $(LOCAL_PATH)/../third_party/FFmpeg
  LOCAL_C_INCLUDES := $(call PREMAKE_ANDROIDNDK_SHELL_ESCAPE,$(LOCAL_C_INCLUDES))
  LOCAL_MODULE_FILENAME := liblibavutil
  LOCAL_SRC_FILES := \
    ../third_party/FFmpeg/libavutil/adler32.c \
    ../third_party/FFmpeg/libavutil/aes.c \
    ../third_party/FFmpeg/libavutil/aes_ctr.c \
    ../third_party/FFmpeg/libavutil/audio_fifo.c \
    ../third_party/FFmpeg/libavutil/avsscanf.c \
    ../third_party/FFmpeg/libavutil/avstring.c \
    ../third_party/FFmpeg/libavutil/base64.c \
    ../third_party/FFmpeg/libavutil/blowfish.c \
    ../third_party/FFmpeg/libavutil/bprint.c \
    ../third_party/FFmpeg/libavutil/buffer.c \
    ../third_party/FFmpeg/libavutil/camellia.c \
    ../third_party/FFmpeg/libavutil/cast5.c \
    ../third_party/FFmpeg/libavutil/channel_layout.c \
    ../third_party/FFmpeg/libavutil/color_utils.c \
    ../third_party/FFmpeg/libavutil/cpu.c \
    ../third_party/FFmpeg/libavutil/crc.c \
    ../third_party/FFmpeg/libavutil/des.c \
    ../third_party/FFmpeg/libavutil/dict.c \
    ../third_party/FFmpeg/libavutil/display.c \
    ../third_party/FFmpeg/libavutil/dovi_meta.c \
    ../third_party/FFmpeg/libavutil/downmix_info.c \
    ../third_party/FFmpeg/libavutil/encryption_info.c \
    ../third_party/FFmpeg/libavutil/error.c \
    ../third_party/FFmpeg/libavutil/eval.c \
    ../third_party/FFmpeg/libavutil/fifo.c \
    ../third_party/FFmpeg/libavutil/file.c \
    ../third_party/FFmpeg/libavutil/file_open.c \
    ../third_party/FFmpeg/libavutil/film_grain_params.c \
    ../third_party/FFmpeg/libavutil/fixed_dsp.c \
    ../third_party/FFmpeg/libavutil/float_dsp.c \
    ../third_party/FFmpeg/libavutil/frame.c \
    ../third_party/FFmpeg/libavutil/hash.c \
    ../third_party/FFmpeg/libavutil/hdr_dynamic_metadata.c \
    ../third_party/FFmpeg/libavutil/hmac.c \
    ../third_party/FFmpeg/libavutil/hwcontext.c \
    ../third_party/FFmpeg/libavutil/imgutils.c \
    ../third_party/FFmpeg/libavutil/integer.c \
    ../third_party/FFmpeg/libavutil/intmath.c \
    ../third_party/FFmpeg/libavutil/lfg.c \
    ../third_party/FFmpeg/libavutil/lls.c \
    ../third_party/FFmpeg/libavutil/log.c \
    ../third_party/FFmpeg/libavutil/log2_tab.c \
    ../third_party/FFmpeg/libavutil/mastering_display_metadata.c \
    ../third_party/FFmpeg/libavutil/mathematics.c \
    ../third_party/FFmpeg/libavutil/md5.c \
    ../third_party/FFmpeg/libavutil/mem.c \
    ../third_party/FFmpeg/libavutil/murmur3.c \
    ../third_party/FFmpeg/libavutil/opt.c \
    ../third_party/FFmpeg/libavutil/parseutils.c \
    ../third_party/FFmpeg/libavutil/pixdesc.c \
    ../third_party/FFmpeg/libavutil/pixelutils.c \
    ../third_party/FFmpeg/libavutil/random_seed.c \
    ../third_party/FFmpeg/libavutil/rational.c \
    ../third_party/FFmpeg/libavutil/rc4.c \
    ../third_party/FFmpeg/libavutil/reverse.c \
    ../third_party/FFmpeg/libavutil/ripemd.c \
    ../third_party/FFmpeg/libavutil/samplefmt.c \
    ../third_party/FFmpeg/libavutil/sha.c \
    ../third_party/FFmpeg/libavutil/sha512.c \
    ../third_party/FFmpeg/libavutil/slicethread.c \
    ../third_party/FFmpeg/libavutil/spherical.c \
    ../third_party/FFmpeg/libavutil/stereo3d.c \
    ../third_party/FFmpeg/libavutil/tea.c \
    ../third_party/FFmpeg/libavutil/threadmessage.c \
    ../third_party/FFmpeg/libavutil/time.c \
    ../third_party/FFmpeg/libavutil/timecode.c \
    ../third_party/FFmpeg/libavutil/tree.c \
    ../third_party/FFmpeg/libavutil/twofish.c \
    ../third_party/FFmpeg/libavutil/tx.c \
    ../third_party/FFmpeg/libavutil/tx_double.c \
    ../third_party/FFmpeg/libavutil/tx_float.c \
    ../third_party/FFmpeg/libavutil/tx_int32.c \
    ../third_party/FFmpeg/libavutil/utils.c \
    ../third_party/FFmpeg/libavutil/video_enc_params.c \
    ../third_party/FFmpeg/libavutil/x86/cpu.c \
    ../third_party/FFmpeg/libavutil/x86/fixed_dsp_init.c \
    ../third_party/FFmpeg/libavutil/x86/float_dsp_init.c \
    ../third_party/FFmpeg/libavutil/x86/imgutils_init.c \
    ../third_party/FFmpeg/libavutil/x86/lls_init.c \
    ../third_party/FFmpeg/libavutil/xga_font_data.c \
    ../third_party/FFmpeg/libavutil/xtea.c
  include $(BUILD_STATIC_LIBRARY)
else ifeq ($(filter-out ABI=$(TARGET_ARCH_ABI) $(PREMAKE_ANDROIDNDK_CONFIGURATIONS_PREFIXED) $(PREMAKE_ANDROIDNDK_PLATFORMS_PREFIXED),ABI=x86_64 CONFIGURATION=Debug PLATFORM=Android-x86_64),)
  LOCAL_ARM_NEON := true
  LOCAL_CONLYFLAGS := \
    -D_UNICODE \
    -DUNICODE \
    -D_LIB \
    -DDEBUG \
    -D_NO_DEBUG_HEAP=1 \
    -DHAVE_AV_CONFIG_H \
    -D_USE_MATH_DEFINES \
    -fvisibility=hidden \
    -w
  LOCAL_CONLYFLAGS += \
    -mavx
  LOCAL_CPPFLAGS += \
    -mavx
  LOCAL_CPP_FEATURES := exceptions rtti
  LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/.. \
    $(LOCAL_PATH)/../src \
    $(LOCAL_PATH)/../third_party \
    $(LOCAL_PATH)/../third_party/glslang \
    $(LOCAL_PATH)/../third_party/FFmpeg
  LOCAL_C_INCLUDES := $(call PREMAKE_ANDROIDNDK_SHELL_ESCAPE,$(LOCAL_C_INCLUDES))
  LOCAL_MODULE_FILENAME := liblibavutil
  LOCAL_SRC_FILES := \
    ../third_party/FFmpeg/libavutil/adler32.c \
    ../third_party/FFmpeg/libavutil/aes.c \
    ../third_party/FFmpeg/libavutil/aes_ctr.c \
    ../third_party/FFmpeg/libavutil/audio_fifo.c \
    ../third_party/FFmpeg/libavutil/avsscanf.c \
    ../third_party/FFmpeg/libavutil/avstring.c \
    ../third_party/FFmpeg/libavutil/base64.c \
    ../third_party/FFmpeg/libavutil/blowfish.c \
    ../third_party/FFmpeg/libavutil/bprint.c \
    ../third_party/FFmpeg/libavutil/buffer.c \
    ../third_party/FFmpeg/libavutil/camellia.c \
    ../third_party/FFmpeg/libavutil/cast5.c \
    ../third_party/FFmpeg/libavutil/channel_layout.c \
    ../third_party/FFmpeg/libavutil/color_utils.c \
    ../third_party/FFmpeg/libavutil/cpu.c \
    ../third_party/FFmpeg/libavutil/crc.c \
    ../third_party/FFmpeg/libavutil/des.c \
    ../third_party/FFmpeg/libavutil/dict.c \
    ../third_party/FFmpeg/libavutil/display.c \
    ../third_party/FFmpeg/libavutil/dovi_meta.c \
    ../third_party/FFmpeg/libavutil/downmix_info.c \
    ../third_party/FFmpeg/libavutil/encryption_info.c \
    ../third_party/FFmpeg/libavutil/error.c \
    ../third_party/FFmpeg/libavutil/eval.c \
    ../third_party/FFmpeg/libavutil/fifo.c \
    ../third_party/FFmpeg/libavutil/file.c \
    ../third_party/FFmpeg/libavutil/file_open.c \
    ../third_party/FFmpeg/libavutil/film_grain_params.c \
    ../third_party/FFmpeg/libavutil/fixed_dsp.c \
    ../third_party/FFmpeg/libavutil/float_dsp.c \
    ../third_party/FFmpeg/libavutil/frame.c \
    ../third_party/FFmpeg/libavutil/hash.c \
    ../third_party/FFmpeg/libavutil/hdr_dynamic_metadata.c \
    ../third_party/FFmpeg/libavutil/hmac.c \
    ../third_party/FFmpeg/libavutil/hwcontext.c \
    ../third_party/FFmpeg/libavutil/imgutils.c \
    ../third_party/FFmpeg/libavutil/integer.c \
    ../third_party/FFmpeg/libavutil/intmath.c \
    ../third_party/FFmpeg/libavutil/lfg.c \
    ../third_party/FFmpeg/libavutil/lls.c \
    ../third_party/FFmpeg/libavutil/log.c \
    ../third_party/FFmpeg/libavutil/log2_tab.c \
    ../third_party/FFmpeg/libavutil/mastering_display_metadata.c \
    ../third_party/FFmpeg/libavutil/mathematics.c \
    ../third_party/FFmpeg/libavutil/md5.c \
    ../third_party/FFmpeg/libavutil/mem.c \
    ../third_party/FFmpeg/libavutil/murmur3.c \
    ../third_party/FFmpeg/libavutil/opt.c \
    ../third_party/FFmpeg/libavutil/parseutils.c \
    ../third_party/FFmpeg/libavutil/pixdesc.c \
    ../third_party/FFmpeg/libavutil/pixelutils.c \
    ../third_party/FFmpeg/libavutil/random_seed.c \
    ../third_party/FFmpeg/libavutil/rational.c \
    ../third_party/FFmpeg/libavutil/rc4.c \
    ../third_party/FFmpeg/libavutil/reverse.c \
    ../third_party/FFmpeg/libavutil/ripemd.c \
    ../third_party/FFmpeg/libavutil/samplefmt.c \
    ../third_party/FFmpeg/libavutil/sha.c \
    ../third_party/FFmpeg/libavutil/sha512.c \
    ../third_party/FFmpeg/libavutil/slicethread.c \
    ../third_party/FFmpeg/libavutil/spherical.c \
    ../third_party/FFmpeg/libavutil/stereo3d.c \
    ../third_party/FFmpeg/libavutil/tea.c \
    ../third_party/FFmpeg/libavutil/threadmessage.c \
    ../third_party/FFmpeg/libavutil/time.c \
    ../third_party/FFmpeg/libavutil/timecode.c \
    ../third_party/FFmpeg/libavutil/tree.c \
    ../third_party/FFmpeg/libavutil/twofish.c \
    ../third_party/FFmpeg/libavutil/tx.c \
    ../third_party/FFmpeg/libavutil/tx_double.c \
    ../third_party/FFmpeg/libavutil/tx_float.c \
    ../third_party/FFmpeg/libavutil/tx_int32.c \
    ../third_party/FFmpeg/libavutil/utils.c \
    ../third_party/FFmpeg/libavutil/video_enc_params.c \
    ../third_party/FFmpeg/libavutil/x86/cpu.c \
    ../third_party/FFmpeg/libavutil/x86/fixed_dsp_init.c \
    ../third_party/FFmpeg/libavutil/x86/float_dsp_init.c \
    ../third_party/FFmpeg/libavutil/x86/imgutils_init.c \
    ../third_party/FFmpeg/libavutil/x86/lls_init.c \
    ../third_party/FFmpeg/libavutil/xga_font_data.c \
    ../third_party/FFmpeg/libavutil/xtea.c
  include $(BUILD_STATIC_LIBRARY)
else ifeq ($(filter-out ABI=$(TARGET_ARCH_ABI) $(PREMAKE_ANDROIDNDK_CONFIGURATIONS_PREFIXED) $(PREMAKE_ANDROIDNDK_PLATFORMS_PREFIXED),ABI=x86_64 CONFIGURATION=Release PLATFORM=Android-x86_64),)
  LOCAL_ARM_NEON := true
  LOCAL_CONLYFLAGS := \
    -D_UNICODE \
    -DUNICODE \
    -D_LIB \
    -DNDEBUG \
    -D_NO_DEBUG_HEAP=1 \
    -DHAVE_AV_CONFIG_H \
    -D_USE_MATH_DEFINES \
    -fvisibility=hidden \
    -w
  LOCAL_CONLYFLAGS += \
    -mavx
  LOCAL_CPPFLAGS += \
    -mavx
  LOCAL_CPP_FEATURES := exceptions rtti
  LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/.. \
    $(LOCAL_PATH)/../src \
    $(LOCAL_PATH)/../third_party \
    $(LOCAL_PATH)/../third_party/glslang \
    $(LOCAL_PATH)/../third_party/FFmpeg
  LOCAL_C_INCLUDES := $(call PREMAKE_ANDROIDNDK_SHELL_ESCAPE,$(LOCAL_C_INCLUDES))
  LOCAL_MODULE_FILENAME := liblibavutil
  LOCAL_SRC_FILES := \
    ../third_party/FFmpeg/libavutil/adler32.c \
    ../third_party/FFmpeg/libavutil/aes.c \
    ../third_party/FFmpeg/libavutil/aes_ctr.c \
    ../third_party/FFmpeg/libavutil/audio_fifo.c \
    ../third_party/FFmpeg/libavutil/avsscanf.c \
    ../third_party/FFmpeg/libavutil/avstring.c \
    ../third_party/FFmpeg/libavutil/base64.c \
    ../third_party/FFmpeg/libavutil/blowfish.c \
    ../third_party/FFmpeg/libavutil/bprint.c \
    ../third_party/FFmpeg/libavutil/buffer.c \
    ../third_party/FFmpeg/libavutil/camellia.c \
    ../third_party/FFmpeg/libavutil/cast5.c \
    ../third_party/FFmpeg/libavutil/channel_layout.c \
    ../third_party/FFmpeg/libavutil/color_utils.c \
    ../third_party/FFmpeg/libavutil/cpu.c \
    ../third_party/FFmpeg/libavutil/crc.c \
    ../third_party/FFmpeg/libavutil/des.c \
    ../third_party/FFmpeg/libavutil/dict.c \
    ../third_party/FFmpeg/libavutil/display.c \
    ../third_party/FFmpeg/libavutil/dovi_meta.c \
    ../third_party/FFmpeg/libavutil/downmix_info.c \
    ../third_party/FFmpeg/libavutil/encryption_info.c \
    ../third_party/FFmpeg/libavutil/error.c \
    ../third_party/FFmpeg/libavutil/eval.c \
    ../third_party/FFmpeg/libavutil/fifo.c \
    ../third_party/FFmpeg/libavutil/file.c \
    ../third_party/FFmpeg/libavutil/file_open.c \
    ../third_party/FFmpeg/libavutil/film_grain_params.c \
    ../third_party/FFmpeg/libavutil/fixed_dsp.c \
    ../third_party/FFmpeg/libavutil/float_dsp.c \
    ../third_party/FFmpeg/libavutil/frame.c \
    ../third_party/FFmpeg/libavutil/hash.c \
    ../third_party/FFmpeg/libavutil/hdr_dynamic_metadata.c \
    ../third_party/FFmpeg/libavutil/hmac.c \
    ../third_party/FFmpeg/libavutil/hwcontext.c \
    ../third_party/FFmpeg/libavutil/imgutils.c \
    ../third_party/FFmpeg/libavutil/integer.c \
    ../third_party/FFmpeg/libavutil/intmath.c \
    ../third_party/FFmpeg/libavutil/lfg.c \
    ../third_party/FFmpeg/libavutil/lls.c \
    ../third_party/FFmpeg/libavutil/log.c \
    ../third_party/FFmpeg/libavutil/log2_tab.c \
    ../third_party/FFmpeg/libavutil/mastering_display_metadata.c \
    ../third_party/FFmpeg/libavutil/mathematics.c \
    ../third_party/FFmpeg/libavutil/md5.c \
    ../third_party/FFmpeg/libavutil/mem.c \
    ../third_party/FFmpeg/libavutil/murmur3.c \
    ../third_party/FFmpeg/libavutil/opt.c \
    ../third_party/FFmpeg/libavutil/parseutils.c \
    ../third_party/FFmpeg/libavutil/pixdesc.c \
    ../third_party/FFmpeg/libavutil/pixelutils.c \
    ../third_party/FFmpeg/libavutil/random_seed.c \
    ../third_party/FFmpeg/libavutil/rational.c \
    ../third_party/FFmpeg/libavutil/rc4.c \
    ../third_party/FFmpeg/libavutil/reverse.c \
    ../third_party/FFmpeg/libavutil/ripemd.c \
    ../third_party/FFmpeg/libavutil/samplefmt.c \
    ../third_party/FFmpeg/libavutil/sha.c \
    ../third_party/FFmpeg/libavutil/sha512.c \
    ../third_party/FFmpeg/libavutil/slicethread.c \
    ../third_party/FFmpeg/libavutil/spherical.c \
    ../third_party/FFmpeg/libavutil/stereo3d.c \
    ../third_party/FFmpeg/libavutil/tea.c \
    ../third_party/FFmpeg/libavutil/threadmessage.c \
    ../third_party/FFmpeg/libavutil/time.c \
    ../third_party/FFmpeg/libavutil/timecode.c \
    ../third_party/FFmpeg/libavutil/tree.c \
    ../third_party/FFmpeg/libavutil/twofish.c \
    ../third_party/FFmpeg/libavutil/tx.c \
    ../third_party/FFmpeg/libavutil/tx_double.c \
    ../third_party/FFmpeg/libavutil/tx_float.c \
    ../third_party/FFmpeg/libavutil/tx_int32.c \
    ../third_party/FFmpeg/libavutil/utils.c \
    ../third_party/FFmpeg/libavutil/video_enc_params.c \
    ../third_party/FFmpeg/libavutil/x86/cpu.c \
    ../third_party/FFmpeg/libavutil/x86/fixed_dsp_init.c \
    ../third_party/FFmpeg/libavutil/x86/float_dsp_init.c \
    ../third_party/FFmpeg/libavutil/x86/imgutils_init.c \
    ../third_party/FFmpeg/libavutil/x86/lls_init.c \
    ../third_party/FFmpeg/libavutil/xga_font_data.c \
    ../third_party/FFmpeg/libavutil/xtea.c
  include $(BUILD_STATIC_LIBRARY)
endif