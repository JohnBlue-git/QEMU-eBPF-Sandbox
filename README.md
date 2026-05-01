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


## Discovering All Possible eBPF Capabilities in the Linux Kernel

### Available eBPF Program Types

The Linux kernel supports many eBPF program types. To discover them all on your system:

**Via bpftool:**
```bash
bpftool prog help
```

This list includes all supported program types on your kernel. Common types include:

| Program Type | Purpose | Attach Point | Example |
|---|---|---|---|
| `XDP` | Early packet filtering | Network interface (ingress) | Drop malicious packets before stack processing |
| `KPROBE` / `KRETPROBE` | Dynamic kernel function tracing | Any kernel function | Trace memory allocation, function arguments |
| `UPROBE` / `URETPROBE` | Dynamic user-space function tracing | Any user-space function | Trace library calls, application behavior |
| `TRACEPOINT` | Kernel static tracepoints | Predefined kernel events | Syscalls, scheduler events, disk I/O |
| `PERF_EVENT` | Performance monitoring | Hardware/software events | CPU cycles, cache misses, page faults |
| `SOCKET_FILTER` | Socket-level packet filtering | Raw sockets | Protocol-level filtering per socket |
| `CGROUP_SKB` | Cgroup-based network filtering | Ingress/egress of cgroup | Egress policy, bandwidth limits |
| `CGROUP_SOCK` | Cgroup socket creation hook | Socket creation | Control which ports can bind |
| `LSM` | Linux Security Module hooks | Security operations | SELinux/AppArmor-like policies |
| `SYSCALL` | Syscall tracing | Syscall entry/exit | Monitor all syscalls system-wide |
| `RAW_TRACEPOINT` | Optimized tracepoint attachment | Kernel events | Lower-level, less stable than TRACEPOINT |
| `RINGBUF` | Event buffering | In-kernel ringbuffer | Efficient event streaming to user space |

### Discovering Available Hook Points

#### 1. List All Kernel Tracepoints

Every kernel subsystem exposes static tracepoints. To see them:

```bash
# List all available tracepoints on your system
cat /sys/kernel/debug/tracing/available_events

# Examples:
#   syscalls:sys_enter_open
#   syscalls:sys_exit_open
#   sched:sched_switch
#   sched:sched_wakeup
#   vfs:vfs_read
#   vfs:vfs_write
#   ext4:ext4_sync_file_enter
#   net:net_dev_xmit
#   block:block_rq_issue

# Or query a specific subsystem:
cat /sys/kernel/debug/tracing/events/ | grep syscalls
```

#### 2. List All Kernel Functions for Kprobes

Any kernel function can be dynamically traced with kprobe/kretprobe:

```bash
# List all kernel functions available for kprobes:
grep -E '^[a-zA-Z_]' /proc/kallsyms | wc -l  # Count of available functions

# Search for specific functions:
grep "do_sys_openat2" /proc/kallsyms         # Check if available
grep "vfs_read" /proc/kallsyms

# Example functions commonly probed:
#   do_sys_openat2
#   do_sys_open
#   vfs_read
#   vfs_write
#   __alloc_skb
#   tcp_v4_connect
#   security_task_wait
```

#### 3. List All Cgroup Hooks

Cgroup-based filtering hooks depend on cgroup version:

```bash
cat /proc/cgroups                    # Check cgroup version
mount | grep cgroup                  # Mount points

# cgroup_skb hooks (v1 & v2):
#   - cgroup_skb/ingress
#   - cgroup_skb/egress

# cgroup_sock hooks (v2 only):
#   - cgroup_sock/inet_create
#   - cgroup_sock/inet4_bind
#   - cgroup_sock/inet6_bind
#   - cgroup_sock/inet_listen
```

#### 4. List Available LSM Hooks

Linux Security Modules provide security-related attachment points:

```bash
# Check LSM support:
cat /sys/kernel/security/lsm

# LSM eBPF hooks include:
#   - bprm_committing_creds
#   - bprm_committed_creds
#   - task_alloc
#   - task_free
#   - file_permission
#   - file_open
#   - inode_rename
#   - socket_bind
#   - socket_connect
#   - socket_sendmsg
#   - socket_recvmsg
```

### Resources to Learn All eBPF Capabilities

