# How To eBPF - QEMU on GitHub Codespace

A practical eBPF development environment using QEMU, optimized for GitHub Codespace with 2 CPUs. Includes both procedural and object-oriented eBPF program designs.

## Design documents

- [DESIGN.html (GitHub)](https://github.com/JohnBlue-git/QEMU-eBPF-Sandbox/blob/main/DESIGN.html)
- [DESIGN.html (rendered)](https://htmlpreview.github.io/?https://github.com/JohnBlue-git/QEMU-eBPF-Sandbox/blob/main/DESIGN.html)
- [docs/BASIC_DESIGN.md](docs/BASIC_DESIGN.md)
- [docs/OOP_DESIGN.md](docs/OOP_DESIGN.md)
- [docs/BOOST_ASIO_DESIGN.md](docs/BOOST_ASIO_DESIGN.md)
- [docs/HOOKS.md](docs/HOOKS.md) — hook selection and platform constraints
- [docs/BPFTOOL.md](docs/BPFTOOL.md) — target-kernel capability checks with `bpftool`

## Features

- **Lightweight QEMU VM**: Minimal Linux kernel + BusyBox for resource efficiency
- **Three eBPF Design Patterns**:
  - **eBPF Basic Design**: Procedural C implementation with libbpf, CO-RE, and BTF metadata
  - **eBPF OOP Design**: Modern C++ object-oriented architecture with custom coroutines and async event handling
  - **eBPF Boost.Asio Design**: Production-grade C++ with Boost.Asio async I/O and stream caching
- **Multiple eBPF Programs**: XDP, syscall tracing, socket filtering, and cgroup egress examples
- **Host & Guest Development**: Run eBPF programs on both the host and inside QEMU
- **Async Event Processing**: Coroutine-based asynchronous logging and action queue system
- **Quick Setup**: Automated dependency installation and image building

## Architecture

```
┌─────────────────────────────────────────┐
│     GitHub Codespace (2 CPUs)           │
├─────────────────────────────────────────┤
│  Host System (Ubuntu)                   │
│  - QEMU/KVM                             │
│  - libbpf, clang/llvm, bpftool          │
│  - eBPF development tools               │
├─────────────────────────────────────────┤
│  QEMU Guest (Alpine Linux + BusyBox)    │
│  - Linux kernel (compiled with eBPF)    │
│  - libbpf for eBPF runtime              │
│  - Can run eBPF programs                │
└─────────────────────────────────────────┘
```

## eBPF Design Approaches

This project provides two distinct design patterns for eBPF program development:

### eBPF Basic Design (Procedural C)

Located in `eBPF_basic_design/`, this design uses procedural C with modern eBPF tooling:

- **Language**: C with libbpf
- **CO-RE & BTF**: Compile-once, run-anywhere capability using kernel BTF metadata
- **Type Safety**: Generated `vmlinux.h` from kernel BTF replaces hand-written type shims
- **Logging**: Simple synchronous logging with shared utilities
- **Use Cases**: 
  - Learning eBPF fundamentals
  - High-performance filtering with minimal overhead
  - Simple event logging and counting

**Programs Included**:
- `xdp_drop`: Fast packet filtering at network ingress
- `syscall_trace`: Monitor syscall events kernel-wide
- `socket_filter`: Per-socket packet filtering
- `cgroup_egress`: Network filtering at cgroup boundaries

### eBPF OOP Design (Modern C++)

Located in `eBPF_oop_design/`, this design emphasizes modularity and asynchronous processing:

- **Language**: C++ with CMake
- **Architecture**: Event-driven with decoupled kernel event generation and user-space processing
- **Async Processing**: C++20 coroutines for non-blocking event handling
- **Modularity**: Base `BpfProgram` class for code reuse across different eBPF types
- **Action Pattern**: ActionLoop singleton dispatcher with thread-safe queues
- **Use Cases**:
  - Production monitoring systems requiring scalability
  - Complex event processing pipelines
  - Integration with larger C++ applications
  - Asynchronous logging at high throughput

**Key Features**:
- `ActionLoop`: Singleton dispatcher for async action execution
- `LogAction`: Async file I/O with coroutines
- `AsyncFileMutex`: Thread-safe file operations in coroutine context
- Thread-safe ring buffer event polling

### eBPF Boost.Asio Design (Production C++)

Located in `eBPF_boost_asio_design/`, this design combines OOP modularity with **Boost.Asio**, a production-grade async I/O framework:

- **Language**: C++ with CMake
- **Architecture**: Event-driven with Boost.Asio io_context for optimal event multiplexing
- **Async Framework**: Boost.Asio awaitables with epoll/kqueue/IOCP support
- **Performance**: Stream descriptor caching and optimized file access patterns
- **Scalability**: Handles thousands of concurrent events with minimal overhead
- **Use Cases**:
  - High-throughput monitoring systems in production
  - Integration with existing Boost.Asio applications
  - Systems requiring cross-platform async I/O (Linux/Windows/macOS)
  - Performance-critical event processing pipelines

**Key Features**:
- `ActionLoop`: Boost.Asio-based dispatcher with executor work guards
- `LogAction`: Async file I/O using boost::asio::async_write
- `AsyncFileStreamManager`: Stream descriptor pooling for connection reuse
- Automatic platform-optimal I/O multiplexing (epoll on Linux)

**Comparison**: See [docs/BOOST_ASIO_DESIGN.md](docs/BOOST_ASIO_DESIGN.md#comparison-with-oop-design) for a detailed comparison with the OOP design.

## Project Structure

```
├── README.md                 # This file
├── Makefile                  # Build orchestration
├── scripts/
│   ├── setup.sh             # Install dependencies
│   ├── build-kernel.sh      # Build minimal Linux kernel
│   ├── build-rootfs.sh      # Build filesystem image
│   ├── run-qemu.sh          # Run QEMU
│   ├── run_eBPF_basic_design.sh  # Host-side eBPF helper (basic design)
│   └── test.sh              # eBPF validation tests
├── docs/                    # Design, hook, and capability documentation
│   ├── BASIC_DESIGN.md
│   ├── OOP_DESIGN.md
│   ├── BOOST_ASIO_DESIGN.md
│   ├── HOOKS.md
│   └── BPFTOOL.md
├── DESIGN.html             # Architecture overview rendered as HTML
├── eBPF_basic_design/       # Procedural C implementation with CO-RE & BTF
│   ├── Makefile
│   ├── vmlinux.h            # Generated kernel types for CO-RE
│   ├── xdp_drop/            # XDP packet drop example
│   ├── syscall_trace/       # Syscall tracing example
│   ├── socket_filter/       # Socket filtering example
│   ├── cgroup_egress/       # Cgroup egress filtering example
│   ├── logging/             # Async event logging utilities
│   └── build/               # Compiled eBPF objects and loaders
├── eBPF_oop_design/         # Modern C++ OOP design with coroutines
│   ├── CMakeLists.txt       # CMake build configuration
│   ├── actions/             # Action loop and async logging implementation
│   ├── coroutine/           # C++20 coroutine utilities
│   ├── ebpf/                # Base eBPF program wrapper
│   ├── xdp_drop/            # OOP loader and user-space wrapper
│   ├── syscall_trace/       # OOP loader and user-space wrapper
│   ├── socket_filter/       # OOP loader and user-space wrapper
│   ├── cgroup_egress/       # OOP loader and user-space wrapper
│   └── build/               # Compiled artifacts
├── eBPF_boost_asio_design/  # Production C++ with Boost.Asio async I/O
│   ├── CMakeLists.txt       # CMake build configuration with external dependencies
│   ├── actions/             # Boost.Asio action loop and async logging implementation
│   ├── coroutine/           # Boost.Asio async utilities (stream caching, file locking)
│   ├── xdp_drop/            # Boost.Asio loader and user-space wrapper
│   ├── syscall_trace/       # Boost.Asio loader and user-space wrapper
│   ├── socket_filter/       # Boost.Asio loader and user-space wrapper
│   ├── cgroup_egress/       # Boost.Asio loader and user-space wrapper
│   └── build/               # Compiled artifacts and external dependencies
├── guest/
│   └── init                 # Guest init; auto-attempts eBPF load
└── build/                   # Generated kernel and rootfs artifacts
```

The canonical kernel-side sources are `eBPF_basic_design/<example>/<example>.bpf.c`. The OOP and Boost.Asio directories contain only their design-specific user-space code; their CMake files compile the canonical Basic Design sources.

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

The four kernel-side programs have one canonical source copy under `eBPF_basic_design/`:

- `eBPF_basic_design/xdp_drop/xdp_drop.bpf.c`
- `eBPF_basic_design/syscall_trace/syscall_trace.bpf.c`
- `eBPF_basic_design/socket_filter/socket_filter.bpf.c`
- `eBPF_basic_design/cgroup_egress/cgroup_egress.bpf.c`

The OOP and Boost.Asio designs reuse these files from CMake and add their own user-space wrappers, loaders, and event-processing architecture. They do not keep duplicate `.bpf.c` files.

This project provides comprehensive eBPF learning and development resources with three distinct design approaches:

1. **eBPF Fundamentals**: For a deeper explanation of what eBPF does, how the examples work, and how CO-RE/BTF are used here, see [docs/BASIC_DESIGN.md](docs/BASIC_DESIGN.md).

2. **OOP Architecture**: For details on C++ object-oriented architecture, async event handling with custom coroutines, and the action-based design pattern, see [docs/OOP_DESIGN.md](docs/OOP_DESIGN.md).

3. **Production-Grade Async**: For production systems, learn about Boost.Asio integration, stream descriptor caching, epoll-based event multiplexing, and high-throughput performance optimization in [docs/BOOST_ASIO_DESIGN.md](docs/BOOST_ASIO_DESIGN.md).

Choose the design that best fits your use case:
- **Learning**: Start with Basic Design (C + libbpf)
- **Prototyping**: Use OOP Design (custom coroutines)
- **Production**: Deploy Boost.Asio Design (battle-tested async framework)

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


## eBPF capability guides

The detailed capability and hook discovery material now lives in the documentation folder:

- [Hook comparison and selection](docs/HOOKS.md)
- [Target-kernel checks with `bpftool`](docs/BPFTOOL.md)
- [Official kernel BPF documentation](https://docs.kernel.org/bpf/)

## References

- [libbpf Documentation](https://github.com/libbpf/libbpf)
- [BPF and XDP Reference Guide](https://cilium.io/blog/2020/02/17/cilium-wireguard/)
- [eBPF.io](https://ebpf.io/)
- [Linux Kernel Documentation](https://www.kernel.org/doc/html/latest/bpf/index.html)

## License

MIT - See LICENSE file for details

## Contributing

Feel free to submit issues and pull requests!
