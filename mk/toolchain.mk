# Linux Kernel-style Toolchain Configuration
# ============================================
#
# This file implements toolchain detection and configuration following
# Linux kernel conventions. Supports native and cross-compilation builds.
#
# Usage:
#   make                              # Native build with system compiler
#   make CROSS_COMPILE=arm-none-eabi- # Cross-compile for ARM bare-metal
#   make CC=clang                     # Use specific compiler
#   make CC=emcc                      # Build for WebAssembly with Emscripten
#
# Reference: https://www.kernel.org/doc/html/latest/kbuild/kbuild.html

# Include guard to prevent double inclusion
ifndef TOOLCHAIN_MK_INCLUDED
TOOLCHAIN_MK_INCLUDED := 1

# Compiler identification flags
CC_IS_CLANG :=
CC_IS_GCC :=
CC_IS_EMCC :=

# Detect host architecture for platform-specific optimizations
HOSTARCH := $(shell uname -m)

# Set CC based on CROSS_COMPILE or default compiler
# Check if CC comes from Make's default or command line/environment
ifeq ($(origin CC),default)
    # CC is Make's built-in default, override it
    ifdef CROSS_COMPILE
        CC := $(CROSS_COMPILE)gcc
    else
        CC := cc
    endif
endif

# Resolve CC to absolute path and validate
CC_PATH := $(shell which $(CC) 2>/dev/null)
ifeq ($(CC_PATH),)
    $(error Compiler '$(CC)' not found. Please install the toolchain or check your PATH.)
endif
override CC := $(CC_PATH)

# Compiler type detection (Emscripten, Clang, GCC)
CC_VERSION := $(shell $(CC) --version 2>/dev/null)
ifneq ($(findstring emcc,$(CC_VERSION)),)
    CC_IS_EMCC := 1
else ifneq ($(findstring clang,$(CC_VERSION)),)
    CC_IS_CLANG := 1
else ifneq ($(findstring Free Software Foundation,$(CC_VERSION)),)
    CC_IS_GCC := 1
endif

# Validate compiler type
ifeq ($(CC_IS_CLANG)$(CC_IS_GCC)$(CC_IS_EMCC),)
    $(warning Unsupported C compiler: $(CC))
endif

# C++ compiler setup
ifndef CXX
    ifeq ($(CC_IS_CLANG),1)
        override CXX := $(subst clang,clang++,$(CC))
    else ifeq ($(CC_IS_EMCC),1)
        override CXX := em++
    else
        CXX := $(CROSS_COMPILE)g++
    endif
endif

# Target toolchain configuration
# These tools are used for building the target binaries (potentially cross-compiled)

ifndef CPP
CPP := $(CC) -E
endif

# Emscripten uses its own toolchain (emar, emranlib, etc.)
# For other compilers, use CROSS_COMPILE prefix
ifeq ($(CC_IS_EMCC),1)
    # Emscripten toolchain
    # Use $(origin) to detect if variables come from Make's built-in defaults
    ifeq ($(origin AR),default)
        AR := emar
    endif
    # RANLIB has no built-in default in Make, check if undefined or default
    ifeq ($(filter-out undefined default,$(origin RANLIB)),)
        RANLIB := emranlib
    endif
    ifndef STRIP
        STRIP := emstrip
    endif
    # Emscripten doesn't use traditional ld/objcopy
    LD := emcc
    OBJCOPY := true
else
    # Traditional GCC/Clang toolchain
    # Use $(origin) to detect if variables come from Make's built-in defaults
    ifeq ($(origin LD),default)
        LD := $(CROSS_COMPILE)ld
    endif
    ifeq ($(origin AR),default)
        AR := $(CROSS_COMPILE)ar
    endif
    # RANLIB may have origin 'default' or 'undefined' depending on Make version
    ifeq ($(filter-out undefined default,$(origin RANLIB)),)
        RANLIB := $(CROSS_COMPILE)ranlib
    endif
    ifndef STRIP
        STRIP := $(CROSS_COMPILE)strip -sx
    endif
    ifndef OBJCOPY
        OBJCOPY := $(CROSS_COMPILE)objcopy
    endif
endif

# Host toolchain configuration
# These tools are used for building host utilities (always native compilation)
# Useful when cross-compiling: target tools build for embedded, host tools for development machine

ifndef HOSTCC
HOSTCC := $(HOST_COMPILE)gcc
endif

ifndef HOSTCXX
HOSTCXX := $(HOST_COMPILE)g++
endif

ifndef HOSTCPP
HOSTCPP := $(HOSTCC) -E
endif

ifndef HOSTLD
HOSTLD := $(HOST_COMPILE)ld
endif

ifndef HOSTAR
HOSTAR := $(HOST_COMPILE)ar
endif

ifndef HOSTRANLIB
HOSTRANLIB := $(HOST_COMPILE)ranlib
endif

ifndef HOSTSTRIP
HOSTSTRIP := $(HOST_COMPILE)strip
endif

ifndef HOSTOBJCOPY
HOSTOBJCOPY := $(HOST_COMPILE)objcopy
endif

# Platform-specific compiler optimizations
# Based on target architecture and compiler capabilities

# macOS-specific linker flags
# Xcode 15+ generates warnings about duplicate library options
# Only apply to native macOS builds (not cross-compilation or Emscripten)
ifeq ($(shell uname -s),Darwin)
    ifeq ($(CROSS_COMPILE)$(CC_IS_EMCC),)
        # Check if ld supports -no_warn_duplicate_libraries
        # Use /dev/null as input to ensure compiler invokes the linker
        LD_SUPPORTS_NO_WARN := $(shell printf '' | $(CC) -Wl,-no_warn_duplicate_libraries -x c - -o /dev/null 2>&1 | grep -q "unknown option" && echo 0 || echo 1)
        ifeq ($(LD_SUPPORTS_NO_WARN),1)
            LDFLAGS += -Wl,-no_warn_duplicate_libraries
        endif
    endif
endif

# Toolchain information display
ifdef CROSS_COMPILE
    $(info Cross-compiling with: $(CROSS_COMPILE))
    $(info Target toolchain: $(CC))
endif

ifeq ($(CC_IS_EMCC),1)
    $(info WebAssembly build with Emscripten)
    $(info Compiler: $(CC))
    $(info Toolchain: emar, emranlib, emstrip)
endif

endif # TOOLCHAIN_MK_INCLUDED