**Official Kernel Documentation:**
- [BPF and XDP Reference Guide](https://docs.kernel.org/bpf/) — authoritative kernel BPF documentation
- [Kernel Source: include/uapi/linux/bpf.h](https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tree/include/uapi/linux/bpf.h) — all program types and helper functions
- [Kernel Source: samples/bpf/](https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tree/samples/bpf) — in-tree eBPF examples

**Community Documentation:**
- [eBPF.io](https://ebpf.io/) — project homepage with learning resources
- [Cilium eBPF Documentation](https://docs.cilium.io/en/stable/bpf/) — high-quality tutorials and guides
- [Linux eBPF Reference](https://github.com/iovisor/bcc/tree/master/docs) — BCC framework docs

**Tools to Explore:**
```bash
# bpftool — official BPF introspection tool
sudo bpftool prog list              # Loaded programs
sudo bpftool map list               # Loaded maps
sudo bpftool prog show id <ID>      # Program details
sudo bpftool prog dump xlated id <ID>  # Disassemble BPF bytecode

# bpf-gdb / bpftrace — tracing frameworks
bpftrace -l 'tracepoint:syscalls:*'  # List all syscall tracepoints
bpftrace -l 'kprobe:*'               # List all kernel functions
bpftrace -l 'uprobe:*'               # List all user-space functions

# llvm-readelf — inspect BPF objects
llvm-readelf -S program.bpf.o        # Check sections, BTF, CO-RE
```

### How to Use Any eBPF Capability: General Pattern

Regardless of the specific eBPF program type, the general flow is always:

1. **Discover** — Find the hook point (tracepoint name, function name, etc.)
   ```bash
   # Examples
   cat /sys/kernel/debug/tracing/available_events | grep syscalls
   grep "my_function" /proc/kallsyms
   ```

2. **Write kernel program** (`.bpf.c`)
   ```c
   #include <bpf/bpf_helpers.h>
   #include <bpf/vmlinux.h>
   
   SEC("kprobe/do_sys_openat2")
   int trace_open(struct pt_regs *ctx) {
       // Your logic here
       return 0;
   }
   ```

3. **Compile** with clang for BPF target
   ```bash
   clang -O2 -target bpf -c program.bpf.c -o program.bpf.o
   ```

4. **Load and attach** via libbpf (user space)
   ```c
   struct bpf_object *obj = bpf_object__open("program.bpf.o");
   bpf_object__load(obj);
   struct bpf_program *prog = bpf_object__find_program_by_name(obj, "trace_open");
   bpf_program__attach_kprobe(prog, false, "do_sys_openat2");
   ```

5. **Read events** via ring buffer/perf buffer
   ```bash
   # In kernel: bpf_ringbuf_reserve() / bpf_perf_event_output()
   # In user space: ring_buffer__poll() or perf_buffer__poll()
   ```

### Finding Specific Capabilities: Examples

**Example 1: Trace all file opens**
```bash
# Discover tracepoint
grep "sys_enter_openat\|sys_exit_openat" /sys/kernel/debug/tracing/available_events

# Write: SEC("tracepoint/syscalls/sys_enter_openat")
# Example: See syscall_trace.bpf.c in this repo
```

**Example 2: Monitor TCP connections**
```bash
# Discover function
grep "tcp_v4_connect\|inet_csk_get_port" /proc/kallsyms

# Write: SEC("kprobe/tcp_v4_connect")
# Attach: bpf_program__attach_kprobe(prog, false, "tcp_v4_connect")
```

**Example 3: Enforce egress policy**
```bash
# Discover cgroup capability
cat /proc/cgroups | grep cgroup2

# Write: SEC("cgroup_skb/egress")
# Attach: bpf_program__attach_cgroup(prog, cgroup_fd)
```

### Useful Commands Cheat Sheet

```bash
# List all loaded eBPF programs
sudo bpftool prog list

# Show a specific program's code (disassembled BPF)
sudo bpftool prog dump xlated id <ID>

# List all available tracepoints on this kernel
cat /sys/kernel/debug/tracing/available_events

# List all kernel functions for kprobe/kretprobe
wc -l /proc/kallsyms

# Check eBPF helper functions available on this kernel
grep "BPF_.*HELPER" /sys/kernel/debug/bpf

# Compile a BPF program for your kernel
clang -O2 -target bpf -c my_program.bpf.c -o my_program.bpf.o

# Check BTF (type metadata) on your kernel
bpftool btf dump file /sys/kernel/btf/vmlinux format c | head -50
```

## References

- [libbpf Documentation](https://github.com/libbpf/libbpf)
- [BPF and XDP Reference Guide](https://cilium.io/blog/2020/02/17/cilium-wireguard/)
- [eBPF.io](https://ebpf.io/)
- [Linux Kernel Documentation](https://www.kernel.org/doc/html/latest/bpf/index.html)

## License

MIT - See LICENSE file for details

## Contributing

Feel free to submit issues and pull requests!
