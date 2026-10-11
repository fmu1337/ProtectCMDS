# (C)2004-2010 SourceMod Development Team
# Makefile written by David "BAILOPAN" Anderson

###########################################
### EDIT THESE PATHS FOR YOUR OWN SETUP ###
###########################################

SMSDK = /home/src_sdk/sourcemod/1.6
MMSOURCE110 = /home/src_sdk/mmsource/1.10
HL2SDK_ORIG = /home/src_sdk/hl2sdk-episode1
HL2SDK_CSS = /home/src_sdk/hl2sdk-css

#####################################
### EDIT BELOW FOR OTHER PROJECTS ###
#####################################

PROJECT = protectcmds

#Uncomment for Metamod: Source enabled extension
USEMETA = true

OBJECTS = sdk/smsdk_ext.cpp extension.cpp helpers/util.cpp CDetour/detours.cpp asm/asm.c
OBJECTS += helpers/symbolmanager.cpp helpers/strtools.cpp helpers/sha256.cpp protectcmds/ifix.cpp
OBJECTS += helpers/inireader.cpp helpers/ini.c
OBJECTS += updater/updater.cpp updater/http.cpp

BEARSSL = third_party/bearssl
BEARSSL_SRC := $(shell find $(BEARSSL)/src -name '*.c') updater/trust_anchors.c

##############################################
### CONFIGURE ANY OTHER FLAGS/OPTIONS HERE ###
##############################################

C_OPT_FLAGS = -DNDEBUG -O3 -funroll-loops -pipe -fno-strict-aliasing -msse -msse2
C_DEBUG_FLAGS = -D_DEBUG -DDEBUG -g -ggdb3
C_GCC4_FLAGS = -mfpmath=sse -Wno-maybe-uninitialized
CPP_GCC4_FLAGS = -fno-omit-frame-pointer
CPP = gcc
CPP_OSX = clang

##########################
### SDK CONFIGURATIONS ###
##########################

override ENGSET = false

# Check for valid list of engines
ifneq (,$(filter css34 css,$(ENGINE)))
	override ENGSET = true
endif

ifeq "$(ENGINE)" "css34"
	HL2SDK = $(HL2SDK_ORIG)
	ENGINE_EXT = ext
	CFLAGS += -DENGINE_CSS34
	CFLAGS += -D_strnicmp=strncasecmp -Dstrcmpi=strcasecmp -D_stricmp=strcasecmp
	OBJECTS += protectcmds/blockloadplugin.cpp protectcmds/blockcmds.cpp
	OBJECTS += protectcmds/boneindexbynamefix.cpp protectcmds/gamestatsfix.cpp
	OBJECTS += protectcmds/gameuifix.cpp protectcmds/hashingsteamid.cpp
	OBJECTS += protectcmds/gamepacketfix.cpp protectcmds/observerloopfix.cpp
	OBJECTS += protectcmds/osfix.cpp protectcmds/preconnecthook.cpp
	OBJECTS += protectcmds/sourcetvfix.cpp protectcmds/takedamagefix.cpp
	OBJECTS += protectcmds/voicefix.cpp protectcmds/filterpacket.cpp
	OBJECTS += protectcmds/namefix.cpp protectcmds/netmessages.cpp
	OBJECTS += protectcmds/voxelhashfix.cpp protectcmds/specgotofix.cpp
	OBJECTS += protectcmds/vacpatch.cpp protectcmds/rcon_access.cpp
	OBJECTS += protectcmds/firegameeventfix.cpp protectcmds/helpers/vtfvalidator.cpp
endif
ifeq "$(ENGINE)" "css"
	HL2SDK = $(HL2SDK_CSS)
	ENGINE_EXT = ext
	CFLAGS += -DENGINE_CSS -DGNUC -Wno-dangling-else -Wno-unknown-pragmas
	OBJECTS += protectcmds/blockcmds.cpp protectcmds/voicefix.cpp protectcmds/hashingsteamid.cpp
	OBJECTS += protectcmds/filterpacket.cpp
endif

HL2PUB = $(HL2SDK)/public

