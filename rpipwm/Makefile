ifneq	($(KERNELRELEASE),)
obj-m := rpipwm.o
else
KERNELDIR ?= /workspace/linux-rpi
PWD := $(shell pwd)
ARCH ?= arm

modules:
	$(MAKE) ARCH=$(ARCH) -C $(KERNELDIR) M=$(PWD) modules
clean:
	$(MAKE) ARCH=$(ARCH) -C $(KERNELDIR) M=$(PWD) clean
endif

