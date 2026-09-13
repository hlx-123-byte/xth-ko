# Compatible with the KernelSU "ddk" container (which exports KDIR) and with
# manual hosts:
#   KDIR=/path/to/kernel-tree make
#
# The DDK image used by CI is:  ghcr.io/ylarod/ddk-min:android12-5.10-20260313
KDIR := $(KDIR)
MDIR := $(realpath $(dir $(abspath $(lastword $(MAKEFILE_LIST)))))

$(info -- KDIR: $(KDIR))
$(info -- MDIR: $(MDIR))

.PHONY: all clean

all:
	@test -n "$(KDIR)" || (echo "ERROR: set KDIR to a prepared arm64 kernel tree" && exit 1)
	make -C $(KDIR) M=$(MDIR) modules -j$(shell nproc)

clean:
	make -C $(KDIR) M=$(MDIR) clean
