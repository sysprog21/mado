# Overview
# --------
#
# This documentation provides an overview of a set of helper rules designed for
# GNU Make to facilitate the basic management of source and target files. These
# helper rules enable the efficient handling of multiple targets, dependencies,
# and the generation of useful files during the build process.
#
# Managing Multiple Targets
# -------------------------
# You can define multiple targets within your Makefile. Simply add the desired
# targets, and files will be compiled with the appropriate flags for each. For
# example, to specify that a target depends on target.so, you can use the
# following:
#
#     ${target}_depends-y = target.so
#
# Handling Dependencies
# ---------------------
# Targets can depend on other targets or subdirectories. This ensures that the
# commands for a target will not be executed until the commands for its
# dependencies have been completed. For instance, if a target relies on files
# in a subdirectory, the build process will ensure that the subdirectory's
# commands are executed first.
#
# Useful Files Generated During the Make Process
# ----------------------------------------------
# Several useful files are generated during the make process for debugging and
# analysis purposes:
#
#     .target/*.d                : Contains dependency information for the file.
#     .target/*.d.cmd            : The command used to generate the .d file.
#     .target/*.o                : The object file for the source file.
#     .target/*.o.cmd            : The command used to create the object file.
#     .target/target[a,so,o]     : The target file.
#     .target/target[a,so,o].cmd : The command used to create the target file.
#     target                     : The final target file.
#
# Available Targets
# -----------------
#     target-[y,n, ]         : a binary target.
#                              target will be created with $(CC) -o
#     target.o-[y,n, ]       : an object target.
#                              target.o will be created with $(LD) -r -o
#     target.a-[y,n, ]       : a static library target.
#                              target.a will be created with $(AR)
#     target.so-[y,n, ]      : a shared library target.
#                              target.so will be created with $(CC) -o -shared
#
#     target.host-[y,n, ]    : a binary target.
#                              target will be created with $(HOSTCC) -o
#     target.o.host-[y,n, ]  : an object target.
#                              target.o will be created with $(HOSTLD) -r -o
#     target.a.host-[y,n, ]  : a static library target.
#                              target.a will be created with $(HOSTAR)
#     target.so.host-[y,n, ] : a shared library target.
#                              target.so will be created with $(HOSTCC) -o -shared
#
#     subdir-[y,n, ]         : subdirectory targets are executed with
#                              $(subdir-y)_makeflags-y $(MAKE) -C $(subdir-y)
#
# Available Target Flags
# ----------------------
#    $(target)_makeflags-[y,n, ]        : makeflags for target  will be passed to make
#                                         command only for corresponding target.
#    $(target)_files-[y,n, ]            : files must match *.[cho] pattern. *.[ch] files
#                                         will be exemined with $(CC) -M command to
#                                         generate dependency files (*.d) files. *.[o]
#                                         files will be used only in linking stage. all
#                                         files generated with make command will be
#                                         removed with $(RM) command.
#    $(target)_cflags-[y,n, ]           : cflags will be added to global $(CFLAGS) for
#                                         corresponding target only.
#    $(target)_cxxflags-[y,n, ]         : cxxflags will be added to global $(CXXFLAGS)
#                                         for corresponding target only.
#    $(target)_${file}_cflags-[y,n, ]   : cflags will be added to global $(CFLAGS) for
#                                         corresponding target file only.
#    $(target)_${file}_cxxflags-[y,n, ] : cxxflags will be added to global $(CXXFLAGS)
#                                         for corresponding target file only.
#    $(target)_includes-[y,n, ]         : a '-I' will be added to all words in includes
#                                         flag, and will be used only for corresponding
#                                         target.
#    $(target)_libraries-[y,n, ]        : a '-L' will be added to all words in libraries
#                                         flag, and will be used only for corresponding
#                                         target.
#    $(target)_ldflags-[y,n, ]          : ldflags will be added to gloabal $(LDFLAGS) for
#                                         corresponding target only.
#    $(target)_depends-[y,n, ]          : all words in depends flag will be added to
#                                         prerequisite's list.

TOPDIR ?= $(CURDIR)

CC         := $(CROSS_COMPILE)gcc $(SYSROOT)
CXX        := $(CROSS_COMPILE)g++ $(SYSROOT)
CPP        := $(CROSS_COMPILE)cpp $(SYSROOT)
LD         := $(CROSS_COMPILE)ld
AR         := $(CROSS_COMPILE)ar
RANLIB     := $(CROSS_COMPILE)ranlib
STRIP      := $(CROSS_COMPILE)strip -sx
OBJCOPY    := $(CROSS_COMPILE)objcopy

HOSTCC     := $(HOST_COMPILE)gcc
HOSTCXX    := $(HOST_COMPILE)g++
HOSTCPP    := $(HOST_COMPILE)cpp
HOSTLD     := $(HOST_COMPILE)ld
HOSTAR     := $(HOST_COMPILE)ar
HOSTRANLIB := $(HOST_COMPILE)ranlib
HOSTSTRIP  := $(HOST_COMPILE)strip
HOSTOBJCOPY:= $(HOST_COMPILE)objcpy

SED        := sed

CD         := cd
CP         := cp -rf
MV         := mv
RM         := rm -rf
MKDIR      := mkdir -p

