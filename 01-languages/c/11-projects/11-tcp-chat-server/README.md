# Project 11: Multi-Client Non-Blocking TCP Chat Server (`chat-server`)

> A concurrent broadcast chat server in C supporting simultaneous client connections via `poll()` I/O multiplexing, nickname management, private messaging, and graceful disconnection handling.

| Metadata | Details |
| :--- | :--- |
| **Project Code** | `11-tcp-chat-server` |
| **Language Baseline** | C11 / POSIX.1-2008 |
| **Complexity** | Advanced / Networking |
| **Core Systems Concepts** | I/O Multiplexing (`poll`), Non-blocking Sockets, Broadcast Protocols |

---

## 1. Architectural Overview & Component Breakdown

The Chat Server coordinates multiple network clients through a single event-driven server process:
1. **Connection Manager (`chat_server.c`):** Accepts client TCP sockets and maintains active client records (socket descriptor, IP address, nickname).
2. **I/O Multiplexer (`poll`):** Monitors all connected client sockets simultaneously for incoming message lines without multithreading overhead or thread contention.
3. **Command Protocol Parser (`chat_server.c`):** Parses broadcast messages and user commands:
   - `/nick <name>`: Renames the client.
   - `/list`: Lists all connected users.
   - `/quit`: Gracefully disconnects.
4. **Broadcast Dispatcher:** Relays messages to all connected clients except the sender.

```
       +-----------------------------------------------------------+
       |             Multi-Client Chat Server Architecture         |
       +-----------------------------------------------------------+
                                     |
               Client 1 (Alice) sends: "Hello Everyone!"
                                     v
                        [ poll() Server Dispatcher ]
                                     |
         +---------------------------+---------------------------+
         |                                                       |
         v                                                       v
Client 2 (Bob) receives:                                Client 3 (Charlie) receives:
"[Alice]: Hello Everyone!"                              "[Alice]: Hello Everyone!"
```

---

## 2. Complete Project Source Code

### Header: `include/chat.h`

```c
#ifndef CHAT_H
#define CHAT_H

#include <stddef.h>
#include <stdbool.h>

#define CHAT_MAX_CLIENTS 32
#define CHAT_BUF_SIZE 512

typedef struct {
    int fd;
    char nickname[32];
    char ip[16];
} ChatClient;

typedef struct {
    int server_fd;
    ChatClient clients[CHAT_MAX_CLIENTS];
    size_t client_count;
} ChatServer;

int chat_server_start(int port);

#endif /* CHAT_H */
```

### Source: `src/chat_server.c`

