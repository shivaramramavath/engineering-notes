# I/O Multiplexing: select, poll, and epoll

> Master scalable event-driven I/O architectures in C: the synchronous readiness notification model, legacy `select()`, flexible `poll()`, scalable Linux `epoll` (`epoll_create1`, `epoll_ctl`, `epoll_wait`), Level-Triggered vs Edge-Triggered notifications, and the Reactor pattern.

| Metadata | Details |
| :--- | :--- |
| **Level** | Systems / Advanced |
| **Prerequisites** | [01-file-descriptors-and-posix-io.md](./01-file-descriptors-and-posix-io.md), [09-sockets-tcp-udp.md](./09-sockets-tcp-udp.md) |
| **Time to read** | 40 minutes |
| **Status** | Complete |
| **Standard Baseline**| POSIX.1-2008 / Linux kernel 2.6+ (epoll), BSD (kqueue contrast), Windows (IOCP contrast) |

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
In early server design, handling multiple concurrent network clients required either spawning a dedicated process for each client (`fork()`) or assigning a dedicated thread per connection (`pthread_create()`). While conceptually simple, this **thread-per-client** model scales poorly: each thread consumes megabytes of virtual memory for stack allocation, and scheduling thousands of threads causes devastating context switching overhead.

The solution to the "C10K problem" (handling 10,000+ concurrent connections on a single server) is **I/O Multiplexing**. Instead of blocking in a thread waiting for one descriptor, an event loop asks the operating system kernel: *"Here is a set of 10,000 descriptors. Put my single thread to sleep, and wake me up as soon as ANY of them become ready to read or write."*

---

## 2. Core Concepts
- **Readiness Notification**: A file descriptor is considered "ready" if an I/O operation (e.g. `read()` or `write()`) can proceed immediately without blocking the calling thread.
- **The Three Multiplexing Generations**:
  1. `select()`: The 1983 BSD original. Uses fixed-size bitmasks (`fd_set`). Hard limit of 1024 descriptors (`FD_SETSIZE`). $O(N)$ linear scanning in both user space and kernel space.
  2. `poll()`: The System V successor. Replaces bitmasks with a dynamically allocated array of `struct pollfd`. Eliminates the 1024 descriptor limit, but still requires $O(N)$ scanning on every poll cycle.
  3. `epoll`: The modern Linux notification facility (kernel 2.6+). Uses an in-kernel red-black tree to register interest once, and returns only active descriptors via a ready list in $O(1)$ time complexity relative to the total number of monitored connections!
- **Level-Triggered (LT) vs Edge-Triggered (ET)**:
  - *Level-Triggered (Default)*: Notifies you repeatedly as long as data remains in the socket buffer. If you read only 50 bytes of a 100-byte packet, `epoll_wait` notifies you again on the next iteration.
  - *Edge-Triggered (`EPOLLET`)*: Notifies you **only once** when state transitions from unready to ready. You MUST drain the buffer in a loop with non-blocking I/O until `read()` returns `-1` with `EAGAIN` or `EWOULDBLOCK`.

---

## 3. How It Works Internally

### epoll Kernel Architecture (Red-Black Tree & Ready List)
Unlike `select()` and `poll()`, which copy the entire array of monitored descriptors from user space to kernel space on *every single system call*, `epoll` splits registration from waiting:

```
User Space                                                 Kernel Space (epoll instance)
+------------------------------------+                    +------------------------------------+
| epoll_ctl(epfd, EPOLL_CTL_ADD, fd) |------------------->| Red-Black Tree (O(log N) lookup)   |
| (Registers fd interest ONCE)       |                    | Stores all monitored descriptors   |
+------------------------------------+                    +------------------------------------+
                                                                            |
                                                          Hardware Interrupt / Packet Arrives
                                                          Kernel callback fires: puts fd onto
                                                                            v
+------------------------------------+                    +------------------------------------+
| epoll_wait(epfd, events, max, -1)  |<===================| Doubly Linked Ready List (O(1))    |
| (Returns ONLY active ready events!)|                    | Populated solely with active FDs   |
+------------------------------------+                    +------------------------------------+
```

1. **`epoll_create1()`**: Allocates an in-kernel `struct eventpoll` containing an interest list (Red-Black tree) and a ready list (doubly linked list).
2. **`epoll_ctl()`**: Inserts, modifies, or deletes descriptors in the Red-Black tree. Attaches a kernel wait-queue callback (`ep_poll_callback`) to the file's socket.
3. **Hardware Event**: When network packets arrive on a network card, the NIC interrupt handler triggers `ep_poll_callback()`, which moves the corresponding file descriptor directly onto the ready list.
4. **`epoll_wait()`**: Simply inspects the ready list. If non-empty, it copies active events to the caller's array in $O(	ext{events})$ time, sleeping on a wait queue if empty.

