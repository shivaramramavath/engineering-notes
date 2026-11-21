# Network Sockets: TCP and UDP

> Master network systems programming in C: Berkeley sockets API, IPv4/IPv6 socket addresses, stream sockets (TCP), datagram sockets (UDP), non-blocking I/O, handling partial transfers, and connection handshakes.

| Metadata | Details |
| :--- | :--- |
| **Level** | Systems / Advanced |
| **Prerequisites** | [01-file-descriptors-and-posix-io.md](./01-file-descriptors-and-posix-io.md), [04-pipes-and-ipc.md](./04-pipes-and-ipc.md), [04-advanced/06-endianness-and-serialization.md](../04-advanced/06-endianness-and-serialization.md) |
| **Time to read** | 40 minutes |
| **Status** | Complete |
| **Standard Baseline**| POSIX.1-2008 / RFC 793 (TCP) / RFC 768 (UDP) (Linux, BSD, Winsock2 contrast) |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Usage](#4-syntax--usage)
5. [Practical Examples](#5-practical-examples)
6. [Best Practices](#6-best-practices)
7. [Common Mistakes & Pitfalls](#7-common-mistakes--pitfalls)
8. [Performance & Security Notes](#8-performance--security-notes)
9. [Comparison / When to Use What](#9-comparison--when-to-use-what)
10. [Interview Questions](#10-interview-questions)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary / Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why
While pipes and shared memory provide inter-process communication on a single local machine, modern distributed systems communicate across local area networks and the global Internet. The universal programming abstraction for network communication in C is the **Berkeley Sockets API**.

Originally developed in 4.2BSD Unix in 1983, Berkeley sockets generalize the Unix "everything is a file" philosophy: once a connection is established, network communication uses standard file descriptor operations (`read()`, `write()`, `close()`), allowing developers to build web servers, database wire protocols, RPC engines, and peer-to-peer applications.

---

## 2. Core Concepts
- **Socket Types**:
  - `SOCK_STREAM` (TCP): Reliable, ordered, two-way connection-oriented byte stream. Handled via 3-way handshake; automatically retransmits lost packets and enforces flow control.
  - `SOCK_DGRAM` (UDP): Connectionless, unreliable datagrams. Preserves message boundaries; zero handshake overhead, but packets can arrive out-of-order, duplicated, or be dropped.
- **Address Families**:
  - `AF_INET`: IPv4 addresses (32-bit `struct sockaddr_in`).
  - `AF_INET6`: IPv6 addresses (128-bit `struct sockaddr_in6`).
  - `AF_UNSPEC`: Protocol-independent address resolution used with `getaddrinfo()`.
- **Network Byte Order**: Network wire protocols strictly mandate **Big-Endian** byte ordering. Port numbers and IPv4 addresses must be converted using `htons()`, `htonl()`, `ntohs()`, and `ntohl()`.
- **The TCP Server Lifecycle**:
  1. `socket()`: Allocate network socket descriptor.
  2. `bind()`: Associate socket with local IP address and port number.
  3. `listen()`: Transition socket into passive mode with a backlog connection queue.
  4. `accept()`: Block until client connects; returns a *new file descriptor* for the established client connection.
- **The TCP Client Lifecycle**:
  1. `socket()`: Allocate socket descriptor.
  2. `connect()`: Initiate TCP 3-way handshake with remote host/port.
  3. `send()` / `recv()`: Exchange streaming data.

---

## 3. How It Works Internally

### TCP Handshake & Kernel Socket Queues
```
Client Process                                          Server Process
+--------------------+                                  +--------------------+
| socket()           |                                  | socket(), bind()   |
| connect()          |------- SYN (Seq=X) ------------->| listen() (Backlog) |
|                    |<------ SYN-ACK (Seq=Y, Ack=X+1) -| [In SYN Queue]     |
| [Connected]        |------- ACK (Ack=Y+1) ----------->| [To Accept Queue]  |
|                    |                                  | accept() returns fd|
+--------------------+                                  +--------------------+
```

When a server calls `listen(sockfd, backlog)`:
1. The kernel maintains two queues:
   - **SYN Queue (Incomplete Connection Queue)**: Tracks clients that sent SYN and are waiting for the final ACK of the 3-way handshake.
   - **Accept Queue (Completed Connection Queue)**: Tracks fully established connections waiting for the user application to call `accept()`.
2. When `accept()` is called, the kernel dequeues a completed connection from the Accept Queue, allocates a new `struct file` and `int fd` for it, and returns the descriptor. The original listening socket remains open and continues listening for new incoming connections.

---

## 4. Syntax & Usage

### Primary Header Inclusions
```c
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h> /* sockaddr_in, htons, in_addr */
#include <arpa/inet.h>   /* inet_pton, inet_ntop */
#include <netdb.h>       /* getaddrinfo, freeaddrinfo */
#include <unistd.h>      /* close, read, write */
```

### Signature Overview
```c
int socket(int domain, int type, int protocol);
int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
int listen(int sockfd, int backlog);
int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
ssize_t send(int sockfd, const void *buf, size_t len, int flags);
ssize_t recv(int sockfd, void *buf, size_t len, int flags);
```

---

## 5. Practical Examples

### Example 1: Network Frame Serialization Simulation
```c
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#pragma pack(push, 1)
typedef struct {
    uint16_t magic;      // 0x5359 ('SY')
    uint16_t msg_type;   // 1 = PING, 2 = PONG
    uint32_t payload_len;
} SocketPacketHeader;
#pragma pack(pop)

void serialize_packet(uint8_t *dest, uint16_t type, const char *payload) {
    uint32_t len = (uint32_t)strlen(payload);
    SocketPacketHeader hdr = {
        .magic = 0x5359,
        .msg_type = type,
        .payload_len = len
    };
    memcpy(dest, &hdr, sizeof(hdr));
    memcpy(dest + sizeof(hdr), payload, len);
}

int main(void) {
    printf("Simulating Berkeley Stream Socket Wire Framing (AF_INET / SOCK_STREAM)...\n");
    uint8_t wire_buffer[128] = {0};

    const char *msg = "HEARTBEAT_ACK";
    serialize_packet(wire_buffer, 2, msg);

    SocketPacketHeader *recv_hdr = (SocketPacketHeader *)wire_buffer;
    printf("Received Socket Frame: Magic=0x%04X, Type=%u, Len=%u\n",
           recv_hdr->magic, recv_hdr->msg_type, recv_hdr->payload_len);
    
    char payload_buf[64] = {0};
    memcpy(payload_buf, wire_buffer + sizeof(SocketPacketHeader), recv_hdr->payload_len);
    printf("Frame Payload Decoded: '%s'\n", payload_buf);
    return 0;
}
```

```text
Simulating Berkeley Stream Socket Wire Framing (AF_INET / SOCK_STREAM)...
Received Socket Frame: Magic=0x5359, Type=2, Len=13
Frame Payload Decoded: 'HEARTBEAT_ACK'
```

### Example 2: Minimal TCP Echo Server Outline
```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>

int start_echo_server(uint16_t port) {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) return -1;

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        close(server_fd);
        return -1;
    }

    if (listen(server_fd, 128) < 0) {
        close(server_fd);
        return -1;
    }

    return server_fd;
}
```

---

## 6. Best Practices
1. **Always Set `SO_REUSEADDR`**: When a server restarts, previous connections linger in the kernel `TIME_WAIT` state for 60-120 seconds. Without `setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))`, calling `bind()` immediately after restarting fails with `EADDRINUSE`.
2. **TCP Streams Have No Message Boundaries**: TCP is an unstructured stream of bytes. Never assume one `send()` corresponds to one `recv()`. Always implement explicit protocol framing (length prefixes or delimiter characters like `\r\n`).
3. **Always Check for Short Writes and Reads**: Network calls can transfer fewer bytes than requested. Always wrap `send()` and `recv()` in loops that track cumulative byte offsets.
4. **Use Modern `getaddrinfo()` Instead of `gethostbyname()`**: `gethostbyname()` is obsolete, thread-unsafe, and IPv4-only. `getaddrinfo()` is thread-safe and supports dual-stack IPv4/IPv6 seamlessly.
5. **Handle `SIGPIPE`**: Closing sockets on the remote end causes subsequent writes to trigger `SIGPIPE`, crashing the program. Always ignore `SIGPIPE` or pass `MSG_NOSIGNAL` to `send()`.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It's Wrong | Correct Fix |
| :--- | :--- | :--- |
| Forgetting `htons()` on port numbers | Port number stored in host byte order (Little-Endian on x86); port 80 (0x0050) becomes port 20480 (0x5000) | Always write `address.sin_port = htons(port);` |
| Assuming `recv(fd, buf, 1024, 0)` returns a complete message | TCP may split packets across IP MTU boundaries; a single message can arrive in fragments across multiple `recv()` calls | Implement framing with a length header or delimiter loop |
| Binding without `SO_REUSEADDR` | Server crash/restart fails to bind with "Address already in use" due to lingering `TIME_WAIT` state | Set `SO_REUSEADDR` socket option before calling `bind()` |
| Ignoring `SIGPIPE` in network daemons | Writing to a disconnected client socket instantly terminates the entire server | Use `signal(SIGPIPE, SIG_IGN)` or pass `MSG_NOSIGNAL` to `send()` |
| Leaking client file descriptors | Forgetting `close(client_fd)` after handling a client eventually exhausts system descriptor table (`EMFILE`) | Close client descriptors on all exit paths |
| Casting `struct sockaddr_in *` to `struct sockaddr *` without correct size | Passing incorrect length argument to `bind()` or `connect()` fails with `EINVAL` | Always pass `sizeof(struct sockaddr_in)` |

---

## 8. Performance & Security Notes
- **Nagle's Algorithm and `TCP_NODELAY`**: By default, TCP delays sending small packets to coalesce them into larger segments (Nagle's algorithm). In interactive request-response protocols (like RPC or Redis), this introduces 40 ms latency delays. Disable it using `setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt))`.
- **SYN Flood Attacks & SYN Cookies**: Malicious clients flood the server with SYN packets without sending final ACKs, overflowing the SYN Queue. Operating systems protect against this using SYN Cookies (`net.ipv4.tcp_syncookies = 1`), encoding connection state directly into the TCP sequence number without allocating kernel memory.
- **Buffer Bloat & Socket Buffer Sizing**: Fine-tune send and receive buffer capacities with `SO_SNDBUF` and `SO_RCVBUF` for high-bandwidth delay product (BDP) connections.

---

## 9. Comparison / When to Use What

| Feature | TCP (`SOCK_STREAM`) | UDP (`SOCK_DGRAM`) |
| :--- | :--- | :--- |
| **Reliability** | Guaranteed (Retransmissions) | Unreliable (Best-effort delivery) |
| **Ordering** | Strictly in-order | Packets may arrive out-of-order |
| **Framing** | Unframed stream of bytes | Preserves distinct datagram packet boundaries |
| **Connection** | Handshake required (`connect`/`accept`)| Connectionless (`sendto`/`recvfrom`) |
| **Overhead** | 20-byte header, handshake, ACKs | 8-byte header, minimal latency |
| **Ideal For** | HTTP/REST, SSH, DB protocols, file transfer | Video streaming, DNS, VoIP, multiplayer games |

---

## 10. Interview Questions

<details>
<summary>1. What is the TCP 3-way handshake, and what socket calls trigger it?</summary>
The client initiates the handshake by calling `connect()`, sending a SYN packet with an initial sequence number $X$. The server's listening socket kernel receives the SYN and responds with a SYN-ACK packet containing its own sequence number $Y$ and acknowledging $X+1$. The client kernel responds with an ACK packet acknowledging $Y+1$. At this point, the connection is complete; the client's `connect()` returns, and the connection moves to the server's Accept Queue to be returned by `accept()`.
</details>

<details>
<summary>2. Why does the `TIME_WAIT` state exist in TCP?</summary>
1. To ensure the final ACK sent by the local endpoint is received by the remote endpoint (if the final ACK is lost, the remote peer will retransmit its FIN, which must be acknowledged).
2. To allow lingering duplicate packets from the connection to expire in the network rather than being misinterpreted as belonging to a new connection reusing the same IP/port tuple.
</details>

<details>
<summary>3. Why must application protocols define their own message framing on top of TCP?</summary>
Because TCP is a byte-stream protocol, not a message-oriented protocol. The kernel TCP stack aggregates or segments data according to network MTU and congestion windows. A single 100-byte `send()` might be delivered to the receiver in two separate 50-byte chunks, or three 100-byte `send()` calls might be merged into a single 300-byte `recv()`. The application must delimit messages using fixed-length headers, length-prefix fields, or sentinel delimiters (e.g. `\n`).
</details>

<details>
<summary>4. What is the purpose of `SO_REUSEADDR`?</summary>
`SO_REUSEADDR` allows a server process to bind to a local port that is currently occupied by sockets in the `TIME_WAIT` state. This enables servers to restart and rebind immediately without waiting for the 1–2 minute `TIME_WAIT` timeout to expire.
</details>

<details>
<summary>5. What is the difference between `read()`/`write()` and `recv()`/`send()`?</summary>
`read()` and `write()` are general-purpose POSIX file descriptor system calls that operate on files, pipes, and sockets identically. `recv()` and `send()` are socket-specific calls that accept an additional `int flags` parameter, allowing options such as `MSG_PEEK` (inspect incoming data without consuming it) or `MSG_NOSIGNAL` (suppress `SIGPIPE` if the connection is broken).
</details>

<details>
<summary>6. How does UDP handle message boundaries?</summary>
UDP is a datagram protocol: each `sendto()` generates an independent IP packet that preserves the exact message boundary. If a sender transmits a 50-byte buffer via `sendto()`, the receiver will read exactly 50 bytes in a single `recvfrom()` call. If the receiver provides a buffer smaller than 50 bytes, the remaining bytes are discarded and the message is truncated.
</details>

<details>
<summary>7. What does a return value of 0 from `recv()` indicate?</summary>
A return value of 0 indicates that the remote peer performed an orderly shutdown of the connection (sent a FIN packet, e.g. via `close()` or `shutdown(fd, SHUT_WR)`).
</details>

<details>
<summary>8. How do you convert an IPv4 string "192.168.1.1" into binary network format?</summary>
Using `inet_pton(AF_INET, "192.168.1.1", &addr.sin_addr)`. The "pton" stands for "presentation to numeric". It returns 1 on success, 0 for invalid format, and -1 on error.
</details>

---

## 11. Practice Exercises

### Easy
1. **Network Byte Order Swapper**: Write a function that takes a port integer (e.g. 8080) and prints its hex representation in host order vs network order.
2. **IP String Validator**: Write a function using `inet_pton` that verifies whether a given string is a valid IPv4 address.
3. **UDP Datagram Sender**: Write a small function that sends a 10-byte datagram to `127.0.0.1:9000` using `sendto()`.

<details>
<summary>Solutions (Easy)</summary>

```c
/* Exercise 1 */
#include <stdio.h>
#include <stdint.h>
#include <arpa/inet.h>
void print_port_orders(uint16_t port) {
    printf("Host: 0x%04X, Network: 0x%04X\n", port, htons(port));
}

/* Exercise 2 */
#include <arpa/inet.h>
int is_valid_ipv4(const char *ip_str) {
    struct in_addr addr;
    return inet_pton(AF_INET, ip_str, &addr) == 1;
}
```
</details>

### Medium
4. **Length-Prefixed Framing Protocol**: Implement `int send_msg(int fd, const char *msg)` that sends a 4-byte network-order length header followed by the string payload.
5. **Robust `recv_all` Loop**: Implement a function that reads exactly `N` bytes from a TCP socket into a buffer, handling partial reads and `EINTR`.
6. **UDP Ping-Pong Client**: Write a client that sends "PING" to a UDP server and waits for "PONG" with a 2-second timeout using `SO_RCVTIMEO`.

<details>
<summary>Solutions (Medium)</summary>

```c
/* Exercise 4: Length-Prefixed Sender */
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>

int send_msg(int fd, const char *msg) {
    uint32_t len = (uint32_t)strlen(msg);
    uint32_t net_len = htonl(len);
    
    /* Write 4-byte length prefix */
    if (write(fd, &net_len, 4) != 4) return -1;
    
    /* Write message body */
    size_t total = 0;
    while (total < len) {
        ssize_t n = write(fd, msg + total, len - total);
        if (n <= 0) return -1;
        total += (size_t)n;
    }
    return 0;
}
```
</details>

### Hard
7. **Thread-per-Connection Echo Server**: Implement a complete multi-threaded TCP server where `accept()` passes each client descriptor to a detached worker thread.
8. **Asynchronous Non-blocking Connect**: Implement `int nonblocking_connect(const char *ip, int port, int timeout_ms)` using `O_NONBLOCK` and `select()` to verify connection completion without blocking.
9. **Dual-Stack IPv4/IPv6 TCP Server**: Use `getaddrinfo()` with `AI_PASSIVE` to create a server socket that accepts both IPv4 and IPv6 connections on a single listening descriptor (`IPV6_V6ONLY = 0`).

<details>
<summary>Solutions (Hard)</summary>

```c
/* Exercise 8: Non-blocking Connect with Timeout */
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/select.h>

int nonblocking_connect(const char *ip, int port, int timeout_ms) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;

    /* Set non-blocking */
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, ip, &addr.sin_addr);

    int res = connect(fd, (struct sockaddr *)&addr, sizeof(addr));
    if (res == 0) return fd; /* Connected immediately */

    fd_set wset;
    FD_ZERO(&wset);
    FD_SET(fd, &wset);
    struct timeval tv = { .tv_sec = timeout_ms / 1000, .tv_usec = (timeout_ms % 1000) * 1000 };

    if (select(fd + 1, NULL, &wset, NULL, &tv) > 0) {
        int err = 0;
        socklen_t len = sizeof(err);
        getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &len);
        if (err == 0) {
            /* Reset to blocking mode */
            fcntl(fd, F_SETFL, flags);
            return fd;
        }
    }
    close(fd);
    return -1;
}
```
</details>

---

## 12. Summary / Cheat Sheet
- **Byte Order**: Host to Network: `htons()`, `htonl()`. Network to Host: `ntohs()`, `ntohl()`.
- **Server Lifecycle**: `socket()` $	o$ `bind()` $	o$ `listen()` $	o$ `accept()` $	o$ `recv()`/`send()`.
- **Client Lifecycle**: `socket()` $	o$ `connect()` $	o$ `send()`/`recv()`.
- **Rebind Option**: `setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))`.
- **TCP Rule**: No packet boundaries; always use explicit framing.
- **EOF**: `recv()` returning 0 indicates orderly peer shutdown.

---

## 13. Further Reading
- W. Richard Stevens, *UNIX Network Programming, Volume 1: The Sockets Networking API* (The Bible of Network C).
- Michael Kerrisk, *The Linux Programming Interface*, Chapters 56–61 (Sockets).
- Beej's Guide to Network Programming (Using Internet Sockets).

---

[← Previous Note: ISO C11 Standard Threads](./08-c11-threads-h.md) | [Module Index](README.md) | [Next Note: I/O Multiplexing (select, poll, epoll)](./10-io-multiplexing-select-poll-epoll.md)
