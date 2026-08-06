# Orion Framework Makefile
# Builds Orion library, examples, and tests for Linux, macOS, and Windows

# Keep plain `make` anchored to the complete build even if generated rules
# appear before the all target below.
.DEFAULT_GOAL := all

# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -I. -DGL_SILENCE_DEPRECATION -D_DEFAULT_SOURCE
# silence unused parameter warnings
CFLAGS += -Wno-unused-parameter
# silence partial struct initializers where trailing fields intentionally default
CFLAGS += -Wno-missing-field-initializers
LDFLAGS = 
LIBS =

# Host uname value (used directly in platform conditionals below).
UNAME_S ?= $(shell uname -s)

# Platform flags
ifeq ($(OS),Windows_NT)
LIBS += -lglew32 -lopengl32 -lgdi32 -lwinmm -limm32 -lole32 -loleaut32 -lversion -luuid -lsetupapi -lxmllite
LDFLAGS_EXAMPLE = -mwindows
LDFLAGS_TEST = -mconsole
LIB_EXT = dll
LIB_FLAGS = -shared
EXE_EXT = .exe
IMPLIB_FLAGS = -Wl,--out-implib,$@.a
else ifeq ($(UNAME_S),Darwin)
ARCH ?= arm64
CFLAGS += -arch $(ARCH)
LDFLAGS += -arch $(ARCH)
LIBS += -framework OpenGL
LIB_EXT = dylib
LIB_FLAGS = -dynamiclib
else ifeq ($(UNAME_S),Linux)
LIBS += -lGL -lutil
LIB_EXT = so
LIB_FLAGS = -shared -fPIC
CFLAGS += -fPIC
else ifneq (,$(findstring MINGW,$(UNAME_S))$(findstring MSYS,$(UNAME_S)))
LIBS += -lglew32 -lopengl32 -lgdi32 -lwinmm -limm32 -lole32 -loleaut32 -lversion -luuid -lsetupapi -lxmllite
LDFLAGS_EXAMPLE = -mwindows
LDFLAGS_TEST = -mconsole
LIB_EXT = dll
LIB_FLAGS = -shared
EXE_EXT = .exe
IMPLIB_FLAGS = -Wl,--out-implib,$@.a
else
$(error Unsupported platform: OS=$(OS) UNAME_S=$(UNAME_S))
endif

DARWIN_CORE_UNDEF_FLAGS =
ifeq ($(UNAME_S),Darwin)
DARWIN_CORE_UNDEF_FLAGS = -undefined dynamic_lookup
endif

# Dependencies
PKG_CONFIG ?= pkg-config
LUA_CFLAGS := $(shell $(PKG_CONFIG) --cflags lua5.4 2>/dev/null || $(PKG_CONFIG) --cflags lua 2>/dev/null)
LUA_LIBS := $(shell $(PKG_CONFIG) --libs lua5.4 2>/dev/null || $(PKG_CONFIG) --libs lua 2>/dev/null)
LIBXML_CFLAGS := $(shell $(PKG_CONFIG) --cflags libxml-2.0 2>/dev/null)
LIBXML_LIBS := $(shell $(PKG_CONFIG) --libs libxml-2.0 2>/dev/null)

CFLAGS += $(LUA_CFLAGS) $(LIBXML_CFLAGS) -DHAVE_LUA
LIBS += $(filter-out -lm,$(LUA_LIBS) $(LIBXML_LIBS))
LIBS += -lm

# Compile flags for .gem shared libraries
GEM_CFLAGS = $(CFLAGS) -DBUILD_AS_GEM

APPS = examples
COMPS = components

# Build directories
BUILD_DIR = build
LIB_DIR = $(BUILD_DIR)/lib
BIN_DIR = $(BUILD_DIR)/bin
SHARE_DIR = $(BUILD_DIR)/share
GEM_DIR = $(BUILD_DIR)/gem
GENERATED_DIR = $(BUILD_DIR)/generated
TEST_DIR = tests