---

## 4. Syntax & Usage

### Primary Header Inclusions
```c
#include <sys/epoll.h> /* epoll_create1, epoll_ctl, epoll_wait */
#include <poll.h>      /* poll, struct pollfd */
#include <sys/select.h>/* select, fd_set, FD_ZERO, FD_SET */
#include <fcntl.h>     /* fcntl, O_NONBLOCK */
```

### Signature Overview
```c
int epoll_create1(int flags); /* e.g. EPOLL_CLOEXEC */
int epoll_ctl(int epfd, int op, int fd, struct epoll_event *event);
int epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout);

int poll(struct pollfd *fds, nfds_t nfds, int timeout);
int select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout);
```

---

## 5. Practical Examples

### Example 1: High-Performance Reactor Pattern Simulation
```c
#include <stdio.h>
#include <stdbool.h>

typedef enum {
    EV_READ = 1 << 0,
    EV_WRITE = 1 << 1,
    EV_ERROR = 1 << 2
} EventFlags;

typedef struct {
    int fd;
    EventFlags events;
    EventFlags revents;
} PollFD;

int poll_event_simulator(PollFD *fds, int nfds) {
    int ready = 0;
    for (int i = 0; i < nfds; i++) {
        fds[i].revents = 0;
        if (fds[i].events & EV_READ) {
            fds[i].revents |= EV_READ;
            ready++;
        }
    }
    return ready;
}

int main(void) {
    printf("Simulating High-Performance I/O Multiplexer (Reactor Pattern)...\n");
    PollFD monitored_fds[3] = {
        { .fd = 3, .events = EV_READ, .revents = 0 },
        { .fd = 4, .events = EV_WRITE, .revents = 0 },
        { .fd = 5, .events = EV_READ | EV_WRITE, .revents = 0 }
    };

    int ready = poll_event_simulator(monitored_fds, 3);
    printf("Multiplexer returned %d active ready descriptors:\n", ready);

    for (int i = 0; i < 3; i++) {
        if (monitored_fds[i].revents & EV_READ) {
            printf("  FD %d is READY FOR READ (Level-Triggered)\n", monitored_fds[i].fd);
        }
        if (monitored_fds[i].revents & EV_WRITE) {
            printf("  FD %d is READY FOR WRITE\n", monitored_fds[i].fd);
        }
    }
    return 0;
}
```

```text
Simulating High-Performance I/O Multiplexer (Reactor Pattern)...
Multiplexer returned 2 active ready descriptors:
  FD 3 is READY FOR READ (Level-Triggered)
  FD 5 is READY FOR READ (Level-Triggered)
```

### Example 2: Classic Linux Epoll Event Loop Pattern
```c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>

#define MAX_EVENTS 64

int make_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

void run_epoll_loop(int server_fd) {
    int epfd = epoll_create1(EPOLL_CLOEXEC);
    if (epfd < 0) return;

    struct epoll_event ev, events[MAX_EVENTS];
    ev.events = EPOLLIN;
    ev.data.fd = server_fd;
    epoll_ctl(epfd, EPOLL_CTL_ADD, server_fd, &ev);

    while (1) {
        int n = epoll_wait(epfd, events, MAX_EVENTS, -1);
        for (int i = 0; i < n; i++) {
            if (events[i].data.fd == server_fd) {
                /* New connection arrived on listening socket */
                // int client_fd = accept(...);
                // make_nonblocking(client_fd);
                // ev.events = EPOLLIN | EPOLLET;
                // ev.data.fd = client_fd;
                // epoll_ctl(epfd, EPOLL_CTL_ADD, client_fd, &ev);
            } else {
                /* Client descriptor ready for reading */
                // drain_client_data(events[i].data.fd);
            }
        }
    }
}
```

---

