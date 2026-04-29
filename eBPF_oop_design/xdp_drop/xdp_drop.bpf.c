// SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause
/*
 * XDP packet handling eBPF program used by the OOP design.
 */

#include "vmlinux.h"
#include <bpf/bpf_helpers.h>

struct xdp_drop_event {
    __u64 ts_ns;
    __u32 pkt_len;
    __u32 ifindex;
    __u8 action;
    __u8 pad[3];
};

struct {
    __uint(type, BPF_MAP_TYPE_ARRAY);
    __uint(max_entries, 1);
    __type(key, __u32);
    __type(value, __u64);
} pkt_count SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_RINGBUF);
    __uint(max_entries, 1 << 20);
} events SEC(".maps");

SEC("xdp")
int xdp_pass(struct xdp_md *ctx)
{
    __u32 key = 0;
    __u64 *count;
    struct xdp_drop_event *event;

    count = bpf_map_lookup_elem(&pkt_count, &key);
    if (count)
        __sync_fetch_and_add(count, 1);

    event = bpf_ringbuf_reserve(&events, sizeof(*event), 0);
    if (event) {
        event->ts_ns = bpf_ktime_get_ns();
        event->pkt_len = ctx->data_end - ctx->data;
        event->ifindex = ctx->ingress_ifindex;
        event->action = 1; /* PASS */
        bpf_ringbuf_submit(event, 0);
    }

    return XDP_PASS;
}

char LICENSE[] SEC("license") = "GPL";
