// SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause

#include "logging/log_utils.h"

#include <errno.h>
#include <string.h>
#include <sys/stat.h>

static void *logutil_writer_thread(void *arg)
{
    struct logutil_async_writer *writer = arg;

    for (;;) {
        char line[LOGUTIL_LINE_SIZE];

        pthread_mutex_lock(&writer->lock);
        while (writer->count == 0 && !writer->stop)
            pthread_cond_wait(&writer->cond, &writer->lock);

        if (writer->count == 0 && writer->stop) {
            pthread_mutex_unlock(&writer->lock);
            break;
        }

        memcpy(line, writer->queue[writer->head], sizeof(line));
        writer->head = (writer->head + 1) % LOGUTIL_QUEUE_SIZE;
        writer->count--;
        pthread_mutex_unlock(&writer->lock);

        fputs(line, writer->fp);
        fflush(writer->fp);
    }

    fflush(writer->fp);
    return NULL;
}

int logutil_ensure_default_dirs(void)
{
    if (mkdir("/var", 0755) && errno != EEXIST)
        return -1;
    if (mkdir("/var/log", 0755) && errno != EEXIST)
        return -1;
    if (mkdir("/var/log/ebpf_basic_design", 0755) && errno != EEXIST)
        return -1;
    return 0;
}

int logutil_pick_writable_path(const char *const *candidates, size_t count,
                               char *out_path, size_t out_size)
{
    size_t i;

    if (!candidates || !count || !out_path || !out_size)
        return -1;

    for (i = 0; i < count; i++) {
        FILE *fp = fopen(candidates[i], "a");
        if (fp) {
            fclose(fp);
            strncpy(out_path, candidates[i], out_size - 1);
            out_path[out_size - 1] = '\0';
            return 0;
        }
    }

    return -1;
}

int logutil_writer_start(struct logutil_async_writer *writer, const char *path)
{
    memset(writer, 0, sizeof(*writer));
    writer->fp = fopen(path, "a");
    if (!writer->fp)
        return -1;

    /* Keep event logs visible to tail -f even while the process is running. */
    setvbuf(writer->fp, NULL, _IOLBF, 0);

    if (pthread_mutex_init(&writer->lock, NULL))
        goto fail;
    if (pthread_cond_init(&writer->cond, NULL)) {
        pthread_mutex_destroy(&writer->lock);
        goto fail;
    }

    if (pthread_create(&writer->thread, NULL, logutil_writer_thread, writer)) {
        pthread_cond_destroy(&writer->cond);
        pthread_mutex_destroy(&writer->lock);
        goto fail;
    }

    return 0;

fail:
    fclose(writer->fp);
    writer->fp = NULL;
    return -1;
}

void logutil_writer_enqueue(struct logutil_async_writer *writer, const char *line)
{
    pthread_mutex_lock(&writer->lock);
    if (writer->count == LOGUTIL_QUEUE_SIZE) {
        writer->dropped++;
        pthread_mutex_unlock(&writer->lock);
        return;
    }

    strncpy(writer->queue[writer->tail], line, LOGUTIL_LINE_SIZE - 1);
    writer->queue[writer->tail][LOGUTIL_LINE_SIZE - 1] = '\0';
    writer->tail = (writer->tail + 1) % LOGUTIL_QUEUE_SIZE;
    writer->count++;
    pthread_cond_signal(&writer->cond);
    pthread_mutex_unlock(&writer->lock);
}

void logutil_writer_stop(struct logutil_async_writer *writer)
{
    if (!writer->fp)
        return;

    pthread_mutex_lock(&writer->lock);
    writer->stop = 1;
    pthread_cond_signal(&writer->cond);
    pthread_mutex_unlock(&writer->lock);

    pthread_join(writer->thread, NULL);
    if (writer->dropped)
        fprintf(stderr, "Warning: dropped %lu events due to full queue\n", writer->dropped);
    pthread_cond_destroy(&writer->cond);
    pthread_mutex_destroy(&writer->lock);
    fclose(writer->fp);
    writer->fp = NULL;
}
