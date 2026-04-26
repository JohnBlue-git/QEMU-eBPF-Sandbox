#!/bin/bash
set -e

echo "Running eBPF tests..."
echo ""

# Check if eBPF programs exist
if [ ! -f "eBPF_basic_design/build/xdp_drop.bpf.o" ]; then
    echo "Error: eBPF programs not built. Run 'make ebpf_basic' first."
    exit 1
fi

echo "[Test 1] Checking eBPF bytecode..."
if llvm-objdump -d eBPF_basic_design/build/xdp_drop.bpf.o | grep -q "xdp_pass"; then
    echo "✓ XDP drop program found"
else
    echo "✗ XDP drop program not found"
    exit 1
fi

echo ""
echo "[Test 2] Checking BPF system..."
if bpftool version &>/dev/null; then
    echo "✓ bpftool is available"
else
    echo "✗ bpftool not found"
    exit 1
fi

echo ""
echo "[Test 3] Checking eBPF kernel support..."
if [ -f "/proc/sys/kernel/bpf_stats_enabled" ]; then
    echo "✓ eBPF kernel support detected"
else
    echo "⚠ eBPF support might be limited (can still work)"
fi

echo ""
echo "[Test 4] Checking available eBPF programs..."
ls -lh eBPF_basic_design/build/*.bpf.o 2>/dev/null | awk '{print "✓", $NF}' || echo "No eBPF programs found"

echo ""
echo "✓ All tests passed!"
