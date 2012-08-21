#
# Makefile for the vibrator driver ICs with vibrator driver
#

obj-$(CONFIG_VIBETONZ) += vibrator.o

vibrator-objs +=tspdrv.o 

all:
	@echo "$(MAKE) --no-print-directory -C $(KDIR) SUBDIRS=$(CURDIR) modules"
	@$(MAKE) --no-print-directory -C $(KDIR) \
		SUBDIRS=$(CURDIR) modules

clean:
	rm -r *.o *.ko *.mod.c *.mod.o *~ .*.cmd .*.o.cmd Module.symvers modules.order 

install:
	@$(MAKE) --no-print-directory -C $(KDIR) \
		SUBDIRS=$(CURDIR) modules_install

