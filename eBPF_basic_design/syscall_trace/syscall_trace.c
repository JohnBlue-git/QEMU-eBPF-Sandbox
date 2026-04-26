// SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause
/*
 * User-space loader for syscall tracing eBPF program.
 */

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <bpf/libbpf.h>
#include "logging/log_utils.h"

static volatile sig_atomic_t exiting;

struct syscall_trace_event {
    __u64 ts_ns;
    __u32 syscall_id;
    __u32 pid;
    __u32 tid;
    __u64 count;
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
    const struct syscall_trace_event *event = data;
    char line[LOGUTIL_LINE_SIZE];

    if (data_sz < sizeof(*event))
        return 0;

    snprintf(line, sizeof(line),
             "ts_ns=%llu syscall_id=%u pid=%u tid=%u count=%llu\n",
             (unsigned long long)event->ts_ns,
             event->syscall_id,
             event->pid,
             event->tid,
             (unsigned long long)event->count);
    logutil_writer_enqueue(writer, line);
    return 0;
}

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    const char *obj_candidates[] = {
        "build/syscall_trace.bpf.o",
        "/opt/ebpf_basic_design/syscall_trace.bpf.o",
        "./syscall_trace.bpf.o",
    };
    const char *log_candidates[] = {
        "/var/log/ebpf_basic_design/syscall_trace.events.log",
        "/opt/ebpf_basic_design/syscall_trace.events.log",
        "./syscall_trace.events.log",
    };
    const char *obj_path = NULL;
    char log_path[256];
    struct bpf_object *obj = NULL;
    struct bpf_program *prog_openat;
    struct bpf_program *prog_read;
    struct bpf_program *prog_write;
    struct ring_buffer *rb = NULL;
    struct logutil_async_writer writer = {0};
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
        fprintf(stderr, "Error: syscall_trace.bpf.o not found\n");
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

    prog_openat = bpf_object__find_program_by_name(obj, "trace_sys_exit_openat");
    prog_read = bpf_object__find_program_by_name(obj, "trace_sys_exit_read");
    prog_write = bpf_object__find_program_by_name(obj, "trace_sys_exit_write");

    if (!prog_openat || !prog_read || !prog_write) {
        fprintf(stderr, "Error: one or more tracepoint programs not found\n");
        err = 1;
        goto cleanup;
    }

    if (bpf_program__attach_tracepoint(prog_openat, "syscalls", "sys_exit_openat") < 0) {
        fprintf(stderr, "Error: failed to attach sys_exit_openat: %s\n", strerror(errno));
        err = 1;
        goto cleanup;
    }

    if (bpf_program__attach_tracepoint(prog_read, "syscalls", "sys_exit_read") < 0) {
        fprintf(stderr, "Error: failed to attach sys_exit_read: %s\n", strerror(errno));
        err = 1;
        goto cleanup;
    }

    if (bpf_program__attach_tracepoint(prog_write, "syscalls", "sys_exit_write") < 0) {
        fprintf(stderr, "Error: failed to attach sys_exit_write: %s\n", strerror(errno));
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

    printf("Syscall tracing loaded (openat, read, write).\n");
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
    bpf_object__close(obj);
    logutil_writer_stop(&writer);
    return err;
}