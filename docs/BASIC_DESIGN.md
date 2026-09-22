# eBPF Examples in This Repository

This directory contains the kernel-side eBPF programs, the user-space loaders that open, verify, load, and attach them, and the CO-RE metadata needed to compile them against kernel BTF.

## What eBPF Does

eBPF lets you run sandboxed programs inside the Linux kernel without writing a full kernel module. In practice, that means you can add logic at specific hook points such as:

- network ingress and egress paths
- syscall and kernel tracepoints
- packet processing pipelines
- cgroup, socket, and security hooks

The kernel verifies each eBPF program before loading it. That verifier checks safety properties such as bounded memory access, valid control flow, and restricted helper usage. Once accepted, the program can observe or influence kernel events with much lower overhead than user-space polling.

In this repository, eBPF is used for multiple concrete purposes:

- XDP packet handling: inspect packets very early in the networking path and make a fast decision such as drop or pass.
- syscall tracing: observe syscall events from the kernel and keep counters or emit debug output for analysis.
- socket-level filtering: allow only selected packet types for a raw socket.
- cgroup egress filtering: allow or deny packets as they leave a cgroup.

## CO-RE and BTF in This Repo

This repository now uses the standard libbpf CO-RE flow.

- BTF is the kernel's type metadata. It describes kernel structs, enums, typedefs, and field layouts.
- `vmlinux.h` is generated from `/sys/kernel/btf/vmlinux` with `bpftool`.
- the `.bpf.c` files include `vmlinux.h` instead of relying on hand-written type shims.
- when field access depends on kernel type layout, CO-RE relocation lets libbpf adjust the program for the target kernel at load time.

That gives you a much more standard eBPF development model than hand-maintaining local copies of kernel structs.

This does not mean one binary runs on every kernel with no constraints. It means the program is built from real kernel type metadata and can adapt to compatible layout differences through CO-RE relocation.

## Direct `vmlinux.h` Access and `BPF_CORE_READ`

There are two related ways to access types declared by `vmlinux.h`:

- Direct access is appropriate for the BPF program context supplied by a hook, such as `ctx->data_end` for `struct xdp_md` or `skb->len` for `struct __sk_buff`. These context fields are part of the BPF hook ABI and the verifier understands these accesses.
- Use `BPF_CORE_READ(ptr, field)` for ordinary kernel structs, nested fields, pointers, or fields whose layout can differ between kernel versions. The macro records a CO-RE relocation, and libbpf resolves the field offset using the target kernel's BTF when loading the program.
- Do not hand-copy a kernel struct into the source just to read one field. Include `vmlinux.h` and use the type it declares so the CO-RE metadata can describe the access.

In this repository, `syscall_trace/syscall_trace.bpf.c` uses `BPF_CORE_READ(args, id)` to read `id` from the `struct trace_event_raw_sys_exit` tracepoint context. That struct is declared in `vmlinux.h`; the syscall example uses the macro because it is a kernel tracepoint record rather than a BPF context such as `struct xdp_md`.

The event structs such as `struct syscall_trace_event` are local ABI structs owned by this example, not kernel structs from `vmlinux.h`. They can be accessed directly because both the BPF program and its user-space loader use the layout defined in this repository.

## How the Pieces Fit Together

- `vmlinux.h`: generated kernel type header for CO-RE builds.
- `xdp_drop/*.bpf.c`, `syscall_trace/*.bpf.c`, `socket_filter/*.bpf.c`, and `cgroup_egress/*.bpf.c`: the canonical kernel-side eBPF source code compiled for the BPF target and reused by the OOP and Boost.Asio designs.
- `xdp_drop/*.c`, `syscall_trace/*.c`, `socket_filter/*.c`, and `cgroup_egress/*.c`: user-space loaders built with libbpf.
- `build/*.bpf.o`: ELF objects containing BPF bytecode, BTF, and CO-RE relocation metadata.
- `build/*_loader`: executables that call libbpf to load and attach the programs.
- `logging/log_utils.*`: shared async event logging utilities used by all user-space loaders.

