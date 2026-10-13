#---------------------------------------------------------------------------------
# RDoom vktest: minimal Vulkan (Mesa NVK) smoke test for Horizon.
# Built inside the rdoom-nx-build image: scripts/build-vktest.sh
#---------------------------------------------------------------------------------
.SUFFIXES:

ifeq ($(strip $(DEVKITPRO)),)
$(error "Please set DEVKITPRO in your environment. export DEVKITPRO=<path to>/devkitpro")
endif

TOPDIR ?= $(CURDIR)
include $(DEVKITPRO)/libnx/switch_rules

TARGET      := vkmatrix
BUILD       := build
SOURCES     := source
INCLUDES    := source ../source ../../engine/src/common/platform/posix/nx
APP_TITLE   := RDoom Vulkan Matrix
APP_AUTHOR  := RDoom
APP_VERSION := 1.0.0
APP_ICON    := $(TOPDIR)/../../assets/build/icon_256.jpg

ARCH     := -march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIE
CFLAGS   := -g -Wall -O2 -ffunction-sections $(ARCH) $(DEFINES) $(INCLUDE) -D__SWITCH__
CXXFLAGS := $(CFLAGS) -fno-rtti -fno-exceptions
ASFLAGS  := -g $(ARCH)
LDFLAGS   = -specs=$(DEVKITPRO)/libnx/switch.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map) \
            -Wl,--wrap=threadCreate $(NVCOMPAT) -Wl,-u,vk_icdGetInstanceProcAddr \
            -Wl,-u,vk_icdNegotiateLoaderICDInterfaceVersion -pthread

NVCOMPAT := -Wl,--wrap=nvioctlNvhostAsGpu_AllocSpace -Wl,--wrap=nvioctlNvhostAsGpu_FreeSpace -Wl,--wrap=nvioctlNvhostAsGpu_InitializeEx
# diagnostic tracing of the remaining nvhost-as-gpu calls (source/nvtrace.c)
NVTRACE  := $(NVCOMPAT) -Wl,--wrap=nvioctlNvhostAsGpu_MapBufferEx -Wl,--wrap=nvioctlNvhostAsGpu_GetVARegions
LIBS     := -Wl,--start-group -lvulkan -lexpat -lzstd -lz -lnx -lstdc++ -lm -Wl,--end-group
LIBDIRS  := $(PORTLIBS) $(LIBNX)

ifneq ($(BUILD),$(notdir $(CURDIR)))
export OUTPUT   := $(CURDIR)/$(TARGET)
export TOPDIR   := $(CURDIR)
export VPATH    := $(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) $(CURDIR)/../../engine/src/common/platform/posix/nx
export DEPSDIR  := $(CURDIR)/$(BUILD)
CFILES          := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c))) nx_tls_bridge.c nx_nvcompat.c
export LD       := $(CC)
export OFILES   := $(CFILES:.c=.o)
export INCLUDE  := $(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
                   $(foreach dir,$(LIBDIRS),-I$(dir)/include) -I$(CURDIR)/$(BUILD)
export LIBPATHS := $(foreach dir,$(LIBDIRS),-L$(dir)/lib)
export APP_ICON
export NROFLAGS += --icon=$(APP_ICON) --nacp=$(CURDIR)/$(TARGET).nacp

.PHONY: $(BUILD) clean all
all: $(BUILD)
$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile
clean:
	@rm -rf $(BUILD) $(TARGET).nro $(TARGET).nacp $(TARGET).elf
else
DEPENDS := $(OFILES:.o=.d)
$(OUTPUT).nro : $(OUTPUT).elf $(OUTPUT).nacp
$(OUTPUT).elf : $(OFILES)
-include $(DEPENDS)
endif
