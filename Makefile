# Top level makefile for building VHDL project

TOP := $(CURDIR)


# We define two sets of targets which can be overridden in the CONFIG file.

# This defines the targets which are built when `make` is run with no target.
# This target is defined for developer convenience.
DEFAULT_TARGETS = driver-rpm

# These targets are built when `make install` is run, and should define all the
# targets which are expected to be built as part of the system installation.
INSTALL_TARGETS = $(DEFAULT_TARGETS)


include Makefile.common


MAKE_LOCAL = \
    $(MAKE) -C $< -f $(TOP)/$1/Makefile.local TOP=$(TOP) $@


default: $(DEFAULT_TARGETS)
.PHONY: default

install: $(INSTALL_TARGETS)
.PHONY: install

# ------------------------------------------------------------------------------
# Driver build

DRIVER_TARGETS = driver insmod rmmod install-dkms remove-dkms driver-rpm udev
.PHONY: $(DRIVER_TARGETS)

$(DRIVER_TARGETS): $(DRIVER_BUILD_DIR)
	$(call MAKE_LOCAL,driver)

$(DRIVER_BUILD_DIR):
	mkdir -p $@

clean-driver:
	rm -rf $(DRIVER_BUILD_DIR)
.PHONY: clean-driver


# ------------------------------------------------------------------------------
# Note that because we use pattern matching for our subdirectory clean targets,
# we can't mark these targets as .PHONY, because it seems that .PHONY targets
# don't participate in pattern matching.
clean-%:
	make -C $* clean

clean: $(DIR_TARGETS:%=clean-%)
	rm -rf $(BUILD_DIR)
.PHONY: clean
