PREMAKE_ANDROIDNDK_WORKSPACE_DIR := $(call my-dir)
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
PREMAKE_ANDROIDNDK_SHELL_ESCAPE_PRE_QUOTES = $(subst ",\",$(subst \,\\,$(1)))
PREMAKE_ANDROIDNDK_SHELL_ESCAPE_POST_QUOTES_NO_BRACKETS = $(subst },\},$(subst |,\|,$(subst {,\{,$(subst `,\`,$(subst ^,\^,$(subst ],\],$(subst [,\[,$(subst ?,\?,$(subst >,\>,$(subst <,\<,$(subst ;,\;,$(subst *,\*,$(subst ',\',$(subst &,\&,$(subst $$,\$$,$(subst $()\#,\$()\#,$(1)))))))))))))))))
PREMAKE_ANDROIDNDK_SHELL_ESCAPE_POST_QUOTES = ${subst ),\),${subst (,\(,${call PREMAKE_ANDROIDNDK_SHELL_ESCAPE_POST_QUOTES_NO_BRACKETS,${1}}}}
PREMAKE_ANDROIDNDK_SHELL_ESCAPE = $(call PREMAKE_ANDROIDNDK_SHELL_ESCAPE_POST_QUOTES,$(call PREMAKE_ANDROIDNDK_SHELL_ESCAPE_PRE_QUOTES,$(1)))
PREMAKE_ANDROIDNDK_SHELL_ESCAPE_MODULE_FILENAME = $(subst :,\:,$(call PREMAKE_ANDROIDNDK_SHELL_ESCAPE,$(1)))
ifeq ($(HOST_OS),windows)
  PREMAKE_ANDROIDNDK_SHELL_ESCAPE_SRC_FILES = $(call PREMAKE_ANDROIDNDK_SHELL_ESCAPE,$(1))
else
  PREMAKE_ANDROIDNDK_SHELL_ESCAPE_SRC_FILES = $(subst :,\:,$(call PREMAKE_ANDROIDNDK_SHELL_ESCAPE,$(1)))
endif
include $(PREMAKE_ANDROIDNDK_WORKSPACE_DIR)/aes_128.prj.Android.mk
include $(PREMAKE_ANDROIDNDK_WORKSPACE_DIR)/capstone.prj.Android.mk
include $(PREMAKE_ANDROIDNDK_WORKSPACE_DIR)/dxbc.prj.Android.mk
include $(PREMAKE_ANDROIDNDK_WORKSPACE_DIR)/discord-rpc.prj.Android.mk
include $(PREMAKE_ANDROIDNDK_WORKSPACE_DIR)/cxxopts.prj.Android.mk
include $(PREMAKE_ANDROIDNDK_WORKSPACE_DIR)/cpptoml.prj.Android.mk
include $(PREMAKE_ANDROIDNDK_WORKSPACE_DIR)/libavcodec.prj.Android.mk
include $(PREMAKE_ANDROIDNDK_WORKSPACE_DIR)/libavutil.prj.Android.mk
include $(PREMAKE_ANDROIDNDK_WORKSPACE_DIR)/fmt.prj.Android.mk
include $(PREMAKE_ANDROIDNDK_WORKSPACE_DIR)/glslang-spirv.prj.Android.mk
include $(PREMAKE_ANDROIDNDK_WORKSPACE_DIR)/imgui.prj.Android.mk
include $(PREMAKE_ANDROIDNDK_WORKSPACE_DIR)/mspack.prj.Android.mk
include $(PREMAKE_ANDROIDNDK_WORKSPACE_DIR)/snappy.prj.Android.mk
include $(PREMAKE_ANDROIDNDK_WORKSPACE_DIR)/xxhash.prj.Android.mk
include $(PREMAKE_ANDROIDNDK_WORKSPACE_DIR)/xenia-core.prj.Android.mk
include $(PREMAKE_ANDROIDNDK_WORKSPACE_DIR)/xenia-app.prj.Android.mk
include $(PREMAKE_ANDROIDNDK_WORKSPACE_DIR)/xenia-app-discord.prj.Android.mk
include $(PREMAKE_ANDROIDNDK_WORKSPACE_DIR)/xenia-apu.prj.Android.mk
include $(PREMAKE_ANDROIDNDK_WORKSPACE_DIR)/xenia-apu-nop.prj.Android.mk
include $(PREMAKE_ANDROIDNDK_WORKSPACE_DIR)/xenia-base.prj.Android.mk
include $(PREMAKE_ANDROIDNDK_WORKSPACE_DIR)/xenia-cpu.prj.Android.mk
include $(PREMAKE_ANDROIDNDK_WORKSPACE_DIR)/xenia-cpu-backend-x64.prj.Android.mk
include $(PREMAKE_ANDROIDNDK_WORKSPACE_DIR)/xenia-cpu-backend-a64.prj.Android.mk
include $(PREMAKE_ANDROIDNDK_WORKSPACE_DIR)/arm64emitter.prj.Android.mk
include $(PREMAKE_ANDROIDNDK_WORKSPACE_DIR)/xenia-cpu-backend-arm64.prj.Android.mk
include $(PREMAKE_ANDROIDNDK_WORKSPACE_DIR)/xenia-debug-ui.prj.Android.mk
include $(PREMAKE_ANDROIDNDK_WORKSPACE_DIR)/xenia-gpu.prj.Android.mk
include $(PREMAKE_ANDROIDNDK_WORKSPACE_DIR)/xenia-gpu-shader-compiler.prj.Android.mk
include $(PREMAKE_ANDROIDNDK_WORKSPACE_DIR)/xenia-gpu-null.prj.Android.mk
include $(PREMAKE_ANDROIDNDK_WORKSPACE_DIR)/xenia-gpu-vulkan.prj.Android.mk
include $(PREMAKE_ANDROIDNDK_WORKSPACE_DIR)/xenia-gpu-vulkan-trace-viewer.prj.Android.mk
include $(PREMAKE_ANDROIDNDK_WORKSPACE_DIR)/xenia-gpu-vulkan-trace-dump.prj.Android.mk
include $(PREMAKE_ANDROIDNDK_WORKSPACE_DIR)/xenia-hid.prj.Android.mk
include $(PREMAKE_ANDROIDNDK_WORKSPACE_DIR)/xenia-hid-demo.prj.Android.mk
include $(PREMAKE_ANDROIDNDK_WORKSPACE_DIR)/xenia-hid-nop.prj.Android.mk
include $(PREMAKE_ANDROIDNDK_WORKSPACE_DIR)/xenia-hid-android.prj.Android.mk
include $(PREMAKE_ANDROIDNDK_WORKSPACE_DIR)/xenia-kernel.prj.Android.mk
include $(PREMAKE_ANDROIDNDK_WORKSPACE_DIR)/xenia-ui.prj.Android.mk
include $(PREMAKE_ANDROIDNDK_WORKSPACE_DIR)/xenia-ui-vulkan.prj.Android.mk
include $(PREMAKE_ANDROIDNDK_WORKSPACE_DIR)/xenia-ui-window-vulkan-demo.prj.Android.mk
include $(PREMAKE_ANDROIDNDK_WORKSPACE_DIR)/xenia-vfs.prj.Android.mk
include $(PREMAKE_ANDROIDNDK_WORKSPACE_DIR)/xenia-vfs-dump.prj.Android.mk
include $(PREMAKE_ANDROIDNDK_WORKSPACE_DIR)/xenia-apu-sdl.prj.Android.mk
include $(PREMAKE_ANDROIDNDK_WORKSPACE_DIR)/xenia-helper-sdl.prj.Android.mk
include $(PREMAKE_ANDROIDNDK_WORKSPACE_DIR)/xenia-hid-sdl.prj.Android.mk