# Orion Framework Makefile
# Builds Orion library, examples, and tests for Linux, macOS, and Windows

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
LIBS += -lGL
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
CORE_LIBS = $(USER_LIB) $(COMMCTL_LIB) $(KERNEL_LIB)

# Shared rpath used exactly once per link command to avoid duplicate-rpath warnings.
RPATH_FLAGS = -Wl,-rpath,$(abspath $(LIB_DIR))

# Link flags for platform library
PLATFORM_LDFLAGS = -L$(LIB_DIR) -lplatform

# Link flags for the core Orion libraries.
CORE_LDLIBS = -L$(LIB_DIR) -lkernel -lcommctl -luser
USER_LDLIBS =
COMMCTL_LDLIBS = -L$(LIB_DIR) -luser
KERNEL_LDLIBS = -L$(LIB_DIR) -lcommctl -luser
FE_PLUGIN_LDLIBS = $(CORE_LDLIBS)

# Tools directory
ORIONC_BIN = $(BIN_DIR)/orionc$(EXE_EXT)
TOOLS_SRCS = $(filter-out tools/orionc.c,$(wildcard tools/*.c))
TOOLS_BINS = $(patsubst tools/%.c,$(BIN_DIR)/%$(EXE_EXT),$(TOOLS_SRCS)) $(ORIONC_BIN)
TOOLS_CFLAGS = $(CFLAGS) -Wno-unused-function

# Examples are directory names under examples/, independent of their contents.
EXAMPLES = $(patsubst examples/%/,%,$(filter %/,$(wildcard examples/*/)))
EXAMPLE_BINS = $(patsubst %,$(BIN_DIR)/%$(EXE_EXT),$(EXAMPLES))
GEM_BINS = $(patsubst %,$(GEM_DIR)/%.gem,$(EXAMPLES))

