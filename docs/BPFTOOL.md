# BPF Capability Checks with bpftool

Kernel version is a useful clue, but it is not a capability contract. Probe the target kernel, configuration, BTF, hook inventory, permissions, and device driver. The target QEMU image may not ship `bpftool`; run these commands on a host with access to the target kernel, in a bridged/TAP environment, or use the daemon's attach log.

## Capability probe

```sh
sudo bpftool feature probe kernel
sudo bpftool feature probe kernel | grep program_type
sudo bpftool feature probe dev eth0
```

`bpftool feature probe` attempts minimal loads and reports program types, map types, helpers, and detected configuration. It primarily proves **load** capability. It does not prove that a particular symbol, tracepoint, cgroup, or NIC driver accepts an attach.

A successful load is not a successful attach. Always validate the real hook on the target.

## Inspect loaded state

```sh
sudo bpftool prog show
sudo bpftool link show
sudo bpftool map show
sudo bpftool net show
sudo bpftool btf list
sudo bpftool btf dump file /sys/kernel/btf/vmlinux format c >/tmp/vmlinux.h
```

For a loaded program, inspect verifier-accepted instructions with:

```sh
sudo bpftool prog dump xlated id ID
sudo bpftool prog dump jited id ID
```

The JIT dump can fail on an architecture without the relevant JIT even when load and attach succeeded. `bpftool net show` and `ip -details link show dev eth0` help distinguish XDP mode and existing ownership.

## Check the layers before attaching

A hook is usable only when all five layers pass:

| Layer | What to ask | Useful check |
|---|---|---|
| Kernel version | Was this program or attach type introduced? | `uname -r`, then feature probe |
| Kernel configuration | Is the required `CONFIG_*` enabled? | `/proc/config.gz`, `/boot/config-$(uname -r)`, or build config |
| CPU architecture | Does the architecture provide the required JIT or trampoline? | `uname -m`, config, and attach test |
| Runtime environment | Are tracefs, bpffs, LSM, cgroups, and permissions ready? | Mounts, sysctls, `/sys/kernel/security/lsm`, capabilities |
| Device or symbol | Does the requested event, function, process, or NIC exist? | Tracepoint files, `/proc/kallsyms`, `ethtool`, and attach test |

Useful paths:

| Path or command | Meaning |
|---|---|
| `/sys/kernel/btf/vmlinux` | Kernel BTF; needed by many CO-RE, fentry, and LSM workflows |
| `/sys/kernel/security/lsm` | Active LSM list; BPF LSM needs `bpf` |
| `/sys/kernel/tracing/available_events` | Exported tracepoints |
| `/sys/kernel/tracing/events/<subsystem>/<event>/format` | Fields for one tracepoint |
| `/sys/kernel/tracing/available_filter_functions` | Functions exposed through ftrace-based attachment |
| `/proc/kallsyms` | Candidate kprobe symbols |
| `/sys/kernel/debug/kprobes/blacklist` | Symbols that cannot accept kprobes |
| `/sys/fs/bpf/` | bpffs for pinned programs, maps, and links |
| `/proc/sys/net/core/bpf_jit_enable` | JIT state; `0` means interpreter-only execution |
| `/proc/sys/kernel/unprivileged_bpf_disabled` | Restriction on unprivileged BPF |
| `stat -fc %T /sys/fs/cgroup` | `cgroup2fs` indicates cgroup v2 |

Embedded images may need mounts, subject to target policy:

```sh
mount -t tracefs nodev /sys/kernel/tracing 2>/dev/null || true
mount -t debugfs nodev /sys/kernel/debug 2>/dev/null || true
mount -t bpf bpf /sys/fs/bpf 2>/dev/null || true
```

## Enumerate and test hooks

```sh
tracing=/sys/kernel/tracing
[ -r "$tracing/available_events" ] || tracing=/sys/kernel/debug/tracing
cat "$tracing/available_events"
grep -E '^(xdp|net|tcp|sock|syscalls):' "$tracing/available_events"
find "$tracing/events" -maxdepth 3 -type f -name format -print 2>/dev/null
```

