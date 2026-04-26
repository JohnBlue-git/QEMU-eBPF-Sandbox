// SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause
#ifndef LOG_UTILS_H
#define LOG_UTILS_H

#include <pthread.h>
#include <stddef.h>
#include <stdio.h>

#define LOGUTIL_QUEUE_SIZE 1024
#define LOGUTIL_LINE_SIZE 256

struct logutil_async_writer {
    FILE *fp;
    pthread_t thread;
    pthread_mutex_t lock;
    pthread_cond_t cond;
    char queue[LOGUTIL_QUEUE_SIZE][LOGUTIL_LINE_SIZE];
    size_t head;
    size_t tail;
    size_t count;
    unsigned long dropped;
    int stop;
};

int logutil_ensure_default_dirs(void);
int logutil_pick_writable_path(const char *const *candidates, size_t count,
                               char *out_path, size_t out_size);

int logutil_writer_start(struct logutil_async_writer *writer, const char *path);
void logutil_writer_enqueue(struct logutil_async_writer *writer, const char *line);
void logutil_writer_stop(struct logutil_async_writer *writer);

#endif