ifeq "$(ENGINE)" "css34"
	INCLUDE += -I$(HL2SDK)/public/dlls
	METAMOD = $(MMSOURCE110)/core-legacy
else
	INCLUDE += -I$(HL2SDK)/public/game/server
	METAMOD = $(MMSOURCE110)/core
endif

OS := $(shell uname -s)

ifeq "$(OS)" "Darwin"
	LIB_EXT = dylib
	HL2LIB = $(HL2SDK)/lib/mac
else
	LIB_EXT = so
	ifeq "$(ENGINE)" "css34"
		HL2LIB = $(HL2SDK)/linux_sdk
	else
		HL2LIB = $(HL2SDK)/lib/public/linux
	endif
endif

#if ENGINE is original or OB
ifneq (,$(filter css34,$(ENGINE)))
	LIB_SUFFIX = _i486.$(LIB_EXT)
else
	LIB_PREFIX = lib
	ifneq "$(OS)" "Darwin"
		ifneq (,$(filter css,$(ENGINE)))
			LIB_SUFFIX = _srv.$(LIB_EXT)
		else
			LIB_SUFFIX = .$(LIB_EXT)
		endif
	endif
endif

INCLUDE += -I. -I.. -Isdk -I$(SMSDK)/public -I$(SMSDK)/public/sourcepawn -I$(BEARSSL)/inc

ifeq "$(USEMETA)" "true"
	LINK_HL2 = $(HL2LIB)/tier1_i486.a $(LIB_PREFIX)vstdlib$(LIB_SUFFIX) $(LIB_PREFIX)tier0$(LIB_SUFFIX)
	ifeq "$(ENGINE)" "csgo"
		LINK_HL2 += $(HL2LIB)/interfaces_i486.a -lstdc++
	endif

	LINK += $(LINK_HL2)

	INCLUDE += -I. -I.. -Isdk -I$(HL2PUB) -I$(HL2PUB)/engine -I$(HL2PUB)/steam -I$(HL2PUB)/mathlib -I$(HL2PUB)/tier0 \
	-I$(HL2PUB)/tier1 -I$(METAMOD) -I$(METAMOD)/sourcehook -I$(SMSDK)/public -I$(SMSDK)/public/extensions \
	-I$(SMSDK)/public/sourcepawn -I$(HL2SDK)/common
endif

LINK += -m32 -lm -ldl -lstdc++

CFLAGS += -DPOSIX -Dstricmp=strcasecmp -Dstrnicmp=strncasecmp \
	-D_snprintf=snprintf -D_vsnprintf=vsnprintf -D_alloca=alloca -DCOMPILER_GCC -Wall -Werror \
	-Wno-overloaded-virtual -Wno-switch -Wno-unused -msse -DSOURCEMOD_BUILD -DHAVE_STDINT_H -m32 -Wno-invalid-offsetof \
	-Wno-write-strings -Wno-class-memaccess -Wno-packed-not-aligned -Wno-address
CPPFLAGS += -Wno-non-virtual-dtor -fno-exceptions -fno-rtti

ifneq "$(PCMDS_REPOSITORY)" ""
	CFLAGS += -DPCMDS_REPOSITORY=\"$(PCMDS_REPOSITORY)\"
endif

################################################
### DO NOT EDIT BELOW HERE FOR MOST PROJECTS ###
################################################

BINARY = $(PROJECT).$(ENGINE_EXT).$(LIB_EXT)

ifeq "$(DEBUG)" "true"
	BIN_DIR = Debug
	CFLAGS += $(C_DEBUG_FLAGS)
else
	BIN_DIR = Release
	CFLAGS += $(C_OPT_FLAGS)
endif

ifeq "$(USEMETA)" "true"
	BIN_DIR := $(BIN_DIR).$(ENGINE)
endif

ifeq "$(OS)" "Darwin"
	CPP = $(CPP_OSX)
	LIB_EXT = dylib
	CFLAGS += -DOSX -D_OSX
	LINK += -dynamiclib -lstdc++ -mmacosx-version-min=10.5
else
	LIB_EXT = so
	CFLAGS += -D_LINUX -DLINUX -DPOSIX
	LINK += -shared
endif