With `bpftrace` or `perf` installed:

```sh
sudo bpftrace -l 'tracepoint:*'
sudo bpftrace -l 'tracepoint:syscalls:*'
sudo bpftrace -lv 'tracepoint:syscalls:sys_enter_openat'
sudo perf list tracepoint
```

Use disposable programs and a maintenance window for attach tests:

```sh
sudo bpftrace -e 'kprobe:tcp_v4_connect { exit(); }'
sudo bpftrace -e 'tracepoint:syscalls:sys_enter_openat { exit(); }'
sudo bpftrace -e 'uprobe:/usr/lib/libssl.so.3:SSL_write { exit(); }'
sudo bpftrace -e 'fentry:tcp_v4_connect { exit(); }'
sudo bpftrace -e 'lsm:file_open { exit(); }'
```

For XDP and TC, explicitly test and roll back each mode:

```sh
sudo ip link set dev eth0 xdpdrv obj prog.o sec xdp
sudo ip link set dev eth0 xdpgeneric obj prog.o sec xdp
sudo ip link set dev eth0 xdp off
sudo tc qdisc add dev eth0 clsact
sudo tc qdisc del dev eth0 clsact
```

`xdpdrv` is native driver mode, `xdpgeneric` is the compatibility fallback, and `xdpoffload` requires hardware and firmware support. Use `ethtool -i eth0` to record the driver.

## Diagnose load versus attach failures

Keep the full daemon error and errno:

```sh
journalctl -u https-guard-daemon -b --no-pager | grep -iE 'bpf|libbpf|xdp|uprobe|lsm|errno|attach|verif'
uname -a
dmesg | tail -100
```

Typical meanings are:

- `EPERM`: privilege, lockdown policy, or unprivileged-BPF restriction.
- `EINVAL`: invalid attributes, context access, attach arguments, or mode mismatch.
- `E2BIG`: verifier instruction, map, or log limits.
- `ENOTSUPP`/`EOPNOTSUPP`: unsupported program type, attach mode, driver, or trampoline.
- `EBUSY`: another XDP program or incompatible link owns the device.
- `ENOENT`/`ENODEV`: missing symbol, tracepoint, process, or device.

For verifier detail, enable libbpf's `libbpf_set_print()` callback in a diagnostic build and provide a sufficiently large `BPF_PROG_LOAD` log buffer. A generic libbpf summary is not the verifier explanation. Compare native XDP failure with generic success to identify a driver or netdev problem.

## Capability snapshot

Save a snapshot with the kernel release when bringing up a new board:

```sh
#!/bin/sh
printf '== kernel/arch ==\n'; uname -r; uname -m
printf '== BTF ==\n'; test -r /sys/kernel/btf/vmlinux && echo present || echo missing
printf '== active LSM ==\n'; cat /sys/kernel/security/lsm 2>/dev/null || true
printf '== JIT ==\n'; cat /proc/sys/net/core/bpf_jit_enable 2>/dev/null || true
printf '== tracefs ==\n'; test -r /sys/kernel/tracing/available_events && echo present || echo missing
printf '== cgroup ==\n'; stat -fc %T /sys/fs/cgroup
printf '== config ==\n'
zcat /proc/config.gz 2>/dev/null | grep -E 'CONFIG_(BPF_SYSCALL|BPF_JIT|BPF_EVENTS|KPROBES|UPROBES|DEBUG_INFO_BTF|BPF_LSM|NET_CLS_BPF|NET_SCH_INGRESS|CGROUP_BPF|FTRACE_SYSCALLS)=' || true
printf '== program types ==\n'
bpftool feature probe kernel 2>/dev/null | grep program_type || true
```

Record kernel release, architecture, BTF, active LSMs, JIT state, program types, required tracepoints, and the XDP result for every interface. This creates a comparable capability set instead of relying on a version string.
