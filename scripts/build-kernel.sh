#!/bin/bash
set -e

KERNEL_VERSION=${1:-6.1.94}
KERNEL_DIR="build/kernel/linux-${KERNEL_VERSION}"

kernel_config_ok() {
    local cfg="$1"
    [ -f "$cfg" ] || return 1
    grep -q '^CONFIG_PCI=y' "$cfg" || return 1
    grep -q '^CONFIG_VIRTIO_PCI=y' "$cfg" || return 1
    grep -q '^CONFIG_VIRTIO_BLK=y' "$cfg" || return 1
    grep -q '^CONFIG_BLK_DEV_SD=y' "$cfg" || return 1
    grep -q '^CONFIG_EXT4_FS=y' "$cfg" || return 1
    grep -q '^CONFIG_DEVTMPFS=y' "$cfg" || return 1
    grep -q '^CONFIG_DEVTMPFS_MOUNT=y' "$cfg" || return 1
    grep -q '^CONFIG_BLK_DEV_INITRD=y' "$cfg" || return 1
    grep -q '^CONFIG_RD_GZIP=y' "$cfg" || return 1
    grep -q '^CONFIG_DEBUG_INFO=y' "$cfg" || return 1
    grep -q '^CONFIG_DEBUG_INFO_BTF=y' "$cfg" || return 1
    grep -q '^CONFIG_TRACEFS_FS=y' "$cfg" || return 1
    grep -q '^CONFIG_FTRACE_SYSCALLS=y' "$cfg" || return 1
    grep -q '^CONFIG_CGROUPS=y' "$cfg" || return 1
    grep -q '^CONFIG_CGROUP_BPF=y' "$cfg" || return 1
}

if [ -f "$KERNEL_DIR/arch/x86/boot/bzImage" ]; then
    if kernel_config_ok "$KERNEL_DIR/.config" && [ "$KERNEL_DIR/.config" -ot "$KERNEL_DIR/arch/x86/boot/bzImage" ]; then
        echo "Kernel already built at $KERNEL_DIR/arch/x86/boot/bzImage"
        exit 0
    fi
    echo "Kernel image is stale or missing required options; rebuilding..."
fi

echo "Building Linux kernel $KERNEL_VERSION..."
echo "This may take 5-10 minutes..."

mkdir -p build/kernel
cd build/kernel

# Re-download if missing or incomplete kernel source tree.
if [ -d "linux-${KERNEL_VERSION}" ] && [ ! -f "linux-${KERNEL_VERSION}/scripts/Makefile.extrawarn" ]; then
    echo "Found incomplete kernel tree at linux-${KERNEL_VERSION}, re-downloading..."
    rm -rf "linux-${KERNEL_VERSION}"
fi

# Download kernel if not present
if [ ! -d "linux-${KERNEL_VERSION}" ]; then
    echo "Downloading Linux $KERNEL_VERSION..."
    wget -q https://cdn.kernel.org/pub/linux/kernel/v6.x/linux-${KERNEL_VERSION}.tar.xz
    tar xf linux-${KERNEL_VERSION}.tar.xz
    rm linux-${KERNEL_VERSION}.tar.xz
fi

cd linux-${KERNEL_VERSION}

if [ ! -f "scripts/Makefile.extrawarn" ]; then
    echo "Error: kernel source tree is incomplete (missing scripts/Makefile.extrawarn)"
    echo "Please remove build/kernel/linux-${KERNEL_VERSION} and retry."
    exit 1
fi

# Copy minimal eBPF config
cat > .config << 'EOF'
# Minimal kernel config for eBPF on QEMU (2 CPU x86_64)
CONFIG_64BIT=y
CONFIG_X86_64=y
CONFIG_GENERIC_BUG=y
CONFIG_DEFCONFIG_LIST="/lib/modules/$UNAME_RELEASE/.config"

# Processor type
CONFIG_SMP=y
CONFIG_NR_CPUS=4
CONFIG_SMPBOOT=y
CONFIG_X86_LOCAL_APIC=y
CONFIG_X86_IO_APIC=y
CONFIG_X86_BIGSMP=n

# Build options
CONFIG_CC_OPTIMIZE_FOR_SIZE=y
CONFIG_HAVE_EFFICIENT_UNALIGNED_ACCESS=y