__CFLAGS := -Wall -Wextra -pipe -g -O2
__CFLAGS += -O2 -g -pipe
__CFLAGS += $(CFLAGS)

uname_S := $(shell sh -c 'uname -s 2>/dev/null || echo not')
TARGET_MACHINE := $(subst -, ,$(shell $(CC) -dumpmachine 2>/dev/null))

ifneq ($(findstring mingw, $(TARGET_MACHINE)),)
    TARGET_OS := WINDOWS
else ifneq ($(findstring apple, $(TARGET_MACHINE)),)
    TARGET_OS := MACOS
else ifneq ($(findstring linux, $(TARGET_MACHINE)),)
    TARGET_OS := LINUX
else
    TARGET_OS := UNKNOWN
endif

ifeq      ($(TARGET_OS),WINDOWS)
    override __WINDOWS__  = y
    override MFLAGS     += __WINDOWS__=y
    __CFLAGS      += -D__WINDOWS__=1
    __CFLAGS      += -D_POSIX=1
else ifeq ($(TARGET_OS),MACOS)
    override __DARWIN__  = y
    override MFLAGS     += __DARWIN__=y
    __CFLAGS      += -D__DARWIN__=1
    __CFLAGS      += -I/opt/local/include
    #__CFLAGS     += -fassociative-math -fno-signed-zeros
    override LDOPTS     += -keep_private_externs
else ifeq ($(TARGET_OS),LINUX)
    override __LINUX__  = y
    override MFLAGS     += __LINUX__=y
    __CFLAGS      += -D__LINUX__=1
    #__CFLAGS     += -fassociative-math -fno-signed-zeros
endif

__CXXFLAGS := $(CXXFLAGS) $(EXTRA_CXXFLAGS) $(__CFLAGS)
__LDFLAGS  := $(LDFLAGS) $(EXTRA_LDFLAGS)

__ARCHFLAGS := $(ARCHFLAGS)
__HOSTARCHFLAGS := $(HOSTARCHFLAGS)

MAKE := CFLAGS="$(CFLAGS)" EXTRA_CFLAGS="$(EXTRA_CFLAGS)" CXXFLAGS="$(CXXFLAGS)" EXTRA_CXXFLAGS="$(EXTRA_CXXFLAGS)" \
        ARCHFLAGS="$(ARCHFLAGS)" HOSTARCHFLAGS="$(HOSTARCHFLAGS)" \
        LDFLAGS="$(LDFLAGS)" EXTRA_LDFLAGS="$(EXTRA_LDFLAGS)" TOPDIR='$(TOPDIR)' uname_S=$(uname_S) TARGET_OS=$(TARGET_OS) \
        $(MAKE) $(MFLAGS)

all:

ifneq ($(V)$(VERBOSE),)
    verbose := ver
    Q =
else
    verbose := quiet
    Q = @
    MAKE += --no-print-directory
endif

quiet_objects = $(CURDIR)/$$@
quiet_objects = $$(subst __UPDIR__/,../,$(CURDIR)/$$@)

# QUIET

quiet_disp_depend.c         = echo "  DEP        $(quiet_objects)"
quiet_disp_compile.c        = echo "  CC         $(quiet_objects)"
quiet_disp_link.c           = echo "  LINK       $(quiet_objects)"
quiet_disp_link_so.c        = echo "  LINKSO     $(quiet_objects)"

quiet_disp_depend.cxx       = echo "  DEP        $(quiet_objects)"
quiet_disp_compile.cxx      = echo "  CXX        $(quiet_objects)"
quiet_disp_link.cxx         = echo "  LINK       $(quiet_objects)"
quiet_disp_link_so.cxx      = echo "  LINKSO     $(quiet_objects)"

quiet_disp_depend.c.host    = echo "  HOSTDEP    $(quiet_objects)"
quiet_disp_compile.c.host   = echo "  HOSTCC     $(quiet_objects)"
quiet_disp_link.c.host      = echo "  HOSTLINK   $(quiet_objects)"
quiet_disp_link_so.c.host   = echo "  HOSTLINKSO $(quiet_objects)"

quiet_disp_depend.cxx.host  = echo "  HOSTDEP    $(quiet_objects)"
quiet_disp_compile.cxx.host = echo "  HOSTCXX    $(quiet_objects)"
quiet_disp_link.cxx.host    = echo "  HOSTLINK   $(quiet_objects)"
quiet_disp_link_so.cxx.host = echo "  HOSTLINKSO $(quiet_objects)"

quiet_disp_ar               = echo "  AR         $(quiet_objects)"
quiet_disp_ranlib           = echo "  RANLIB     $(quiet_objects)"
quiet_disp_ld               = echo "  LD         $(quiet_objects)"

quiet_disp_ld.host          = echo "  HOSTLD     $(quiet_objects)"

quiet_disp_cp               = echo "  CP         $(quiet_objects)"
quiet_disp_mkdir            = echo "  MKDIR      $(CURDIR)/$@"
quiet_disp_clean            = echo "  CLEAN      $$(subst _clean,,$(quiet_objects))"

quiet_disp_print_deps       =

# verbose

ver_disp_depend.c         = echo "$(subst ",\",$(_cmd_depend.c))"
ver_disp_compile.c        = echo "$(subst ",\",$(_cmd_compile.c))"
ver_disp_link.c           = echo "$(subst ",\",$(_cmd_link.c))"
ver_disp_link_so.c        = echo "$(subst ",\",$(_cmd_link_so.c))"

