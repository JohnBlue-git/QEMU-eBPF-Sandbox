# How To eBPF - QEMU on GitHub Codespace

A practical eBPF development environment using QEMU, optimized for GitHub Codespace with 2 CPUs.

## Features

- **Lightweight QEMU VM**: Minimal Linux kernel + BusyBox for resource efficiency
- **eBPF Programs**: XDP (eXpress Data Path) and syscall tracing examples
- **Host & Guest Development**: Run eBPF programs on both the host and inside QEMU
- **Quick Setup**: Automated dependency installation and image building

## Architecture

```
┌─────────────────────────────────────────┐
│     GitHub Codespace (2 CPUs)           │
├─────────────────────────────────────────┤
│  Host System (Ubuntu)                   │
│  - QEMU/KVM                             │
│  - libbpf, clang/llvm, bpftool         │
│  - eBPF development tools               │
├─────────────────────────────────────────┤
│  QEMU Guest (Alpine Linux + BusyBox)    │
│  - Linux kernel (compiled with eBPF)    │
│  - libbpf for eBPF runtime              │
│  - Can run eBPF programs                │
└─────────────────────────────────────────┘
```

## Quick Start

### 1. Install Dependencies
```bash
make setup
```

### 2. Build Everything
```bash
make build
```

This builds in this order:
1. Kernel
2. eBPF programs
3. Rootfs image (packages `/opt/ebpf_basic_design` for guest)

### 3. Run QEMU with eBPF
```bash
make qemu-net

# In QEMU guest, check auto-load logs
tail -f /var/log/ebpf_basic_design/syscall_trace_loader.log
tail -f /var/log/ebpf_basic_design/xdp_loader.log

# Ring buffer event logs (async writer)
tail -f /var/log/ebpf_basic_design/syscall_trace.events.log
tail -f /var/log/ebpf_basic_design/xdp_drop.events.log
```

Note: `make rootfs` packages `eBPF_basic_design/build/*.bpf.o` and loaders into `/opt/ebpf_basic_design` inside the VM image.
`guest/init` is used as the VM init process and will auto-attempt to load eBPF samples at boot.

### 4. (Optional) Rebuild eBPF, Host-side eBPF Test/Load
```bash
# Build
make ebpf_basic

# Validate build artifacts and environment
./scripts/run_eBPF_basic_design.sh test

# Load XDP sample (requires root)
sudo ./scripts/run_eBPF_basic_design.sh xdp lo

# Load syscall trace sample (requires root)
sudo ./scripts/run_eBPF_basic_design.sh trace

# Load socket filter sample (requires root)
sudo ./scripts/run_eBPF_basic_design.sh socket lo

# Load cgroup egress filter sample (requires root)
sudo ./scripts/run_eBPF_basic_design.sh cgroup /sys/fs/cgroup
```

## Project Structure

```
├── README.md                 # This file
├── Makefile                  # Build orchestration
├── scripts/
│   ├── setup.sh             # Install dependencies
│   ├── build-kernel.sh      # Build minimal Linux kernel
│   ├── build-rootfs.sh      # Build filesystem image
│   ├── run-qemu.sh          # Run QEMU
│   ├── run_eBPF_basic_design.sh  # Host-side eBPF helper
│   └── test.sh              # eBPF validation tests
├── eBPF_basic_design/       # eBPF concepts, examples, and commands
├── guest/
│   └── init                 # Guest init; auto-attempts eBPF load
└── build/                   # Generated artifacts
```

## System Requirements

- **2+ CPUs** (optimized for Codespace 2 CPU)
- **4GB+ RAM** (Codespace standard: 8GB)
- **10GB+ Disk** (Codespace standard: 32GB)
- Ubuntu 20.04+ or other Linux distro

## Performance Tips

1. **Reduce kernel size**: Config only includes eBPF-related features
2. **Use Alpine**: Lightweight rootfs (1-2 MB)
3. **KVM acceleration**: Enabled by default
4. **2 vCPU setup**: Balanced for Codespace resources

## Kernel

The project uses Linux kernel version 6.1.94 as the default. Based on the repository's evidence, the reasons is that 6.1.x provides a reasonable foundation for eBPF tutorials. This project requires features like BPF syscalls, BTF, CO-RE, tracepoints, XDP, and cgroup hooks (visible in `build-kernel.sh`'s kernel config). Version 6.1 is a long-term support release, sufficiently modern, feature-complete, and stable for eBPF examples—more suitable for teaching and reproducing in Codespaces/QEMU than tracking the latest mainline kernel.

This version represents the currently fixed, verified default. If needed, the build scripts support version overrides:

- Pass a version as the first parameter to `build-kernel.sh`: `bash build-kernel.sh 6.6.30`
- Use `make KERNEL_VERSION=6.6.30` to override in the Makefile

If you need to change the version, I can help evaluate compatibility with the eBPF examples.

## eBPF

For a deeper explanation of what eBPF does, how the examples in this repo work, and how CO-RE/BTF are used here, see [eBPF_basic_design/README.md](eBPF_basic_design/README.md).

## Troubleshooting

### QEMU fails to start
- Check `/proc/cpuinfo` for CPU features
- Fallback mode: `make qemu-no-kvm`

### eBPF program won't load
- Check kernel config: `cat boot/config-*`
- View errors: `sudo dmesg | tail -20`

### Performance issues
- Monitor: `top`, `htop`, `iotop`
- Inside QEMU: `ps aux`, `free -h`

## Docker image

I am planning to create docker image for easy to replicate the enviroment. But not we would pause to wait for repo contents become more complete.

https://hub.docker.com/repository/docker/johnbluedocker/qemu-ebpf/general

## References

- [libbpf Documentation](https://github.com/libbpf/libbpf)
- [BPF and XDP Reference Guide](https://cilium.io/blog/2020/02/17/cilium-wireguard/)
- [eBPF.io](https://ebpf.io/)
- [Linux Kernel Documentation](https://www.kernel.org/doc/html/latest/bpf/index.html)

## License

MIT - See LICENSE file for details

## Contributing

Feel free to submit issues and pull requests!