# Console & serial
CONFIG_TTY=y
CONFIG_SERIAL_8250=y
CONFIG_SERIAL_8250_CONSOLE=y
CONFIG_PRINTK=y

# Filesystems
CONFIG_EXT4_FS=y
CONFIG_EXT4_FS_POSIX_ACL=n
CONFIG_TMPFS=y
CONFIG_TMPFS_POSIX_ACL=n
CONFIG_DEVTMPFS=y
CONFIG_DEVTMPFS_MOUNT=y
CONFIG_BLK_DEV_INITRD=y
CONFIG_RD_GZIP=y

# Control groups
CONFIG_CGROUPS=y
CONFIG_CGROUP_BPF=y
CONFIG_CGROUP_SKB=y

# Networking
CONFIG_NET=y
CONFIG_PACKET=y
CONFIG_UNIX=y
CONFIG_INET=y
CONFIG_IP_PNP=y
CONFIG_IP_PNP_DHCP=y
CONFIG_NETDEVICES=y
CONFIG_NET_CORE=y
CONFIG_PCI=y
CONFIG_PCI_MSI=y
CONFIG_VIRTIO_NET=y
CONFIG_VIRTIO=y
CONFIG_VIRTIO_PCI=y

# EBF and tracing
CONFIG_BPF=y
CONFIG_BPF_SYSCALL=y
CONFIG_BPF_JIT=y
CONFIG_BPF_JIT_ALWAYS_ON=n
CONFIG_BPF_JIT_DEFAULT_ON=y
CONFIG_HAVE_EBPF_JIT=y
CONFIG_BPF_EVENTS=y
CONFIG_DEBUG_INFO=y
CONFIG_DEBUG_INFO_DWARF4=y
CONFIG_DEBUG_INFO_BTF=y
CONFIG_DEBUG_INFO_BTF_MODULES=y

# Tracing
CONFIG_FTRACE=y
CONFIG_FTRACE_SYSCALLS=y
CONFIG_FUNCTION_TRACER=y
CONFIG_FUNCTION_GRAPH_TRACER=y
CONFIG_TRACEPOINTS=y
CONFIG_TRACEFS_FS=y
CONFIG_KPROBES=y
CONFIG_KPROBE_EVENTS=y
CONFIG_UPROBE_EVENTS=y
CONFIG_TRACE_KPROBES=y
CONFIG_BPF_KPROBE_OVERRIDE=n

# XDP support
CONFIG_NET_XDP=y
CONFIG_XDP_SOCKETS=y

# Debug
CONFIG_DEBUG_KERNEL=y
CONFIG_PANIC_ON_OOPS=n

# Module support
CONFIG_MODULES=y
CONFIG_MODULE_UNLOAD=y

# Device drivers
CONFIG_BLOCK=y
CONFIG_ATA=y
CONFIG_ATA_PIIX=y
CONFIG_SCSI=y
CONFIG_BLK_DEV_SD=y
CONFIG_SCSI_LOWLEVEL=y
CONFIG_ATA_GENERIC=y

# Loop devices for disk images
CONFIG_BLK_DEV_LOOP=y

# Virtio storage
CONFIG_VIRTIO_BLK=y
CONFIG_VIRTIO_PCI_LEGACY=y

# Console
CONFIG_SERIAL_CORE=y
CONFIG_SERIAL_CORE_CONSOLE=y

# Misc
CONFIG_RTC_CLASS=y
CONFIG_RTC_DRV_CMOS=y
CONFIG_WATCHDOG=y

# Keep it minimal
CONFIG_NETFILTER=n
CONFIG_NF_TABLES=n
CONFIG_SECURITY=n
CONFIG_CRYPTO=n
CONFIG_ZLIB_DEFLATE=y
CONFIG_ZLIB_INFLATE=y
EOF

echo "Normalizing kernel config (olddefconfig)..."
make olddefconfig

echo "Building kernel..."
make -j$(nproc) bzImage

if [ ! -f "arch/x86/boot/bzImage" ]; then
    echo "Error: Kernel build failed"
    exit 1
fi

echo "✓ Kernel built successfully!"
echo "  Location: arch/x86/boot/bzImage"