ver_disp_depend.cxx       = echo "$(subst ",\",$(_cmd_depend.cxx))"
ver_disp_compile.cxx      = echo "$(subst ",\",$(_cmd_compile.cxx))"
ver_disp_link.cxx         = echo "$(subst ",\",$(_cmd_link.cxx))"
ver_disp_link_so.cxx      = echo "$(subst ",\",$(_cmd_link_so.cxx))"

ver_disp_depend.c.host    = echo "$(subst ",\",$(_cmd_depend.c.host))"
ver_disp_compile.c.host   = echo "$(subst ",\",$(_cmd_compile.c.host))"
ver_disp_link.c.host      = echo "$(subst ",\",$(_cmd_link.c.host))"
ver_disp_link_so.c.host   = echo "$(subst ",\",$(_cmd_link_so.c.host))"

ver_disp_depend.cxx.host  = echo "$(subst ",\",$(_cmd_depend.cxx.host))"
ver_disp_compile.cxx.host = echo "$(subst ",\",$(_cmd_compile.cxx.host))"
ver_disp_link.cxx.host    = echo "$(subst ",\",$(_cmd_link.cxx.host))"
ver_disp_link_so.cxx.host = echo "$(subst ",\",$(_cmd_link_so.cxx.host))"

ver_disp_ar               = echo "$(subst ",\",$(_cmd_ar))"
ver_disp_ranlib           = echo "$(subst ",\",$(_cmd_ranlib))"
ver_disp_ld               = echo "$(subst ",\",$(_cmd_ld))"

ver_disp_ld.host          = echo "$(subst ",\",$(_cmd_ld.host))"

ver_disp_cp               = echo "$(subst ",\",$(_cmd_cp))"
ver_disp_mkdir            = echo "$(subst ",\",$(_cmd_mkdir))"
ver_disp_clean            = echo "$(subst ",\",$(_cmd_clean) $1 .$1 $$@.cmd)"

ver_disp_print_deps       = echo "Target $$@ depends on prerequisites '$$^'"

# DISP

disp_depend.c             = $($(verbose)_disp_depend.c)
disp_compile.c            = $($(verbose)_disp_compile.c)
disp_link.c               = $($(verbose)_disp_link.c)
disp_link_so.c            = $($(verbose)_disp_link_so.c)

disp_depend.cxx           = $($(verbose)_disp_depend.cxx)
disp_compile.cxx          = $($(verbose)_disp_compile.cxx)
disp_link.cxx             = $($(verbose)_disp_link.cxx)
disp_link_so.cxx          = $($(verbose)_disp_link_so.cxx)

disp_depend.c.host        = $($(verbose)_disp_depend.c.host)
disp_compile.c.host       = $($(verbose)_disp_compile.c.host)
disp_link.c.host          = $($(verbose)_disp_link.c.host)
disp_link_so.c.host       = $($(verbose)_disp_link_so.c.host)

disp_depend.cxx.host      = $($(verbose)_disp_depend.cxx.host)
disp_compile.cxx.host     = $($(verbose)_disp_compile.cxx.host)
disp_link.cxx.host        = $($(verbose)_disp_link.cxx.host)
disp_link_so.cxx.host     = $($(verbose)_disp_link_so.cxx.host)

disp_ar                   = $($(verbose)_disp_ar)
disp_ranlib               = $($(verbose)_disp_ranlib)
disp_ld                   = $($(verbose)_disp_ld)

disp_ld.host              = $($(verbose)_disp_ld.host)

disp_cp                   = $($(verbose)_disp_cp)
disp_mkdir                = $($(verbose)_disp_mkdir)
disp_clean                = $($(verbose)_disp_clean)

disp_print_deps           = $($(verbose)_disp_print_deps)

# _CMD

_cmd_depend.c             = $(CC) $(__CFLAGS) $($1_cflags) \
                            $$($1_$$<_cflags-y) $($1_includes) -M $$< | $(SED) 's,\($$(basename $$(basename $$(notdir $$@)))\.o\) *:,$$(dir $$@)\1 $$@: ,' > $$@; \
                            $(CP) $$@ $$@.p; \
                            $(SED) -e 's/\#.*//' -e 's/^[^:]*: *//' -e 's/ *\\$$$$//' -e '/^$$$$/ d' -e 's/$$$$/ :/' < $$@ >> $$@.p; \
                            $(MV) $$@.p $$@
_cmd_compile.c            = $(CC) $(__ARCHFLAGS) $(__CFLAGS) $($1_archflags) $($1_cflags) \
                            $$($1_$$<_cflags-y) $($1_includes) -c -o $$@ $$<
_cmd_link.c               = $(CC) $(__ARCHFLAGS) $(__CFLAGS) $($1_libraries) -o $$@ $$(filter-out __FORCE $($1_depends_y),$$^) \
                            $($1_archflags) $($1_ldflags) $(__LDFLAGS)
_cmd_link_so.c            = $(CC) $(__ARCHFLAGS) $(__CFLAGS) $($1_libraries) -o $$@ $$(filter-out __FORCE $($1_depends_y),$$^) \
                            $($1_archflags) $($1_ldflags) $$(filter-out -static,$(__LDFLAGS)) -shared $$(if $($1_soname-y),-Wl$$(comma)-soname$$(comma)$($1_soname-y),)