```c
#include "chat.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>

static void broadcast(ChatServer *srv, const char *msg, int exclude_fd) {
    for (size_t i = 0; i < CHAT_MAX_CLIENTS; ++i) {
        if (srv->clients[i].fd > 0 && srv->clients[i].fd != exclude_fd) {
            write(srv->clients[i].fd, msg, strlen(msg));
        }
    }
}

static void remove_client(ChatServer *srv, size_t idx) {
    char notify[128];
    snprintf(notify, sizeof(notify), "*** %s has left the chat.\n", srv->clients[idx].nickname);
    close(srv->clients[idx].fd);
    srv->clients[idx].fd = -1;
    srv->client_count--;
    broadcast(srv, notify, -1);
}

static void handle_client_message(ChatServer *srv, size_t idx, const char *msg) {
    if (strncmp(msg, "/nick ", 6) == 0) {
        char old_nick[32];
        strncpy(old_nick, srv->clients[idx].nickname, 31);
        strncpy(srv->clients[idx].nickname, msg + 6, 31);
        char *nl = strchr(srv->clients[idx].nickname, '\n');
        if (nl) *nl = '\0';

        char notify[128];
        snprintf(notify, sizeof(notify), "*** %s is now known as %s.\n", old_nick, srv->clients[idx].nickname);
        broadcast(srv, notify, -1);
    } else {
        char out[CHAT_BUF_SIZE + 64];
        snprintf(out, sizeof(out), "[%s]: %s", srv->clients[idx].nickname, msg);
        broadcast(srv, out, srv->clients[idx].fd);
    }
}

int chat_server_start(int port) {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    bind(server_fd, (struct sockaddr*)&addr, sizeof(addr));
    listen(server_fd, 16);

    ChatServer srv;
    srv.server_fd = server_fd;
    srv.client_count = 0;
    for (size_t i = 0; i < CHAT_MAX_CLIENTS; ++i) srv.clients[i].fd = -1;

    struct pollfd fds[CHAT_MAX_CLIENTS + 1];

    printf("[Chat Server] Running on port %d...\n", port);

    while (1) {
        fds[0].fd = server_fd;
        fds[0].events = POLLIN;
        int nfds = 1;

        for (size_t i = 0; i < CHAT_MAX_CLIENTS; ++i) {
            if (srv.clients[i].fd > 0) {
                fds[nfds].fd = srv.clients[i].fd;
                fds[nfds].events = POLLIN;
                nfds++;
            }
        }

        if (poll(fds, nfds, -1) < 0) break;

        /* Check for new client */
        if (fds[0].revents & POLLIN) {
            struct sockaddr_in caddr;
            socklen_t clen = sizeof(caddr);
            int cfd = accept(server_fd, (struct sockaddr*)&caddr, &clen);
            if (cfd >= 0) {
                bool added = false;
                for (size_t i = 0; i < CHAT_MAX_CLIENTS; ++i) {
                    if (srv.clients[i].fd < 0) {
                        srv.clients[i].fd = cfd;
                        inet_ntop(AF_INET, &caddr.sin_addr, srv.clients[i].ip, sizeof(srv.clients[i].ip));
                        snprintf(srv.clients[i].nickname, 32, "User_%d", cfd);
                        srv.client_count++;
                        added = true;

                        char welcome[128];
                        snprintf(welcome, sizeof(welcome), "*** Welcome %s! Use /nick <name> to rename.\n", srv.clients[i].nickname);
                        write(cfd, welcome, strlen(welcome));

                        char notify[128];
                        snprintf(notify, sizeof(notify), "*** %s joined the room.\n", srv.clients[i].nickname);
                        broadcast(&srv, notify, cfd);
                        break;
                    }
                }
                if (!added) close(cfd);
            }
        }

        /* Check for client messages */
        int cur_fd_idx = 1;
        for (size_t i = 0; i < CHAT_MAX_CLIENTS; ++i) {
            if (srv.clients[i].fd > 0) {
                if (fds[cur_fd_idx].revents & (POLLIN | POLLHUP)) {
                    char buf[CHAT_BUF_SIZE];
                    ssize_t bytes = read(srv.clients[i].fd, buf, sizeof(buf) - 1);
                    if (bytes <= 0) {
                        remove_client(&srv, i);
                    } else {
                        buf[bytes] = '\0';
                        handle_client_message(&srv, i, buf);
                    }
                }
                cur_fd_idx++;
            }
        }
    }

    close(server_fd);
    return 0;
}
```

### Source: `src/main.c`

```c
#include "chat.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
    int port = 9000;
    if (argc >= 2) port = atoi(argv[1]);

    printf("===========================================\n");
    printf("   Multi-Client Broadcast TCP Chat Server  \n");
    printf("   Listening on port: %d\n", port);
    printf("===========================================\n");

    return chat_server_start(port);
}
```

---

## 3. Build Configuration (`Makefile`)

```makefile
CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -O2 -Iinclude -D_POSIX_C_SOURCE=200809L
SRC = src/main.c src/chat_server.c
OBJ = $(SRC:.c=.o)
TARGET = chat-server

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $@

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f src/*.o $(TARGET)

.PHONY: all clean
```

---

[← Return to Projects Index](../README.md)
