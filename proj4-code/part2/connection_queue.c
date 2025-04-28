#include "connection_queue.h"

#include <stdio.h>
#include <string.h>

int connection_queue_init(connection_queue_t *queue) {
    queue->head = queue->tail = queue->count = 0;
    queue->shutting_down = 0;
    if (pthread_mutex_init(&queue->mtx, NULL))
        return -1;
    if (pthread_cond_init(&queue->nonempty, NULL))
        return -1;
    if (pthread_cond_init(&queue->nonfull, NULL))
        return -1;
    return 0;
}

int connection_queue_enqueue(connection_queue_t *queue, int connection_fd) {
    pthread_mutex_lock(&queue->mtx);
    while (!queue->shutting_down && queue->count == CAPACITY)
        pthread_cond_wait(&queue->nonfull, &queue->mtx);
    if (queue->shutting_down) {
        pthread_mutex_unlock(&queue->mtx);
        return -1;
    }
    queue->fds[queue->tail] = connection_fd;
    queue->tail = (queue->tail + 1) % CAPACITY;
    queue->count++;
    pthread_cond_signal(&queue->nonempty);
    pthread_mutex_unlock(&queue->mtx);
    return 0;
}

int connection_queue_dequeue(connection_queue_t *queue) {
    pthread_mutex_lock(&queue->mtx);
    while (!queue->shutting_down && queue->count == 0)
        pthread_cond_wait(&queue->nonempty, &queue->mtx);
    if (queue->count == 0 && queue->shutting_down) {
        pthread_mutex_unlock(&queue->mtx);
        return -1;
    }
    int fd = queue->fds[queue->head];
    queue->head = (queue->head + 1) % CAPACITY;
    queue->count--;
    pthread_cond_signal(&queue->nonfull);
    pthread_mutex_unlock(&queue->mtx);
    return fd;
}

int connection_queue_shutdown(connection_queue_t *queue) {
    pthread_mutex_lock(&queue->mtx);
    queue->shutting_down = 1;
    pthread_cond_broadcast(&queue->nonempty);
    pthread_cond_broadcast(&queue->nonfull);
    pthread_mutex_unlock(&queue->mtx);
    return 0;
}

int connection_queue_free(connection_queue_t *queue) {
    pthread_mutex_destroy(&queue->mtx);
    pthread_cond_destroy(&queue->nonempty);
    pthread_cond_destroy(&queue->nonfull);
    return 0;
}