Typical flow:

1. `bpftool` generates `vmlinux.h` from kernel BTF.
2. clang compiles the `.bpf.c` source into BPF bytecode.
3. libbpf opens the ELF object and reads maps, programs, BTF, and CO-RE relocation records.
4. the kernel verifier validates the program.
5. the loader attaches the program to a hook such as XDP or a tracepoint.
6. you inspect behavior through logs, bpftool, or map contents.

## Layout

```text
eBPF_basic_design/
├── Makefile
├── README.md
├── vmlinux.h
├── xdp_drop/
│   ├── xdp_drop.bpf.c
│   └── xdp_drop.c
├── syscall_trace/
│   ├── syscall_trace.bpf.c
│   └── syscall_trace.c
├── socket_filter/
│   ├── socket_filter.bpf.c
│   └── socket_filter.c
├── logging/
│   ├── log_utils.h
│   └── log_utils.c
└── cgroup_egress/
    ├── cgroup_egress.bpf.c
    └── cgroup_egress.c
```

### vmlinux.h

This file is generated, not hand-written. It comes from kernel BTF and provides the kernel types used by the CO-RE examples.

### Shared kernel-side source

The `.bpf.c` files in this directory are the only copies of the four kernel-side programs in the repository. `eBPF_oop_design/CMakeLists.txt` and `eBPF_boost_asio_design/CMakeLists.txt` compile these same files while supplying their own user-space loaders and event-processing implementations. Keep kernel hook logic here so the three designs cannot drift apart.

### xdp_drop/xdp_drop.bpf.c

This is the kernel-side XDP example. XDP runs before the normal networking stack, so it is one of the fastest places in Linux to inspect or reject packets. This example is useful for understanding:

- packet-path hooks
- very low-latency filtering
- the difference between early packet handling and higher-level networking hooks

### syscall_trace/syscall_trace.bpf.c

This is the kernel-side tracing example. It attaches to syscall tracepoints and counts selected syscalls. This is useful for understanding:

- observability-oriented eBPF programs
- tracepoint attachment
- map-based state tracking inside the kernel
- CO-RE field reads from kernel-provided type metadata

### xdp_drop/xdp_drop.c and syscall_trace/syscall_trace.c

These are user-space loaders. Their main jobs are to:

- locate the compiled `.bpf.o` object
- call libbpf to open and load it
- attach the program to the intended hook
- keep the process alive while the eBPF program remains attached

## Folder Walkthrough

### xdp_drop/

This folder contains the XDP example.

- `xdp_drop/xdp_drop.bpf.c`: kernel-side XDP program
- `xdp_drop/xdp_drop.c`: libbpf-based loader that attaches the program to a network interface

What this example does:

- attaches at the XDP hook on a selected interface
- counts packets in the `pkt_count` array map
- currently returns `XDP_PASS`, so it observes traffic without dropping it

Build example:

```bash
make ebpf_basic
```

Host-side run example:

```bash
sudo ./scripts/run_eBPF_basic_design.sh xdp lo
```

Guest-side run example:

```sh
/opt/ebpf_basic_design/xdp_drop_loader lo
```

Debug ideas:

```bash
# Find the loaded XDP program
sudo bpftool prog list

# Inspect the packet counter map
sudo bpftool map list
sudo bpftool map dump name pkt_count
```

In the VM, the auto-start path writes loader output to `/var/log/ebpf_basic_design/xdp_loader.log`.
Event records are written asynchronously to `/var/log/ebpf_basic_design/xdp_drop.events.log`.

### syscall_trace/

This folder contains the syscall tracing example.

- `syscall_trace/syscall_trace.bpf.c`: kernel-side tracepoint program
- `syscall_trace/syscall_trace.c`: libbpf-based loader that opens, attaches, and consumes ring buffer events