# Platform submodule library
PLATFORM_DIR = platform
PLATFORM_LIB = $(LIB_DIR)/libplatform.$(LIB_EXT)

# Core Orion libraries
USER_LIB = $(LIB_DIR)/libuser.$(LIB_EXT)
KERNEL_LIB = $(LIB_DIR)/libkernel.$(LIB_EXT)
COMMCTL_LIB = $(LIB_DIR)/libcommctl.$(LIB_EXT)
COMMDLG_LIB = $(LIB_DIR)/libcommdlg.$(LIB_EXT)
CORE_LIBS = $(USER_LIB) $(COMMCTL_LIB) $(COMMDLG_LIB) $(KERNEL_LIB)

USER_SRCS = $(filter-out orion/user/dialog.c orion/user/component_registry.c,$(wildcard orion/user/*.c))

# Shared rpath used exactly once per link command to avoid duplicate-rpath warnings.
RPATH_FLAGS = -Wl,-rpath,$(abspath $(LIB_DIR))

# Link flags for platform library
PLATFORM_LDFLAGS = -L$(LIB_DIR) -lplatform

# Link flags for the core Orion libraries.
CORE_LDLIBS = -L$(LIB_DIR) -lkernel -lcommctl -lcommdlg -luser
USER_LDLIBS = -L$(LIB_DIR) -lcommdlg -lkernel
COMMCTL_LDLIBS = -L$(LIB_DIR) -luser -lkernel
KERNEL_LDLIBS =
FE_PLUGIN_LDLIBS = $(CORE_LDLIBS)

# Tools directory
ORIONC_BIN = $(BIN_DIR)/orionc$(EXE_EXT)
TOOLS_SRCS = $(filter-out tools/orionc.c,$(wildcard tools/*.c))
TOOLS_BINS = $(patsubst tools/%.c,$(BIN_DIR)/%$(EXE_EXT),$(TOOLS_SRCS)) $(ORIONC_BIN)
TOOLS_CFLAGS = $(CFLAGS) -Wno-unused-function

# Examples are directory names under $(APPS)/, independent of their contents.
EXAMPLES = $(patsubst $(APPS)/%/,%,$(filter %/,$(wildcard $(APPS)/*/)))
EXAMPLE_BINS = $(patsubst %,$(BIN_DIR)/%$(EXE_EXT),$(EXAMPLES))
GEM_BINS = $(patsubst %,$(GEM_DIR)/%.gem,$(filter-out shell,$(EXAMPLES)))
COMPONENT_PLUGIN_BINS = $(patsubst $(APPS)/%/$(COMPS),$(LIB_DIR)/%_components.$(LIB_EXT),$(wildcard $(APPS)/*/$(COMPS)))

# ── Phony apps ─────────────────────────────────────────────────────────────
# Phony apps are alternative builds of existing examples with extra compiler
# flags. Add an entry below with PHONY_APPS_SRC_<name> and
# PHONY_APPS_CFLAGS_<name>, then add <name> to PHONY_APP_NAMES.
# The source example and its component plugin must exist under $(APPS)/
PHONY_APPS_SRC_penciltest   = imageeditor
PHONY_APPS_CFLAGS_penciltest = -DIMAGEEDITOR_BW=1 -DIMAGEEDITOR_BW_RETINA

PHONY_APP_NAMES = \
	penciltest

PHONY_APP_BINS = $(patsubst %,$(BIN_DIR)/%$(EXE_EXT),$(PHONY_APP_NAMES))
PHONY_APP_GEMS = $(patsubst %,$(GEM_DIR)/%.gem,$(PHONY_APP_NAMES))

# Unity-built examples still need ordinary source prerequisites.  Without
# these, editing an example leaves its existing binary newer than every listed
# prerequisite and make silently runs the stale executable.
$(foreach e,$(EXAMPLES),$(eval $(BIN_DIR)/$(e)$(EXE_EXT): $(shell find $(APPS)/$(e) -name "*.c" -o -name "*.h")))
$(foreach e,$(EXAMPLES),$(eval $(GEM_DIR)/$(e).gem: $(shell find $(APPS)/$(e) -name "*.c" -o -name "*.h")))
$(foreach a,$(PHONY_APP_NAMES),$(eval $(BIN_DIR)/$(a)$(EXE_EXT): $(shell find $(APPS)/$(PHONY_APPS_SRC_$(a)) -name "*.c" -o -name "*.h")))
$(foreach a,$(PHONY_APP_NAMES),$(eval $(GEM_DIR)/$(a).gem: $(shell find $(APPS)/$(PHONY_APPS_SRC_$(a)) -name "*.c" -o -name "*.h")))

GENERATED_HEADERS = $(patsubst $(APPS)/%.orion,$(GENERATED_DIR)/$(APPS)/%.h,$(wildcard $(APPS)/*/*.orion))

