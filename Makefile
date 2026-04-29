.PHONY: help setup build qemu qemu-net qemu-no-kvm ebpf_basic clean clean_ebpf_basic clean_rootfs distclean test

KERNEL_VERSION ?= 6.1.94
ROOTFS_SIZE ?= 512M
QEMU_MEMORY ?= 2048M
QEMU_CPUS ?= 2

help:
	@echo "eBPF + QEMU Development Environment"
	@echo "===================================="
	@echo ""
	@echo "Setup & Build:"
	@echo "  make setup        - Install dependencies"
	@echo "  make build        - Build kernel, rootfs, and eBPF programs"
	@echo "  make kernel       - Build Linux kernel only"
	@echo "  make rootfs       - Build rootfs image only"
	@echo "  make ebpf_basic   - Build eBPF programs only"
	@echo "  make ebpf_oop     - Build eBPF OOP design example only"
	@echo ""
	@echo "Run QEMU:"
	@echo "  make qemu         - Run QEMU with KVM (default)"
	@echo "  make qemu-net     - Run QEMU with network interface"
	@echo "  make qemu-no-kvm  - Run QEMU without KVM (slower)"
	@echo ""
	@echo "Development:"
	@echo "  make test         - Run eBPF tests"
	@echo "  make clean_ebpf_basic - Remove eBPF example build artifacts only"
	@echo "  make clean_ebpf_oop - Remove eBPF OOP design build artifacts only"
	@echo "  make clean_rootfs - Remove rootfs staging/image artifacts only"
	@echo "  make clean        - Remove build artifacts"
	@echo "  make distclean    - Remove everything (including images)"
	@echo ""

setup:
	@echo "Installing dependencies..."
	@bash scripts/setup.sh

build: kernel ebpf_basic ebpf_oop rootfs
	@echo "✓ Build complete!"

kernel:
	@echo "Building Linux kernel..."
	@bash scripts/build-kernel.sh $(KERNEL_VERSION)

ebpf_basic:
	@echo "Building eBPF programs..."
	@$(MAKE) -C eBPF_basic_design all

ebpf_oop:
	@echo "Building eBPF OOP design example..."
	@$(MAKE) -C eBPF_oop_design all

rootfs:
	@echo "Building rootfs..."
	@bash scripts/build-rootfs.sh $(ROOTFS_SIZE)

qemu-net: build
	@echo "Starting QEMU with network..."
	@bash scripts/run-qemu.sh kvm-net $(QEMU_MEMORY) $(QEMU_CPUS)

qemu-no-kvm: build
	@echo "Starting QEMU without KVM..."
	@bash scripts/run-qemu.sh no-kvm $(QEMU_MEMORY) $(QEMU_CPUS)

test:
	@echo "Running tests..."
	@bash scripts/test.sh

clean:
	@echo "Cleaning build artifacts..."
	@rm -rf build/
	@$(MAKE) -C eBPF_basic_design clean
	@$(MAKE) -C eBPF_oop_design clean
	@echo "✓ Clean complete"

clean_ebpf_basic:
	@echo "Cleaning eBPF basic artifacts..."
	@$(MAKE) -C eBPF_basic_design clean
	@echo "✓ eBPF basic clean complete"

clean_ebpf_oop:
	@echo "Cleaning eBPF OOP artifacts..."
	@$(MAKE) -C eBPF_oop_design clean
	@echo "✓ eBPF OOP clean complete"

clean_rootfs:
	@echo "Cleaning rootfs artifacts..."
	@rm -rf build/rootfs build/images/rootfs.ext4 build/images/rootfs.cpio.gz
	@echo "✓ Rootfs clean complete"

distclean: clean
	@echo "Removing all generated files and images..."
	@rm -rf build/
	@echo "✓ Distclean complete"

.DEFAULT_GOAL := help
