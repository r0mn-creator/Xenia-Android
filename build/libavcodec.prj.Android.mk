LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_RENDERSCRIPT_COMPATIBILITY :=
LOCAL_MODULE := libavcodec
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
  LOCAL_MODULE_FILENAME := liblibavcodec
  LOCAL_SRC_FILES := \
    ../third_party/FFmpeg/libavcodec/aarch64/fft_init_aarch64.c \
    ../third_party/FFmpeg/libavcodec/aarch64/fft_neon.S \
    ../third_party/FFmpeg/libavcodec/aarch64/idctdsp_init_aarch64.c \
    ../third_party/FFmpeg/libavcodec/aarch64/mdct_neon.S \
    ../third_party/FFmpeg/libavcodec/aarch64/simple_idct_neon.S \
    ../third_party/FFmpeg/libavcodec/ac3_parser.c \
    ../third_party/FFmpeg/libavcodec/adts_parser.c \
    ../third_party/FFmpeg/libavcodec/allcodecs.c \
    ../third_party/FFmpeg/libavcodec/avcodec.c \
    ../third_party/FFmpeg/libavcodec/avdct.c \
    ../third_party/FFmpeg/libavcodec/avfft.c \
    ../third_party/FFmpeg/libavcodec/avpacket.c \
    ../third_party/FFmpeg/libavcodec/avpicture.c \
    ../third_party/FFmpeg/libavcodec/bitstream.c \
    ../third_party/FFmpeg/libavcodec/bitstream_filter.c \
    ../third_party/FFmpeg/libavcodec/bitstream_filters.c \
    ../third_party/FFmpeg/libavcodec/bsf.c \
    ../third_party/FFmpeg/libavcodec/codec_desc.c \
    ../third_party/FFmpeg/libavcodec/codec_par.c \
    ../third_party/FFmpeg/libavcodec/d3d11va.c \
    ../third_party/FFmpeg/libavcodec/decode.c \
    ../third_party/FFmpeg/libavcodec/dirac.c \
    ../third_party/FFmpeg/libavcodec/dv_profile.c \
    ../third_party/FFmpeg/libavcodec/encode.c \
    ../third_party/FFmpeg/libavcodec/faandct.c \
    ../third_party/FFmpeg/libavcodec/faanidct.c \
    ../third_party/FFmpeg/libavcodec/fdctdsp.c \
    ../third_party/FFmpeg/libavcodec/fft_fixed_32.c \
    ../third_party/FFmpeg/libavcodec/fft_float.c \
    ../third_party/FFmpeg/libavcodec/fft_init_table.c \
    ../third_party/FFmpeg/libavcodec/idctdsp.c \
    ../third_party/FFmpeg/libavcodec/imgconvert.c \
    ../third_party/FFmpeg/libavcodec/jfdctfst.c \
    ../third_party/FFmpeg/libavcodec/jfdctint.c \
    ../third_party/FFmpeg/libavcodec/jni.c \
    ../third_party/FFmpeg/libavcodec/jrevdct.c \
    ../third_party/FFmpeg/libavcodec/mathtables.c \
    ../third_party/FFmpeg/libavcodec/mdct_fixed_32.c \
    ../third_party/FFmpeg/libavcodec/mdct_float.c \
    ../third_party/FFmpeg/libavcodec/mediacodec.c \
    ../third_party/FFmpeg/libavcodec/mpeg12framerate.c \
    ../third_party/FFmpeg/libavcodec/null_bsf.c \
    ../third_party/FFmpeg/libavcodec/options.c \
    ../third_party/FFmpeg/libavcodec/parser.c \
    ../third_party/FFmpeg/libavcodec/parsers.c \
    ../third_party/FFmpeg/libavcodec/profiles.c \
    ../third_party/FFmpeg/libavcodec/pthread.c \
    ../third_party/FFmpeg/libavcodec/pthread_frame.c \
    ../third_party/FFmpeg/libavcodec/pthread_slice.c \
    ../third_party/FFmpeg/libavcodec/qsv_api.c \
    ../third_party/FFmpeg/libavcodec/raw.c \
    ../third_party/FFmpeg/libavcodec/simple_idct.c \
    ../third_party/FFmpeg/libavcodec/sinewin.c \
    ../third_party/FFmpeg/libavcodec/utils.c \
    ../third_party/FFmpeg/libavcodec/vorbis_parser.c \
    ../third_party/FFmpeg/libavcodec/wma.c \
    ../third_party/FFmpeg/libavcodec/wma_common.c \
    ../third_party/FFmpeg/libavcodec/wma_freqs.c \
    ../third_party/FFmpeg/libavcodec/wmaprodec.c \
    ../third_party/FFmpeg/libavcodec/xiph.c
  LOCAL_STATIC_LIBRARIES := \
    libavutil
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
  LOCAL_MODULE_FILENAME := liblibavcodec
  LOCAL_SRC_FILES := \
    ../third_party/FFmpeg/libavcodec/aarch64/fft_init_aarch64.c \
    ../third_party/FFmpeg/libavcodec/aarch64/fft_neon.S \
    ../third_party/FFmpeg/libavcodec/aarch64/idctdsp_init_aarch64.c \
    ../third_party/FFmpeg/libavcodec/aarch64/mdct_neon.S \
    ../third_party/FFmpeg/libavcodec/aarch64/simple_idct_neon.S \
    ../third_party/FFmpeg/libavcodec/ac3_parser.c \
    ../third_party/FFmpeg/libavcodec/adts_parser.c \
    ../third_party/FFmpeg/libavcodec/allcodecs.c \
    ../third_party/FFmpeg/libavcodec/avcodec.c \
    ../third_party/FFmpeg/libavcodec/avdct.c \
    ../third_party/FFmpeg/libavcodec/avfft.c \
    ../third_party/FFmpeg/libavcodec/avpacket.c \
    ../third_party/FFmpeg/libavcodec/avpicture.c \
    ../third_party/FFmpeg/libavcodec/bitstream.c \
    ../third_party/FFmpeg/libavcodec/bitstream_filter.c \
    ../third_party/FFmpeg/libavcodec/bitstream_filters.c \
    ../third_party/FFmpeg/libavcodec/bsf.c \
    ../third_party/FFmpeg/libavcodec/codec_desc.c \
    ../third_party/FFmpeg/libavcodec/codec_par.c \
    ../third_party/FFmpeg/libavcodec/d3d11va.c \
    ../third_party/FFmpeg/libavcodec/decode.c \
    ../third_party/FFmpeg/libavcodec/dirac.c \
    ../third_party/FFmpeg/libavcodec/dv_profile.c \
    ../third_party/FFmpeg/libavcodec/encode.c \
    ../third_party/FFmpeg/libavcodec/faandct.c \
    ../third_party/FFmpeg/libavcodec/faanidct.c \
    ../third_party/FFmpeg/libavcodec/fdctdsp.c \
    ../third_party/FFmpeg/libavcodec/fft_fixed_32.c \
    ../third_party/FFmpeg/libavcodec/fft_float.c \
    ../third_party/FFmpeg/libavcodec/fft_init_table.c \
    ../third_party/FFmpeg/libavcodec/idctdsp.c \
    ../third_party/FFmpeg/libavcodec/imgconvert.c \
    ../third_party/FFmpeg/libavcodec/jfdctfst.c \
    ../third_party/FFmpeg/libavcodec/jfdctint.c \
    ../third_party/FFmpeg/libavcodec/jni.c \
    ../third_party/FFmpeg/libavcodec/jrevdct.c \
    ../third_party/FFmpeg/libavcodec/mathtables.c \
    ../third_party/FFmpeg/libavcodec/mdct_fixed_32.c \
    ../third_party/FFmpeg/libavcodec/mdct_float.c \
    ../third_party/FFmpeg/libavcodec/mediacodec.c \
    ../third_party/FFmpeg/libavcodec/mpeg12framerate.c \
    ../third_party/FFmpeg/libavcodec/null_bsf.c \
    ../third_party/FFmpeg/libavcodec/options.c \
    ../third_party/FFmpeg/libavcodec/parser.c \
    ../third_party/FFmpeg/libavcodec/parsers.c \
    ../third_party/FFmpeg/libavcodec/profiles.c \
    ../third_party/FFmpeg/libavcodec/pthread.c \
    ../third_party/FFmpeg/libavcodec/pthread_frame.c \
    ../third_party/FFmpeg/libavcodec/pthread_slice.c \
    ../third_party/FFmpeg/libavcodec/qsv_api.c \
    ../third_party/FFmpeg/libavcodec/raw.c \
    ../third_party/FFmpeg/libavcodec/simple_idct.c \
    ../third_party/FFmpeg/libavcodec/sinewin.c \
    ../third_party/FFmpeg/libavcodec/utils.c \
    ../third_party/FFmpeg/libavcodec/vorbis_parser.c \
    ../third_party/FFmpeg/libavcodec/wma.c \
    ../third_party/FFmpeg/libavcodec/wma_common.c \
    ../third_party/FFmpeg/libavcodec/wma_freqs.c \
    ../third_party/FFmpeg/libavcodec/wmaprodec.c \
    ../third_party/FFmpeg/libavcodec/xiph.c
  LOCAL_STATIC_LIBRARIES := \
    libavutil
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
  LOCAL_MODULE_FILENAME := liblibavcodec
  LOCAL_SRC_FILES := \
    ../third_party/FFmpeg/libavcodec/aarch64/fft_init_aarch64.c \
    ../third_party/FFmpeg/libavcodec/aarch64/fft_neon.S \
    ../third_party/FFmpeg/libavcodec/aarch64/idctdsp_init_aarch64.c \
    ../third_party/FFmpeg/libavcodec/aarch64/mdct_neon.S \
    ../third_party/FFmpeg/libavcodec/aarch64/simple_idct_neon.S \
    ../third_party/FFmpeg/libavcodec/ac3_parser.c \
    ../third_party/FFmpeg/libavcodec/adts_parser.c \
    ../third_party/FFmpeg/libavcodec/allcodecs.c \
    ../third_party/FFmpeg/libavcodec/avcodec.c \
    ../third_party/FFmpeg/libavcodec/avdct.c \
    ../third_party/FFmpeg/libavcodec/avfft.c \
    ../third_party/FFmpeg/libavcodec/avpacket.c \
    ../third_party/FFmpeg/libavcodec/avpicture.c \
    ../third_party/FFmpeg/libavcodec/bitstream.c \
    ../third_party/FFmpeg/libavcodec/bitstream_filter.c \
    ../third_party/FFmpeg/libavcodec/bitstream_filters.c \
    ../third_party/FFmpeg/libavcodec/bsf.c \
    ../third_party/FFmpeg/libavcodec/codec_desc.c \
    ../third_party/FFmpeg/libavcodec/codec_par.c \
    ../third_party/FFmpeg/libavcodec/d3d11va.c \
    ../third_party/FFmpeg/libavcodec/decode.c \
    ../third_party/FFmpeg/libavcodec/dirac.c \
    ../third_party/FFmpeg/libavcodec/dv_profile.c \
    ../third_party/FFmpeg/libavcodec/encode.c \
    ../third_party/FFmpeg/libavcodec/faandct.c \
    ../third_party/FFmpeg/libavcodec/faanidct.c \
    ../third_party/FFmpeg/libavcodec/fdctdsp.c \
    ../third_party/FFmpeg/libavcodec/fft_fixed_32.c \
    ../third_party/FFmpeg/libavcodec/fft_float.c \
    ../third_party/FFmpeg/libavcodec/fft_init_table.c \
    ../third_party/FFmpeg/libavcodec/idctdsp.c \
    ../third_party/FFmpeg/libavcodec/imgconvert.c \
    ../third_party/FFmpeg/libavcodec/jfdctfst.c \
    ../third_party/FFmpeg/libavcodec/jfdctint.c \
    ../third_party/FFmpeg/libavcodec/jni.c \
    ../third_party/FFmpeg/libavcodec/jrevdct.c \
    ../third_party/FFmpeg/libavcodec/mathtables.c \
    ../third_party/FFmpeg/libavcodec/mdct_fixed_32.c \
    ../third_party/FFmpeg/libavcodec/mdct_float.c \
    ../third_party/FFmpeg/libavcodec/mediacodec.c \
    ../third_party/FFmpeg/libavcodec/mpeg12framerate.c \
    ../third_party/FFmpeg/libavcodec/null_bsf.c \
    ../third_party/FFmpeg/libavcodec/options.c \
    ../third_party/FFmpeg/libavcodec/parser.c \
    ../third_party/FFmpeg/libavcodec/parsers.c \
    ../third_party/FFmpeg/libavcodec/profiles.c \
    ../third_party/FFmpeg/libavcodec/pthread.c \
    ../third_party/FFmpeg/libavcodec/pthread_frame.c \
    ../third_party/FFmpeg/libavcodec/pthread_slice.c \
    ../third_party/FFmpeg/libavcodec/qsv_api.c \
    ../third_party/FFmpeg/libavcodec/raw.c \
    ../third_party/FFmpeg/libavcodec/simple_idct.c \
    ../third_party/FFmpeg/libavcodec/sinewin.c \
    ../third_party/FFmpeg/libavcodec/utils.c \
    ../third_party/FFmpeg/libavcodec/vorbis_parser.c \
    ../third_party/FFmpeg/libavcodec/wma.c \
    ../third_party/FFmpeg/libavcodec/wma_common.c \
    ../third_party/FFmpeg/libavcodec/wma_freqs.c \
    ../third_party/FFmpeg/libavcodec/wmaprodec.c \
    ../third_party/FFmpeg/libavcodec/xiph.c
  LOCAL_STATIC_LIBRARIES := \
    libavutil
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
  LOCAL_MODULE_FILENAME := liblibavcodec
  LOCAL_SRC_FILES := \
    ../third_party/FFmpeg/libavcodec/ac3_parser.c \
    ../third_party/FFmpeg/libavcodec/adts_parser.c \
    ../third_party/FFmpeg/libavcodec/allcodecs.c \
    ../third_party/FFmpeg/libavcodec/avcodec.c \
    ../third_party/FFmpeg/libavcodec/avdct.c \
    ../third_party/FFmpeg/libavcodec/avfft.c \
    ../third_party/FFmpeg/libavcodec/avpacket.c \
    ../third_party/FFmpeg/libavcodec/avpicture.c \
    ../third_party/FFmpeg/libavcodec/bitstream.c \
    ../third_party/FFmpeg/libavcodec/bitstream_filter.c \
    ../third_party/FFmpeg/libavcodec/bitstream_filters.c \
    ../third_party/FFmpeg/libavcodec/bsf.c \
    ../third_party/FFmpeg/libavcodec/codec_desc.c \
    ../third_party/FFmpeg/libavcodec/codec_par.c \
    ../third_party/FFmpeg/libavcodec/d3d11va.c \
    ../third_party/FFmpeg/libavcodec/decode.c \
    ../third_party/FFmpeg/libavcodec/dirac.c \
    ../third_party/FFmpeg/libavcodec/dv_profile.c \
    ../third_party/FFmpeg/libavcodec/encode.c \
    ../third_party/FFmpeg/libavcodec/faandct.c \
    ../third_party/FFmpeg/libavcodec/faanidct.c \
    ../third_party/FFmpeg/libavcodec/fdctdsp.c \
    ../third_party/FFmpeg/libavcodec/fft_fixed_32.c \
    ../third_party/FFmpeg/libavcodec/fft_float.c \
    ../third_party/FFmpeg/libavcodec/fft_init_table.c \
    ../third_party/FFmpeg/libavcodec/idctdsp.c \
    ../third_party/FFmpeg/libavcodec/imgconvert.c \
    ../third_party/FFmpeg/libavcodec/jfdctfst.c \
    ../third_party/FFmpeg/libavcodec/jfdctint.c \
    ../third_party/FFmpeg/libavcodec/jni.c \
    ../third_party/FFmpeg/libavcodec/jrevdct.c \
    ../third_party/FFmpeg/libavcodec/mathtables.c \
    ../third_party/FFmpeg/libavcodec/mdct_fixed_32.c \
    ../third_party/FFmpeg/libavcodec/mdct_float.c \
    ../third_party/FFmpeg/libavcodec/mediacodec.c \
    ../third_party/FFmpeg/libavcodec/mpeg12framerate.c \
    ../third_party/FFmpeg/libavcodec/null_bsf.c \
    ../third_party/FFmpeg/libavcodec/options.c \
    ../third_party/FFmpeg/libavcodec/parser.c \
    ../third_party/FFmpeg/libavcodec/parsers.c \
    ../third_party/FFmpeg/libavcodec/profiles.c \
    ../third_party/FFmpeg/libavcodec/pthread.c \
    ../third_party/FFmpeg/libavcodec/pthread_frame.c \
    ../third_party/FFmpeg/libavcodec/pthread_slice.c \
    ../third_party/FFmpeg/libavcodec/qsv_api.c \
    ../third_party/FFmpeg/libavcodec/raw.c \
    ../third_party/FFmpeg/libavcodec/simple_idct.c \
    ../third_party/FFmpeg/libavcodec/sinewin.c \
    ../third_party/FFmpeg/libavcodec/utils.c \
    ../third_party/FFmpeg/libavcodec/vorbis_parser.c \
    ../third_party/FFmpeg/libavcodec/wma.c \
    ../third_party/FFmpeg/libavcodec/wma_common.c \
    ../third_party/FFmpeg/libavcodec/wma_freqs.c \
    ../third_party/FFmpeg/libavcodec/wmaprodec.c \
    ../third_party/FFmpeg/libavcodec/x86/constants.c \
    ../third_party/FFmpeg/libavcodec/x86/fdct.c \
    ../third_party/FFmpeg/libavcodec/x86/fdctdsp_init.c \
    ../third_party/FFmpeg/libavcodec/x86/fft_init.c \
    ../third_party/FFmpeg/libavcodec/x86/idctdsp_init.c \
    ../third_party/FFmpeg/libavcodec/xiph.c
  LOCAL_STATIC_LIBRARIES := \
    libavutil
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
  LOCAL_MODULE_FILENAME := liblibavcodec
  LOCAL_SRC_FILES := \
    ../third_party/FFmpeg/libavcodec/ac3_parser.c \
    ../third_party/FFmpeg/libavcodec/adts_parser.c \
    ../third_party/FFmpeg/libavcodec/allcodecs.c \
    ../third_party/FFmpeg/libavcodec/avcodec.c \
    ../third_party/FFmpeg/libavcodec/avdct.c \
    ../third_party/FFmpeg/libavcodec/avfft.c \
    ../third_party/FFmpeg/libavcodec/avpacket.c \
    ../third_party/FFmpeg/libavcodec/avpicture.c \
    ../third_party/FFmpeg/libavcodec/bitstream.c \
    ../third_party/FFmpeg/libavcodec/bitstream_filter.c \
    ../third_party/FFmpeg/libavcodec/bitstream_filters.c \
    ../third_party/FFmpeg/libavcodec/bsf.c \
    ../third_party/FFmpeg/libavcodec/codec_desc.c \
    ../third_party/FFmpeg/libavcodec/codec_par.c \
    ../third_party/FFmpeg/libavcodec/d3d11va.c \
    ../third_party/FFmpeg/libavcodec/decode.c \
    ../third_party/FFmpeg/libavcodec/dirac.c \
    ../third_party/FFmpeg/libavcodec/dv_profile.c \
    ../third_party/FFmpeg/libavcodec/encode.c \
    ../third_party/FFmpeg/libavcodec/faandct.c \
    ../third_party/FFmpeg/libavcodec/faanidct.c \
    ../third_party/FFmpeg/libavcodec/fdctdsp.c \
    ../third_party/FFmpeg/libavcodec/fft_fixed_32.c \
    ../third_party/FFmpeg/libavcodec/fft_float.c \
    ../third_party/FFmpeg/libavcodec/fft_init_table.c \
    ../third_party/FFmpeg/libavcodec/idctdsp.c \
    ../third_party/FFmpeg/libavcodec/imgconvert.c \
    ../third_party/FFmpeg/libavcodec/jfdctfst.c \
    ../third_party/FFmpeg/libavcodec/jfdctint.c \
    ../third_party/FFmpeg/libavcodec/jni.c \
    ../third_party/FFmpeg/libavcodec/jrevdct.c \
    ../third_party/FFmpeg/libavcodec/mathtables.c \
    ../third_party/FFmpeg/libavcodec/mdct_fixed_32.c \
    ../third_party/FFmpeg/libavcodec/mdct_float.c \
    ../third_party/FFmpeg/libavcodec/mediacodec.c \
    ../third_party/FFmpeg/libavcodec/mpeg12framerate.c \
    ../third_party/FFmpeg/libavcodec/null_bsf.c \
    ../third_party/FFmpeg/libavcodec/options.c \
    ../third_party/FFmpeg/libavcodec/parser.c \
    ../third_party/FFmpeg/libavcodec/parsers.c \
    ../third_party/FFmpeg/libavcodec/profiles.c \
    ../third_party/FFmpeg/libavcodec/pthread.c \
    ../third_party/FFmpeg/libavcodec/pthread_frame.c \
    ../third_party/FFmpeg/libavcodec/pthread_slice.c \
    ../third_party/FFmpeg/libavcodec/qsv_api.c \
    ../third_party/FFmpeg/libavcodec/raw.c \
    ../third_party/FFmpeg/libavcodec/simple_idct.c \
    ../third_party/FFmpeg/libavcodec/sinewin.c \
    ../third_party/FFmpeg/libavcodec/utils.c \
    ../third_party/FFmpeg/libavcodec/vorbis_parser.c \
    ../third_party/FFmpeg/libavcodec/wma.c \
    ../third_party/FFmpeg/libavcodec/wma_common.c \
    ../third_party/FFmpeg/libavcodec/wma_freqs.c \
    ../third_party/FFmpeg/libavcodec/wmaprodec.c \
    ../third_party/FFmpeg/libavcodec/x86/constants.c \
    ../third_party/FFmpeg/libavcodec/x86/fdct.c \
    ../third_party/FFmpeg/libavcodec/x86/fdctdsp_init.c \
    ../third_party/FFmpeg/libavcodec/x86/fft_init.c \
    ../third_party/FFmpeg/libavcodec/x86/idctdsp_init.c \
    ../third_party/FFmpeg/libavcodec/xiph.c
  LOCAL_STATIC_LIBRARIES := \
    libavutil
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
  LOCAL_MODULE_FILENAME := liblibavcodec
  LOCAL_SRC_FILES := \
    ../third_party/FFmpeg/libavcodec/ac3_parser.c \
    ../third_party/FFmpeg/libavcodec/adts_parser.c \
    ../third_party/FFmpeg/libavcodec/allcodecs.c \
    ../third_party/FFmpeg/libavcodec/avcodec.c \
    ../third_party/FFmpeg/libavcodec/avdct.c \
    ../third_party/FFmpeg/libavcodec/avfft.c \
    ../third_party/FFmpeg/libavcodec/avpacket.c \
    ../third_party/FFmpeg/libavcodec/avpicture.c \
    ../third_party/FFmpeg/libavcodec/bitstream.c \
    ../third_party/FFmpeg/libavcodec/bitstream_filter.c \
    ../third_party/FFmpeg/libavcodec/bitstream_filters.c \
    ../third_party/FFmpeg/libavcodec/bsf.c \
    ../third_party/FFmpeg/libavcodec/codec_desc.c \
    ../third_party/FFmpeg/libavcodec/codec_par.c \
    ../third_party/FFmpeg/libavcodec/d3d11va.c \
    ../third_party/FFmpeg/libavcodec/decode.c \
    ../third_party/FFmpeg/libavcodec/dirac.c \
    ../third_party/FFmpeg/libavcodec/dv_profile.c \
    ../third_party/FFmpeg/libavcodec/encode.c \
    ../third_party/FFmpeg/libavcodec/faandct.c \
    ../third_party/FFmpeg/libavcodec/faanidct.c \
    ../third_party/FFmpeg/libavcodec/fdctdsp.c \
    ../third_party/FFmpeg/libavcodec/fft_fixed_32.c \
    ../third_party/FFmpeg/libavcodec/fft_float.c \
    ../third_party/FFmpeg/libavcodec/fft_init_table.c \
    ../third_party/FFmpeg/libavcodec/idctdsp.c \
    ../third_party/FFmpeg/libavcodec/imgconvert.c \
    ../third_party/FFmpeg/libavcodec/jfdctfst.c \
    ../third_party/FFmpeg/libavcodec/jfdctint.c \
    ../third_party/FFmpeg/libavcodec/jni.c \
    ../third_party/FFmpeg/libavcodec/jrevdct.c \
    ../third_party/FFmpeg/libavcodec/mathtables.c \
    ../third_party/FFmpeg/libavcodec/mdct_fixed_32.c \
    ../third_party/FFmpeg/libavcodec/mdct_float.c \
    ../third_party/FFmpeg/libavcodec/mediacodec.c \
    ../third_party/FFmpeg/libavcodec/mpeg12framerate.c \
    ../third_party/FFmpeg/libavcodec/null_bsf.c \
    ../third_party/FFmpeg/libavcodec/options.c \
    ../third_party/FFmpeg/libavcodec/parser.c \
    ../third_party/FFmpeg/libavcodec/parsers.c \
    ../third_party/FFmpeg/libavcodec/profiles.c \
    ../third_party/FFmpeg/libavcodec/pthread.c \
    ../third_party/FFmpeg/libavcodec/pthread_frame.c \
    ../third_party/FFmpeg/libavcodec/pthread_slice.c \
    ../third_party/FFmpeg/libavcodec/qsv_api.c \
    ../third_party/FFmpeg/libavcodec/raw.c \
    ../third_party/FFmpeg/libavcodec/simple_idct.c \
    ../third_party/FFmpeg/libavcodec/sinewin.c \
    ../third_party/FFmpeg/libavcodec/utils.c \
    ../third_party/FFmpeg/libavcodec/vorbis_parser.c \
    ../third_party/FFmpeg/libavcodec/wma.c \
    ../third_party/FFmpeg/libavcodec/wma_common.c \
    ../third_party/FFmpeg/libavcodec/wma_freqs.c \
    ../third_party/FFmpeg/libavcodec/wmaprodec.c \
    ../third_party/FFmpeg/libavcodec/x86/constants.c \
    ../third_party/FFmpeg/libavcodec/x86/fdct.c \
    ../third_party/FFmpeg/libavcodec/x86/fdctdsp_init.c \
    ../third_party/FFmpeg/libavcodec/x86/fft_init.c \
    ../third_party/FFmpeg/libavcodec/x86/idctdsp_init.c \
    ../third_party/FFmpeg/libavcodec/xiph.c
  LOCAL_STATIC_LIBRARIES := \
    libavutil
  include $(BUILD_STATIC_LIBRARY)
endif