#!/bin/bash
set -e

MODE=${1:-kvm}
MEMORY=${2:-2048M}
CPUS=${3:-2}
SSH_FWD_PORT=${QEMU_SSH_FWD_PORT:-2222}

KERNEL="build/kernel/linux-*/arch/x86/boot/bzImage"
ROOTFS="build/images/rootfs.ext4"
INITRD="build/images/rootfs.cpio.gz"

# Find kernel path (handle wildcard)
KERNEL_PATH=$(ls -1 $KERNEL 2>/dev/null | head -1)

if [ ! -f "$KERNEL_PATH" ]; then
    echo "Error: Kernel not found. Run 'make kernel' first."
    exit 1
fi

if [ ! -f "$ROOTFS" ]; then
    echo "Error: Rootfs not found. Run 'make rootfs' first."
    exit 1
fi

can_use_kvm() {
    [ -e /dev/kvm ] && [ -r /dev/kvm ] && [ -w /dev/kvm ]
}

is_port_in_use() {
    local port="$1"
    ss -lnt 2>/dev/null | awk '{print $4}' | grep -Eq "(^|:)${port}$"
}

pick_available_ssh_port() {
    local start_port="$1"
    local p
    for p in $(seq "$start_port" 2299); do
        if ! is_port_in_use "$p"; then
            echo "$p"
            return 0
        fi
    done
    return 1
}

if [ "$MODE" = "kvm" ] || [ "$MODE" = "kvm-net" ]; then
    if ! can_use_kvm; then
        echo "Warning: KVM is unavailable in this environment; falling back to software emulation."
        if [ "$MODE" = "kvm-net" ]; then
            MODE="no-kvm-net"
        else
            MODE="no-kvm"
        fi
    fi
fi

echo "Starting QEMU..."
echo "  Kernel: $KERNEL_PATH"
echo "  Rootfs: $ROOTFS"
if [ -f "$INITRD" ]; then
    echo "  Initramfs: $INITRD"
fi
echo "  Memory: $MEMORY"
echo "  CPUs: $CPUS"
echo "  Mode: $MODE"
echo ""
echo "To exit QEMU, press: Ctrl+A then X"
echo ""

# Build QEMU command
QEMU_CMD="qemu-system-x86_64"
QEMU_ARGS=(
    "-m" "$MEMORY"
    "-smp" "$CPUS"
    "-kernel" "$KERNEL_PATH"
    "-append" "rw console=ttyS0 rdinit=/init"
    "-drive" "file=$ROOTFS,format=raw,if=none,id=vdisk0"
    "-device" "virtio-blk-pci,drive=vdisk0"
    "-nographic"
    "-serial" "mon:stdio"
)

if [ -f "$INITRD" ]; then
    QEMU_ARGS+=("-initrd" "$INITRD")
fi

if [ "$MODE" = "kvm-net" ] || [ "$MODE" = "no-kvm-net" ]; then
    if is_port_in_use "$SSH_FWD_PORT"; then
        if [ -n "${QEMU_SSH_FWD_PORT:-}" ]; then
            echo "Error: QEMU_SSH_FWD_PORT=$SSH_FWD_PORT is already in use. Choose another port."
            exit 1
        fi
        picked_port=$(pick_available_ssh_port "$SSH_FWD_PORT") || {
            echo "Warning: no free port found in range ${SSH_FWD_PORT}-2299; disabling SSH host forward."
            SSH_FWD_PORT=""
        }
        if [ -n "$picked_port" ]; then
            SSH_FWD_PORT="$picked_port"
            echo "Info: port 2222 is busy, using host SSH forward port: $SSH_FWD_PORT"
        fi
    fi
fi

case "$MODE" in
    kvm)
        QEMU_ARGS+=("-enable-kvm" "-cpu" "host")
        ;;
    kvm-net)
        QEMU_ARGS+=("-enable-kvm" "-cpu" "host")
        if [ -n "$SSH_FWD_PORT" ]; then
            QEMU_ARGS+=("-nic" "user,model=virtio-net-pci,hostfwd=tcp::${SSH_FWD_PORT}-:22")
        else
            QEMU_ARGS+=("-nic" "user,model=virtio-net-pci")
        fi
        ;;
    no-kvm)
        QEMU_ARGS+=("-cpu" "max")
        ;;
    no-kvm-net)
        QEMU_ARGS+=("-cpu" "max")
        if [ -n "$SSH_FWD_PORT" ]; then
            QEMU_ARGS+=("-nic" "user,model=virtio-net-pci,hostfwd=tcp::${SSH_FWD_PORT}-:22")
        else
            QEMU_ARGS+=("-nic" "user,model=virtio-net-pci")
        fi
        ;;
    *)
        echo "Unknown mode: $MODE"
        echo "Valid modes: kvm, kvm-net, no-kvm"
        exit 1
        ;;
esac

# Run QEMU
exec $QEMU_CMD "${QEMU_ARGS[@]}"
