ifeq ($(filter Checked Debug Release,$(PREMAKE_ANDROIDNDK_CONFIGURATIONS)),)
  $(warning No configurations to build are specified in PREMAKE_ANDROIDNDK_CONFIGURATIONS.)
  $(warning Specify one or multiple "PREMAKE_ANDROIDNDK_CONFIGURATIONS+=configuration" in the ndk-build arguments to build those configurations.)
  $(warning For workspaces with multiple target platforms, you can also provide the list of the platforms to build via PREMAKE_ANDROIDNDK_PLATFORMS.)
  $(warning If PREMAKE_ANDROIDNDK_PLATFORMS is not specified, the projects in this workspace will be built for all platforms they're targeting.)
  $(warning Note that configuration and platform names are case-sensitive in this script.)
  $(warning )
  $(warning It's heavily recommended that only at most one configuration and platform specified corresponds to each targeted ABI.)
  $(warning Otherwise, which set of project settings will actually be chosen for building for each ABI will be undefined.)
  $(warning )
  $(warning The recommended approach is to use configurations for the optimization mode, and, if needed, platforms for ABI filtering.)
  $(warning This can be done by specifying different values of the Premake architecture setting for each platform using a platform filter.)
  $(warning In this case, each ABI will unambiguously correspond to only one configuration and platform pair.)
  $(warning )
  $(warning Configurations for this workspace:)
  $(warning $()  Checked)
  $(warning $()  Debug)
  $(warning $()  Release)
  $(warning )
  $(warning Platforms for this workspace:)
  $(warning $()  Android-ARM64)
  $(warning $()  Android-x86_64)
  $(error Aborting.)
endif
PREMAKE_ANDROIDNDK_CONFIGURATIONS_PREFIXED := $(addprefix CONFIGURATION=,$(PREMAKE_ANDROIDNDK_CONFIGURATIONS))
ifneq ($(PREMAKE_ANDROIDNDK_PLATFORMS),)
  PREMAKE_ANDROIDNDK_PLATFORMS_PREFIXED := $(addprefix PLATFORM=,$(PREMAKE_ANDROIDNDK_PLATFORMS))
else
  PREMAKE_ANDROIDNDK_PLATFORMS_PREFIXED := PLATFORM=Android-ARM64 PLATFORM=Android-x86_64
endif
undefine APP_ABI
undefine APP_DEBUG
undefine APP_OPTIM
undefine APP_PLATFORM
undefine PREMAKE_ANDROIDNDK_APP_STL_RUNTIME
undefine PREMAKE_ANDROIDNDK_APP_STL_LINKAGE
ifneq ($(filter CONFIGURATION=Checked,$(PREMAKE_ANDROIDNDK_CONFIGURATIONS_PREFIXED)),)
  ifneq ($(filter PLATFORM=Android-ARM64,$(PREMAKE_ANDROIDNDK_PLATFORMS_PREFIXED)),)
    APP_ABI += arm64-v8a
    APP_DEBUG ?= true
    APP_OPTIM ?= debug
    APP_PLATFORM ?= android-24
    PREMAKE_ANDROIDNDK_APP_STL_RUNTIME ?= c++
    PREMAKE_ANDROIDNDK_APP_STL_LINKAGE ?= _static
  endif
  ifneq ($(filter PLATFORM=Android-x86_64,$(PREMAKE_ANDROIDNDK_PLATFORMS_PREFIXED)),)
    APP_ABI += x86_64
    APP_DEBUG ?= true
    APP_OPTIM ?= debug
    APP_PLATFORM ?= android-24
    PREMAKE_ANDROIDNDK_APP_STL_RUNTIME ?= c++
    PREMAKE_ANDROIDNDK_APP_STL_LINKAGE ?= _static
  endif
endif
ifneq ($(filter CONFIGURATION=Debug,$(PREMAKE_ANDROIDNDK_CONFIGURATIONS_PREFIXED)),)
  ifneq ($(filter PLATFORM=Android-ARM64,$(PREMAKE_ANDROIDNDK_PLATFORMS_PREFIXED)),)
    APP_ABI += arm64-v8a
    APP_DEBUG ?= true
    APP_OPTIM ?= debug
    APP_PLATFORM ?= android-24
    PREMAKE_ANDROIDNDK_APP_STL_RUNTIME ?= c++
    PREMAKE_ANDROIDNDK_APP_STL_LINKAGE ?= _static
  endif
  ifneq ($(filter PLATFORM=Android-x86_64,$(PREMAKE_ANDROIDNDK_PLATFORMS_PREFIXED)),)
    APP_ABI += x86_64
    APP_DEBUG ?= true
    APP_OPTIM ?= debug
    APP_PLATFORM ?= android-24
    PREMAKE_ANDROIDNDK_APP_STL_RUNTIME ?= c++
    PREMAKE_ANDROIDNDK_APP_STL_LINKAGE ?= _static
  endif
endif
ifneq ($(filter CONFIGURATION=Release,$(PREMAKE_ANDROIDNDK_CONFIGURATIONS_PREFIXED)),)
  ifneq ($(filter PLATFORM=Android-ARM64,$(PREMAKE_ANDROIDNDK_PLATFORMS_PREFIXED)),)
    APP_ABI += arm64-v8a
    APP_OPTIM ?= release
    APP_PLATFORM ?= android-24
    PREMAKE_ANDROIDNDK_APP_STL_RUNTIME ?= c++
    PREMAKE_ANDROIDNDK_APP_STL_LINKAGE ?= _static
  endif
  ifneq ($(filter PLATFORM=Android-x86_64,$(PREMAKE_ANDROIDNDK_PLATFORMS_PREFIXED)),)
    APP_ABI += x86_64
    APP_OPTIM ?= release
    APP_PLATFORM ?= android-24
    PREMAKE_ANDROIDNDK_APP_STL_RUNTIME ?= c++
    PREMAKE_ANDROIDNDK_APP_STL_LINKAGE ?= _static
  endif
endif
APP_ABI := $(sort $(APP_ABI))
APP_BUILD_SCRIPT := $(call my-dir)/xenia.wks.Android.mk
APP_OPTIM ?= debug
ifeq ($(APP_OPTIM),release)
  APP_DEBUG := false
else
  APP_DEBUG ?= false
endif
APP_PLATFORM ?=
PREMAKE_ANDROIDNDK_APP_STL_RUNTIME ?= none
PREMAKE_ANDROIDNDK_APP_STL_LINKAGE ?= _static
ifneq ($(filter none system,$(PREMAKE_ANDROIDNDK_APP_STL_RUNTIME)),)
  PREMAKE_ANDROIDNDK_APP_STL_LINKAGE :=
endif
APP_STL := $(PREMAKE_ANDROIDNDK_APP_STL_RUNTIME)$(PREMAKE_ANDROIDNDK_APP_STL_LINKAGE)