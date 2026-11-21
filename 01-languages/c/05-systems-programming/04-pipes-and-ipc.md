# Pipes and Inter-Process Communication (IPC)

> Master Unix inter-process communication: anonymous unidirectional pipes (`pipe()`), multi-stage shell pipelines, named pipes (`mkfifo`), pipe capacity and atomic write boundaries (`PIPE_BUF`), and handling `SIGPIPE` broken pipelines.

| Metadata | Details |
| :--- | :--- |
| **Level** | Systems / Advanced |
| **Prerequisites** | [01-file-descriptors-and-posix-io.md](./01-file-descriptors-and-posix-io.md), [02-processes-fork-exec-wait.md](./02-processes-fork-exec-wait.md), [03-signals.md](./03-signals.md) |
| **Time to read** | 35 minutes |
| **Status** | Complete |
| **Standard Baseline**| POSIX.1-2008 / ISO C11 (Linux kernel 2.6+, glibc, Windows Named Pipes contrast) |

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
The Unix philosophy dictates creating small, modular programs that perform one task exceptionally well and compose them together using a universal byte-stream protocol. The primary mechanism enabling this composition is the **Pipe**.

Pipes provide unidirectional, FIFO (first-in, first-out) byte streams managed entirely in kernel memory. Data written to the write end of a pipe is buffered by the operating system kernel until read from the read end. In contrast to temporary disk files, pipes never write to physical disks, eliminate filesystem clutter, and synchronize processes automatically: writers block when the pipe buffer is full, and readers block when the buffer is empty.

---

## 2. Core Concepts
- **Anonymous Pipes (`pipe()`)**: Created in memory without a filesystem path. Intended exclusively for communication between processes that share a common ancestor (parent/child or sibling processes formed via `fork()`).
- **File Descriptor Pair**: `pipe(int fds[2])` yields two descriptors:
  - `fds[0]`: Open for **reading** (`O_RDONLY`).
  - `fds[1]`: Open for **writing** (`O_WRONLY`).
- **Atomic Writes & `PIPE_BUF`**: Writes up to `PIPE_BUF` bytes (guaranteed to be at least 512 bytes by POSIX; 4096 bytes on Linux) are guaranteed to be atomic. If multiple processes write concurrently, data chunks smaller than or equal to `PIPE_BUF` will never be interleaved. Writes exceeding `PIPE_BUF` may be interleaved.
- **Named Pipes (FIFOs, `mkfifo()`)**: Possess a directory entry in the filesystem, allowing unrelated processes running under different user sessions to communicate using standard `open()`, `read()`, and `write()` calls.
- **`SIGPIPE` and `EPIPE`**: If a process writes to a pipe whose read end has been closed by all readers, the kernel sends a `SIGPIPE` signal to the writer. If ignored, `write()` returns `-1` with `errno == EPIPE`.

---

## 3. How It Works Internally

### Kernel Circular Ring Buffer
In Linux, an anonymous pipe is backed by a circular ring buffer composed of kernel memory pages (default capacity is 64 KB, or 16 pages of 4096 bytes each).

```
Writer Process                                     Kernel Ring Buffer (64 KB)                               Reader Process
+--------------------+                             +----------------------------------------+              +--------------------+
| fd[1] (Write End)  |===[ write(fd[1], buf, n) ]==> [ Head -> [Page 0][Page 1]... -> Tail ] ==[ read ]==> | fd[0] (Read End)   |
+--------------------+                             +----------------------------------------+              +--------------------+
                                                        |                               |
                                           [If Buffer Full: Sleep]             [If Buffer Empty: Sleep]
                                           [Wait on write_wait queue]          [Wait on read_wait queue]
```

When a pipeline is established:
1. `pipe(fds)` allocates an in-memory inode and two file structures pointing to a shared `struct pipe_inode_info`.
2. Both descriptors inherit standard file descriptor traits, but `lseek()` on a pipe fails with `ESPIPE`.
3. When `fork()` is executed, both child and parent inherit both ends. Crucially, **unused ends must be closed**:
   - If the parent is a writer, it must close `fds[0]`.
   - If the child is a reader, it must close `fds[1]`.
   - If any write end remains open anywhere in the system, `read()` will never encounter EOF (0 bytes), blocking indefinitely when the stream empties.

---

## 4. Syntax & Usage

