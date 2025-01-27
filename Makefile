LINUX_OS_DIR ?= $(CURDIR)
OS_SOFTWARE_DIR := $(LINUX_OS_DIR)/software
OS_DIR := $(OS_SOFTWARE_DIR)/OS_build
OS_SUBMODULES_DIR := $(LINUX_OS_DIR)/submodules

# Build Linux OS for IOb-SoC-OpenCryptoLinux
build-OS: build-dts build-opensbi build-rootfs build-linux-kernel

$(OS_DIR):
	mkdir $(OS_DIR)

## OpenSBI Linux on RISC-V stage 1 Bootloader (Implements the SBI interface)
build-opensbi: clean-opensbi $(OS_DIR)
	cp -r $(OS_SOFTWARE_DIR)/opensbi_platform/* $(OS_SUBMODULES_DIR)/OpenSBI/platform/ && \
		cd $(OS_SUBMODULES_DIR)/OpenSBI && \
		CROSS_COMPILE=riscv64-unknown-linux-gnu- $(MAKE) run PLATFORM=iob_soc

## Busybox rootfs target (for a minimal Linux OS)
build-rootfs: clean-rootfs $(OS_DIR)
	cd $(OS_SUBMODULES_DIR)/busybox && \
		cp $(OS_SOFTWARE_DIR)/rootfs_busybox/busybox_config $(OS_SUBMODULES_DIR)/busybox/configs/iob_defconfig && \
		$(MAKE) ARCH=riscv CROSS_COMPILE=riscv64-unknown-linux-gnu- iob_defconfig && \
		CROSS_COMPILE=riscv64-unknown-linux-gnu- $(MAKE) -j$(nproc) && \
		CROSS_COMPILE=riscv64-unknown-linux-gnu- $(MAKE) install && \
		cd _install/ && cp $(OS_SOFTWARE_DIR)/rootfs_busybox/init init && \
		mkdir -p dev && sudo mknod dev/console c 5 1 && sudo mknod dev/ram0 b 1 0 && \
		find -print0 | cpio -0oH newc | gzip -9 > $(OS_DIR)/rootfs.cpio.gz

## Linux Kernel Makefile Variables and Targets
LINUX_VERSION?=5.15.98
LINUX_NAME=linux-$(LINUX_VERSION)
LINUX_DIR=$(OS_SUBMODULES_DIR)/$(LINUX_NAME)
LINUX_IMAGE=$(LINUX_DIR)/arch/riscv/boot/Image

build-linux-kernel: $(OS_DIR) $(LINUX_IMAGE)
		cp $(LINUX_IMAGE) $(OS_DIR)

$(LINUX_IMAGE): $(LINUX_DIR)
	cd $(LINUX_DIR) && \
		cp $(OS_SOFTWARE_DIR)/linux_config $(LINUX_DIR)/arch/riscv/configs/iob_soc_defconfig && \
		$(MAKE) ARCH=riscv CROSS_COMPILE=riscv64-unknown-linux-gnu- iob_soc_defconfig && \
		$(MAKE) ARCH=riscv CROSS_COMPILE=riscv64-unknown-linux-gnu- -j`nproc`

$(LINUX_DIR):
	@wget https://cdn.kernel.org/pub/linux/kernel/v5.x/$(LINUX_NAME).tar.xz && \
		tar -xf $(LINUX_NAME).tar.xz -C $(OS_SUBMODULES_DIR)

## IOb-SoC Device Tree target
build-dts: $(OS_DIR)
	dtc -O dtb -o $(OS_DIR)/iob_soc.dtb $(OS_SOFTWARE_DIR)/iob_soc.dts

## Buildroot Makefile Variables and Targets
BUILDROOT_VERSION=buildroot-2022.02.10
BUILDROOT_DIR=$(OS_SUBMODULES_DIR)/$(BUILDROOT_VERSION)

build-buildroot: $(OS_DIR) $(BUILDROOT_DIR)
	cd $(BUILDROOT_DIR) && \
		$(MAKE) BR2_EXTERNAL=$(OS_SOFTWARE_DIR)/buildroot iob_soc_defconfig && $(MAKE) -j`nproc` && \
		cp $(BUILDROOT_DIR)/output/images/rootfs.cpio.gz $(OS_DIR)

$(BUILDROOT_DIR):
	@wget https://buildroot.org/downloads/$(BUILDROOT_VERSION).tar.gz && \
		tar -xvzf $(BUILDROOT_VERSION).tar.gz -C $(OS_SUBMODULES_DIR)

# Test RootFS in QEMU # WIP
QEMU_OS_DIR=$(OS_DIR)/QEMU
QEMU_IMAGE=$(QEMU_OS_DIR)/Image

$(QEMU_OS_DIR): $(OS_DIR)
	mkdir $(QEMU_OS_DIR)

$(QEMU_IMAGE): $(QEMU_OS_DIR) $(LINUX_DIR)
	cd $(OS_SUBMODULES_DIR)/$(LINUX_NAME) && \
		$(MAKE) ARCH=riscv CROSS_COMPILE=riscv64-unknown-linux-gnu- rv32_defconfig && \
		$(MAKE) ARCH=riscv CROSS_COMPILE=riscv64-unknown-linux-gnu- -j`nproc` && \
		cp $(LINUX_IMAGE) $(QEMU_OS_DIR)

build-qemu: $(QEMU_IMAGE)

run-qemu:
	qemu-system-riscv32 -machine virt -kernel $(QEMU_OS_DIR)/Image \
		-initrd $(OS_DIR)/rootfs.cpio.gz -nographic \
		-append "rootwait root=/dev/vda ro" 

#
# Clean
#
clean-opensbi:
	-@cd $(OS_SUBMODULES_DIR)/OpenSBI && $(MAKE) distclean && \
		rm $(OS_DIR)/fw_*.bin

clean-rootfs:
	-@cd $(OS_SUBMODULES_DIR)/busybox && $(MAKE) distclean && \
		rm $(OS_DIR)/rootfs.cpio.gz

clean-linux-kernel:
	-@rm -r $(LINUX_DIR)
	-@rm $(OS_DIR)/Image
	-@rm $(LINUX_NAME).tar.xz

clean-buildroot:
	-@rm -rf $(OS_SUBMODULES_DIR)/$(BUILDROOT_VERSION) && \
		rm $(BUILDROOT_VERSION).tar.gz

clean-OS:
	@rm -rf $(OS_DIR)

# Phony targets
.PHONY: build-OS build-opensbi build-rootfs build-linux-kernel build-dts build-buildroot clean-OS clean-opensbi clean-rootfs clean-linux-kernel clean-buildroot build-qemu
