// SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause
/*
 * Syscall tracing eBPF program.
 *
 * Observe syscall events from the kernel and keep counters or emit debug output for analysis.
 */

#include "vmlinux.h"
#include <bpf/bpf_helpers.h>

struct syscall_trace_event {
    __u64 ts_ns;
    __u32 syscall_id;
    __u32 pid;
    __u32 tid;
    __u64 count;
};

struct {
    __uint(type, BPF_MAP_TYPE_ARRAY);
    __uint(max_entries, 1024);
    __type(key, __u32);
    __type(value, __u64);
} syscall_counts SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_RINGBUF);
    __uint(max_entries, 1 << 20);
} events SEC(".maps");

SEC("tracepoint/syscalls/sys_exit_openat")
int trace_sys_exit_openat(void *ctx)
{
    __u32 syscall_id;
    __u32 key;
    __u64 *count;
    struct syscall_trace_event *event;
    struct trace_event_raw_sys_exit *args = (struct trace_event_raw_sys_exit *)ctx;

    syscall_id = (__u32)args->id;
    key = syscall_id;

    count = bpf_map_lookup_elem(&syscall_counts, &key);
    if (count) {
        __sync_fetch_and_add(count, 1);
    } else {
        __u64 init_count = 1;
        bpf_map_update_elem(&syscall_counts, &key, &init_count, BPF_ANY);
    }

    event = bpf_ringbuf_reserve(&events, sizeof(*event), 0);
    if (event) {
        event->ts_ns = bpf_ktime_get_ns();
        event->syscall_id = syscall_id;
        event->pid = bpf_get_current_pid_tgid() >> 32;
        event->tid = bpf_get_current_pid_tgid() & 0xFFFFFFFF;
        event->count = count ? *count : 1;
        bpf_ringbuf_submit(event, 0);
    }

    return 0;
}

SEC("tracepoint/syscalls/sys_exit_read")
int trace_sys_exit_read(void *ctx)
{
    __u32 syscall_id;
    __u32 key;
    __u64 *count;
    struct syscall_trace_event *event;
    struct trace_event_raw_sys_exit *args = (struct trace_event_raw_sys_exit *)ctx;

    syscall_id = (__u32)args->id;
    key = syscall_id;

    count = bpf_map_lookup_elem(&syscall_counts, &key);
    if (count) {
        __sync_fetch_and_add(count, 1);
    } else {
        __u64 init_count = 1;
        bpf_map_update_elem(&syscall_counts, &key, &init_count, BPF_ANY);
    }

    event = bpf_ringbuf_reserve(&events, sizeof(*event), 0);
    if (event) {
        event->ts_ns = bpf_ktime_get_ns();
        event->syscall_id = syscall_id;
        event->pid = bpf_get_current_pid_tgid() >> 32;
        event->tid = bpf_get_current_pid_tgid() & 0xFFFFFFFF;
        event->count = count ? *count : 1;
        bpf_ringbuf_submit(event, 0);
    }

    return 0;
}

SEC("tracepoint/syscalls/sys_exit_write")
int trace_sys_exit_write(void *ctx)
{
    __u32 syscall_id;
    __u32 key;
    __u64 *count;
    struct syscall_trace_event *event;
    struct trace_event_raw_sys_exit *args = (struct trace_event_raw_sys_exit *)ctx;

    syscall_id = (__u32)args->id;
    key = syscall_id;

    count = bpf_map_lookup_elem(&syscall_counts, &key);
    if (count) {
        __sync_fetch_and_add(count, 1);
    } else {
        __u64 init_count = 1;
        bpf_map_update_elem(&syscall_counts, &key, &init_count, BPF_ANY);
    }

    event = bpf_ringbuf_reserve(&events, sizeof(*event), 0);
    if (event) {
        event->ts_ns = bpf_ktime_get_ns();
        event->syscall_id = syscall_id;
        event->pid = bpf_get_current_pid_tgid() >> 32;
        event->tid = bpf_get_current_pid_tgid() & 0xFFFFFFFF;
        event->count = count ? *count : 1;
        bpf_ringbuf_submit(event, 0);
    }

    return 0;
}

char LICENSE[] SEC("license") = "GPL";