.SECONDEXPANSION:

TEST_SRCS = $(shell find $(TEST_DIR) -name "*.c" \
    ! -name "test_env.c" \
    ! -path "$(TEST_DIR)/*/support/*" \
    ! -path "$(TEST_DIR)/*/tests/*" | sort)
TEST_BINS = $(addprefix $(BIN_DIR)/test_,$(addsuffix $(EXE_EXT),$(basename $(notdir $(TEST_SRCS)))))

# Default target
.PHONY: all
ifeq ($(OS),Windows_NT)
all: library examples tools
else
all: library examples gems tools
endif

.PHONY: tools
tools: $(TOOLS_BINS)
	@echo "All tools built"

fonts: tools
	@mkdir -p share/fonts
	@$(BIN_DIR)/font_atlas share/fonts/ChiKareGo2.ttf share/fonts/Chicago-12.png -pixelsize=16 -em -sharp -cellw=10 -cellh=15 -v
	@$(BIN_DIR)/font_atlas share/fonts/FindersKeepers.ttf share/fonts/FindersKeepers.png -pixelsize=16 -em -sharp -cellw=8 -cellh=9 -v
# 	$(BIN_DIR)/font_atlas share/fonts/Pix32.ttf share/fonts/Geneva-12.png -pixelsize=12 -em -sharp -cellw=8 -cellh=16 -v
	@$(BIN_DIR)/font_atlas share/fonts/PixelOperator.ttf share/fonts/Geneva-12.png -pixelsize=16 -em -sharp -cellw=8 -cellh=16 -v -scan-width -letter-spacing=2
	@$(BIN_DIR)/font_atlas share/fonts/PixelOperatorMono.ttf share/fonts/Mono-12.png -pixelsize=16 -em -sharp -cellw=8 -cellh=16 -v
	
$(BIN_DIR)/%$(EXE_EXT): tools/%.c $(CORE_LIBS) | $(BIN_DIR)
	@echo "TOOL    $@"
	@$(CC) $(TOOLS_CFLAGS) -I. -Itools -o $@ $< \
	    $(LDFLAGS) $(CORE_LDLIBS) $(PLATFORM_LDFLAGS) $(RPATH_FLAGS) $(LIBS)
ifeq ($(OS),Windows_NT)
	@cp -f $(LIB_DIR)/libplatform.dll $(BIN_DIR)/
	@cp -f $(USER_LIB) $(BIN_DIR)/
	@cp -f $(COMMCTL_LIB) $(BIN_DIR)/
	@cp -f $(COMMDLG_LIB) $(BIN_DIR)/
	@cp -f $(KERNEL_LIB) $(BIN_DIR)/
endif

$(BIN_DIR)/gen_toolbox_atlas$(EXE_EXT): tools/gen_toolbox_atlas.c | $(BIN_DIR)
	@echo "TOOL    $@"
	@$(CC) $(TOOLS_CFLAGS) -I. -Itools -o $@ $< -lm

$(ORIONC_BIN): tools/orionc.c | $(BIN_DIR)
	@echo "TOOL    $@"
	@$(CC) $(TOOLS_CFLAGS) -I. -Itools -o $@ $< $(LDFLAGS) $(LIBS)

