// SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause
/*
 * Socket filter eBPF program.
 *
 * Allow TCP packets and drop others at the socket filter hook.
 */

#include "vmlinux.h"
#include <bpf/bpf_helpers.h>

#define IPPROTO_TCP 6

struct socket_filter_event {
    __u64 ts_ns;
    __u32 pkt_len;
    __u32 ifindex;
    __u8 ip_proto;
    __u8 action;
    __u8 pad[2];
};

struct {
    __uint(type, BPF_MAP_TYPE_ARRAY);
    __uint(max_entries, 2);
    __type(key, __u32);
    __type(value, __u64);
} filter_stats SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_RINGBUF);
    __uint(max_entries, 1 << 20);
} events SEC(".maps");

SEC("socket")
int filter_tcp_only(struct __sk_buff *skb)
{
    __u8 ip_proto = 0;
    __u32 pass_key = 0;
    __u32 drop_key = 1;
    __u8 action = 0;
    __u64 *pass_count;
    __u64 *drop_count;
    struct socket_filter_event *event;

    /* Ethernet(14) + IPv4 protocol byte offset(9) */
    if (bpf_skb_load_bytes(skb, 23, &ip_proto, sizeof(ip_proto)) < 0)
        return 0;

    if (ip_proto == IPPROTO_TCP) {
        pass_count = bpf_map_lookup_elem(&filter_stats, &pass_key);
        if (pass_count)
            __sync_fetch_and_add(pass_count, 1);
        action = 1;
    } else {
        drop_count = bpf_map_lookup_elem(&filter_stats, &drop_key);
        if (drop_count)
            __sync_fetch_and_add(drop_count, 1);
    }

    event = bpf_ringbuf_reserve(&events, sizeof(*event), 0);
    if (event) {
        event->ts_ns = bpf_ktime_get_ns();
        event->pkt_len = skb->len;
        event->ifindex = skb->ifindex;
        event->ip_proto = ip_proto;
        event->action = action;
        bpf_ringbuf_submit(event, 0);
    }

    return action ? skb->len : 0;
}

char LICENSE[] SEC("license") = "GPL";