### Primary Header Inclusions
```c
#include <unistd.h>   /* pipe, read, write, close, dup2 */
#include <sys/types.h>/* mkfifo */
#include <sys/stat.h> /* mkfifo permissions */
#include <fcntl.h>    /* O_NONBLOCK */
```

### Signature Overview
```c
int pipe(int pipefd[2]);
int pipe2(int pipefd[2], int flags); /* Linux specific: O_CLOEXEC, O_NONBLOCK */
int mkfifo(const char *pathname, mode_t mode);
```

---

## 5. Practical Examples

### Example 1: IPC Pipeline Simulation
```c
#include <stdio.h>
#include <string.h>

typedef struct {
    char data[256];
    size_t size;
    int is_eof;
} PipeBuffer;

void pipe_write(PipeBuffer *p, const char *msg) {
    size_t len = strlen(msg);
    if (len > sizeof(p->data) - 1) len = sizeof(p->data) - 1;
    memcpy(p->data, msg, len);
    p->data[len] = '\0';
    p->size = len;
    p->is_eof = 0;
}

void pipe_close_write(PipeBuffer *p) {
    p->is_eof = 1;
}

int pipe_read(PipeBuffer *p, char *out, size_t out_cap) {
    if (p->size == 0 && p->is_eof) return 0; /* EOF */
    size_t to_copy = p->size < (out_cap - 1) ? p->size : (out_cap - 1);
    memcpy(out, p->data, to_copy);
    out[to_copy] = '\0';
    p->size = 0;
    return (int)to_copy;
}

int main(void) {
    PipeBuffer pipe_channel = {0};
    printf("Simulating IPC Pipeline [Stage 1 -> Pipeline Channel -> Stage 2]\n");
    
    pipe_write(&pipe_channel, "SELECT * FROM metrics WHERE cpu_load > 85;");
    printf("Stage 1 sent: %s (%zu bytes)\n", pipe_channel.data, pipe_channel.size);

    char receiver_buf[128] = {0};
    int n = pipe_read(&pipe_channel, receiver_buf, sizeof(receiver_buf));
    printf("Stage 2 received (%d bytes): %s\n", n, receiver_buf);

    pipe_close_write(&pipe_channel);
    n = pipe_read(&pipe_channel, receiver_buf, sizeof(receiver_buf));
    printf("Stage 2 read after pipe close returned: %d (EOF detected)\n", n);
    return 0;
}
```

```text
Simulating IPC Pipeline [Stage 1 -> Pipeline Channel -> Stage 2]
Stage 1 sent: SELECT * FROM metrics WHERE cpu_load > 85; (42 bytes)
Stage 2 received (42 bytes): SELECT * FROM metrics WHERE cpu_load > 85;
Stage 2 read after pipe close returned: 0 (EOF detected)
```

### Example 2: Classic Unix Shell Pipeline (`ls | grep .c`)
```c
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>

void run_pipeline(void) {
    int pipefd[2];
    if (pipe(pipefd) < 0) {
        perror("pipe failed");
        exit(1);
    }

    pid_t pid1 = fork();
    if (pid1 == 0) {
        /* Child 1: Producer ('ls') */
        close(pipefd[0]);               /* Close unused read end */
        dup2(pipefd[1], STDOUT_FILENO); /* Redirect stdout to pipe write end */
        close(pipefd[1]);               /* Close duplicate */
        execlp("ls", "ls", (char *)NULL);
        _exit(127);
    }

    pid_t pid2 = fork();
    if (pid2 == 0) {
        /* Child 2: Consumer ('grep .c') */
        close(pipefd[1]);              /* Close unused write end */
        dup2(pipefd[0], STDIN_FILENO); /* Redirect stdin from pipe read end */
        close(pipefd[0]);              /* Close duplicate */
        execlp("grep", "grep", "\.c$", (char *)NULL);
        _exit(127);
    }

    /* Parent process closes both ends so EOF is reachable */
    close(pipefd[0]);
    close(pipefd[1]);

    /* Reap both children */
    waitpid(pid1, NULL, 0);
    waitpid(pid2, NULL, 0);
}
```

---