_cmd_depend.c.host        = $(HOSTCC) $(__CFLAGS) $($1_cflags) \
                            $$($1_$$<_cflags-y) $($1_includes) -M $$< | $(SED) 's,\($$(basename $$(basename $$(notdir $$@)))\.o\) *:,$$(dir $$@)\1 $$@: ,' > $$@; \
                            $(CP) $$@ $$@.p; \
                            $(SED) -e 's/\#.*//' -e 's/^[^:]*: *//' -e 's/ *\\$$$$//' -e '/^$$$$/ d' -e 's/$$$$/ :/' < $$@ >> $$@.p; \
                            $(MV) $$@.p $$@
_cmd_compile.c.host       = $(HOSTCC) $(__HOSTARCHFLAGS) $(__CFLAGS) $($1_archflags) $($1_cflags) \
                            $$($1_$$<_cflags-y) $($1_includes) -c -o $$@ $$<
_cmd_link.c.host          = $(HOSTCC) $(__HOSTARCHFLAGS) $(__CFLAGS) $($1_libraries) -o $$@ $$(filter-out __FORCE $($1_depends_y),$$^) \
                            $($1_archflags) $($1_ldflags) $(__LDFLAGS)
_cmd_link_so.c.host       = $(HOSTCC) $(__HOSTARCHFLAGS) $(__CFLAGS) $($1_libraries) -o $$@ $$(filter-out __FORCE $($1_depends_y),$$^) \
                            $($1_archflags) $($1_ldflags) $$(filter-out -static,$(__LDFLAGS)) -shared $$(if $($1_soname-y),-Wl$$(comma)-soname$$(comma)$($1_soname-y),)

_cmd_depend.cxx           = $(CXX) $(__CXXFLAGS) $($1_cxxflags) \
                            $$($1_$$<_cxxflags-y) $($1_includes) -M $$< | $(SED) 's,\($$(basename $$(basename $$(notdir $$@)))\.o\) *:,$$(dir $$@)\1 $$@: ,' > $$@; \
                            $(CP) $$@ $$@.p; \
                            $(SED) -e 's/\#.*//' -e 's/^[^:]*: *//' -e 's/ *\\$$$$//' -e '/^$$$$/ d' -e 's/$$$$/ :/' < $$@ >> $$@.p; \
                            $(MV) $$@.p $$@
_cmd_compile.cxx          = $(CXX) $(__ARCHFLAGS) $(__CXXFLAGS) $($1_archflags) $($1_cxxflags) \
                            $$($1_$$<_cxxflags-y) $($1_includes) -c -o $$@ $$<
_cmd_link.cxx             = $(CXX) $(__ARCHFLAGS) $(__CXXFLAGS) $($1_libraries) -o $$@ $$(filter-out __FORCE $($1_depends_y),$$^) \
                            $($1_archflags) $($1_ldflags) $(__LDFLAGS)
_cmd_link_so.cxx          = $(CXX) $(__ARCHFLAGS) $(__CXXFLAGS) $($1_libraries) -o $$@ $$(filter-out __FORCE $($1_depends_y),$$^) \
                            $($1_archflags) $($1_ldflags) $$(filter-out -static,$(__LDFLAGS)) -shared $$(if $($1_soname-y),-Wl$$(comma)-soname$$(comma)$($1_soname-y),)

_cmd_depend.cxx.host      = $(HOSTCXX) $(__CXXFLAGS) $($1_cxxflags) \
                            $$($1_$$<_cxxflags-y) $($1_includes) -M $$< | $(SED) 's,\($$(basename $$(basename $$(notdir $$@)))\.o\) *:,$$(dir $$@)\1 $$@: ,' > $$@; \
                            $(CP) $$@ $$@.p; \
                            $(SED) -e 's/\#.*//' -e 's/^[^:]*: *//' -e 's/ *\\$$$$//' -e '/^$$$$/ d' -e 's/$$$$/ :/' < $$@ >> $$@.p; \
                            $(MV) $$@.p $$@
_cmd_compile.cxx.host     = $(HOSTCXX) $(__HOSTARCHFLAGS) $(__CXXFLAGS) $($1_archflags) $($1_cxxflags) \
                            $$($1_$$<_cxxflags-y) $($1_includes) -c -o $$@ $$<
_cmd_link.cxx.host        = $(HOSTCXX) $(__HOSTARCHFLAGS) $(__CXXFLAGS) $($1_libraries) -o $$@ $$(filter-out __FORCE $($1_depends_y),$$^) \
                            $($1_archflags) $($1_ldflags) $(__LDFLAGS)
_cmd_link_so.cxx.host     = $(HOSTCXX) $(__HOSTARCHFLAGS) $(__CXXFLAGS)$($1_libraries) -o $$@ $$(filter-out __FORCE $($1_depends_y),$$^) \
                            $($1_archflags) $($1_ldflags) $$(filter-out -static,$(__LDFLAGS)) -shared $$(if $($1_soname-y),-Wl$$(comma)-soname$$(comma)$($1_soname-y),)

