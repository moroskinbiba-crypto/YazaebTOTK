# TOTK Explorer — Stage 1: minimal Tesla baseline
# Toolchain versions are fixed in .github/workflows/build.yml.
.SUFFIXES:

ifeq ($(strip $(DEVKITPRO)),)
$(error "Please set DEVKITPRO in your environment. export DEVKITPRO=<path to>/devkitpro")
endif

TOPDIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
TOPDIR := $(patsubst %/,%,$(TOPDIR))

include $(DEVKITPRO)/libnx/switch_rules

APP_TITLE := TOTK Explorer
APP_VERSION := 0.1.0-stage1
TARGET := TOTK-Explorer
BUILD := build
SOURCES := source
INCLUDES :=

# Exact libultrahand/libtesla snapshot, fetched by CI.
# ultrahand.mk appends its common/libultra/libtesla source and include paths.
include $(TOPDIR)/libs/libultrahand/ultrahand.mk

NO_ICON := 1

ifeq ($(strip $(NO_NACP)),)
	export NROFLAGS += --nacp=$(TOPDIR)/$(TARGET).nacp
endif

ARCH := -march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIE
LIBDIRS := $(PORTLIBS) $(LIBNX)

# Compute include paths before CXXFLAGS is formed so both outer and recursive
# make invocations compile C++ files with all libultrahand dependencies.
INCLUDE := $(foreach dir,$(INCLUDES),-I$(TOPDIR)/$(dir)) \
           $(foreach dir,$(LIBDIRS),-I$(dir)/include) \
           -I$(TOPDIR)/$(BUILD)

CFLAGS := -g -O2 -ffunction-sections -w $(ARCH) $(DEFINES) $(INCLUDE) -D__SWITCH__
CXXFLAGS := $(CFLAGS) -fno-exceptions -std=c++20
ASFLAGS := -g $(ARCH)
LDFLAGS = -specs=$(DEVKITPRO)/libnx/switch.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map)
LIBS := -lcurl -lz -lminizip -lmbedtls -lmbedx509 -lmbedcrypto -lnx

ifneq ($(BUILD),$(notdir $(CURDIR)))

export OUTPUT := $(TOPDIR)/$(TARGET)
export TOPDIR := $(TOPDIR)
export VPATH := $(foreach dir,$(SOURCES),$(TOPDIR)/$(dir))
export DEPSDIR := $(TOPDIR)/$(BUILD)
export INCLUDE
export LIBPATHS := $(foreach dir,$(LIBDIRS),-L$(dir)/lib)

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

.PHONY: all clean

all: $(BUILD)

$(BUILD):
	@mkdir -p $@
	@$(MAKE) --no-print-directory -C $@ -f $(TOPDIR)/Makefile all

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
