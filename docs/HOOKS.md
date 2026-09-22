# eBPF Hook Types

This guide compares the main eBPF hook families, what they can observe, and which one to choose. A hook is useful only when the target kernel can load the program **and** the requested symbol, tracepoint, cgroup, or device can accept the attach.

## Hook overview

| Hook | Attach location | Best for | Important limitation |
|---|---|---|---|
| XDP | Network driver receive path | Earliest packet filtering, DDoS mitigation, rate limiting | Sees packet bytes, not HTTPS plaintext; native mode depends on the NIC driver |
| TC | `skb` ingress/egress path | Packet policy with richer metadata and both directions | Requires TC/qdisc infrastructure and still does not see encrypted payload |
| cgroup skb/socket | Cgroup network boundary or socket operations | Container or service-level policy | Requires `CONFIG_CGROUP_BPF`; cgroup v2 is usually simplest |
| sockops | TCP socket state events | Socket state, 4-tuples, sockmap acceleration | Does not expose packet contents |
| socket filter | Packets delivered to one socket | Per-socket filtering | Cannot observe other sockets |
| kprobe/kretprobe | Kernel function entry/return | Functions without a suitable tracepoint | Tied to implementation details; symbols may be inlined or blacklisted |
| tracepoint | Stable kernel event | Syscalls and kernel event monitoring | Only events compiled into the target kernel exist |
| raw tracepoint | Raw tracepoint arguments | Lower-overhead tracing | Less stable argument interface than a formatted tracepoint |
| uprobe/uretprobe | User-space function entry/return | Application and library instrumentation | Symbols and binary paths change; static Go binaries need different symbols |
| fentry/fexit | BPF trampoline at a kernel function | High-frequency typed tracing | Requires BTF and architecture trampoline support |
| BPF LSM | Linux Security Module decision point | Enforcement and sensitive-file protection | Requires `CONFIG_BPF_LSM`, active `bpf` LSM, BTF, and trampoline support |
| perf event | Hardware/software performance event | Profiling and sampling | Event availability and permissions vary |

## Choosing between common hooks

### XDP versus TC

Choose XDP when the decision must happen as early as possible and raw packet parsing is sufficient. Choose TC when egress matters or the program needs `sk_buff` metadata. XDP has native (`xdpdrv`), generic (`xdpgeneric`), and hardware offload (`xdpoffload`) modes; test the mode explicitly because a driver may reject native or offload attachment.

### kprobe, tracepoint, and fentry/fexit

Prefer a tracepoint when one exists because its event layout is a maintained interface. Use a kprobe for implementation functions that have no tracepoint, accepting that kernel refactoring can break it. Use fentry/fexit for lower-overhead typed tracing only after confirming target BTF and trampoline support.

### uprobe versus kprobe

A uprobe can observe application arguments at functions such as OpenSSL `SSL_write` and `SSL_read`, where plaintext exists before encryption or after decryption. A kprobe observes kernel-side activity such as `tcp_v4_connect`, but not application plaintext. A Go program commonly uses its own statically linked `crypto/tls` implementation, so an OpenSSL uprobe may never trigger.

## HTTPS monitoring decisions

| Requirement | Candidate hook | Reason |
|---|---|---|
| Who connects to whom, traffic rate, and packet volume | XDP or TC | Network headers are enough; XDP is earliest |
| Associate a connection with a process or cgroup | sockops plus a connection kprobe or tracepoint | Combines socket state with process/cgroup context |
| Observe ClientHello metadata such as SNI or cipher suites | XDP or TC | The relevant handshake data may be visible before encryption, subject to protocol and packet parsing |
| Observe HTTP URL, headers, or body plaintext | uprobe on the TLS library/application | The plaintext exists at the user-space encryption boundary |
| Prevent access to a TLS private key | BPF LSM at a file hook | This is an access-control decision, not packet inspection |
| Observe QUIC/HTTP-3 | XDP or TC on UDP traffic | QUIC has its own handshake and packet format |

A uprobe and XDP solve different timing problems. A uprobe can inspect application data after the application reaches the TLS boundary, while XDP can synchronously return `XDP_DROP` before the packet enters the normal stack. Neither replaces the other for policy and observability.

## Platform constraints

Kernel version is an orientation point, not a capability contract. Confirm the actual target with `bpftool feature probe` and a disposable attach test. Approximate introduction versions and dependencies are useful for triage:

| Feature | Approximate minimum | Key dependencies |
|---|---:|---|
| Socket filter | 3.19 | `CONFIG_BPF_SYSCALL` |
| kprobe/kretprobe | 4.1 | `CONFIG_KPROBES`, `CONFIG_KPROBE_EVENTS`, `CONFIG_BPF_EVENTS` |
| TC `cls_bpf` | 4.1 | `CONFIG_NET_CLS_BPF`, `CONFIG_NET_ACT_BPF`, suitable qdisc |
| uprobe/uretprobe | 4.x | `CONFIG_UPROBES`, `CONFIG_UPROBE_EVENTS`, resolvable symbol |
| Tracepoint | 4.7 | `CONFIG_BPF_EVENTS`, `CONFIG_FTRACE`; syscall events also need `CONFIG_FTRACE_SYSCALLS` |
| XDP | 4.8 | `CONFIG_NET_XDP`; native mode requires driver support |
| perf event | 4.9 | `CONFIG_PERF_EVENTS` |
| cgroup skb/socket | 4.10 | `CONFIG_CGROUP_BPF` |
| sockops | 4.13 | `CONFIG_CGROUP_BPF` |
| raw tracepoint | 4.17 | Tracepoint infrastructure |
| BTF/CO-RE | 5.2 | `CONFIG_DEBUG_INFO_BTF` |
| fentry/fexit | 5.5 | BTF and architecture trampoline |
| BPF LSM | 5.7 | `CONFIG_BPF_LSM`, `CONFIG_SECURITY`, active `bpf` LSM, trampoline |
| Ring buffer and `CAP_BPF` | 5.8 | Kernel support and permission policy |

On ARM32 targets such as AST2600, kprobes, tracepoints, uprobes, XDP, TC, cgroup hooks, and socket filters can work when configured, but fentry/fexit and BPF LSM are not attachable without BPF trampoline support. JIT support and trampoline support are separate capabilities.

For the exact target checklist and `bpftool` commands, see [BPFTOOL.md](BPFTOOL.md).
