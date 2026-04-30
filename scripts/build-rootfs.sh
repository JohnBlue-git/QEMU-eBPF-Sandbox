#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$ROOT_DIR"

ROOTFS_SIZE=${1:-512M}
ROOTFS_DIR="build/rootfs"
ROOTFS_IMG="build/images/rootfs.ext4"
ROOTFS_INITRD="build/images/rootfs.cpio.gz"

if [ -f "$ROOTFS_IMG" ] || [ -f "$ROOTFS_INITRD" ]; then
    echo "Refreshing existing rootfs artifacts..."
    rm -f "$ROOTFS_IMG" "$ROOTFS_INITRD"
fi

echo "Building rootfs image ($ROOTFS_SIZE)..."

if ! command -v cpio >/dev/null 2>&1; then
    echo "Error: cpio command not found. Run ./scripts/setup.sh first."
    exit 1
fi

echo "Building eBPF artifacts for guest image..."
make -C eBPF_basic_design all
make ebpf_oop

# Prepare staging rootfs directory
rm -rf "$ROOTFS_DIR"
mkdir -p "$ROOTFS_DIR"
mkdir -p "$ROOTFS_DIR"/{bin,sbin,lib,usr,sys,proc,tmp,dev,etc,root,home}

# Copy busybox (prefer static binary)
echo "Installing BusyBox..."
if [ -x /bin/busybox-static ]; then
    cp /bin/busybox-static "$ROOTFS_DIR/bin/busybox"
elif [ -x /usr/bin/busybox-static ]; then
    cp /usr/bin/busybox-static "$ROOTFS_DIR/bin/busybox"
elif [ -x /bin/busybox ]; then
    cp /bin/busybox "$ROOTFS_DIR/bin/busybox"
else
    echo "Error: busybox(-static) not found"
    exit 1
fi
chmod +x "$ROOTFS_DIR/bin/busybox"

# Install required busybox applets with relative symlinks.
# Using busybox --install in this environment can create host-absolute links.
install_applet() {
    local applet="$1"
    local dir="$2"
    mkdir -p "$ROOTFS_DIR/$dir"
    ln -sfn busybox "$ROOTFS_DIR/$dir/$applet"
}

for app in sh ash mount umount cat echo ls mkdir mknod chmod chown cp mv rm sleep grep tail head wc find ping wget nslookup nc; do
    install_applet "$app" "bin"
done

for app in ifconfig ip udhcpc hostname; do
    install_applet "$app" "sbin"
done

# Package eBPF artifacts into rootfs

# Package eBPF_basic_design artifacts
mkdir -p "$ROOTFS_DIR/opt/ebpf_basic_design"
cp eBPF_basic_design/build/*.bpf.o "$ROOTFS_DIR/opt/ebpf_basic_design/"
cp eBPF_basic_design/build/*_loader "$ROOTFS_DIR/opt/ebpf_basic_design/"

# Package eBPF_oop_design artifacts
mkdir -p "$ROOTFS_DIR/opt/ebpf_oop_design"
cp eBPF_oop_design/build/*.bpf.o "$ROOTFS_DIR/opt/ebpf_oop_design/" 2>/dev/null || true
cp eBPF_oop_design/build/*_loader "$ROOTFS_DIR/opt/ebpf_oop_design/" 2>/dev/null || true

# Copy required shared libraries for user-space loaders
copy_lib() {
    local src="$1"
    local dst_dir="$ROOTFS_DIR$(dirname "$src")"
    mkdir -p "$dst_dir"
    cp -L "$src" "$dst_dir/"
}

copy_lib /lib/x86_64-linux-gnu/libbpf.so.1
copy_lib /lib/x86_64-linux-gnu/libelf.so.1
copy_lib /lib/x86_64-linux-gnu/libz.so.1
copy_lib /lib/x86_64-linux-gnu/libzstd.so.1
copy_lib /lib/x86_64-linux-gnu/libc.so.6
copy_lib /lib64/ld-linux-x86-64.so.2
copy_lib /usr/lib/x86_64-linux-gnu/libstdc++.so.6
copy_lib /lib/x86_64-linux-gnu/libgcc_s.so.1
copy_lib /lib/x86_64-linux-gnu/libm.so.6

# Install guest init script
if [ -f guest/init ]; then
    cp guest/init "$ROOTFS_DIR/init"
else
    cat > "$ROOTFS_DIR/init" << 'INIT_SCRIPT'
#!/bin/sh
exec /bin/sh
INIT_SCRIPT
fi

chmod +x "$ROOTFS_DIR/init"

# Create minimal fstab
cat > "$ROOTFS_DIR/etc/fstab" << 'FSTAB'
proc    /proc   proc    defaults    0 0
sysfs   /sys    sysfs   defaults    0 0
tmpfs   /tmp    tmpfs   defaults    0 0
FSTAB

# Create ext4 image from staging directory without loop-mount.
mkdir -p build/images
truncate -s "$ROOTFS_SIZE" "$ROOTFS_IMG"
mkfs.ext4 -q -F -d "$ROOTFS_DIR" "$ROOTFS_IMG"

# Also create initramfs for environments where block device discovery is limited.
(cd "$ROOTFS_DIR" && find . -xdev -print | cpio -o -H newc 2>/dev/null | gzip -9 > "../images/rootfs.cpio.gz")

if [ ! -s "$ROOTFS_INITRD" ] || [ "$(stat -c%s "$ROOTFS_INITRD")" -lt 1024 ]; then
    echo "Error: generated initramfs is unexpectedly small: $ROOTFS_INITRD"
    exit 1
fi

echo "✓ Rootfs built successfully!"
echo "  Location: $ROOTFS_IMG"
echo "  Initramfs: $ROOTFS_INITRD"