IS_CLANG := $(shell $(CPP) --version | head -1 | grep clang > /dev/null && echo "1" || echo "0")

ifeq "$(IS_CLANG)" "1"
	CPP_MAJOR := $(shell $(CPP) --version | grep clang | sed "s/.*version \([0-9]\)*\.[0-9]*.*/\1/")
	CPP_MINOR := $(shell $(CPP) --version | grep clang | sed "s/.*version [0-9]*\.\([0-9]\)*.*/\1/")
else
	CPP_MAJOR := $(shell $(CPP) -dumpversion >&1 | cut -b1)
	CPP_MINOR := $(shell $(CPP) -dumpversion >&1 | cut -b3)
endif

# If not clang
ifeq "$(IS_CLANG)" "0"
	CFLAGS += -mfpmath=sse
endif

# Clang || GCC >= 4
ifeq "$(shell expr $(IS_CLANG) \| $(CPP_MAJOR) \>= 4)" "1"
	CFLAGS += $(C_GCC4_FLAGS)
	CPPFLAGS += $(CPP_GCC4_FLAGS)
endif

# Clang >= 3 || GCC >= 4.7
ifeq "$(shell expr $(IS_CLANG) \& $(CPP_MAJOR) \>= 3 \| $(CPP_MAJOR) \>= 4 \& $(CPP_MINOR) \>= 7)" "1"
	CFLAGS += -Wno-delete-non-virtual-dtor
endif

# OS is Linux and not using clang
ifeq "$(shell expr $(OS) \= Linux \& $(IS_CLANG) \= 0)" "1"
	LINK += -static-libgcc
endif

OBJ_BIN := $(OBJECTS:%.cpp=$(BIN_DIR)/%.o)

BEARSSL_OBJ := $(BEARSSL_SRC:%.c=$(BIN_DIR)/%.o)
BEARSSL_LIB = $(BIN_DIR)/libbearssl.a
BEARSSL_CFLAGS = -Os -m32 -fPIC -fvisibility=hidden -ffunction-sections -fdata-sections -I$(BEARSSL)/inc -I$(BEARSSL)/src

# This will break if we include other Makefiles, but is fine for now. It allows
#  us to make a copy of this file that uses altered paths (ie. Makefile.mine)
#  or other changes without mucking up the original.
MAKEFILE_NAME := $(CURDIR)/$(word $(words $(MAKEFILE_LIST)),$(MAKEFILE_LIST))

$(BIN_DIR)/%.o: %.cpp
	$(CPP) $(INCLUDE) $(CFLAGS) $(CPPFLAGS) -o $@ -c $<

all: check
	mkdir -p $(BIN_DIR)/sdk
	mkdir -p $(BIN_DIR)/CDetour
	mkdir -p $(BIN_DIR)/helpers
	mkdir -p $(BIN_DIR)/protectcmds/helpers
	mkdir -p $(BIN_DIR)/updater
	if [ "$(USEMETA)" = "true" ]; then \
		ln -sf $(HL2LIB)/$(LIB_PREFIX)vstdlib$(LIB_SUFFIX); \
		ln -sf $(HL2LIB)/$(LIB_PREFIX)tier0$(LIB_SUFFIX); \
	fi
	$(MAKE) -f $(MAKEFILE_NAME) extension

check:
	if [ "$(USEMETA)" = "true" ] && [ "$(ENGSET)" = "false" ]; then \
		echo "You must supply one of the following values for ENGINE:"; \
		echo "css34 or css"; \
		exit 1; \
	fi

$(BEARSSL_OBJ): $(BIN_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CPP) $(BEARSSL_CFLAGS) -o $@ -c $<

$(BEARSSL_LIB): $(BEARSSL_OBJ)
	$(AR) rcs $@ $^

extension: check $(OBJ_BIN) $(BEARSSL_LIB)
	$(CPP) $(INCLUDE) $(OBJ_BIN) $(BEARSSL_LIB) $(LINK) -o $(BIN_DIR)/$(BINARY)

debug:
	$(MAKE) -f $(MAKEFILE_NAME) all DEBUG=true

default: all

clean: check
	rm -rf $(BIN_DIR)
	rm -rf *.so

