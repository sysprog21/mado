# Unified dependency checking and flag retrieval system
#
# Usage: $(call dep,flag-type,package-name[s])
#   flag-type: cflags or libs
#   package-name[s]: single package or space-separated list (e.g., "cairo", "neatvnc aml")
#
# Automatically detects the right tool:
# 1. Try package-config tool first (e.g., sdl2-config) - single package only
# 2. Fall back to pkg-config if available - supports multiple packages
# 3. Return empty string if package(s) not found
#
# Verbose mode: Set V=1 to show dependency-checking errors
#   make V=1         # Show all pkg-config/config-tool errors
#   make             # Silent mode (default)

# Conditional stderr redirection based on V= flag
ifeq ($(V),1)
_dep_stderr :=
else
_dep_stderr := 2>/dev/null
endif

# Internal: Try package-specific config tool (single package only)
# Usage: $(call _dep-config,package-name,flag-type)
define _dep-config
$(shell command -v $(1)-config >/dev/null 2>&1 && $(1)-config --$(2) $(_dep_stderr))
endef

# Internal: Try pkg-config (supports multiple packages)
# Usage: $(call _dep-pkg,package-names,flag-type)
define _dep-pkg
$(shell pkg-config --$(2) $(1) $(_dep_stderr))
endef

# Internal: Check if input contains multiple packages
# Usage: $(call _dep-multi,package-names)
define _dep-multi
$(filter-out $(firstword $(1)),$(1))
endef

# Main entry point: Unified dependency checker
# Usage: $(call dep,flag-type,package-name[s])
# Example: $(call dep,cflags,cairo)
#          $(call dep,libs,sdl2)
#          $(call dep,cflags,neatvnc aml pixman-1)
define dep
$(if $(call _dep-multi,$(2)),\
	$(call _dep-pkg,$(2),$(1)),\
	$(if $(call _dep-config,$(2),$(1)),\
		$(call _dep-config,$(2),$(1)),\
		$(call _dep-pkg,$(2),$(1))))
endef