_cmd_ar                   = $(AR) rcs $$@ $$(filter-out __FORCE $($1_depends_y),$$^)
_cmd_ranlib               = $(RANLIB) $$@
_cmd_ld                   = $(LD) $(LDOPTS) -r -o $$@ $$(filter-out __FORCE $($1_depends_y),$$^)

_cmd_ld.host              = $(HOSTLD) $(LDOPTS) -r -o $$@ $$(filter-out __FORCE $($1_depends_y),$$^)

_cmd_cp                   = $(CP) $$< $$@
_cmd_mkdir                = $(MKDIR) $@
_cmd_clean                = $(RM)

# CMD

cmd_depend.c              = echo "$(subst ",\",$(_cmd_depend.c))" > $$@.cmd        ; $(_cmd_depend.c)
cmd_compile.c             = echo "$(subst ",\",$(_cmd_compile.c))" > $$@.cmd       ; $(_cmd_compile.c)
cmd_link.c                = echo "$(subst ",\",$(_cmd_link.c))" > $$@.cmd          ; $(_cmd_link.c)
cmd_link_so.c             = echo "$(subst ",\",$(_cmd_link_so.c))" > $$@.cmd       ; $(_cmd_link_so.c)

cmd_depend.c.host         = echo "$(subst ",\",$(_cmd_depend.c.host))" > $$@.cmd   ; $(_cmd_depend.c.host)
cmd_compile.c.host        = echo "$(subst ",\",$(_cmd_compile.c.host))" > $$@.cmd  ; $(_cmd_compile.c.host)
cmd_link.c.host           = echo "$(subst ",\",$(_cmd_link.c.host))" > $$@.cmd     ; $(_cmd_link.c.host)
cmd_link_so.c.host        = echo "$(subst ",\",$(_cmd_link_so.c.host))" > $$@.cmd  ; $(_cmd_link_so.c.host)

cmd_depend.cxx            = echo "$(subst ",\",$(_cmd_depend.cxx))" > $$@.cmd      ; $(_cmd_depend.cxx)
cmd_compile.cxx           = echo "$(subst ",\",$(_cmd_compile.cxx))" > $$@.cmd     ; $(_cmd_compile.cxx)
cmd_link.cxx              = echo "$(subst ",\",$(_cmd_link.cxx))" > $$@.cmd        ; $(_cmd_link.cxx)
cmd_link_so.cxx           = echo "$(subst ",\",$(_cmd_link_so.cxx))" > $$@.cmd     ; $(_cmd_link_so.cxx)

cmd_depend.cxx.host       = echo "$(subst ",\",$(_cmd_depend.cxx.host))" > $$@.cmd ; $(_cmd_depend.cxx.host)
cmd_compile.cxx.host      = echo "$(subst ",\",$(_cmd_compile.cxx.host))" > $$@.cmd; $(_cmd_compile.cxx.host)
cmd_link.cxx.host         = echo "$(subst ",\",$(_cmd_link.cxx.host))" > $$@.cmd   ; $(_cmd_link.cxx.host)
cmd_link_so.cxx.host      = echo "$(subst ",\",$(_cmd_link_so.cxx.host))" > $$@.cmd; $(_cmd_link_so.cxx.host)

cmd_ar                    = echo "$(subst ",\",$(_cmd_ar))" > $$@.cmd              ; $(_cmd_ar)
cmd_ranlib                = echo "$(subst ",\",$(_cmd_ranlib))" >> $$@.cmd         ; $(_cmd_ranlib)
cmd_ld                    = echo "$(subst ",\",$(_cmd_ld))" > $$@.cmd              ; $(_cmd_ld)

cmd_ld.host               = echo "$(subst ",\",$(_cmd_ld.host))" > $$@.cmd         ; $(_cmd_ld.host)

cmd_cp                    = $(_cmd_cp)
cmd_mkdir                 = $(_cmd_mkdir)
cmd_clean                 = $(_cmd_clean)

# DO

do_depend.c               = @$(disp_depend.c)         ; $(MKDIR) $$(dir $$@); $(cmd_depend.c)
do_compile.c              = @$(disp_compile.c)        ; $(MKDIR) $$(dir $$@); $(cmd_compile.c)
do_link.c                 = @$(disp_link.c)           ; $(MKDIR) $$(dir $$@); $(cmd_link.c)
do_link_so.c              = @$(disp_link_so.c)        ; $(MKDIR) $$(dir $$@); $(cmd_link_so.c)

do_depend.c.host          = @$(disp_depend.c.host)    ; $(MKDIR) $$(dir $$@); $(cmd_depend.c.host)
do_compile.c.host         = @$(disp_compile.c.host)   ; $(MKDIR) $$(dir $$@); $(cmd_compile.c.host)
do_link.c.host            = @$(disp_link.c.host)      ; $(MKDIR) $$(dir $$@); $(cmd_link.c.host)
do_link_so.c.host         = @$(disp_link_so.c.host)   ; $(MKDIR) $$(dir $$@); $(cmd_link_so.c.host)

do_depend.cxx             = @$(disp_depend.cxx)       ; $(MKDIR) $$(dir $$@); $(cmd_depend.cxx)
do_compile.cxx            = @$(disp_compile.cxx)      ; $(MKDIR) $$(dir $$@); $(cmd_compile.cxx)
do_link.cxx               = @$(disp_link.cxx)         ; $(MKDIR) $$(dir $$@); $(cmd_link.cxx)
do_link_so.cxx            = @$(disp_link_so.cxx)      ; $(MKDIR) $$(dir $$@); $(cmd_link_so.cxx)

