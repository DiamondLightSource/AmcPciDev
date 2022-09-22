# This makefile is designed to be called from dkms: it builds in place.
# This makefile should only be called from dkms as defined in dkms.conf.in

# Must define KERNEL_DIR and VERSION before calling this

amc_pci.ko:
	make -C $(KERNEL_DIR) M=$(CURDIR) modules
