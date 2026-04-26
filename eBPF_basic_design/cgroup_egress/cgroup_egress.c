// SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause
/*
 * User-space loader for cgroup egress eBPF filter.
 */

#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <bpf/libbpf.h>
#include "logging/log_utils.h"

static volatile sig_atomic_t exiting;

struct cgroup_egress_event {
    __u64 ts_ns;
    __u32 pkt_len;
    __u32 ifindex;
    __u8 ip_proto;
    __u8 action;
    __u8 pad[2];
};


static void sig_handler(int sig)
{
    (void)sig;
    exiting = 1;
}

static int libbpf_print_fn(enum libbpf_print_level level, const char *format, va_list args)
{
    return vfprintf(stderr, format, args);
}

static int handle_event(void *ctx, void *data, size_t data_sz)
{
    struct logutil_async_writer *writer = ctx;
    const struct cgroup_egress_event *event = data;
    char line[LOGUTIL_LINE_SIZE];

    if (data_sz < sizeof(*event))
        return 0;

    snprintf(line, sizeof(line),
             "ts_ns=%llu ifindex=%u pkt_len=%u proto=%u action=%s\n",
             (unsigned long long)event->ts_ns,
             event->ifindex,
             event->pkt_len,
             event->ip_proto,
             event->action ? "ALLOW" : "DENY");
    logutil_writer_enqueue(writer, line);
    return 0;
}

int main(int argc, char *argv[])
{
    const char *cgroup_path = argc > 1 ? argv[1] : "/sys/fs/cgroup";
    const char *obj_candidates[] = {
        "build/cgroup_egress.bpf.o",
        "/opt/ebpf_basic_design/cgroup_egress.bpf.o",
        "./cgroup_egress.bpf.o",
    };
    const char *log_candidates[] = {
        "/var/log/ebpf_basic_design/cgroup_egress.events.log",
        "/opt/ebpf_basic_design/cgroup_egress.events.log",
        "./cgroup_egress.events.log",
    };
    const char *obj_path = NULL;
    char log_path[256];
    struct bpf_object *obj = NULL;
    struct bpf_program *prog;
    struct bpf_link *link = NULL;
    struct ring_buffer *rb = NULL;
    struct logutil_async_writer writer = {0};
    int cgroup_fd = -1;
    int map_fd = -1;
    int err = 0;
    int i;

    libbpf_set_print(libbpf_print_fn);
    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);

    if (logutil_ensure_default_dirs())
        fprintf(stderr, "Warning: failed to prepare /var/log/ebpf_basic_design, will try fallback paths\n");

    if (logutil_pick_writable_path(log_candidates,
                                   sizeof(log_candidates) / sizeof(log_candidates[0]),
                                   log_path, sizeof(log_path))) {
        fprintf(stderr, "Error: no writable log file path found\n");
        return 1;
    }

    if (logutil_writer_start(&writer, log_path)) {
        fprintf(stderr, "Error: failed to start async writer for %s\n", log_path);
        return 1;
    }

    for (i = 0; i < (int)(sizeof(obj_candidates) / sizeof(obj_candidates[0])); i++) {
        if (access(obj_candidates[i], R_OK) == 0) {
            obj_path = obj_candidates[i];
            break;
        }
    }

    if (!obj_path) {
        fprintf(stderr, "Error: cgroup_egress.bpf.o not found\n");
        err = 1;
        goto cleanup;
    }

    cgroup_fd = open(cgroup_path, O_RDONLY | O_DIRECTORY);
    if (cgroup_fd < 0) {
        perror("Error: failed to open cgroup path");
        err = 1;
        goto cleanup;
    }

    obj = bpf_object__open_file(obj_path, NULL);
    if (libbpf_get_error(obj)) {
        fprintf(stderr, "Error: failed to open %s\n", obj_path);
        obj = NULL;
        err = 1;
        goto cleanup;
    }

    if (bpf_object__load(obj)) {
        fprintf(stderr, "Error: failed to load eBPF object\n");
        err = 1;
        goto cleanup;
    }

    prog = bpf_object__find_program_by_name(obj, "deny_icmp_egress");
    if (!prog) {
        fprintf(stderr, "Error: egress program not found\n");
        err = 1;
        goto cleanup;
    }

    link = bpf_program__attach_cgroup(prog, cgroup_fd);
    if (libbpf_get_error(link)) {
        link = NULL;
        fprintf(stderr, "Error: failed to attach cgroup egress program\n");
        err = 1;
        goto cleanup;
    }

    map_fd = bpf_object__find_map_fd_by_name(obj, "events");
    if (map_fd < 0) {
        fprintf(stderr, "Error: failed to find events map\n");
        err = 1;
        goto cleanup;
    }

    rb = ring_buffer__new(map_fd, handle_event, &writer, NULL);
    if (!rb) {
        fprintf(stderr, "Error: failed to create ring buffer\n");
        err = 1;
        goto cleanup;
    }

    printf("cgroup egress filter loaded on %s (deny ICMP).\n", cgroup_path);
    printf("Writing events asynchronously to %s\n", log_path);
    printf("Press Ctrl+C to exit.\n");
    while (!exiting) {
        int poll_ret = ring_buffer__poll(rb, 200);
        if (poll_ret < 0 && poll_ret != -EINTR) {
            fprintf(stderr, "Error: ring buffer poll failed: %d\n", poll_ret);
            err = 1;
            break;
        }
    }

cleanup:
    ring_buffer__free(rb);
    bpf_link__destroy(link);
    if (cgroup_fd >= 0)
        close(cgroup_fd);
    bpf_object__close(obj);
    logutil_writer_stop(&writer);
    return err;
}
