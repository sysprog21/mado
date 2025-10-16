CC_IS_CLANG :=
CC_IS_GCC :=

# FIXME: Cross-compilation using Clang is not supported.
ifdef CROSS_COMPILE
CC := $(CROSS_COMPILE)gcc
endif

override CC := $(shell which $(CC))
ifndef CC
$(error "Valid C compiler not found.")
endif

ifneq ($(shell $(CC) --version | head -n 1 | grep clang),)
CC_IS_CLANG := 1
else ifneq ($(shell $(CC) --version | grep "Free Software Foundation"),)
CC_IS_GCC := 1
endif

ifeq ("$(CC_IS_CLANG)$(CC_IS_GCC)", "")
$(warning Unsupported C compiler)
endif

ifndef CXX
CXX := $(CROSS_COMPILE)g++
endif
ifeq ("$(CC_IS_CLANG)", "1")
override CXX := $(subst clang,clang++,$(CC))
endif

ifndef CPP
CPP := $(CC) -E
endif

ifndef LD
LD := $(CROSS_COMPILE)ld
endif

ifndef AR
AR := $(CROSS_COMPILE)ar
endif

ifndef RANLIB
RANLIB := $(CROSS_COMPILE)ranlib
endif

ifndef STRIP
STRIP := $(CROSS_COMPILE)strip -sx
endif

ifndef OBJCOPY
OBJCOPY := $(CROSS_COMPILE)objcopy
endif

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
