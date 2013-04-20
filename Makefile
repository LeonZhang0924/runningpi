DIRS = rpipwm

CLEANDIRS = $(DIRS:%=clean-%)
MODULESDIR = $(DIRS:%=modules-%)
INSTALLDIRS = $(DIRS:%=install-%)
INSTALL_IP ?= 10.10.0.5
INSTALL_USER ?= root
INSTALL_DEST ?= ~/

export CROSS_COMPILE=/opt/toolchains/rpi-tools/arm-bcm2708/arm-bcm2708hardfp-linux-gnueabi/bin/arm-bcm2708hardfp-linux-gnueabi-
export KERNELDIR=/workspace/linux-rpi

.PHONY: subdirs $(MODULESDIR)
.PHONY: subdirs $(INSTALLDIRS)

modules: $(MODULESDIR)
$(MODULESDIR):
	$(MAKE) -C $(@:modules-%=%) modules

clean: $(CLEANDIRS)
$(CLEANDIRS):
	$(MAKE) -C $(@:clean-%=%) clean

install: modules $(INSTALLDIRS)
$(INSTALLDIRS):
	scp ${@:install-%=%}/${@:install-%=%}.ko ${INSTALL_USER}@${INSTALL_IP}:${INSTALL_DEST}

.PHONY: install
.PHONY: modules
.PHONY: clean
