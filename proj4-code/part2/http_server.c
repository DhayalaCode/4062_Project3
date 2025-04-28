#define _GNU_SOURCE

#include <errno.h>
#include <netdb.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "connection_queue.h"
#include "http.h"

#define BUFSIZE 512
#define LISTEN_QUEUE_LEN 5
#define N_THREADS 5
#define URI_BUF_SIZE 1024

volatile sig_atomic_t stop = 0;
static connection_queue_t queue;
static pthread_t workers[N_THREADS];

static void handle_sigint(int sig) {
    (void) sig;
    stop = 1;
}

void *worker_main(void *arg) {
    char *serve_dir = (char *) arg;
    while (1) {
        int conn_fd = connection_queue_dequeue(&queue);
        if (conn_fd < 0)
            break;

        char uri[URI_BUF_SIZE] = {0};
        int ret = read_http_request(conn_fd, uri);
        if (ret == 0) {
            char path[4096];
            snprintf(path, sizeof(path), "%s%s", serve_dir, uri);
            if (write_http_response(conn_fd, path) != 0) {
                fprintf(stderr, "Error writing HTTP response for %s\n", path);
            }
        } else {
            if (write_http_response(conn_fd, "") != 0) {
                fprintf(stderr, "Error writing 404 response\n");
            }
        }
        if (close(conn_fd) == -1) {
            perror("close(conn_fd)");
        }
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <serve_dir> <port>\n", argv[0]);
        exit(1);
    }
    char *serve_dir = argv[1];
    char *port = argv[2];

    struct sigaction sa = {0};
    sa.sa_handler = handle_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;    // do not use SA_RESTART
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }

    sigset_t fullset, oldset;
    sigfillset(&fullset);
    int err = pthread_sigmask(SIG_BLOCK, &fullset, &oldset);
    if (err) {
        fprintf(stderr, "pthread_sigmask block failed: %s\n", strerror(err));
        exit(1);
    }

    if (connection_queue_init(&queue) != 0) {
        fprintf(stderr, "connection_queue_init failed\n");
        exit(1);
    }

    for (int i = 0; i < N_THREADS; i++) {
        err = pthread_create(&workers[i], NULL, worker_main, serve_dir);
        if (err) {
            fprintf(stderr, "pthread_create failed: %s\n", strerror(err));
            exit(1);
        }
    }

    err = pthread_sigmask(SIG_SETMASK, &oldset, NULL);
    if (err) {
        fprintf(stderr, "pthread_sigmask restore failed: %s\n", strerror(err));
        exit(1);
    }

    struct addrinfo hints = {0}, *res = NULL, *rp = NULL;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    int gai_err = getaddrinfo(NULL, port, &hints, &res);
    if (gai_err != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(gai_err));
        exit(1);
    }
    int listen_fd = -1;
    for (rp = res; rp != NULL; rp = rp->ai_next) {
        listen_fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (listen_fd == -1) {
            perror("socket");
            continue;
        }
        if (bind(listen_fd, rp->ai_addr, rp->ai_addrlen) == 0) {
            break;
        }
        perror("bind");
        if (close(listen_fd) == -1)
            perror("close");
    }
    if (rp == NULL) {
        fprintf(stderr, "Could not bind to port %s\n", port);
        freeaddrinfo(res);
        exit(1);
    }
    freeaddrinfo(res);

    if (listen(listen_fd, LISTEN_QUEUE_LEN) == -1) {
        perror("listen");
        close(listen_fd);
        exit(1);
    }

    while (!stop) {
        struct sockaddr_storage client_addr;
        socklen_t addrlen = sizeof(client_addr);
        int conn_fd = accept(listen_fd, (struct sockaddr *) &client_addr, &addrlen);
        if (conn_fd == -1) {
            if (errno == EINTR && stop) {
                break;
            }
            perror("accept");
            continue;
        }
        if (connection_queue_enqueue(&queue, conn_fd) != 0) {
            fprintf(stderr, "connection_queue_enqueue failed\n");
            if (close(conn_fd) == -1)
                perror("close(conn_fd)");
        }
    }

    if (close(listen_fd) == -1)
        perror("close(listen_fd)");
    if (connection_queue_shutdown(&queue) != 0) {
        fprintf(stderr, "connection_queue_shutdown failed\n");
    }
    for (int i = 0; i < N_THREADS; i++) {
        err = pthread_join(workers[i], NULL);
        if (err) {
            fprintf(stderr, "pthread_join failed: %s\n", strerror(err));
        }
    }
    if (connection_queue_free(&queue) != 0) {
        fprintf(stderr, "connection_queue_free failed\n");
    }

    return 0;
}