do_depend.cxx.host        = @$(disp_depend.cxx.host)  ; $(MKDIR) $$(dir $$@); $(cmd_depend.cxx.host)
do_compile.cxx.host       = @$(disp_compile.cxx.host) ; $(MKDIR) $$(dir $$@); $(cmd_compile.cxx.host)
do_link.cxx.host          = @$(disp_link.cxx.host)    ; $(MKDIR) $$(dir $$@); $(cmd_link.cxx.host)
do_link_so.cxx.host       = @$(disp_link_so.cxx.host) ; $(MKDIR) $$(dir $$@); $(cmd_link_so.cxx.host)

do_ar                     = @$(disp_ar)               ; $(MKDIR) $$(dir $$@); $(cmd_ar)
do_ranlib                 = @$(disp_ranlib)           ; $(MKDIR) $$(dir $$@); $(cmd_ranlib)
do_ld                     = @$(disp_ld)               ; $(MKDIR) $$(dir $$@); $(cmd_ld)

do_ld.host                = @$(disp_ld.host)          ; $(MKDIR) $$(dir $$@); $(cmd_ld.host)

do_cp                     = @$(disp_cp)               ; $(MKDIR) $$(dir $$@); $(cmd_cp)
do_mkdir                  = @$(disp_mkdir)            ; $(cmd_mkdir)
do_clean                  = @$(disp_clean)            ; $(cmd_clean)

do_print_deps             = @$(disp_print_deps)

# Functions