What this example does:

- attaches to `sys_exit_openat`, `sys_exit_read`, and `sys_exit_write`
- reads the syscall id through `BPF_CORE_READ(ctx, id)`
- counts hits in the `syscall_counts` array map
- emits event records through a BPF ring buffer

This example writes event records asynchronously in user space, so you can inspect event history from log files without relying on `dmesg`.

Build example:

```bash
make ebpf_basic
```

Host-side run example:

```bash
sudo ./scripts/run_eBPF_basic_design.sh trace
```

Guest-side run example:

```sh
/opt/ebpf_basic_design/syscall_trace_loader
```

Debug ideas:

```bash
# Find loaded tracepoint programs
sudo bpftool prog list

# Inspect the syscall counter map
sudo bpftool map list
sudo bpftool map dump name syscall_counts
```

In the VM, the auto-start path writes loader output to `/var/log/ebpf_basic_design/syscall_trace_loader.log`.
Event records are written asynchronously to `/var/log/ebpf_basic_design/syscall_trace.events.log`.

### socket_filter/

This folder contains a socket filtering example.

- `socket_filter/socket_filter.bpf.c`: kernel-side socket filter program
- `socket_filter/socket_filter.c`: loader that attaches the program to a raw socket

What this example does:

- attaches at the socket filter hook for a selected interface
- allows TCP packets
- drops non-TCP packets for that socket
- emits pass/drop events through a BPF ring buffer

Build example:

```bash
make ebpf_basic
```

Host-side run example:

```bash
sudo ./eBPF_basic_design/build/socket_filter_loader lo
```

In the VM, the auto-start path writes loader output to `/var/log/ebpf_basic_design/socket_filter.log`.
Event records are written asynchronously to `/var/log/ebpf_basic_design/socket_filter.events.log`.

### cgroup_egress/

This folder contains a cgroup egress filtering example.

- `cgroup_egress/cgroup_egress.bpf.c`: kernel-side cgroup_skb egress program
- `cgroup_egress/cgroup_egress.c`: loader that attaches the program to a cgroup

What this example does:

- attaches to `cgroup_skb/egress`
- denies ICMP packets
- allows other traffic
- emits allow/deny events through a BPF ring buffer

Build example:

```bash
make ebpf_basic
```

Host-side run example:

```bash
sudo ./eBPF_basic_design/build/cgroup_egress_loader /sys/fs/cgroup
```

In the VM, the auto-start path writes loader output to `/var/log/ebpf_basic_design/cgroup_egress.log`.
Event records are written asynchronously to `/var/log/ebpf_basic_design/cgroup_egress.events.log`.

## Build, Run, Debug by Directory

### Build from `eBPF_basic_design/`

```bash
cd eBPF_basic_design
make clean all
```

This regenerates `vmlinux.h`, rebuilds all `.bpf.o` files, and relinks all loaders into `eBPF_basic_design/build/`.

### Run on the host

```bash
# XDP
sudo ./scripts/run_eBPF_basic_design.sh xdp lo

# Tracepoints
sudo ./scripts/run_eBPF_basic_design.sh trace

# Socket filter (TCP only)
sudo ./eBPF_basic_design/build/socket_filter_loader lo

# cgroup egress filter (deny ICMP)
sudo ./eBPF_basic_design/build/cgroup_egress_loader /sys/fs/cgroup
```

### Run in the guest

```bash
make qemu
```

Inside the VM:

```sh
# filter loader log
tail -f /var/log/ebpf_basic_design/xdp_loader.log
tail -f /var/log/ebpf_basic_design/syscall_trace_loader.log
tail -f /var/log/ebpf_basic_design/socket_filter.log
tail -f /var/log/ebpf_basic_design/cgroup_egress.log

# event streams (ring buffer -> async file writer)
tail -f /var/log/ebpf_basic_design/xdp_drop.events.log
tail -f /var/log/ebpf_basic_design/syscall_trace.events.log
tail -f /var/log/ebpf_basic_design/socket_filter.events.log
tail -f /var/log/ebpf_basic_design/cgroup_egress.events.log
```

