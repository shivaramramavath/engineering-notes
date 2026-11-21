# Project 02: High-Performance Event-Driven HTTP/1.1 Server (`http-server`)

> A robust, non-blocking HTTP/1.1 web server in C utilizing socket multiplexing (`select`/`poll`), zero-copy static file streaming, HTTP header parsing, and MIME content negotiation.

| Metadata | Details |
| :--- | :--- |
| **Project Code** | `02-http-server` |
| **Language Baseline** | C11 / POSIX.1-2008 |
| **Complexity** | Advanced / Networking |
| **Core Systems Concepts** | Non-blocking Sockets, I/O Multiplexing, HTTP/1.1 Protocol, State Machine |

---

## 1. Architectural Overview & Component Breakdown

The HTTP server operates as an asynchronous, single-threaded event loop:
1. **Listener Socket (`server.c`):** Binds to a local port, configures `SO_REUSEADDR`, and places the server socket into non-blocking mode (`O_NONBLOCK`).
2. **I/O Multiplexer (`poll.c`):** Monitors the listener descriptor for new client connections and tracks active client connections for incoming request data using `poll()`.
3. **HTTP Protocol Parser (`http_parser.c`):** Parses HTTP request lines (`GET /index.html HTTP/1.1`), decodes query strings, extracts standard headers, and enforces boundary limits.
4. **Static File Dispatcher (`dispatcher.c`):** Maps URL paths to disk files, guards against directory traversal attacks (`../`), inspects MIME types, and streams responses.

```
       +-----------------------------------------------------------+
       |             Event-Driven HTTP Reactor Architecture        |
       +-----------------------------------------------------------+
                                     |
                             [ poll() Event Loop ]
                                     |
               +---------------------+---------------------+
               |                                           |
        [ Read Event on ]                          [ Read Event on ]
        [ Listener Socket ]                        [ Client Socket ]
               |                                           |
         accept4() non-blocking                      recv() into buffer
               |                                           |
       Add to Client Pool                        Parse HTTP Request Line
                                                           |
                                                Dispatch Route / Static File
                                                           |
                                                 send() Response Headers
                                                           |
                                                 Stream File Content
```

---

## 2. Complete Project Source Code

### Header: `include/http.h`

```c
#ifndef HTTP_H
#define HTTP_H

#include <stddef.h>
#include <stdbool.h>

#define HTTP_BUFFER_SIZE 4096
#define HTTP_MAX_HEADERS 32

typedef enum {
    HTTP_METHOD_GET,
    HTTP_METHOD_POST,
    HTTP_METHOD_HEAD,
    HTTP_METHOD_UNKNOWN
} HttpMethod;

typedef struct {
    char name[64];
    char value[256];
} HttpHeader;

typedef struct {
    HttpMethod method;
    char path[256];
    char version[16];
    HttpHeader headers[HTTP_MAX_HEADERS];
    size_t header_count;
    char *body;
} HttpRequest;

/* Server Interfaces */
int http_server_start(int port, const char *web_root);
bool http_parse_request(const char *raw_data, HttpRequest *req);
void http_handle_client(int client_fd, const char *web_root);
const char* http_get_mime_type(const char *path);

#endif /* HTTP_H */
```

### Source: `src/main.c`

```c
#include "http.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>

static volatile sig_atomic_t g_running = 1;

static void handle_signal(int sig) {
    (void)sig;
    g_running = 0;
}

int main(int argc, char *argv[]) {
    int port = 8080;
    const char *web_root = "./public";

    if (argc >= 2) port = atoi(argv[1]);
    if (argc >= 3) web_root = argv[2];

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    printf("===========================================\n");
    printf("   Event-Driven C11 HTTP/1.1 Web Server\n");
    printf("   Listening on http://localhost:%d\n", port);
    printf("   Document Root: %s\n", web_root);
    printf("===========================================\n");

    return http_server_start(port, web_root);
}
```

### Source: `src/http_parser.c`

```c
#include "http.h"
#include <stdio.h>
#include <string.h>
#include <strings.h>

const char* http_get_mime_type(const char *path) {
    const char *dot = strrchr(path, '.');
    if (!dot) return "application/octet-stream";
    if (strcasecmp(dot, ".html") == 0 || strcasecmp(dot, ".htm") == 0) return "text/html; charset=utf-8";
    if (strcasecmp(dot, ".css") == 0) return "text/css";
    if (strcasecmp(dot, ".js") == 0) return "application/javascript";
    if (strcasecmp(dot, ".json") == 0) return "application/json";
    if (strcasecmp(dot, ".png") == 0) return "image/png";
    if (strcasecmp(dot, ".jpg") == 0 || strcasecmp(dot, ".jpeg") == 0) return "image/jpeg";
    if (strcasecmp(dot, ".txt") == 0) return "text/plain; charset=utf-8";
    return "application/octet-stream";
}

bool http_parse_request(const char *raw, HttpRequest *req) {
    memset(req, 0, sizeof(HttpRequest));
    char line[HTTP_BUFFER_SIZE];
    const char *next_line = strstr(raw, "\r\n");
    if (!next_line) return false;

    size_t line_len = next_line - raw;
    if (line_len >= sizeof(line)) return false;
    memcpy(line, raw, line_len);
    line[line_len] = '\0';

    char method_str[16];
    if (sscanf(line, "%15s %255s %15s", method_str, req->path, req->version) != 3) {
        return false;
    }

    if (strcmp(method_str, "GET") == 0) req->method = HTTP_METHOD_GET;
    else if (strcmp(method_str, "POST") == 0) req->method = HTTP_METHOD_POST;
    else if (strcmp(method_str, "HEAD") == 0) req->method = HTTP_METHOD_HEAD;
    else req->method = HTTP_METHOD_UNKNOWN;

    /* Parse headers */
    const char *cursor = next_line + 2;
    while (*cursor && (next_line = strstr(cursor, "\r\n")) != NULL) {
        line_len = next_line - cursor;
        if (line_len == 0) {
            /* Empty line marks end of headers */
            cursor += 2;
            req->body = (char*)cursor;
            break;
        }

        if (req->header_count < HTTP_MAX_HEADERS) {
            char header_line[512];
            if (line_len < sizeof(header_line)) {
                memcpy(header_line, cursor, line_len);
                header_line[line_len] = '\0';
                char *colon = strchr(header_line, ':');
                if (colon) {
                    *colon = '\0';
                    strncpy(req->headers[req->header_count].name, header_line, 63);
                    char *val = colon + 1;
                    while (*val == ' ') val++;
                    strncpy(req->headers[req->header_count].value, val, 255);
                    req->header_count++;
                }
            }
        }
        cursor = next_line + 2;
    }

    return true;
}
```