APP_COMPONENT_PLUGIN_BINS = $(patsubst components/%,$(LIB_DIR)/%_components.$(LIB_EXT),$(filter-out components/commctl,$(wildcard components/*)))
COMPONENT_PLUGIN_BINS = $(APP_COMPONENT_PLUGIN_BINS)

GENERATED_HEADERS = $(patsubst examples/%.orion,$(GENERATED_DIR)/examples/%.h,$(wildcard examples/*/*.orion))

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
endif

.PHONY: tools
tools: $(TOOLS_BINS)
	@echo "All tools built"

fonts: tools
	@mkdir -p share/fonts
	$(BIN_DIR)/font_atlas fonts/ChiKareGo2.ttf share/fonts/Chicago-12.png -pixelsize=16 -em -sharp -cellw=10 -cellh=15 -v
	$(BIN_DIR)/font_atlas fonts/FindersKeepers.ttf share/fonts/FindersKeepers.png -pixelsize=16 -em -sharp -cellw=8 -cellh=9 -v
# 	$(BIN_DIR)/font_atlas fonts/Pix32.ttf share/fonts/Geneva-12.png -pixelsize=12 -em -sharp -cellw=8 -cellh=16 -v
	$(BIN_DIR)/font_atlas fonts/PixelOperator.ttf share/fonts/Geneva-12.png -pixelsize=16 -em -sharp -cellw=8 -cellh=16 -v -scan-width -letter-spacing=2
	$(BIN_DIR)/font_atlas fonts/PixelOperatorMono.ttf share/fonts/Mono-12.png -pixelsize=16 -em -sharp -cellw=8 -cellh=16 -v
	
$(BIN_DIR)/%$(EXE_EXT): tools/%.c $(CORE_LIBS) | $(BIN_DIR)
	$(CC) $(TOOLS_CFLAGS) -I. -Itools -o $@ $< \
	    $(LDFLAGS) $(CORE_LDLIBS) $(PLATFORM_LDFLAGS) $(RPATH_FLAGS) $(LIBS)
ifeq ($(OS),Windows_NT)
	@cp -f $(LIB_DIR)/libplatform.dll $(BIN_DIR)/
	@cp -f $(USER_LIB) $(BIN_DIR)/
	@cp -f $(COMMCTL_LIB) $(BIN_DIR)/
	@cp -f $(KERNEL_LIB) $(BIN_DIR)/
endif

$(BIN_DIR)/gen_toolbox_atlas$(EXE_EXT): tools/gen_toolbox_atlas.c | $(BIN_DIR)
	$(CC) $(TOOLS_CFLAGS) -I. -Itools -o $@ $< -lm

$(ORIONC_BIN): tools/orionc.c | $(BIN_DIR)
	$(CC) $(TOOLS_CFLAGS) -I. -Itools -o $@ $< $(LDFLAGS) $(LIBS)

$(GENERATED_DIR)/examples/%.h: examples/%.orion $(ORIONC_BIN) | $(GENERATED_DIR)
	@mkdir -p $(dir $@)
	$(ORIONC_BIN) --input $< --output $@ --prefix $(notdir $(basename $<))

# Build the platform submodule shared library
.PHONY: platform
platform: $(PLATFORM_LIB)

$(PLATFORM_LIB): | $(LIB_DIR)
	$(MAKE) -C $(PLATFORM_DIR) OUTDIR=$(abspath $(LIB_DIR)) ARCH="$(ARCH)"

# VGA font sheet — copied from the source tree into build/share/orion/fonts.
# Place your custom font at share/fonts/vga-rom-font-8x16.png and it will be used
# by gitclient at runtime.
VGA_FONT_PNG = $(SHARE_DIR)/orion/fonts/vga-rom-font-8x16.png
VGA_FONT_SRC = share/fonts/vga-rom-font-8x16.png

$(VGA_FONT_PNG): $(VGA_FONT_SRC) | $(SHARE_DIR)
	@mkdir -p $(dir $@)
	cp $(VGA_FONT_SRC) $@

# Shared data assets — copy framework and example resources
.PHONY: share
share: $(VGA_FONT_PNG) | $(SHARE_DIR)
	@mkdir -p $(SHARE_DIR)/orion
	@cp -R share/. $(SHARE_DIR)/orion/
	@for dir in examples/*/share; do \
	  [ -d "$$dir" ] || continue; \
	  name=$$(basename $$(dirname "$$dir")); \
	  mkdir -p $(SHARE_DIR)/$$name; \
	  cp -R $$dir/. $(SHARE_DIR)/$$name/; \
	done

# Core Orion libraries
.PHONY: library
library: $(CORE_LIBS)

$(USER_LIB): $(wildcard user/*.c) $(PLATFORM_LIB) | $(LIB_DIR)
	find user -name "*.c" | sort | sed 's/.*/#include "&"/' | \
	    $(CC) $(CFLAGS) $(LIB_FLAGS) $(DARWIN_CORE_UNDEF_FLAGS) -x c -o $@ - -x none $(LDFLAGS) $(RPATH_FLAGS) $(PLATFORM_LDFLAGS) $(USER_LDLIBS) $(LIBS) $(IMPLIB_FLAGS)

$(COMMCTL_LIB): $(wildcard components/commctl/*.c) $(USER_LIB) $(PLATFORM_LIB) | $(LIB_DIR)
	find components/commctl -name "*.c" | sort | sed 's/.*/#include "&"/' | \
	    $(CC) $(CFLAGS) $(LIB_FLAGS) $(DARWIN_CORE_UNDEF_FLAGS) -Icomponents -x c -o $@ - -x none $(LDFLAGS) $(RPATH_FLAGS) $(PLATFORM_LDFLAGS) $(COMMCTL_LDLIBS) $(LIBS) $(IMPLIB_FLAGS)

$(KERNEL_LIB): $(wildcard kernel/*.c) $(COMMCTL_LIB) $(USER_LIB) $(PLATFORM_LIB) | $(LIB_DIR)
	find kernel -name "*.c" | sort | sed 's/.*/#include "&"/' | \
	    $(CC) $(CFLAGS) $(LIB_FLAGS) $(DARWIN_CORE_UNDEF_FLAGS) -x c -o $@ - -x none $(LDFLAGS) $(RPATH_FLAGS) $(PLATFORM_LDFLAGS) $(KERNEL_LDLIBS) $(LIBS) $(IMPLIB_FLAGS)

# Examples
.PHONY: examples
examples: share $(EXAMPLE_BINS) $(COMPONENT_PLUGIN_BINS)

.PHONY: plugins
plugins: $(COMPONENT_PLUGIN_BINS)

$(LIB_DIR)/%_components.$(LIB_EXT): $$(wildcard components/$$*/*.c) $(CORE_LIBS) | $(LIB_DIR)
	$(CC) $(CFLAGS) $(LIB_FLAGS) -I. -Iexamples/$* -Icomponents/$* -o $@ $(wildcard components/$*/*.c) \
	    $(LDFLAGS) $(FE_PLUGIN_LDLIBS) $(RPATH_FLAGS) $(LIBS)


$(EXAMPLE_BINS): $(BIN_DIR)/%$(EXE_EXT): $(CORE_LIBS) $(COMPONENT_PLUGIN_BINS) $(GENERATED_HEADERS) | $(BIN_DIR) share
	@(find examples/$* -name "*.c" ! -name "main.c" | sort | sed 's/.*/#include "&"/'; \
	 echo '#include "examples/$*/main.c"') | \
		$(CC) $(CFLAGS) -I. -Iexamples/$* -Icomponents -Icomponents/$* -DSHAREDIR='"../share/$*"' -x c -o $@ - -x none \
	    $(LDFLAGS) $(LDFLAGS_EXAMPLE) $(CORE_LDLIBS) $(PLATFORM_LDFLAGS) $(RPATH_FLAGS) -L$(LIB_DIR) \
	    $(COMPONENT_PLUGIN_BINS) $(LIBS)

# Each .gem is built against the split core libraries (kernel/commctl/user)
# so it shares the same runtime infrastructure as the shell.
#
# gem_magic.h is force-included at the top of every gem's unity build so that
# BUILD_AS_GEM macros (running stub, ui_init/shutdown no-ops, etc.) apply to
# every source file in the gem without requiring manual edits to each file.

.PHONY: gems
gems: $(GEM_BINS)
	@echo "OK All .gems built and validated"

$(GEM_BINS): $(GEM_DIR)/%.gem: $(CORE_LIBS) $(COMPONENT_PLUGIN_BINS) $(GENERATED_HEADERS) | $(GEM_DIR)
	@(echo '#include "gem_magic.h"'; \
	 find examples/$* -name "*.c" ! -name "main.c" | sort | sed 's/.*/#include "&"/'; \
	 echo '#include "examples/$*/main.c"') | \
		$(CC) $(GEM_CFLAGS) $(LIB_FLAGS) -I. -Iexamples/$* -Icomponents -Icomponents/$* -DSHAREDIR='"../share/$*"' -x c -o $@ - -x none \
	    $(LDFLAGS) $(CORE_LDLIBS) $(PLATFORM_LDFLAGS) $(RPATH_FLAGS) -L$(LIB_DIR) \
	    $(COMPONENT_PLUGIN_BINS) $(LIBS)
	@$(MAKE) --no-print-directory validate-gem GEM=$@

# Validate that a .gem exports the required gem_get_interface symbol.
.PHONY: validate-gem
validate-gem:
ifeq ($(OS),Windows_NT)
	@dumpbin //EXPORTS $(GEM) 2>/dev/null | grep -q "gem_get_interface" \
	    || (echo "FAIL missing gem_get_interface" && exit 1)
else ifeq ($(UNAME_S),Darwin)
	@nm -g $(GEM) 2>/dev/null | grep -q "T _gem_get_interface" \
	    || (echo "FAIL missing gem_get_interface" && exit 1)
else
	@nm -D $(GEM) 2>/dev/null | grep -q "T gem_get_interface" \
	    || (echo "FAIL missing gem_get_interface" && exit 1)
endif

$(GEM_DIR):
	mkdir -p $@

# === Orion Shell ===
#
# The shell links against the split core libraries and exports its symbols with
# -Wl,--export-dynamic (Linux) so that gems whose unresolved references were
# not satisfied by shared libraries can still resolve them from the shell.

.PHONY: shell
shell: $(SHELL_BIN)

$(SHELL_BIN): $(SHELL_SRCS) $(CORE_LIBS) | $(BIN_DIR)
	@(find shell -name "*.c" | sort | sed 's/.*/#include "&"/') | \
	    $(CC) $(CFLAGS) -I. -Ishell -x c -o $@ - -x none \
	    $(LDFLAGS) $(CORE_LDLIBS) $(PLATFORM_LDFLAGS) $(RPATH_FLAGS) $(LDFLAGS_EXAMPLE) $(COMPONENT_PLUGIN_BINS) $(LIBS) $(SHELL_EXTRA_LDFLAGS)

# Tests
.PHONY: test
test: $(TEST_BINS)
	@echo "Running tests..."
ifeq ($(OS),Windows_NT)
	@cp -f $(LIB_DIR)/libplatform.dll $(BIN_DIR)/
	@cp -f $(USER_LIB) $(BIN_DIR)/
	@cp -f $(COMMCTL_LIB) $(BIN_DIR)/
	@cp -f $(KERNEL_LIB) $(BIN_DIR)/
endif
	@for test in $(TEST_BINS); do \
	    echo "Running $$test..."; \
	    $$test || exit 1; \
	done
	@echo "All tests passed!"

$(TEST_BINS): $(BIN_DIR)/test_%$(EXE_EXT): $(TEST_SRCS) $(TEST_DIR)/test_env.c $(GENERATED_HEADERS) $(CORE_LIBS) $(COMPONENT_PLUGIN_BINS) | $(BIN_DIR)
	@src=$$(find $(TEST_DIR) -name "$*.c" ! -path "$(TEST_DIR)/*/tests/*" | head -n 1); \
	app_dir=""; \
	app=""; \
	stem="$*"; \
	case "$$src" in \
	  $(TEST_DIR)/*/*.c) app_dir=$${src#$(TEST_DIR)/}; app_dir=$${app_dir%%/*}; app="$$app_dir" ;; \
	esac; \
	if [ -z "$$app" ]; then \
	  cand=$${stem%_test}; \
	  if [ -d "examples/$$cand" ]; then app="$$cand"; fi; \
	fi; \
	if [ -z "$$app" ]; then \
	  cand=$${stem%%_*}; \
	  if [ -d "examples/$$cand" ]; then app="$$cand"; fi; \
	fi; \
	{ \
	  printf '#include "%s"\n' "$$src"; \
	  printf '#include "$(TEST_DIR)/test_env.c"\n'; \
	  if [ -n "$$app_dir" ] && [ -d "examples/$$app_dir" ]; then \
	    find "examples/$$app_dir" -name "*.c" ! -name "main.c" | sort | sed 's/.*/#include "&"/'; \
	  fi; \
	  if [ -n "$$app_dir" ] && [ -d "$(TEST_DIR)/$$app_dir/support" ]; then \
	    find "$(TEST_DIR)/$$app_dir/support" -name "*.c" | sort | sed 's/.*/#include "&"/'; \
	  fi; \
	} | \
		$(CC) $(CFLAGS) -I. -Itests -Iexamples/$$app -Icomponents -Icomponents/$$app -x c -o $@ - -x none \
	    $(LDFLAGS) $(LDFLAGS_TEST) $(CORE_LDLIBS) $(PLATFORM_LDFLAGS) $(RPATH_FLAGS) -L$(LIB_DIR) \
	    $(COMPONENT_PLUGIN_BINS) $(LIBS)

# Directory creation
BUILD_DIRS = $(BUILD_DIR) $(LIB_DIR) $(BIN_DIR) $(SHARE_DIR) $(GENERATED_DIR)

$(BUILD_DIRS):
	mkdir -p $@

# Clean
.PHONY: clean
clean:
	@echo "Cleaning build artifacts..."
	rm -rf $(BUILD_DIR)
	$(MAKE) -C $(PLATFORM_DIR) OUTDIR=$(abspath $(LIB_DIR)) ARCH="$(ARCH)" clean 2>/dev/null || true

# Help
.PHONY: help
help:
	@echo "Orion UI Framework - Build System"
	@echo ""
	@echo "Available targets:"
	@echo "  all       - Build library, examples, and gems"
	@echo "  library   - Build shared library"
	@echo "  examples  - Build example applications"
	@echo "  gems      - Build all .gem shared libraries"
	@echo "  test      - Build and run tests"
	@echo "  clean     - Remove all build artifacts"
	@echo "  help      - Show this help message"
	@echo ""
	@echo "Output directories:"
	@echo "  $(LIB_DIR)  - Libraries"
	@echo "  $(SHARE_DIR) - Shared data assets (icons, etc.)"