define target-defaults_base
    $(eval comma           = ,)
    $(eval $1_sources_c    = $(filter %.c,$2))
    $(eval $1_headers_c    = $(filter %.h,$2))
    $(eval $1_sources_cc   = $(filter %.cc,$2))
    $(eval $1_sources_cpp  = $(filter %.cpp,$2))
    $(eval $1_headers_cxx  = $(filter %.h,$2))
    $(eval $1_headers_cxx += $(filter %.hh,$2))
    $(eval $1_headers_cxx += $(filter %.hpp,$2))
    $(eval $1_objects_c    = $(addprefix .$1/, $(subst .c,.c.o,$($1_sources_c))))
    $(eval $1_objects_cc   = $(addprefix .$1/, $(subst .cc,.cc.o,$($1_sources_cc))))
    $(eval $1_objects_cpp  = $(addprefix .$1/, $(subst .cpp,.cpp.o,$($1_sources_cpp))))
    $(eval $1_objects_e    = $(subst ",,$(filter %.o,$2)))
    $(eval $1_objects_e   += $(subst ",,$(filter %.a,$2)))
    $(eval $1_objects_e   += $(subst ",,$(filter %.lib,$2)))
    $(eval $1_depends_c    = $(addprefix .$1/, $(subst .c,.c.d,$($1_sources_c))))
    $(eval $1_depends_cc   = $(addprefix .$1/, $(subst .cc,.cc.d,$($1_sources_cc))))
    $(eval $1_depends_cpp  = $(addprefix .$1/, $(subst .cpp,.cpp.d,$($1_sources_cpp))))
    $(eval $1_archflags    = $($1_archflags-y))
    $(eval $1_cflags       = $($1_cflags-y))
    $(eval $1_cxxflags     = $($1_cxxflags-y))
    $(eval $1_includes     = $(addprefix -I, $($1_includes-y)))
    $(eval $1_libraries    = $(addprefix -L, ./ $($1_libraries-y)))
    $(eval $1_ldflags      = $($1_ldflags-y))

    $(eval $1_depends_c    = $(subst ../,__UPDIR__/,$($1_depends_c)))
    $(eval $1_objects_c    = $(subst ../,__UPDIR__/,$($1_objects_c)))

    $(eval $1_depends_cc   = $(subst ../,__UPDIR__/,$($1_depends_cc)))
    $(eval $1_objects_cc   = $(subst ../,__UPDIR__/,$($1_objects_cc)))

    $(eval $1_depends_cpp  = $(subst ../,__UPDIR__/,$($1_depends_cpp)))
    $(eval $1_objects_cpp  = $(subst ../,__UPDIR__/,$($1_objects_cpp)))

    $(eval $1_depends_m    = $(subst ../,__UPDIR__/,$($1_depends_m)))
    $(eval $1_objects_m    = $(subst ../,__UPDIR__/,$($1_objects_m)))

    $(eval $1_objects      = $($1_objects_c))
    $(eval $1_objects     += $($1_objects_cc))
    $(eval $1_objects     += $($1_objects_cpp))
    $(eval $1_objects     += $($1_objects_m))

    $(eval $1_directories  = $(sort $(dir .$1/ $($1_objects_c) $($1_objects_cpp) $($1_objects_m))))

    $(eval $1_depends      = $(MAKEFILE_LIST) $($1_depends_c) $($1_depends_cc) $($1_depends_cpp) $($1_depends_m))
    $(eval $1_depends_y    = $($1_depends))
    $(eval $1_depends_y   += $($1_depends-y))
    $(eval $1_depends-n   += $($1_headers_c))
    $(eval $1_depends-n   += $($1_headers_m))
    $(eval $1_depends-n   += $($1_headers_cxx))

    $(eval target-builds  += $1)
    $(eval target-objects += $($1_objects_c))
    $(eval target-objects += $($1_objects_cc))
    $(eval target-objects += $($1_objects_cpp))
    $(eval target-objects += $($1_objects_m))
    $(eval target-depends += $($1_depends_c))
    $(eval target-depends += $($1_depends_cc))
    $(eval target-depends += $($1_depends_cpp))
    $(eval target-depends += $($1_depends_m))
    $(eval target-cleans  += $1_clean)

    $($1_directories):
	$(do_mkdir) $($1_directories)

    $($1_depends_c):   $($1_headers_c)
    $($1_depends_cpp): $($1_headers_cxx)
    $($1_depends_m):   $($1_headers_m)

    $($1_objects_c):   .$1/%.o: .$1/%.d $(MAKEFILE_LIST)
    $($1_objects_cc):  .$1/%.o: .$1/%.d $(MAKEFILE_LIST)
    $($1_objects_cpp): .$1/%.o: .$1/%.d $(MAKEFILE_LIST)
    $($1_objects_m):   .$1/%.o: .$1/%.d $(MAKEFILE_LIST)
    $($1_objects_e):   $(MAKEFILE_LIST)

    $(target-depends): $(filter-out $(target-builds) $($1_depends), $($1_depends_y))

    $1: .$1/$1 $(MAKEFILE_LIST)
	$(do_cp)
	$(do_print_deps)

    .NOTPARALLEL: $($1_depends_y) $($1_objects_e) $($1_objects)

    .$1/$1: $($1_depends_y) $($1_objects_e) $($1_objects)

    $1_clean: __FORCE
	$(do_clean) $1 .$1 $$@.cmd $1.dSYM
endef

define target-defaults-rule
    $(eval source = $(patsubst $3,$4,$(subst __UPDIR__/,../,$2)))
    $2: ${source} $(MAKEFILE_LIST)
	$5
endef

define target-defaults
    $(eval $(call target-defaults_base,$1,$2))

    $(eval $(foreach F,$($1_depends_c), $(eval $(call target-defaults-rule,$1,$F,.$1/%.c.d,%.c,$(do_depend.c)))))
    $(eval $(foreach F,$($1_objects_c), $(eval $(call target-defaults-rule,$1,$F,.$1/%.c.o,%.c,$(do_compile.c)))))

    $(eval $(foreach F,$($1_depends_cc), $(eval $(call target-defaults-rule,$1,$F,.$1/%.cc.d,%.cc,$(do_depend.cxx)))))
    $(eval $(foreach F,$($1_objects_cc), $(eval $(call target-defaults-rule,$1,$F,.$1/%.cc.o,%.cc,$(do_compile.cxx)))))

    $(eval $(foreach F,$($1_depends_cpp), $(eval $(call target-defaults-rule,$1,$F,.$1/%.cpp.d,%.cpp,$(do_depend.cxx)))))
    $(eval $(foreach F,$($1_objects_cpp), $(eval $(call target-defaults-rule,$1,$F,.$1/%.cpp.o,%.cpp,$(do_compile.cxx)))))
endef

define target.host-defaults
    $(eval $(call target-defaults_base,$1,$2))

    $(eval $(foreach F,$($1_depends_c), $(eval $(call target-defaults-rule,$1,$F,.$1/%.c.d,%.c,$(do_depend.c.host)))))
    $(eval $(foreach F,$($1_objects_c), $(eval $(call target-defaults-rule,$1,$F,.$1/%.c.o,%.c,$(do_compile.c.host)))))

    $(eval $(foreach F,$($1_depends_cc), $(eval $(call target-defaults-rule,$1,$F,.$1/%.cc.d,%.cc,$(do_depend.cxx.host)))))
    $(eval $(foreach F,$($1_objects_cc), $(eval $(call target-defaults-rule,$1,$F,.$1/%.cc.o,%.cc,$(do_compile.cxx.host)))))

    $(eval $(foreach F,$($1_depends_cpp), $(eval $(call target-defaults-rule,$1,$F,.$1/%.cpp.d,%.cpp,$(do_depend.cxx.host)))))
    $(eval $(foreach F,$($1_objects_cpp), $(eval $(call target-defaults-rule,$1,$F,.$1/%.cpp.o,%.cpp,$(do_compile.cxx.host)))))
endef

define target-variables
    $(eval $(call target-defaults,$1,$2))

    .$1/$1:
ifeq ($($1_sources_cpp),)
	$(do_link.c)
else
	$(do_link.cxx)
endif
ifeq ($(__WINDOWS__), y)
	${Q}${CP} $$@.exe $$@
endif
	$(do_print_deps)
endef

define target.host-variables
    $(eval $(call target.host-defaults,$1,$2))

    .$1/$1:
	$(do_link.c.host)
	$(do_print_deps)
endef

define target.so-variables
    $(eval $(call target-defaults,$1,$2))

    .$1/$1:
ifeq ($($1_sources_cpp),)
	$(do_link_so.c)
else
	$(do_link_so.cxx)
endif
	$(do_print_deps)
endef

define target.so.host-variables
    $(eval $(call target.host-defaults,$1,$2))

    .$1/$1:
	$(do_link_so.c.host)
	$(do_print_deps)
endef

define target.a-variables
    $(eval $(call target-defaults,$1,$2))

    .$1/$1:
	$(do_ar)
	$(do_ranlib)
	$(do_print_deps)
endef

define target.a.host-variables
    $(eval $(call target.host-defaults,$1,$2))

    .$1/$1:
	$(do_ar.host)
	$(do_ranlib.host)
	$(do_print_deps)
endef

define target.o-variables
    $(eval $(call target-defaults,$1,$2))

    .$1/$1:
	$(do_ld)
	$(do_print_deps)
endef

define target.o.host-variables
    $(eval $(call target.host-defaults,$1,$2))

    .$1/$1:
	$(do_ld.host)
	$(do_print_deps)
endef

define target_empty-defaults
    targets-empty += $1

    $(addsuffix _clean, $1): __FORCE
	$(do_clean) $1 .$1 $$@.cmd
endef

define subdir_empty-defaults
    $1: $1_all
	$(do_print_deps)

    $(addsuffix _all, $1): $($1_depends-y) __FORCE
	${Q}+ $(MAKE) $($1_makeflags-y) -C '$$(subst _all,,$$@)' all
	$(do_print_deps)

    $(addsuffix _clean, $1): __FORCE
	${Q}+ $(MAKE) $($1_makeflags-y) -C '$$(subst _clean,,$$@)' clean
endef

define subdir-defaults
    subdirs += $1

    $1: $1_all
	$(do_print_deps)

    $(addsuffix _all, $1): $($1_depends-y) __FORCE
	${Q}+ $(MAKE) $($1_makeflags-y) -C '$$(subst _all,,$$@)' all
	$(do_print_deps)

    $(addsuffix _clean, $1): __FORCE
	${Q}+ $(MAKE) $($1_makeflags-y) -C '$$(subst _clean,,$$@)' clean
endef

# Definitions

# generate target variables

$(eval $(foreach T,$(target-y), $(eval $(call target-variables,$T,$(sort $($T_files-y))))))
$(eval $(foreach T,$(target-n), $(eval $(call target_empty-defaults,$T))))
$(eval $(foreach T,$(target-),  $(eval $(call target_empty-defaults,$T))))

$(eval $(foreach T,$(target.o-y), $(eval $(call target.o-variables,$T,$(sort $($T_files-y))))))
$(eval $(foreach T,$(target.o-n), $(eval $(call target_empty-defaults,$T))))
$(eval $(foreach T,$(target.o-),  $(eval $(call target_empty-defaults,$T))))

$(eval $(foreach T,$(target.a-y), $(eval $(call target.a-variables,$T,$(sort $($T_files-y))))))
$(eval $(foreach T,$(target.a-n), $(eval $(call target_empty-defaults,$T))))
$(eval $(foreach T,$(target.a-),  $(eval $(call target_empty-defaults,$T))))

$(eval $(foreach T,$(target.so-y), $(eval $(call target.so-variables,$T,$(sort $($T_files-y))))))
$(eval $(foreach T,$(target.so-n), $(eval $(call target_empty-defaults,$T))))
$(eval $(foreach T,$(target.so-),  $(eval $(call target_empty-defaults,$T))))

$(eval $(foreach T,$(target.host-y), $(eval $(call target.host-variables,$T,$(sort $($T_files-y))))))
$(eval $(foreach T,$(target.host-n), $(eval $(call target_empty-defaults,$T))))
$(eval $(foreach T,$(target.host-),  $(eval $(call target_empty-defaults,$T))))

$(eval $(foreach T,$(target.o.host-y), $(eval $(call target.o.host-variables,$T,$(sort $($T_files-y))))))
$(eval $(foreach T,$(target.o.host-n), $(eval $(call target_empty-defaults,$T))))
$(eval $(foreach T,$(target.o.host-),  $(eval $(call target_empty-defaults,$T))))

$(eval $(foreach T,$(target.so.host-y), $(eval $(call target.so.host-variables,$T,$(sort $($T_files-y))))))
$(eval $(foreach T,$(target.so.host-n), $(eval $(call target_empty-defaults,$T))))
$(eval $(foreach T,$(target.so.host-),  $(eval $(call target_empty-defaults,$T))))

$(eval $(foreach T,$(target.a.host-y), $(eval $(call target.a.host-variables,$T,$(sort $($T_files-y))))))
$(eval $(foreach T,$(target.a.host-n), $(eval $(call target_empty-defaults,$T))))
$(eval $(foreach T,$(target.a.host-),  $(eval $(call target_empty-defaults,$T))))

# generate subdir targets

$(eval $(foreach S,$(subdir-y),$(eval $(call subdir-defaults,$S))))
$(eval $(foreach S,$(subdir-n),$(eval $(call subdir_empty-defaults,$S,clean))))
$(eval $(foreach S,$(subdir-),$(eval $(call subdir_empty-defaults,$S,clean))))

# generic tags

all: $(addsuffix _all, $(subdirs))
all: $(target-builds)
all: $(target.extra-y)
all: __FORCE
	@true

clean: $(addsuffix _clean, $(subdir-y) $(subdir-n) $(subdir-))
clean: $(target-cleans)
clean: $(addsuffix _clean, $(targets-empty))
clean: __FORCE

__FORCE:
	@true

ifneq "$(MAKECMDGOALS)" "clean"
-include $(target-depends)
endif