## 6. Best Practices
1. **Always Make Monitored FDs Non-Blocking (`O_NONBLOCK`)**: If a descriptor is marked ready by multiplexing, a subsequent `read()` or `write()` can still block if another thread consumed data or due to TCP checksum failures. Blocking in an event loop freezes all other connections.
2. **Always Loop Until `EAGAIN` in Edge-Triggered Mode**: With `EPOLLET`, the kernel alerts you only when new data arrives. You must call `read()` in a loop until it returns `-1` with `errno == EAGAIN` or `EWOULDBLOCK`. Failing to drain leaves unread data stranded forever.
3. **Use `EPOLLONESHOT` for Multi-Threaded Worker Pools**: In worker pool architectures, `EPOLLONESHOT` ensures that once an event fires for an FD, no other thread will be notified about it until you explicitly rearm it with `epoll_ctl(EPOLL_CTL_MOD)`.
4. **Cleanly Handle Remote Hangup (`EPOLLRDHUP` / `EPOLLHUP`)**: Use `EPOLLRDHUP` on Linux to detect when a peer has closed their connection without having to call `recv()` and check for 0.
5. **Never Pass `fd >= 1024` to `select()`**: Calling `FD_SET(fd, &set)` on descriptors $\ge 1024$ corrupts the stack memory beyond the 128-byte `fd_set` bitmask, causing catastrophic undefined behavior.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It's Wrong | Correct Fix |
| :--- | :--- | :--- |
| Using `select()` with `fd >= 1024` | Overflows fixed-size `fd_set` array on the stack, corrupting return addresses | Use `poll()` or `epoll` |
| Using Edge-Triggered (`EPOLLET`) with blocking sockets | When draining buffer, the final `read()` blocks the entire event loop thread waiting for remote client | Set socket to non-blocking via `fcntl(fd, F_SETFL, O_NONBLOCK)` |
| Performing a single `read()` in Edge-Triggered mode | Leaves unread bytes in kernel buffer; kernel will never send another notification for that data | Drain in `while ((n = read(fd, buf, len)) > 0);` until `EAGAIN` |
| Forgetting to re-initialize `fd_set` before `select()` | `select()` modifies the passed `fd_set` in-place to return ready descriptors, wiping the input set | Re-populate the `fd_set` using `FD_ZERO` and `FD_SET` before every `select()` call |
| Starvation in Edge-Triggered loops | A client sending a massive continuous stream of bytes keeps the worker stuck draining, starving other clients | Enforce a maximum read quota per event loop turn before yielding |
| Desynchronized closing | Calling `close(fd)` automatically removes it from epoll, but duplicate FDs (created via `dup` or `fork`) remain active | Explicitly delete from epoll with `EPOLL_CTL_DEL` before closing |

---

## 8. Performance & Security Notes
- **Scaling Complexity**:
  - `select()`: $O(N)$ scanning in kernel + $O(N)$ scanning in user code. Scales terribly beyond $pprox 100$ descriptors.
  - `poll()`: $O(N)$ scanning in kernel + $O(N)$ scanning in user code.
  - `epoll`: $O(1)$ event dispatching. The time required to return from `epoll_wait()` depends only on the number of **active** connections, not the 100,000 idle connections!
- **Thundering Herd Problem**: When multiple worker processes share a single listening socket in an `epoll` instance, a new incoming connection can wake up *all* worker processes, though only one can successfully `accept()`. Modern Linux kernels resolve this via `EPOLLEXCLUSIVE` or the `SO_REUSEPORT` socket option.

---

## 9. Comparison / When to Use What

| Feature | `select()` | `poll()` | Linux `epoll` | BSD / macOS `kqueue` |
| :--- | :--- | :--- | :--- | :--- |
| **Max Descriptors**| 1024 (`FD_SETSIZE`) | Unbounded | Unbounded | Unbounded |
| **Complexity** | $O(N)$ | $O(N)$ | **$O(1)$** | **$O(1)$** |
| **Data Structure** | Bitmask | Array of `pollfd` | In-kernel Red-Black Tree + List | In-kernel filter table |
| **Trigger Modes** | Level only | Level only | Level & Edge (`EPOLLET`) | Level & Edge (`EV_CLEAR`) |
| **Portability** | Universal ANSI/POSIX| POSIX.1-2001 | Linux-only | FreeBSD, macOS, OpenBSD |

---

## 10. Interview Questions

<details>
<summary>1. Why is `epoll` fundamentally more scalable than `select()` and `poll()`?</summary>
`select()` and `poll()` require passing the complete list of all monitored descriptors from user space to kernel space on every system call invocation. The kernel must iterate through every descriptor to check state ($O(N)$), and user code must loop through all descriptors to find which ones were marked ready ($O(N)$). In contrast, `epoll` registers descriptors in an in-kernel Red-Black tree once. Hardware events trigger callbacks that append only ready descriptors to a doubly linked list. `epoll_wait()` returns only the active descriptors in $O(	ext{ready})$ time, completely independent of the number of idle connections.
</details>