$(GENERATED_DIR)/$(APPS)/%.h: $(APPS)/%.orion $(ORIONC_BIN) | $(GENERATED_DIR)
	@mkdir -p $(dir $@)
	@echo "GEN     $@"
	@$(ORIONC_BIN) --input $< --output $@ --prefix $(notdir $(basename $<))

# Build the platform submodule shared library
.PHONY: platform
platform: $(PLATFORM_LIB)

$(PLATFORM_LIB): | $(LIB_DIR)
	@echo "PLATFORM"
	@$(MAKE) -s -C $(PLATFORM_DIR) OUTDIR=$(abspath $(LIB_DIR)) ARCH="$(ARCH)"

# VGA font TTF — copied from the source tree into build/share/orion/fonts.
# The character sheet is generated on the fly at runtime by vga_font.c
# using stb_truetype.  Drop any TTF (e.g. a Nerd Font) at
# share/fonts/monoid.ttf to replace it.
VGA_FONT_TTF = $(SHARE_DIR)/orion/fonts/monoid.ttf
VGA_FONT_SRC = share/fonts/monoid.ttf

$(VGA_FONT_TTF): $(VGA_FONT_SRC) | $(SHARE_DIR)
	@mkdir -p $(dir $@)
	@cp $(VGA_FONT_SRC) $@

# Shared data assets — copy framework and example resources
.PHONY: share
share: $(VGA_FONT_TTF) | $(SHARE_DIR)
	@mkdir -p $(SHARE_DIR)/orion
	@cp -R share/. $(SHARE_DIR)/orion/
	@for dir in $(APPS)/*/share; do \
	  [ -d "$$dir" ] || continue; \
	  name=$$(basename $$(dirname "$$dir")); \
	  mkdir -p $(SHARE_DIR)/$$name; \
	  cp -R $$dir/. $(SHARE_DIR)/$$name/; \
	done

# Core Orion libraries
.PHONY: library
library: $(CORE_LIBS)

$(USER_LIB): $(USER_SRCS) $(COMMDLG_LIB) $(KERNEL_LIB) $(PLATFORM_LIB) | $(LIB_DIR)
	@echo "LIB     $@"
	@printf '%s\n' $(sort $(USER_SRCS)) | sed 's/.*/#include "&"/' | \
	   $(CC) $(CFLAGS) $(LIB_FLAGS) -x c -o $@ - -x none $(LDFLAGS) $(RPATH_FLAGS) $(PLATFORM_LDFLAGS) $(USER_LDLIBS) $(LIBS) $(IMPLIB_FLAGS)
