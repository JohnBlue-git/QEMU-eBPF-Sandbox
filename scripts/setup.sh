#!/bin/bash
set -e

echo "================================"
echo "eBPF + QEMU Setup Script"
echo "================================"
echo ""

# Check if running on Ubuntu/Debian
if ! command -v apt &> /dev/null; then
    echo "Error: This script requires apt (Ubuntu/Debian)"
    exit 1
fi

# Check if running as root for some operations
if [[ $EUID -ne 0 ]]; then
    echo "Some operations require sudo. You may be prompted for your password."
fi

echo "[1/5] Updating package lists..."
sudo apt update -qq

echo "[2/5] Installing build dependencies..."
sudo apt install -y \
    build-essential \
    git \
    wget \
    curl \
    cpio \
    flex \
    bison \
    bc \
    pkg-config \
    libncurses-dev \
    libssl-dev \
    libelf-dev \
    libz-dev \
    libcap-dev \
    openssh-client \
    qemu-system-x86 \
    qemu-utils \
    busybox-static

echo "[3/5] Installing LLVM/Clang..."
sudo apt install -y \
    clang \
    llvm \
    lld

echo "[4/5] Installing eBPF tools..."
sudo apt install -y libbpf-dev

# Install bpftool from matching linux-tools package if standalone package is unavailable.
if ! sudo apt install -y bpftool; then
    echo "bpftool package is not directly installable, trying linux-tools for running kernel..."
    sudo apt install -y linux-tools-$(uname -r) linux-tools-common
    if command -v /usr/lib/linux-tools-$(uname -r)/bpftool >/dev/null 2>&1; then
        sudo ln -sf /usr/lib/linux-tools-$(uname -r)/bpftool /usr/local/bin/bpftool
    fi
fi

echo "[5/5] Installing optional tools..."
if ! sudo apt install -y linux-headers-$(uname -r) linux-tools-$(uname -r); then
    echo "Matching kernel headers/tools not available yet, falling back to meta packages..."
    sudo apt install -y linux-headers-azure linux-tools-azure || true
fi
sudo apt install -y pahole dwarves

# Create necessary directories
echo ""
echo "[Setup] Creating directories..."
mkdir -p build
mkdir -p build/images
mkdir -p build/kernel
mkdir -p guest

echo ""
echo "✓ Setup complete!"
echo ""
echo "Next steps:"
echo "  1. Run: make build"
echo "  2. Run: make qemu-net"
echo ""