<details>
<summary>2. What is the difference between Level-Triggered and Edge-Triggered notification?</summary>
In Level-Triggered (LT) mode, `epoll_wait()` will repeatedly report the descriptor as ready on every invocation as long as data remains in the receive buffer. In Edge-Triggered (ET) mode, `epoll_wait()` reports the descriptor only when a state change occurs (i.e. new data arrives). If the application fails to drain all available data in ET mode, `epoll_wait()` will not notify the application again, and the remaining data will stall indefinitely.
</details>

<details>
<summary>3. Why must Edge-Triggered descriptors be configured with non-blocking I/O (`O_NONBLOCK`)?</summary>
Because ET requires reading in a loop until the buffer is completely exhausted. On a blocking socket, the final `read()` that occurs when no more data remains in the buffer would block the thread indefinitely, hanging the entire event loop. Marking the socket non-blocking ensures the final read immediately returns `-1` with `errno == EAGAIN`, signaling that the drain is complete.
</details>

<details>
<summary>4. What is the purpose of `EPOLLONESHOT`?</summary>
In multi-threaded event loops where multiple threads call `epoll_wait()`, an event on a descriptor might be dispatched to Thread A. While Thread A is processing the data, more data might arrive on that same descriptor, causing `epoll_wait()` to wake up Thread B for the same descriptor, resulting in concurrent execution on the same connection. `EPOLLONESHOT` disables the descriptor after one notification; once Thread A finishes, it re-arms the descriptor using `epoll_ctl(EPOLL_CTL_MOD)`.
</details>

<details>
<summary>5. What happens if a file descriptor registered with epoll is closed?</summary>
When a file descriptor is closed via `close()`, it is automatically removed from all `epoll` sets that reference it *if* and only if all copies of that file description are closed. However, if the descriptor was duplicated via `dup()` or `fork()`, the underlying open file description remains open and remains in the epoll set until explicitly removed or until all duplicate descriptors are closed.
</details>

<details>
<summary>6. What is the "Thundering Herd" problem in socket servers, and how is it mitigated?</summary>
The thundering herd problem occurs when multiple worker processes or threads wait on the same listening socket (e.g. in `epoll_wait`). When a single client connects, the kernel wakes up all waiting workers, but only one succeeds in calling `accept()`, while all others fail with `EAGAIN`, wasting CPU cycles. Linux mitigates this with the `EPOLLEXCLUSIVE` flag or by using `SO_REUSEPORT`, which gives each worker process its own listening socket and distributes incoming connections evenly at the kernel level.
</details>

<details>
<summary>7. What are the limitations of `select()`?</summary>
1. Hardcoded descriptor limit: `FD_SETSIZE` is usually 1024, preventing servers from handling more than 1024 connections.
2. Descriptor destruction: `select()` mutates the `fd_set` bitmasks in-place, requiring user code to reconstruct the bitmask before every call.
3. Inefficiency: Copies bitmasks across user/kernel space on every call and requires an $O(N)$ linear loop over all descriptors up to `maxfd`.
</details>

<details>
<summary>8. How does `poll()` improve upon `select()`?</summary>
`poll()` uses an array of `struct pollfd` structures with separate `events` (input) and `revents` (output) fields, eliminating the need to re-initialize the array on every call. It also removes the 1024 descriptor limit of `FD_SETSIZE` by accepting an arbitrary array length.
</details>

---

## 11. Practice Exercises

### Easy
1. **Poll Standard Input**: Write a program that uses `poll()` to wait for user keyboard input on `STDIN_FILENO` with a 3-second timeout.
2. **Select Timer**: Implement a high-resolution sleep function using `select(0, NULL, NULL, NULL, &tv)` to sleep for 500 milliseconds.
3. **Descriptor Set Setup**: Write a helper function that populates an `fd_set` from an array of 5 integer descriptors and returns the correct `nfds` value for `select()`.

<details>
<summary>Solutions (Easy)</summary>

```c
/* Exercise 1: Poll STDIN */
#include <stdio.h>
#include <poll.h>
#include <unistd.h>

void poll_stdin(void) {
    struct pollfd pfd;
    pfd.fd = STDIN_FILENO;
    pfd.events = POLLIN;
    pfd.revents = 0;

    printf("Waiting for input (3s timeout)...\n");
    int ret = poll(&pfd, 1, 3000);
    if (ret > 0 && (pfd.revents & POLLIN)) {
        printf("Input is ready!\n");
    } else if (ret == 0) {
        printf("Timeout reached. No input.\n");
    }
}
```
</details>