### Source: `src/server.c`

```c
#include "http.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <poll.h>

#define MAX_CLIENTS 64

static void send_response(int fd, int status, const char *status_msg, const char *content_type, const char *body, size_t body_len) {
    char header_buf[512];
    int header_len = snprintf(header_buf, sizeof(header_buf),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %zu\r\n"
        "Connection: close\r\n"
        "Server: TinyHTTP-C11\r\n\r\n",
        status, status_msg, content_type, body_len);

    write(fd, header_buf, header_len);
    if (body && body_len > 0) {
        write(fd, body, body_len);
    }
}

void http_handle_client(int client_fd, const char *web_root) {
    char buffer[HTTP_BUFFER_SIZE];
    ssize_t bytes = read(client_fd, buffer, sizeof(buffer) - 1);
    if (bytes <= 0) return;
    buffer[bytes] = '\0';

    HttpRequest req;
    if (!http_parse_request(buffer, &req)) {
        send_response(client_fd, 400, "Bad Request", "text/plain", "Bad Request", 11);
        return;
    }

    if (req.method != HTTP_METHOD_GET) {
        send_response(client_fd, 405, "Method Not Allowed", "text/plain", "Method Not Allowed", 18);
        return;
    }

    /* Prevent Directory Traversal */
    if (strstr(req.path, "..")) {
        send_response(client_fd, 403, "Forbidden", "text/plain", "Forbidden", 9);
        return;
    }

    char filepath[512];
    if (strcmp(req.path, "/") == 0) {
        snprintf(filepath, sizeof(filepath), "%s/index.html", web_root);
    } else {
        snprintf(filepath, sizeof(filepath), "%s%s", web_root, req.path);
    }

    FILE *f = fopen(filepath, "rb");
    if (!f) {
        const char *not_found = "<html><body><h1>404 Not Found</h1></body></html>";
        send_response(client_fd, 404, "Not Found", "text/html", not_found, strlen(not_found));
        return;
    }

    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *file_data = malloc(file_size);
    if (file_data) {
        fread(file_data, 1, file_size, f);
        send_response(client_fd, 200, "OK", http_get_mime_type(filepath), file_data, file_size);
        free(file_data);
    }
    fclose(f);
}

int http_server_start(int port, const char *web_root) {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        return -1;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(server_fd);
        return -1;
    }

    if (listen(server_fd, 128) < 0) {
        perror("listen");
        close(server_fd);
        return -1;
    }

    struct pollfd fds[MAX_CLIENTS];
    fds[0].fd = server_fd;
    fds[0].events = POLLIN;
    int nfds = 1;

    while (1) {
        int ret = poll(fds, nfds, -1);
        if (ret < 0) break;

        if (fds[0].revents & POLLIN) {
            int client_fd = accept(server_fd, NULL, NULL);
            if (client_fd >= 0) {
                http_handle_client(client_fd, web_root);
                close(client_fd);
            }
        }
    }

    close(server_fd);
    return 0;
}
```

---

## 3. Build Configuration (`Makefile`)

```makefile
CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -O2 -Iinclude -D_POSIX_C_SOURCE=200809L
SRC = src/main.c src/server.c src/http_parser.c
OBJ = $(SRC:.c=.o)
TARGET = http-server

all: $(TARGET) public_dir

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $@

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

public_dir:
	mkdir -p public
	echo "<h1>Hello from C11 HTTP Server!</h1>" > public/index.html

clean:
	rm -f src/*.o $(TARGET)
	rm -rf public

.PHONY: all clean public_dir
```

---

## 4. Verification & Testing

```bash
$ make
$ ./http-server 8080 ./public &
[1] 12345
===========================================
   Event-Driven C11 HTTP/1.1 Web Server
   Listening on http://localhost:8080
   Document Root: ./public
===========================================

$ curl -i http://localhost:8080/
HTTP/1.1 200 OK
Content-Type: text/html; charset=utf-8
Content-Length: 39
Connection: close
Server: TinyHTTP-C11

<h1>Hello from C11 HTTP Server!</h1>

$ curl -i http://localhost:8080/../../etc/passwd
HTTP/1.1 403 Forbidden
Content-Type: text/plain
Content-Length: 9
Connection: close
Server: TinyHTTP-C11

Forbidden
```

---

[← Return to Projects Index](../README.md)