If you want to start them manually instead of using `guest/init`:

```sh
/opt/ebpf_basic_design/xdp_drop_loader lo
/opt/ebpf_basic_design/syscall_trace_loader
/opt/ebpf_basic_design/socket_filter_loader lo
/opt/ebpf_basic_design/cgroup_egress_loader /sys/fs/cgroup
```

### Triggering the Filters: What Actions Generate Events

Each filter monitors specific kernel operations and emits events when triggered. Here's how to generate activity for each:

#### XDP Drop Filter

**What it monitors:** Packet arrival on the network interface.

**How to trigger:**

```sh
# Terminal 1: Watch events
tail -f /var/log/ebpf_basic_design/xdp_drop.events.log

# Terminal 2: Generate traffic on the monitored interface.
# In this VM, guest/init auto-attaches XDP to the first non-loopback IFACE,
# so prefer that interface instead of lo.
IFACE=$(ls /sys/class/net | grep -Ev '^(lo|sit[0-9]*)$' | head -1)
ping -I "$IFACE" -c 5 1.1.1.1 2>/dev/null || true
# or
ping -c 5 1.1.1.1 2>/dev/null || true
# or
wget -O /dev/null http://example.com 2>/dev/null || true
```

**Expected output:** Each packet generates an event showing `src=..., dst=..., sport=..., dport=..., pkt_count=...`

---

#### Syscall Trace Filter

**What it monitors:** Syscall exits for `openat`, `read`, and `write` operations.

**How to trigger:**

```sh
# Terminal 1: Watch events
tail -f /var/log/ebpf_basic_design/syscall_trace.events.log

# Terminal 2: Run file operations
# Generates 'openat' syscalls:
ls /
ls /etc
cat /etc/fstab

# Generates 'read' syscalls:
head /etc/fstab
wc -l /etc/fstab

# Generates 'write' syscalls:
echo "hello" > /tmp/test.txt
cp /etc/fstab /tmp/copy.txt
```

**Expected output:** Each syscall generates an event showing `syscall_name=sys_exit_openat/sys_exit_read/sys_exit_write, count=...`

---

#### Socket Filter

**What it monitors:** Packets flowing through a raw socket on the monitored interface. **Allows TCP packets**, **drops non-TCP** packets.

**How to trigger:**

```sh
# Terminal 1: Watch events
tail -f /var/log/ebpf_basic_design/socket_filter.events.log

# Terminal 2: Generate TCP traffic (will see "allow" events)
wget -O /dev/null http://example.com 2>/dev/null || true
nc -w 2 8.8.8.8 53 < /dev/null 2>&1 || true

# Or generate ICMP traffic (will see "drop" events)
ping -c 2 127.0.0.1 2>&1 || true
```

**Expected output:** Each packet generates an event showing `action=allow/drop, src=..., dst=..., protocol=TCP/ICMP`

**Note:** Requires actual network traffic to flow through the interface. In a VM with limited connectivity, use local traffic on `lo` or generate traffic between multiple interfaces.

---

#### Cgroup Egress Filter

**What it monitors:** Egress (outbound) traffic from a cgroup. **Denies ICMP packets**, **allows all other traffic**.

**How to trigger:**

```sh
# Terminal 1: Watch events
tail -f /var/log/ebpf_basic_design/cgroup_egress.events.log

# Terminal 2: Generate egress traffic
# This will be DENIED (ICMP):
ping -c 2 127.0.0.1 2>&1 || true

# These will be ALLOWED (TCP/HTTP):
wget -O /dev/null http://example.com 2>&1 || true
nc -w 2 8.8.8.8 53 < /dev/null 2>&1 || true
echo "data" > /tmp/outbound.txt

# DNS queries (UDP port 53):
nslookup example.com 2>&1 || true
```

