#include "http.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define BUFSIZE 512
#define MAX_REQUEST_SIZE 8192
const char *get_mime_type(const char *file_extension) {
    if (strcmp(".txt", file_extension) == 0) {
        return "text/plain";
    } else if (strcmp(".html", file_extension) == 0) {
        return "text/html";
    } else if (strcmp(".jpg", file_extension) == 0) {
        return "image/jpeg";
    } else if (strcmp(".png", file_extension) == 0) {
        return "image/png";
    } else if (strcmp(".pdf", file_extension) == 0) {
        return "application/pdf";
    } else if (strcmp(".mp3", file_extension) == 0) {
        return "audio/mpeg";
    }

    return NULL;
}

int read_http_request(int fd, char *resource_name) {
    char buffer[BUFSIZE];
    char request[MAX_REQUEST_SIZE];
    ssize_t bytes_read;
    size_t total_bytes = 0;
    int end_of_headers = 0;

    // Initialize request buffer
    memset(request, 0, MAX_REQUEST_SIZE);

    // Read from socket until we've read the entire HTTP request or hit max
    while (!end_of_headers && (bytes_read = read(fd, buffer, BUFSIZE - 1)) > 0) {

        buffer[bytes_read] = '\0';
        if (total_bytes + bytes_read >= MAX_REQUEST_SIZE - 1) {
            fprintf(stderr, "HTTP request too large\n");
            return -1;
        }


        memcpy(request + total_bytes, buffer, bytes_read);
        total_bytes += bytes_read;

        if (strstr(request, "\r\n\r\n") != NULL || strstr(request, "\n\n") != NULL) {
            end_of_headers = 1;
        }
    }

    if (bytes_read < 0) {
        perror("read");
        return -1;
    }

    request[total_bytes] = '\0';

    char *line = strtok(request, "\r\n");
    if (line == NULL) {
        fprintf(stderr, "Malformed HTTP request\n");
        return -1;
    }


    char *method = strtok(line, " ");
    if (method == NULL) {
        fprintf(stderr, "Malformed HTTP request: no method\n");
        return -1;
    }

    if (strcmp(method, "GET") != 0) {
        fprintf(stderr, "Unsupported method: %s\n", method);
        return -1;
    }


    char *path = strtok(NULL, " ");
    if (path == NULL) {
        fprintf(stderr, "Malformed HTTP request: no path\n");
        return -1;
    }


    if (strcmp(path, "/") == 0) {
        strcpy(resource_name, "/index.html");
    } else {
        strncpy(resource_name, path, BUFSIZE - 1);
        resource_name[BUFSIZE - 1] = '\0';  // Ensure null-termination
    }

    return 0;
}

int write_http_response(int fd, const char *resource_path) {
    int file_fd = open(resource_path, O_RDONLY);
    if (file_fd == -1) {
        const char *not_found =
            "HTTP/1.1 404 Not Found\r\n"
            "Content-Type: text/html\r\n"
            "Connection: close\r\n"
            "\r\n"
            "<html><body><h1>404 Not Found</h1><p>The requested resource could not be found.</p></body></html>\r\n";

        if (write(fd, not_found, strlen(not_found)) < 0) {
            perror("write");
            return -1;
        }
        return 0;
    }


    struct stat file_stat;
    if (fstat(file_fd, &file_stat) < 0) {
        perror("fstat");
        close(file_fd);
        return -1;
    }


    const char *mime_type = "text/plain";
    const char *extension = strrchr(resource_path, '.');
    if (extension != NULL) {
        mime_type = get_mime_type(extension);
    }


    char headers[BUFSIZE];
    snprintf(headers, BUFSIZE,
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: %s\r\n"
            "Content-Length: %ld\r\n"
            "Connection: close\r\n"
            "\r\n", mime_type, (long)file_stat.st_size);

    if (write(fd, headers, strlen(headers)) < 0) {
        perror("write headers");
        close(file_fd);
        return -1;
    }


    char buffer[BUFSIZE];
    ssize_t bytes_read;
    while ((bytes_read = read(file_fd, buffer, BUFSIZE)) > 0) {
        if (write(fd, buffer, bytes_read) < 0) {
            perror("write file");
            close(file_fd);
            return -1;
        }
    }

    if (bytes_read < 0) {
        perror("read file");
        close(file_fd);
        return -1;
    }

    close(file_fd);
    return 0;
}

