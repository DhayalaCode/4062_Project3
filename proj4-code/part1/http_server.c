#define _GNU_SOURCE

#include <errno.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "http.h"

#define BUFSIZE 512
#define LISTEN_QUEUE_LEN 5

int keep_going = 1;

void handle_sigint(int signo) {
    keep_going = 0;
}

int main(int argc, char **argv) {
    // First argument is directory to serve, second is port
    if (argc != 3) {
        printf("Usage: %s <directory> <port>\n", argv[0]);
        return 1;
    }
    // Uncomment the lines below to use these definitions:
    const char *serve_dir = argv[1];
    const char *port = argv[2];

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_sigint;
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("sigaction failed");
        return 1;
    }

    // Set up server socket
    struct addrinfo hints, *server_info, *p;
    int sockfd, status;

    // Initialize hints
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;     // IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM; // TCP
    hints.ai_flags = AI_PASSIVE;     // Fill in my IP for me

    // Get address information
    if ((status = getaddrinfo(NULL, port, &hints, &server_info)) != 0) {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
        return 1;
    }

    // Loop through all results and bind to the first we can
    for (p = server_info; p != NULL; p = p->ai_next) {
        // Create the socket
        sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sockfd == -1) {
            perror("server: socket");
            continue;
        }

        // Set socket options to allow reuse of address
        int yes = 1;
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
            perror("setsockopt");
            close(sockfd);
            freeaddrinfo(server_info);
            return 1;
        }

        // Bind the socket
        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("server: bind");
            continue;
        }

        break; // Successfully bound
    }

    freeaddrinfo(server_info); // All done with this structure

    if (p == NULL) {
        fprintf(stderr, "server: failed to bind\n");
        return 1;
    }

    // Listen for incoming connections
    if (listen(sockfd, LISTEN_QUEUE_LEN) == -1) {
        perror("listen");
        close(sockfd);
        return 1;
    }

    printf("Server listening on port %s...\n", port);

    // Change to the serve directory
    if (chdir(serve_dir) == -1) {
        perror("chdir");
        close(sockfd);
        return 1;
    }

    // Main server loop
    while (keep_going) {
        // Accept a connection
        struct sockaddr_storage client_addr;
        socklen_t addr_size = sizeof(client_addr);
        int client_fd = accept(sockfd, (struct sockaddr *)&client_addr, &addr_size);

        if (client_fd == -1) {
            if (errno == EINTR && !keep_going) {
                // SIGINT received, break out of loop gracefully
                break;
            }
            perror("accept");
            continue;
        }

        // Process the HTTP request
        char resource_name[BUFSIZE];
        memset(resource_name, 0, BUFSIZE);

        if (read_http_request(client_fd, resource_name) == -1) {
            fprintf(stderr, "Error reading HTTP request\n");
            close(client_fd);
            continue;
        }

        // Prepend the current directory to the resource name if needed
        // Make sure we leave room for the prefix and null terminator
        char resource_path[BUFSIZE];
        if (resource_name[0] == '/') {
            // Leave room for the dot and null terminator
            resource_name[BUFSIZE - 2] = '\0';  // Ensure it's truncated safely
            strcpy(resource_path, ".");
            strncat(resource_path, resource_name, BUFSIZE - 2);
        } else {
            // Leave room for "./" and null terminator
            resource_name[BUFSIZE - 3] = '\0';  // Ensure it's truncated safely
            strcpy(resource_path, "./");
            strncat(resource_path, resource_name, BUFSIZE - 3);
        }

        // Send the HTTP response
        if (write_http_response(client_fd, resource_path) == -1) {
            fprintf(stderr, "Error writing HTTP response\n");
        }

        // Close the connection
        close(client_fd);
    }

    // Clean up and exit
    close(sockfd);
    printf("\nServer shut down gracefully\n");
    // TODO Complete the rest of this function
    return 0;
}
