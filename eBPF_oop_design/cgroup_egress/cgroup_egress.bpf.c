// SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause
/*
 * cgroup_skb egress filter.
 *
 * Deny ICMP egress packets and allow the rest.
 */

#include "vmlinux.h"
#include <bpf/bpf_helpers.h>

#define IPPROTO_ICMP 1

struct cgroup_egress_event {
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
} egress_stats SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_RINGBUF);
    __uint(max_entries, 1 << 20);
} events SEC(".maps");

SEC("cgroup_skb/egress")
int deny_icmp_egress(struct __sk_buff *skb)
{
    __u8 ip_proto = 0;
    __u32 allow_key = 0;
    __u32 deny_key = 1;
    __u8 action = 1;
    __u64 *allow_count;
    __u64 *deny_count;
    struct cgroup_egress_event *event;

    /* cgroup_skb sees network-layer data, so IPv4 protocol is at offset 9. */
    if (bpf_skb_load_bytes(skb, 9, &ip_proto, sizeof(ip_proto)) < 0)
        return 1;

    if (ip_proto == IPPROTO_ICMP) {
        deny_count = bpf_map_lookup_elem(&egress_stats, &deny_key);
        if (deny_count)
            __sync_fetch_and_add(deny_count, 1);
        action = 0;
    } else {
        allow_count = bpf_map_lookup_elem(&egress_stats, &allow_key);
        if (allow_count)
            __sync_fetch_and_add(allow_count, 1);
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

    return action;
}

char LICENSE[] SEC("license") = "GPL";
