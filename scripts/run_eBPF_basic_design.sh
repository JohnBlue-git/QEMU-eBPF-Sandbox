#!/bin/bash
set -e

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

MODE="${1:-test}"
IFACE="${2:-lo}"

run_test() {
    echo "[1/2] Building eBPF programs..."
    make ebpf_basic
    echo "[2/2] Running eBPF validation tests..."
    bash scripts/test.sh
    echo ""
    echo "Done. You can now load examples manually:"
    echo "  sudo ./scripts/run_eBPF_basic_design.sh xdp <iface>"
    echo "  sudo ./scripts/run_eBPF_basic_design.sh trace"
    echo "  sudo ./scripts/run_eBPF_basic_design.sh socket <iface>"
    echo "  sudo ./scripts/run_eBPF_basic_design.sh cgroup <cgroup_path>"
}

run_xdp() {
    if [[ $EUID -ne 0 ]]; then
        echo "Error: xdp mode requires root."
        echo "Run: sudo ./scripts/run_eBPF_basic_design.sh xdp ${IFACE}"
        exit 1
    fi

    make ebpf_basic
    echo "Loading XDP program on interface: ${IFACE}"
    cd eBPF_basic_design
    exec ./build/xdp_drop_loader "${IFACE}"
}

run_trace() {
    if [[ $EUID -ne 0 ]]; then
        echo "Error: trace mode requires root."
        echo "Run: sudo ./scripts/run_eBPF_basic_design.sh trace"
        exit 1
    fi

    make ebpf_basic
    echo "Loading syscall trace program..."
    echo "Tip: watch logs in another terminal with: sudo dmesg -w"
    cd eBPF_basic_design
    exec ./build/syscall_trace_loader
}

run_socket() {
    if [[ $EUID -ne 0 ]]; then
        echo "Error: socket mode requires root."
        echo "Run: sudo ./scripts/run_eBPF_basic_design.sh socket ${IFACE}"
        exit 1
    fi

    make ebpf_basic
    echo "Loading socket filter on interface: ${IFACE}"
    cd eBPF_basic_design
    exec ./build/socket_filter_loader "${IFACE}"
}

run_cgroup() {
    local cgroup_path="${2:-/sys/fs/cgroup}"

    if [[ $EUID -ne 0 ]]; then
        echo "Error: cgroup mode requires root."
        echo "Run: sudo ./scripts/run_eBPF_basic_design.sh cgroup ${cgroup_path}"
        exit 1
    fi

    make ebpf_basic
    echo "Loading cgroup egress filter on: ${cgroup_path}"
    cd eBPF_basic_design
    exec ./build/cgroup_egress_loader "${cgroup_path}"
}

case "$MODE" in
    test)
        run_test
        ;;
    xdp)
        run_xdp
        ;;
    trace)
        run_trace
        ;;
    socket)
        run_socket
        ;;
    cgroup)
        run_cgroup "$@"
        ;;
    *)
        echo "Usage: $0 [test|xdp|trace|socket|cgroup] [iface|cgroup_path]"
        echo "  test         Build + validate eBPF artifacts (default)"
        echo "  xdp <iface>  Load XDP sample to interface (requires root)"
        echo "  trace        Load syscall trace sample (requires root)"
        echo "  socket <iface>   Load socket filter sample (requires root)"
        echo "  cgroup <path>    Load cgroup egress filter sample (requires root)"
        exit 1
        ;;
esac