**Expected output:** Each packet generates an event showing `action=deny/allow, src=..., dst=..., protocol=...`

**Note:** ICMP will be denied and dropped (no response). If the cgroup egress filter is active, `ping` failure is expected behavior, not a loader bug.
Use `wget`/`nc` to verify non-ICMP traffic paths.

---

#### Comprehensive Multi-Filter Test

Run all filters simultaneously and trigger them all:

```sh
# Terminal 1: Start loaders (if not using guest/init)
/opt/ebpf_basic_design/xdp_drop_loader lo &
/opt/ebpf_basic_design/syscall_trace_loader &
/opt/ebpf_basic_design/socket_filter_loader lo &
/opt/ebpf_basic_design/cgroup_egress_loader /cgroup2 &

# Terminal 2: Watch all event logs (use separate terminals for clarity)
tail -f /var/log/ebpf_basic_design/*.events.log

# Terminal 3: Generate mixed traffic
# File operations (triggers syscall_trace)
ls / && cat /etc/fstab && echo "test" > /tmp/t.txt

# Network activity (triggers xdp_drop and socket_filter)
ping -c 1 127.0.0.1 2>&1 || true

# More file operations
find /etc -name "*.conf" 2>/dev/null | head -3

# Curl for TCP (triggers all network filters)
curl http://example.com >/dev/null 2>&1 || true
```

**Observing the event streams:**
- Syscall events should appear within 100ms of file operations
- Network events should appear for each packet on the interface
- Cgroup egress events should show deny for ICMP and allow for TCP/UDP

### Debug checklist

```bash
# Rebuild from scratch
make -C eBPF_basic_design clean all

# Inspect BTF and CO-RE sections
llvm-readelf -S eBPF_basic_design/build/syscall_trace.bpf.o | grep -E '\.BTF|\.BTF\.ext'

# List all loaded programs
sudo bpftool prog list

# List all maps
sudo bpftool map list
```

If a loader fails in the VM, start with the per-example log files in `/var/log/ebpf_basic_design/`, then verify that `/opt/ebpf_basic_design/*.bpf.o` and `/opt/ebpf_basic_design/*_loader` are present.

## Learning Path

1. Start with `xdp_drop/xdp_drop.bpf.c` to see how eBPF can influence packet handling at the earliest receive path.
2. Move to `socket_filter/socket_filter.bpf.c` to understand socket-level filtering decisions.
3. Continue with `cgroup_egress/cgroup_egress.bpf.c` to see cgroup-based egress policy.
4. Read the loaders (`xdp_drop/*.c`, `syscall_trace/*.c`, `socket_filter/*.c`, `cgroup_egress/*.c`) to see how libbpf loads, verifies, and attaches programs from user space.
5. Inspect `vmlinux.h` and `BPF_CORE_READ(...)` usage to understand how CO-RE resolves type layouts.
6. Add a small map or counter of your own and rebuild with `make ebpf_basic`.

## Useful Commands

```bash
# Rebuild BPF objects and loaders
make ebpf_basic

# List loaded eBPF programs on the host
sudo bpftool prog list

# View one program in detail
sudo bpftool prog show id <ID>

# List BPF maps
sudo bpftool map list

# Dump map contents
sudo bpftool map dump name <MAP_NAME>

# Watch async event logs in the VM
tail -f /var/log/ebpf_basic_design/*.events.log

# Run the VM
make qemu-net
```

## Host vs Guest

This repository supports both host-side and guest-side usage:

- On the host, you can rebuild and load programs directly for quick iteration.
- In the guest, `make rootfs` packages the compiled objects and loaders into `/opt/ebpf_basic_design`, and `guest/init` tries to load them automatically at boot.

That split is useful because it lets you develop quickly on the host while still validating behavior in the custom QEMU kernel environment.