## 6. Best Practices
1. **Close Unused Pipe Descriptors Immediately**: Immediately after `fork()`, both parent and child must close the ends of the pipe they do not intend to use. Leaving a write end open prevents readers from ever detecting EOF.
2. **Always Ignore or Handle `SIGPIPE`**: Default `SIGPIPE` disposition terminates the process abruptly. Ignore it with `signal(SIGPIPE, SIG_IGN)` and inspect `write()` return values for `-1` and `errno == EPIPE`.
3. **Respect `PIPE_BUF` for Concurrency**: If multiple processes write logs or telemetry to a single FIFO, keep message payloads $\le 4096$ bytes (`PIPE_BUF`) to guarantee atomic writes without interleaving.
4. **Use `pipe2()` with `O_CLOEXEC` on Linux**: Prevents leaking pipe descriptors to unrelated child processes if another thread calls `fork()` + `exec()`.
5. **Set Proper FIFO Permissions**: When calling `mkfifo(path, 0660)`, ensure the umask does not inadvertently grant world-writable access to sensitive IPC channels.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It's Wrong | Correct Fix |
| :--- | :--- | :--- |
| Leaving parent write end open when reading in parent | Reader waits forever for EOF; `read()` blocks indefinitely because parent itself holds an open write reference | Call `close(pipefd[1])` in the reading process before entering the read loop |
| Seeking on a pipe with `lseek()` | Pipes are streaming FIFOs with no disk backing or offset mechanism; fails with `ESPIPE` | Read sequentially; do not attempt seeking |
| Writing to a closed pipe without ignoring `SIGPIPE` | Kernel delivers `SIGPIPE`, killing the entire application immediately | Call `signal(SIGPIPE, SIG_IGN)` during initialization and check for `errno == EPIPE` |
| Writing chunks > `PIPE_BUF` concurrently | Data from different processes becomes interleaved, corrupting message framing | Keep packets $\le 	ext{PIPE\_BUF}$ or synchronize writers with an external mutex/lock |
| Opening a FIFO with `O_WRONLY` without a reader | `open(fifo, O_WRONLY)` blocks until another process opens the FIFO for reading, freezing execution | Open with `O_NONBLOCK` if asynchronous operation is required |
| Confusing `pipefd[0]` and `pipefd[1]` | Writing to `pipefd[0]` fails with `EBADF` (Bad file descriptor) | Mnemonic: `0` looks like an 'O' for "Output of pipe" (read); `1` looks like a pen to "Write" |

---

## 8. Performance & Security Notes
- **Zero Disk I/O**: Anonymous pipes live entirely in kernel RAM. Data is transferred between process memory buffers via kernel memcpy operations, making pipes orders of magnitude faster than temporary file exchanges.
- **Pipe Sizing via `fcntl`**: On modern Linux (2.6.35+), the default 64 KB pipe buffer can be dynamically queried and resized up to `/proc/sys/fs/pipe-max-size` (usually 1 MB) using `fcntl(pipefd[0], F_SETPIPE_SZ, new_size)`. Larger buffers increase throughput for high-bandwidth streaming.
- **FIFO Security & Insecure Directories**: Creating named pipes in shared world-writable directories like `/tmp` exposes applications to symlink attacks, file pre-creation hijacking, and eavesdropping. Always use secure, private directories (`/run/user/$UID/` or `mkdtemp()`).

---

## 9. Comparison / When to Use What

| Feature | Anonymous Pipe (`pipe()`) | Named FIFO (`mkfifo()`) | UNIX Domain Socket (`AF_UNIX`) |
| :--- | :--- | :--- | :--- |
| **Relationship**| Related (Parent/Child) | Any unrelated process | Any unrelated process |
| **Direction** | Strictly unidirectional | Unidirectional | **Bidirectional (Full Duplex)** |
| **Filesystem Entry** | No (Memory only) | Yes (Named node) | Yes (Socket node) |
| **Framing** | Byte stream (unframed) | Byte stream (unframed) | Stream (`SOCK_STREAM`) or Datagram (`SOCK_DGRAM`)|
| **FD Passing**| No | No | **Yes** (`SCM_RIGHTS`) |

---

## 10. Interview Questions

<details>
<summary>1. Why does a reader on a pipe hang indefinitely if the writer has finished sending data?</summary>
A `read()` on a pipe returns 0 (EOF) *only* when all open write descriptors referencing that pipe across all processes in the operating system have been closed. If any process (including the reading process or an unclosed parent descriptor) still holds an open write descriptor (`pipefd[1]`), the kernel assumes more data might arrive and keeps the reader blocked.
</details>