# Build commdlg static library
$(COMMDLG_LIB): $(wildcard orion/commdlg/*.c) | $(LIB_DIR)
	@echo "LIB     $@"
	@$(MAKE) -C orion/commdlg CC="$(CC)" CFLAGS="$(CFLAGS) -I$(abspath .)"
	@cp orion/commdlg/libcommdlg.a $(COMMDLG_LIB)

$(COMMCTL_LIB): $(wildcard orion/commctl/*.c) $(USER_LIB) $(KERNEL_LIB) $(PLATFORM_LIB) | $(LIB_DIR)
	@echo "LIB     $@"
	@find orion/commctl -name "*.c" | sort | sed 's/.*/#include "&"/' | \
	    $(CC) $(CFLAGS) $(LIB_FLAGS) -Icomponents -x c -o $@ - -x none $(LDFLAGS) $(RPATH_FLAGS) $(PLATFORM_LDFLAGS) $(COMMCTL_LDLIBS) $(LIBS) $(IMPLIB_FLAGS)

$(KERNEL_LIB): $(wildcard orion/kernel/*.c) $(PLATFORM_LIB) | $(LIB_DIR)
	@echo "LIB     $@"
	@find orion/kernel -name "*.c" | sort | sed 's/.*/#include "&"/' | \
	    $(CC) $(CFLAGS) $(LIB_FLAGS) -x c -o $@ - -x none $(LDFLAGS) $(RPATH_FLAGS) $(PLATFORM_LDFLAGS) $(KERNEL_LDLIBS) $(LIBS) $(IMPLIB_FLAGS)

# Examples
.PHONY: examples
examples: share $(EXAMPLE_BINS) $(COMPONENT_PLUGIN_BINS) $(PHONY_APP_BINS)

# Individual phony-app convenience targets (e.g. "make penciltest").
$(foreach a,$(PHONY_APP_NAMES),$(eval $(a): $(BIN_DIR)/$(a)$(EXE_EXT)))
.PHONY: $(PHONY_APP_NAMES)

.PHONY: plugins
plugins: $(COMPONENT_PLUGIN_BINS)

$(LIB_DIR)/%_components.$(LIB_EXT): $$(wildcard $(APPS)/$$*/$(COMPS)/*.c) $(CORE_LIBS) $(GENERATED_HEADERS) | $(LIB_DIR)
	@echo "PLUGIN  $@"
	@$(CC) $(CFLAGS) $(LIB_FLAGS) -I. -I$(APPS)/$* -I$(APPS)/$*/$(COMPS) -o $@ $(wildcard $(APPS)/$*/$(COMPS)/*.c) \
	    $(LDFLAGS) $(FE_PLUGIN_LDLIBS) $(PLATFORM_LDFLAGS) $(RPATH_FLAGS) $(LIBS)


$(EXAMPLE_BINS): $(BIN_DIR)/%$(EXE_EXT): $(CORE_LIBS) $(COMPONENT_PLUGIN_BINS) $(GENERATED_HEADERS) | $(BIN_DIR) share
	@echo "BIN     $@"
	@{(find $(APPS)/$* -name "*.c" ! -name "main.c" ! -path "*/$(COMPS)/*" | sort | sed 's/.*/#include "&"/'; \
	 echo '#include "$(APPS)/$*/main.c"') | \
		$(CC) $(CFLAGS) -I. -I$(APPS)/$* -I$(APPS)/$*/$(COMPS) -DSHAREDIR='"../share/$*"' -x c -o $@ - -x none \
	    $(LDFLAGS) $(LDFLAGS_EXAMPLE) $(CORE_LDLIBS) $(PLATFORM_LDFLAGS) $(RPATH_FLAGS) -L$(LIB_DIR) \
	    $(COMPONENT_PLUGIN_BINS) $(LIBS);}

# Phony app binaries — same unity-build pattern as regular examples, but with
# extra CFLAGS from PHONY_APPS_CFLAGS_<name> and SHAREDIR from the source app.
$(PHONY_APP_BINS): $(BIN_DIR)/%$(EXE_EXT): $(CORE_LIBS) $(COMPONENT_PLUGIN_BINS) $(GENERATED_HEADERS) | $(BIN_DIR) share
	@echo "PHONY   $@"
	@{(find $(APPS)/$(PHONY_APPS_SRC_$*) -name "*.c" ! -name "main.c" ! -path "*/$(COMPS)/*" | sort | sed 's/.*/#include "&"/'; \
	 echo '#include "$(APPS)/$(PHONY_APPS_SRC_$*)/main.c"') | \
		$(CC) $(CFLAGS) $(PHONY_APPS_CFLAGS_$*) -I. -I$(APPS)/$(PHONY_APPS_SRC_$*) -I$(APPS)/$(PHONY_APPS_SRC_$*)/$(COMPS) -DSHAREDIR='"../share/$(PHONY_APPS_SRC_$*)"' -x c -o $@ - -x none \
	    $(LDFLAGS) $(LDFLAGS_EXAMPLE) $(CORE_LDLIBS) $(PLATFORM_LDFLAGS) $(RPATH_FLAGS) -L$(LIB_DIR) \
	    $(COMPONENT_PLUGIN_BINS) $(LIBS);}

# Each .gem is built against the split core libraries (kernel/commctl/user)
# so it shares the same runtime infrastructure as the shell.
#
# gem_magic.h is force-included at the top of every gem's unity build so that
# BUILD_AS_GEM macros (running stub, ui_init/shutdown no-ops, etc.) apply to
# every source file in the gem without requiring manual edits to each file.

.PHONY: gems
gems: $(GEM_BINS) $(PHONY_APP_GEMS)
	@echo "OK All .gems built and validated"

$(GEM_BINS): $(GEM_DIR)/%.gem: $(CORE_LIBS) $(COMPONENT_PLUGIN_BINS) $(GENERATED_HEADERS) | $(GEM_DIR)
	@echo "GEM     $@"
	@{(echo '#include "gem_magic.h"'; \
	 find $(APPS)/$* -name "*.c" ! -name "main.c" ! -path "*/$(COMPS)/*" | sort | sed 's/.*/#include "&"/'; \
	 echo '#include "$(APPS)/$*/main.c"') | \
		$(CC) $(GEM_CFLAGS) $(LIB_FLAGS) -I. -I$(APPS)/$* -I$(APPS)/$*/$(COMPS) -DSHAREDIR='"../share/$*"' -x c -o $@ - -x none \
	    $(LDFLAGS) $(CORE_LDLIBS) $(PLATFORM_LDFLAGS) $(RPATH_FLAGS) -L$(LIB_DIR) \
	    $(COMPONENT_PLUGIN_BINS) $(LIBS);}
	@$(MAKE) --no-print-directory validate-gem GEM=$@

# Phony app gems — same pattern as regular gems with extra CFLAGS.
$(PHONY_APP_GEMS): $(GEM_DIR)/%.gem: $(CORE_LIBS) $(COMPONENT_PLUGIN_BINS) $(GENERATED_HEADERS) | $(GEM_DIR)
	@echo "GEM(P)  $@"
	@{(echo '#include "gem_magic.h"'; \
	 find $(APPS)/$(PHONY_APPS_SRC_$*) -name "*.c" ! -name "main.c" ! -path "*/$(COMPS)/*" | sort | sed 's/.*/#include "&"/'; \
	 echo '#include "$(APPS)/$(PHONY_APPS_SRC_$*)/main.c"') | \
		$(CC) $(GEM_CFLAGS) $(PHONY_APPS_CFLAGS_$*) $(LIB_FLAGS) -I. -I$(APPS)/$(PHONY_APPS_SRC_$*) -I$(APPS)/$(PHONY_APPS_SRC_$*)/$(COMPS) -DSHAREDIR='"../share/$(PHONY_APPS_SRC_$*)"' -x c -o $@ - -x none \
	    $(LDFLAGS) $(CORE_LDLIBS) $(PLATFORM_LDFLAGS) $(RPATH_FLAGS) -L$(LIB_DIR) \
	    $(COMPONENT_PLUGIN_BINS) $(LIBS);}
	@$(MAKE) --no-print-directory validate-gem GEM=$@

# Validate that a .gem exports the required gem_get_interface symbol.
.PHONY: validate-gem
validate-gem:
ifeq ($(OS),Windows_NT)
	@dumpbin //EXPORTS $(GEM) 2>/dev/null | grep -q "gem_get_interface" || (echo "FAIL missing gem_get_interface" && exit 1)
else ifeq ($(UNAME_S),Darwin)
	@nm -g $(GEM) 2>/dev/null | grep -q "T _gem_get_interface" || (echo "FAIL missing gem_get_interface" && exit 1)
else
	@nm -D $(GEM) 2>/dev/null | grep -q "T gem_get_interface" || (echo "FAIL missing gem_get_interface" && exit 1)
endif

# Tests
.PHONY: test
test: $(TEST_BINS)
	@echo "Running tests..."
ifeq ($(OS),Windows_NT)
	@cp -f $(LIB_DIR)/libplatform.dll $(BIN_DIR)/
	@cp -f $(USER_LIB) $(BIN_DIR)/
	@cp -f $(COMMCTL_LIB) $(BIN_DIR)/
	@cp -f $(COMMDLG_LIB) $(BIN_DIR)/
	@cp -f $(KERNEL_LIB) $(BIN_DIR)/
endif
	@for test in $(TEST_BINS); do \
	    echo "Running $$test..."; \
	    $$test || exit 1; \
	done
	@echo "All tests passed!"

$(TEST_BINS): $(BIN_DIR)/test_%$(EXE_EXT): $(TEST_SRCS) $(TEST_DIR)/test_env.c $(GENERATED_HEADERS) $(CORE_LIBS) $(COMPONENT_PLUGIN_BINS) | $(BIN_DIR)
	@echo "TEST    $@"
	@src=$$(find $(TEST_DIR) -name "$*.c" ! -path "$(TEST_DIR)/*/tests/*" | head -n 1); \
	app_dir=""; \
	app=""; \
	stem="$*"; \
	case "$$src" in \
	  $(TEST_DIR)/*/*.c) app_dir=$${src#$(TEST_DIR)/}; app_dir=$${app_dir%%/*}; app="$$app_dir" ;; \
	esac; \
	if [ -z "$$app" ]; then \
	  cand=$${stem%_test}; \
	  if [ -d "$(APPS)/$$cand" ]; then app="$$cand"; fi; \
	fi; \
	if [ -z "$$app" ]; then \
	  cand=$${stem%%_*}; \
	  if [ -d "$(APPS)/$$cand" ]; then app="$$cand"; fi; \
	fi; \
	{ \
	  printf '#include "%s"\n' "$$src"; \
	  printf '#include "$(TEST_DIR)/test_env.c"\n'; \
	  if [ -n "$$app_dir" ] && [ -d "$(APPS)/$$app_dir" ]; then \
	    find "$(APPS)/$$app_dir" -name "*.c" ! -name "main.c" ! -path "*/$(COMPS)/*" | sort | sed 's/.*/#include "&"/'; \
	  fi; \
	  if [ -n "$$app_dir" ] && [ -d "$(TEST_DIR)/$$app_dir/support" ]; then \
	    find "$(TEST_DIR)/$$app_dir/support" -name "*.c" | sort | sed 's/.*/#include "&"/'; \
	  fi; \
	} | \
		$(CC) $(CFLAGS) -I. -Itests -I$(APPS)/$$app -I$(APPS)/$$app/$(COMPS) -x c -o $@ - -x none \
	    $(LDFLAGS) $(LDFLAGS_TEST) $(CORE_LDLIBS) $(PLATFORM_LDFLAGS) $(RPATH_FLAGS) -L$(LIB_DIR) \
	    $(COMPONENT_PLUGIN_BINS) $(LIBS)

# Directory creation
BUILD_DIRS = $(BUILD_DIR) $(LIB_DIR) $(BIN_DIR) $(SHARE_DIR) $(GEM_DIR) $(GENERATED_DIR)

$(BUILD_DIRS):
	@mkdir -p $@

# Clean
.PHONY: clean
clean:
	@echo "Cleaning build artifacts..."
	@rm -rf $(BUILD_DIR)
	@$(MAKE) -s -C $(PLATFORM_DIR) OUTDIR=$(abspath $(LIB_DIR)) ARCH="$(ARCH)" clean 2>/dev/null || true

# Help
.PHONY: help
help:
	@echo "Orion UI Framework - Build System"
	@echo ""
	@echo "Available targets:"
	@echo "all          - Build library, examples, gems, and tools"
	@echo "library      - Build shared library"
	@echo "examples     - Build example applications"
	@echo "gems         - Build all .gem shared libraries"
	@echo "test         - Build and run tests"
	@echo "clean        - Remove all build artifacts"
	@echo "help         - Show this help message"
	@echo ""
	@echo "Phony apps (derived builds with extra flags):"
	$(foreach a,$(PHONY_APP_NAMES),@echo "  $a       - $(APPS)/$(PHONY_APPS_SRC_$(a)) + $(PHONY_APPS_CFLAGS_$(a))"
	)
	@echo ""
	@echo "Output directories:"
	@echo "$(LIB_DIR)  - Libraries"
	@echo "$(SHARE_DIR) - Shared data assets (icons, etc.)"
