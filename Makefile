#---------------------------------------------------------------------------------
# TOTK Explorer - Ultrahand Overlay
# Target: The Legend of Zelda: Tears of the Kingdom 1.4.0-1.4.3
#---------------------------------------------------------------------------------
.SUFFIXES:

ifeq ($(strip $(DEVKITPRO)),)
$(error "Please set DEVKITPRO in your environment. export DEVKITPRO=<path to>/devkitpro")
endif

TOPDIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
TOPDIR := $(patsubst %/,%,$(TOPDIR))

include $(DEVKITPRO)/libnx/switch_rules

APP_TITLE := TOTK Explorer
APP_VERSION := 3.5.0
TARGET := TOTK-Explorer-v3
BUILD := build
SOURCES := source
INCLUDES := include

# libultrahand is pinned to the exact submodule commit used by the working Tetris overlay.
include $(TOPDIR)/libs/libultrahand/ultrahand.mk
NO_ICON := 1

# Embed the NACP metadata into the NRO. Ultrahand parses this metadata when
# discovering .ovl files; without it the NRO can still launch, but the overlay
# is rejected from the Ultrahand list.
ifeq ($(strip $(NO_NACP)),)
	export NROFLAGS += --nacp=$(TOPDIR)/$(TARGET).nacp
endif

ARCH := -march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIE
CFLAGS := -g -O2 -ffunction-sections -w $(ARCH) $(DEFINES)
CFLAGS += $(INCLUDE) -D__SWITCH__
CXXFLAGS := $(CFLAGS) -fno-exceptions -std=c++20
ASFLAGS := -g $(ARCH)
LDFLAGS = -specs=$(DEVKITPRO)/libnx/switch.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map)
LIBS := -lcurl -lz -lmbedtls -lmbedx509 -lmbedcrypto $(TOPDIR)/libs/libdmntcht.a -lnx
LIBDIRS := $(TOPDIR)/libs $(PORTLIBS) $(LIBNX)

ifneq ($(BUILD),$(notdir $(CURDIR)))

export OUTPUT := $(TOPDIR)/$(TARGET)
export TOPDIR := $(TOPDIR)
export VPATH := $(foreach dir,$(SOURCES),$(TOPDIR)/$(dir))
export DEPSDIR := $(TOPDIR)/$(BUILD)

CFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(TOPDIR)/$(dir)/*.c)))
CPPFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(TOPDIR)/$(dir)/*.cpp)))
SFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(TOPDIR)/$(dir)/*.s)))

ifeq ($(strip $(CPPFILES)),)
export LD := $(CC)
else
export LD := $(CXX)
endif

export OFILES_BIN :=
export OFILES_SRC := $(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.s=.o)
export OFILES := $(OFILES_SRC)
export HFILES_BIN :=
export INCLUDE := $(foreach dir,$(INCLUDES),-I$(TOPDIR)/$(dir)) \
                  $(foreach dir,$(LIBDIRS),-I$(dir)/include) \
                  -I$(TOPDIR)/$(BUILD)
export LIBPATHS := $(foreach dir,$(LIBDIRS),-L$(dir)/lib)

.PHONY: all clean setup verify-layout

all: $(BUILD)

$(BUILD):
	@mkdir -p $@
	@$(MAKE) --no-print-directory -C $@ -f $(TOPDIR)/Makefile all

setup:
	@bash $(TOPDIR)/tools/setup_deps.sh

verify-layout:
	@test -f $(TOPDIR)/data/points.csv
	@test -f $(TOPDIR)/include/explorer.hpp
	@test -f $(TOPDIR)/source/main.cpp
	@test -f $(TOPDIR)/source/memory.cpp
	@test -f $(TOPDIR)/source/map.cpp
	@test -f $(TOPDIR)/source/ui.cpp
	@test -f $(TOPDIR)/libs/libdmntcht.a
	@test -f $(TOPDIR)/libs/libultrahand/ultrahand.mk
	@test -d $(TOPDIR)/libs/libultrahand/common
	@test -d $(TOPDIR)/libs/libultrahand/libultra/source
	@test -f $(TOPDIR)/libs/libultrahand/libtesla/include/tesla.hpp

clean:
	@rm -fr $(BUILD) $(TARGET).ovl $(TARGET).nro $(TARGET).nacp $(TARGET).elf $(TARGET).map

else

.PHONY: all
DEPENDS := $(OFILES:.o=.d)

all: $(OUTPUT).ovl

$(OUTPUT).nro: $(OUTPUT).elf $(OUTPUT).nacp
	@elf2nro $< $@ $(NROFLAGS)
	@echo "built ... $(notdir $(OUTPUT).nro)"

$(OUTPUT).ovl: $(OUTPUT).nro
	@cp $< $@
	@printf 'ULTR' >> $@
	@echo "built ... $(notdir $(OUTPUT).ovl)"
	@echo "Ultrahand signature (ULTR) appended"

$(OUTPUT).elf: $(OFILES)

-include $(DEPENDS)

endif