<details>
<summary>2. What is `PIPE_BUF` and why does it matter in concurrent architectures?</summary>
`PIPE_BUF` is the maximum number of bytes that the operating system guarantees will be written atomically to a pipe or FIFO. If multiple processes write to the same pipe concurrently, writes of size $\le 	ext{PIPE\_BUF}$ are guaranteed not to be interleaved with data from other writers. Writes larger than `PIPE_BUF` may be fragmented and interleaved, corrupting data integrity.
</details>

<details>
<summary>3. What happens if a process writes to a pipe whose read end has been closed?</summary>
The kernel generates the `SIGPIPE` signal for the writing process. If the process has not caught or ignored `SIGPIPE`, the default action terminates the process immediately. If `SIGPIPE` is ignored, the `write()` system call returns `-1` and sets `errno` to `EPIPE`.
</details>

<details>
<summary>4. How does `popen()` work under the hood?</summary>
`popen("cmd", "r")` creates an anonymous pipe using `pipe()`, calls `fork()`, redirects the child's `stdout` to the pipe write end using `dup2()`, and executes the command via `execl("/bin/sh", "sh", "-c", "cmd", NULL)`. In the parent, it closes the write end and wraps the read descriptor in a standard C buffered stream using `fdopen()`.
</details>

<details>
<summary>5. What error occurs if you attempt to call `lseek()` on a pipe descriptor?</summary>
`lseek()` fails, returning `(off_t)-1` and setting `errno` to `ESPIPE` ("Illegal seek"). Pipes are sequential streams without position state.
</details>

<details>
<summary>6. How do Named FIFOs synchronize during the `open()` call?</summary>
By default, opening a FIFO for reading (`O_RDONLY`) blocks until another process opens the same FIFO for writing (`O_WRONLY`), and vice versa. This provides automatic rendezvous synchronization between independent processes. Passing `O_NONBLOCK` overrides this behavior.
</details>

<details>
<summary>7. Why is a single anonymous pipe insufficient for two-way bidirectional communication?</summary>
Because a pipe is a single shared queue: if Process A and Process B both read and write to the same pipe, Process A might read back the exact data it just wrote before Process B has an opportunity to read it. Full bidirectional communication requires either two separate pipes or a UNIX domain socket pair (`socketpair()`).
</details>

<details>
<summary>8. How can you change the buffer capacity of a pipe on Linux?</summary>
Using `fcntl(pipefd[0], F_SETPIPE_SZ, size)`. The size is rounded up to a page boundary and cannot exceed `/proc/sys/fs/pipe-max-size` unless the process possesses the `CAP_SYS_RESOURCE` capability.
</details>

---

## 11. Practice Exercises

### Easy
1. **Parent-to-Child String Transfer**: Write a program that creates a pipe, forks a child, writes "ping" from parent, and prints the received message in the child.
2. **Child-to-Parent Calculation**: Fork a child that computes the factorial of 5 (120) and transmits the integer result over a pipe to the parent.
3. **Pipe Descriptor Closer**: Write a function that correctly sets up a pipe, closes unused descriptors in both parent and child, and terminates cleanly.

<details>
<summary>Solutions (Easy)</summary>

```c
/* Exercise 1 */
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
void pipe_ping(void) {
    int pfd[2];
    pipe(pfd);
    if (fork() == 0) {
        close(pfd[1]);
        char buf[16] = {0};
        read(pfd[0], buf, sizeof(buf) - 1);
        printf("Child received: %s\n", buf);
        close(pfd[0]);
        _exit(0);
    }
    close(pfd[0]);
    write(pfd[1], "ping", 4);
    close(pfd[1]);
    wait(NULL);
}
```
</details>

### Medium
4. **Bidirectional Two-Pipe Channel**: Implement a bidirectional echo exchange between parent and child using two independent anonymous pipes (`p1` parent->child, `p2` child->parent).
5. **Named FIFO Writer/Reader Pair**: Write two separate standalone C programs (`fifo_writer.c` and `fifo_reader.c`) that communicate over `/tmp/my_test_fifo`.
6. **Non-blocking Pipe Polling**: Set `O_NONBLOCK` on a pipe's read end using `fcntl()` and verify that `read()` returns `-1` with `EAGAIN` when no data is available.

<details>
<summary>Solutions (Medium)</summary>