### Medium
4. **Multiplexed Pipe Reader**: Write a program using `poll()` to monitor read ends of 3 anonymous pipes simultaneously, printing whichever sends data first.
5. **Non-blocking Drain Loop**: Implement a function `void drain_socket(int fd)` that reads non-blocking data into a 256-byte buffer until `read()` returns `-1` with `EAGAIN`.
6. **Epoll Registration Helper**: Implement helper functions `int epoll_add(int epfd, int fd, uint32_t events)` and `int epoll_del(int epfd, int fd)`.

<details>
<summary>Solutions (Medium)</summary>

```c
/* Exercise 5: Non-blocking Drain */
#include <unistd.h>
#include <errno.h>
#include <stdio.h>

void drain_socket(int fd) {
    char buf[256];
    ssize_t n;
    while ((n = read(fd, buf, sizeof(buf))) > 0) {
        /* Process chunk */
    }
    if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
        /* Fully drained */
    } else if (n == 0) {
        /* Peer disconnected */
        close(fd);
    }
}

/* Exercise 6: Epoll Helper */
#include <sys/epoll.h>

int epoll_add(int epfd, int fd, uint32_t events) {
    struct epoll_event ev;
    ev.events = events;
    ev.data.fd = fd;
    return epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev);
}

int epoll_del(int epfd, int fd) {
    return epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
}
```
</details>

### Hard
7. **Complete Single-Threaded Epoll Echo Server**: Implement a non-blocking TCP echo server using Linux `epoll` capable of handling up to 10,000 idle client connections on a single thread.
8. **Edge-Triggered State Machine**: Implement an HTTP request parser driven by `EPOLLET` that handles partial reads, pipelined requests, and graceful disconnects.
9. **Multi-Threaded Worker Pool with EPOLLONESHOT**: Build an event-driven server where a listener thread accepts connections and adds them with `EPOLLONESHOT` to an epoll set shared by 4 worker threads.

<details>
<summary>Solutions (Hard)</summary>

```c
/* Exercise 7: Epoll Echo Server Core Loop */
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#define MAX_EVENTS 128

void run_server(int listen_fd) {
    int epfd = epoll_create1(EPOLL_CLOEXEC);
    struct epoll_event ev, events[MAX_EVENTS];
    ev.events = EPOLLIN;
    ev.data.fd = listen_fd;
    epoll_ctl(epfd, EPOLL_CTL_ADD, listen_fd, &ev);

    while (1) {
        int nfds = epoll_wait(epfd, events, MAX_EVENTS, -1);
        for (int i = 0; i < nfds; i++) {
            if (events[i].data.fd == listen_fd) {
                int cfd = accept(listen_fd, NULL, NULL);
                int flags = fcntl(cfd, F_GETFL, 0);
                fcntl(cfd, F_SETFL, flags | O_NONBLOCK);
                ev.events = EPOLLIN | EPOLLET;
                ev.data.fd = cfd;
                epoll_ctl(epfd, EPOLL_CTL_ADD, cfd, &ev);
            } else {
                int cfd = events[i].data.fd;
                char buf[512];
                ssize_t n;
                while ((n = read(cfd, buf, sizeof(buf))) > 0) {
                    write(cfd, buf, n); /* Echo back */
                }
                if (n == 0 || (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK)) {
                    epoll_ctl(epfd, EPOLL_CTL_DEL, cfd, NULL);
                    close(cfd);
                }
            }
        }
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet
- **select()**: $O(N)$ scan, limited to 1024 descriptors (`FD_SETSIZE`).
- **poll()**: $O(N)$ scan, array of `struct pollfd`, unbounded descriptor count.
- **epoll**: $O(1)$ readiness dispatch via in-kernel Red-Black tree and ready list.
- **LT vs ET**:
  - Level-Triggered (default): Fires whenever data is available.
  - Edge-Triggered (`EPOLLET`): Fires only on state transition; MUST drain until `EAGAIN`.
- **Always Non-Blocking**: Always set `O_NONBLOCK` on descriptors monitored by multiplexers.

---

## 13. Further Reading
- Michael Kerrisk, *The Linux Programming Interface*, Chapter 63 (Alternative I/O Models).
- Dan Kegel, *The C10K Problem* (Seminal paper on scalable network architecture).
- Linux Manual Pages: `man 7 epoll`, `man 2 epoll_ctl`, `man 2 epoll_wait`.

---

[← Previous Note: Network Sockets: TCP & UDP](./09-sockets-tcp-udp.md) | [Module Index](README.md) | [Next Note: Memory Mapping (mmap) & Shared Memory](./11-mmap-and-shared-memory.md)