```c
/* Exercise 4: Two-Pipe Bidirectional Channel */
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
void two_way_pipe(void) {
    int up[2], down[2]; /* up: child->parent, down: parent->child */
    pipe(up);
    pipe(down);

    if (fork() == 0) {
        close(down[1]); close(up[0]);
        char msg[32];
        read(down[0], msg, sizeof(msg));
        printf("Child heard: %s\n", msg);
        write(up[1], "ACK", 3);
        close(down[0]); close(up[1]);
        _exit(0);
    }
    close(down[0]); close(up[1]);
    write(down[1], "HELLO", 5);
    char reply[16] = {0};
    read(up[0], reply, sizeof(reply) - 1);
    printf("Parent received: %s\n", reply);
    close(down[1]); close(up[0]);
    wait(NULL);
}
```
</details>

### Hard
7. **Arbitrary Three-Stage Pipeline Runner**: Implement a function `int run_pipeline_3(char **cmd1, char **cmd2, char **cmd3)` that connects three commands via two pipes (e.g. `cat file | sort | uniq`) and waits for all three children.
8. **Multiplexed Log Collector via FIFO**: Implement a multi-process log aggregator where 4 child processes write log lines concurrently to a single named FIFO, and a parent reader decodes and timestamps each line without interleaving.
9. **Dynamic Pipe Resizer & Flow-Control Benchmark**: Write a benchmarking tool that tests pipe throughput using 1 KB writes across buffer capacities from 4 KB to 1 MB using `fcntl(F_SETPIPE_SZ)`.

<details>
<summary>Solutions (Hard)</summary>

```c
/* Exercise 7: 3-Stage Pipeline Runner */
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>

int run_pipeline_3(char **cmd1, char **cmd2, char **cmd3) {
    int p1[2], p2[2];
    if (pipe(p1) < 0 || pipe(p2) < 0) return -1;

    /* Child 1: cmd1 -> p1 */
    if (fork() == 0) {
        close(p1[0]); close(p2[0]); close(p2[1]);
        dup2(p1[1], STDOUT_FILENO);
        close(p1[1]);
        execvp(cmd1[0], cmd1);
        _exit(127);
    }

    /* Child 2: p1 -> cmd2 -> p2 */
    if (fork() == 0) {
        close(p1[1]); close(p2[0]);
        dup2(p1[0], STDIN_FILENO);
        dup2(p2[1], STDOUT_FILENO);
        close(p1[0]); close(p2[1]);
        execvp(cmd2[0], cmd2);
        _exit(127);
    }

    /* Child 3: p2 -> cmd3 */
    if (fork() == 0) {
        close(p1[0]); close(p1[1]); close(p2[1]);
        dup2(p2[0], STDIN_FILENO);
        close(p2[0]);
        execvp(cmd3[0], cmd3);
        _exit(127);
    }

    /* Parent closes all pipe ends */
    close(p1[0]); close(p1[1]);
    close(p2[0]); close(p2[1]);

    /* Reap all 3 children */
    wait(NULL); wait(NULL); wait(NULL);
    return 0;
}
```
</details>

---

## 12. Summary / Cheat Sheet
- **Pipe Pair**: `pipe(pfd)`: `pfd[0]` is read end, `pfd[1]` is write end.
- **Rule of Closure**: Close unneeded ends immediately after `fork()`.
- **EOF Condition**: `read()` returns 0 only when *all* write ends are closed.
- **Atomic Limit**: Writes $\le 	ext{PIPE\_BUF}$ (4 KB on Linux) are never interleaved.
- **`SIGPIPE`**: Triggered when writing to a pipe with no readers. Always ignore or handle.
- **Named FIFOs**: `mkfifo(path, mode)` creates a filesystem-backed pipe.

---

## 13. Further Reading
- W. Richard Stevens, *Advanced Programming in the UNIX Environment*, Chapter 15 (Interprocess Communication).
- Michael Kerrisk, *The Linux Programming Interface*, Chapter 44 (Pipes and FIFOs).
- Linux Manual Pages: `man 2 pipe`, `man 2 pipe2`, `man 3 mkfifo`.

---

[← Previous Note: Signals & Asynchronous Handlers](./03-signals.md) | [Module Index](README.md) | [Next Note: POSIX Threads (pthreads)](./05-pthreads.md)
