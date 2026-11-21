# File Descriptors and POSIX Low-Level I/O

> Master kernel-level file descriptor tables, direct system calls (`open`, `read`, `write`, `close`, `lseek`), descriptor duplication (`dup`, `dup2`), non-blocking flags (`O_NONBLOCK`), and atomic file operations.

| Metadata | Details |
| :--- | :--- |
| **Level** | Systems / Advanced |
| **Prerequisites** | [02-core-concepts/11-file-io.md](../02-core-concepts/11-file-io.md), [03-intermediate/03-error-handling.md](../03-intermediate/03-error-handling.md) |
| **Time to read** | 35 minutes |
| **Status** | Complete |
| **Standard Baseline**| POSIX.1-2008 / ISO C11 (Linux kernel 2.6+, glibc, Win32 I/O mapping) |

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
Standard C I/O (`<stdio.h>`, `fopen`, `fread`, `fwrite`) provides high-level buffered streams (`FILE *`). While convenient and portable, standard I/O hides underlying operating system mechanics, introduces memory copying overhead, and lacks control over low-level kernel facilities like asynchronous readiness notification, fine-grained locking, direct disk access (`O_DIRECT`), and socket network channels.

POSIX low-level I/O (`<fcntl.h>`, `<unistd.h>`, `<sys/stat.h>`) operates directly on **File Descriptors (FDs)**: small, non-negative integers that serve as process-local handles to kernel file table entries. System calls like `open()`, `read()`, `write()`, and `close()` bypass user-space buffering, dispatching execution directly across the user/kernel privilege boundary.

---

## 2. Core Concepts
- **File Descriptor Table**: Each process maintains an array of pointers to open file descriptions. The integer `fd` is simply an index into this per-process array.
- **Standard Descriptors**: By convention, descriptor 0 is `STDIN_FILENO`, 1 is `STDOUT_FILENO`, and 2 is `STDERR_FILENO`.
- **System Call Flags**:
  - `O_RDONLY`, `O_WRONLY`, `O_RDWR`: Access modes (mutually exclusive).
  - `O_CREAT`: Create file if it does not exist (requires permissions mode argument, e.g., `0644`).
  - `O_TRUNC`: Truncate file length to 0 if it already exists.
  - `O_APPEND`: Reposition file offset to the end before every write atomically.
  - `O_NONBLOCK`: Do not block if data is not immediately available; return `EAGAIN` or `EWOULDBLOCK`.
  - `O_CLOEXEC`: Atomically set close-on-exec flag to prevent descriptor leaking across `execve()`.
- **Descriptor Duplication**: `dup(fd)` allocates the lowest available integer pointing to the same open file description. `dup2(oldfd, newfd)` atomically closes `newfd` (if open) and duplicates `oldfd` onto `newfd`—the foundational mechanism behind Unix shell redirection (`>` and `2>&1`).

---

## 3. How It Works Internally

### Kernel Object Architecture
Understanding POSIX I/O requires visualizing the three-tiered relationship between the process descriptor table, the system-wide open file table, and the filesystem inode table:

```
Process A                        System-Wide Open File Table           V-Node / Inode Table
+-----------------------+        +---------------------------+        +---------------------+
| fd 0: STDIN_FILENO    |------->| Flags: O_RDONLY           |------->| Inode: 104239       |
| fd 1: STDOUT_FILENO   |        | Offset: 0                 |        | Device: /dev/sda1   |
| fd 2: STDERR_FILENO   |        | RefCount: 1               |        | File Size: 4096 B   |
| fd 3: data.bin        |---+    +---------------------------+        | Disk Blocks: [..]   |
+-----------------------+   |                                         +---------------------+
                            |    +---------------------------+                   ^
Process B                   +--->| Flags: O_RDWR | O_APPEND  |-------------------+
+-----------------------+   |    | Offset: 1024              |
| fd 3: shared_data     |---+    | RefCount: 2               |
+-----------------------+        +---------------------------+
```

When a process calls `read()` or `write()`:
1. The CPU transitions from user mode (Ring 3 on x86-64) to kernel mode (Ring 0) via the `syscall` instruction.
2. The kernel verifies that the integer `fd` is within the process's allocated descriptor limits (`0 <= fd < RLIMIT_NOFILE`).
3. The kernel accesses the process's `struct files_struct`, indexes into `fd_array`, and retrieves the pointer to `struct file`.
4. It verifies read/write permission flags, calls the underlying filesystem driver's `file_operations.read` or `.write` callback, copies data between kernel page caches and user memory buffers, and updates the file offset atomically.

---

## 4. Syntax & Usage

### Primary Header Inclusions
```c
#include <fcntl.h>    /* open, fcntl, O_* constants */
#include <unistd.h>   /* read, write, close, lseek, dup, dup2 */
#include <sys/stat.h> /* mode constants: S_IRUSR, S_IWUSR, etc. */
#include <errno.h>    /* errno, EINTR, EAGAIN */
```

### Signature Overview
```c
int open(const char *pathname, int flags, ... /* mode_t mode */);
ssize_t read(int fd, void *buf, size_t count);
ssize_t write(int fd, const void *buf, size_t count);
off_t lseek(int fd, off_t offset, int whence);
int close(int fd);
int dup2(int oldfd, int newfd);
```

---

## 5. Practical Examples

### Example 1: Direct File Creation, Seeking, and Verification
```c
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>

int main(void) {
    const char *filename = "fd_demo.bin";
    int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    if (fd < 0) {
        perror("open failed");
        return 1;
    }

    const char *header = "SYSTEMS_C_DATA_HEADER";
    write(fd, header, strlen(header));
    
    // Seek to byte offset 32 and write 4-byte integer payload
    off_t pos = lseek(fd, 32, SEEK_SET);
    int value = 0x12345678;
    write(fd, &value, sizeof(value));
    close(fd);

    printf("Wrote header and payload at offset %ld\n", (long)pos);

    // Reopen for reading
    fd = open(filename, O_RDONLY);
    char buf[22] = {0};
    read(fd, buf, 21);
    printf("Read Header: %s\n", buf);

    lseek(fd, 32, SEEK_SET);
    int read_val = 0;
    read(fd, &read_val, sizeof(read_val));
    close(fd);

    printf("Read Payload at offset 32: 0x%08X\n", read_val);
    unlink(filename);
    return 0;
}
```

```text
Wrote header and payload at offset 32
Read Header: SYSTEMS_C_DATA_HEADER
Read Payload at offset 32: 0x12345678
```

### Example 2: Robust Full Read/Write Loop with EINTR Handling
`read()` and `write()` do not guarantee transferring all requested bytes in a single call. In network sockets or interrupted system calls, partial reads/writes occur routinely.

```c
#include <unistd.h>
#include <errno.h>
#include <stddef.h>

ssize_t safe_write_all(int fd, const void *buf, size_t count) {
    size_t total_written = 0;
    const char *ptr = (const char *)buf;

    while (total_written < count) {
        ssize_t n = write(fd, ptr + total_written, count - total_written);
        if (n < 0) {
            if (errno == EINTR) {
                continue; /* Interrupted by signal; retry */
            }
            return -1;    /* Unrecoverable error */
        }
        total_written += (size_t)n;
    }
    return (ssize_t)total_written;
}
```

---

## 6. Best Practices
1. **Always Check Return Values**: `open`, `read`, and `write` return negative values (`-1`) on error. Never ignore return values.
2. **Loop Over Partial Transfers**: Always implement loop wrappers for `read()` and `write()` when operating on pipes, sockets, or terminals.
3. **Handle Interrupted System Calls (`EINTR`)**: Signals delivered while blocked in a system call can cause it to return `-1` with `errno == EINTR`. Retry unless the signal handler requested immediate abort.
4. **Always Pass Mode with `O_CREAT`**: Calling `open(path, O_CREAT | O_WRONLY)` without the 3rd argument (`mode_t mode`) reads garbage from the stack or CPU register, producing random filesystem permissions.
5. **Set `O_CLOEXEC` by Default**: Prevent leaking file descriptors into child processes across `execve()` by using `O_CLOEXEC` or `fcntl(fd, F_SETFD, FD_CLOEXEC)`.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It's Wrong | Correct Fix |
| :--- | :--- | :--- |
| `open(path, O_CREAT \| O_WRONLY)` without mode | The 3rd parameter is read from uninitialized stack/register, setting unpredictable file permissions | Always supply 3rd argument when `O_CREAT` is present: `open(path, flags, 0644)` |
| Assuming `read(fd, buf, 100)` returns 100 bytes | POSIX allows short reads; reading from pipes, sockets, or slow disks may return fewer bytes | Check return count and loop until all bytes are received or EOF (0) |
| Ignoring `EINTR` | Signal handling causes system calls to return `-1` unexpectedly, terminating valid I/O | Check `if (errno == EINTR) continue;` |
| Calling `close(fd)` multiple times | In multithreaded programs, another thread may reopen that same integer `fd`; double-closing closes the wrong file | Set `fd = -1` immediately after closing, and guard with `if (fd >= 0)` |
| Modifying file offset across threads without synchronization | The file offset resides in the shared kernel `struct file`, causing race conditions on concurrent writes | Use `pwrite()` / `pread()` which accept an explicit offset without altering file position |
| Leaking FDs on error exit paths | Process hits `RLIMIT_NOFILE` and subsequent `open()` or `socket()` calls fail with `EMFILE` | Ensure all error handling paths invoke `close(fd)` or use cleanup macros |

---

## 8. Performance & Security Notes
- **Direct I/O vs Page Cache**: POSIX `write()` transfers data into the kernel page cache (write-back cache). To ensure physical non-volatile storage persistence, explicit synchronization via `fsync(fd)` or `fdatasync(fd)` is required.
- **Atomic Appending**: Never attempt `lseek(fd, 0, SEEK_END); write(...)` to append in concurrent environments—it is vulnerable to a Time-of-Check to Time-of-Use (TOCTOU) race condition. Always open with `O_APPEND`, which guarantees that the kernel computes offset and writes atomically.
- **Pread and Pwrite**: `pread(fd, buf, len, offset)` and `pwrite(fd, buf, len, offset)` execute positional I/O without modifying the shared file pointer, making them safe for concurrent multi-threaded worker pools reading from a single descriptor.

---

## 9. Comparison / When to Use What

| Feature | Standard C (`<stdio.h>`) | POSIX I/O (`<unistd.h>`) | Win32 API (`<windows.h>`) |
| :--- | :--- | :--- | :--- |
| **Handle Type** | `FILE *` | `int fd` | `HANDLE` |
| **Buffering** | User-space stream buffer | Unbuffered (direct kernel syscall) | Configurable OS caching |
| **Asynchronous I/O** | Not supported | `aio_read`, `O_NONBLOCK`, `epoll` | Overlapped I/O, IOCP |
| **Portability** | Universal ANSI / ISO C | POSIX (Linux, macOS, BSD, WSL) | Windows native |
| **Duplication** | Non-trivial | Native `dup2()` | `DuplicateHandle()` |

---

## 10. Interview Questions

<details>
<summary>1. What is the difference between a File Descriptor Table and an Open File Description?</summary>
The File Descriptor Table is private to each process. It is indexed by the integer `fd` and contains flags (like `FD_CLOEXEC`) and a pointer to an Open File Description. The Open File Description is system-wide in the kernel and contains the current file offset, access mode flags (`O_RDWR`, `O_APPEND`), and a pointer to the v-node/inode. Multiple FDs (e.g. across `fork()` or `dup()`) can point to the same open file description, sharing the file offset.
</details>

<details>
<summary>2. Why does `dup2(oldfd, newfd)` atomically close `newfd`?</summary>
If closing `newfd` and reassigning it were two separate steps (`close(newfd); dup(oldfd);`), another thread in the process could open a new file or socket between the two calls, claiming `newfd`. `dup2()` performs the close and duplication atomically inside the kernel, preventing race conditions during shell redirections.
</details>

<details>
<summary>3. What does a return value of 0 from `read()` indicate?</summary>
A return value of 0 indicates End-Of-File (EOF). For regular files, the current offset has reached or exceeded file size. For network sockets or pipes, it indicates that the remote peer closed their write end of the communication channel.
</details>

<details>
<summary>4. What is the difference between `fsync()` and `fdatasync()`?</summary>
`fsync()` flushes both the modified file data and all associated metadata (inode timestamps, permissions, size) from the page cache to physical disk. `fdatasync()` flushes only the modified data and the metadata strictly necessary to retrieve the data (e.g. file size), skipping non-essential updates like modification timestamps (`mtime`), significantly reducing disk write overhead.
</details>

<details>
<summary>5. How does `O_CLOEXEC` prevent security vulnerabilities?</summary>
Without `O_CLOEXEC`, an open file descriptor remains open across calls to `execve()`. If a privileged process opens a sensitive file (such as a private key or password database) and subsequently executes an unprivileged external command or helper script, the child binary inherits access to that file descriptor. `O_CLOEXEC` ensures the descriptor is automatically closed before the new program begins execution.
</details>

<details>
<summary>6. What happens if a process writes to a pipe or socket whose read end is closed?</summary>
The kernel delivers the `SIGPIPE` signal to the writing process. If `SIGPIPE` is not caught or ignored, the default action terminates the process. If `SIGPIPE` is ignored (`SIG_IGN`), the `write()` system call fails with return code `-1` and sets `errno = EPIPE`.
</details>

<details>
<summary>7. Why is `pread()` preferred over `lseek()` + `read()` in multithreaded servers?</summary>
`lseek()` followed by `read()` is not atomic: if Thread A seeks to offset 100, and Thread B preempts it and seeks to offset 500, Thread A will mistakenly read from offset 500 when resumed. `pread()` takes the offset as an argument and performs the read atomically without altering the shared file description's file pointer.
</details>

<details>
<summary>8. What error does a non-blocking descriptor return when no data is ready to read?</summary>
It returns `-1` and sets `errno` to either `EAGAIN` or `EWOULDBLOCK` (in POSIX.1-2008, these constants may have identical integer values). The caller must treat this as a normal temporary readiness condition, not a fatal failure.
</details>

---

## 11. Practice Exercises

### Easy
1. **File Word Counter via Low-Level I/O**: Write a function `long count_spaces(int fd)` that reads a file using a 512-byte buffer with `read()` and counts total space characters (`' '`).
2. **Atomic Truncate & Write**: Write a program that creates a file `test.txt`, writes 20 bytes, and uses `lseek()` and `ftruncate()` to truncate it to 10 bytes.
3. **Standard Stream Duplication**: Write a snippet using `dup2()` to redirect standard output (`STDOUT_FILENO`) to a log file `out.log`.

<details>
<summary>Solutions (Easy)</summary>

```c
/* Exercise 1: count_spaces */
#include <unistd.h>
long count_spaces(int fd) {
    char buf[512];
    ssize_t n;
    long count = 0;
    while ((n = read(fd, buf, sizeof(buf))) > 0) {
        for (ssize_t i = 0; i < n; i++) {
            if (buf[i] == ' ') count++;
        }
    }
    return count;
}

/* Exercise 2: Truncate */
#include <unistd.h>
#include <fcntl.h>
void truncate_demo(void) {
    int fd = open("test.txt", O_RDWR | O_CREAT | O_TRUNC, 0644);
    write(fd, "01234567890123456789", 20);
    ftruncate(fd, 10);
    close(fd);
}

/* Exercise 3: Redirect stdout */
#include <unistd.h>
#include <fcntl.h>
void redirect_stdout(void) {
    int fd = open("out.log", O_WRONLY | O_CREAT | O_APPEND, 0644);
    dup2(fd, STDOUT_FILENO);
    close(fd); /* Original fd no longer needed */
}
```
</details>

### Medium
4. **Resilient Copy Utility**: Implement `int safe_cp(const char *src, const char *dst)` using 4096-byte chunked `read()` and `write()` loops handling `EINTR` and short transfers.
5. **Atomic File Lock with O_EXCL**: Implement a lockfile acquisition mechanism using `open(path, O_CREAT | O_EXCL | O_WRONLY, 0600)`.
6. **Descriptor Leak Detector**: Write a function that inspects all open file descriptors for the current process up to `sysconf(_SC_OPEN_MAX)` using `fcntl(fd, F_GETFD)`.

<details>
<summary>Solutions (Medium)</summary>

```c
/* Exercise 4: safe_cp */
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

int safe_cp(const char *src, const char *dst) {
    int sfd = open(src, O_RDONLY);
    if (sfd < 0) return -1;
    int dfd = open(dst, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (dfd < 0) { close(sfd); return -1; }

    char buf[4096];
    ssize_t bytes_read;
    while ((bytes_read = read(sfd, buf, sizeof(buf))) != 0) {
        if (bytes_read < 0) {
            if (errno == EINTR) continue;
            close(sfd); close(dfd); return -1;
        }
        char *ptr = buf;
        ssize_t remaining = bytes_read;
        while (remaining > 0) {
            ssize_t written = write(dfd, ptr, remaining);
            if (written < 0) {
                if (errno == EINTR) continue;
                close(sfd); close(dfd); return -1;
            }
            remaining -= written;
            ptr += written;
        }
    }
    close(sfd);
    close(dfd);
    return 0;
}

/* Exercise 5: Atomic Lockfile */
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
int acquire_lock(const char *lockfile) {
    int fd = open(lockfile, O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (fd < 0) {
        if (errno == EEXIST) return 0; /* Locked by another process */
        return -1;                     /* System error */
    }
    close(fd);
    return 1;                          /* Lock acquired */
}

/* Exercise 6: Count Open Descriptors */
#include <unistd.h>
#include <fcntl.h>
int count_open_fds(void) {
    int max_fds = sysconf(_SC_OPEN_MAX);
    int open_count = 0;
    for (int fd = 0; fd < max_fds; fd++) {
        if (fcntl(fd, F_GETFD) != -1) {
            open_count++;
        }
    }
    return open_count;
}
```
</details>

### Hard
7. **Append-Only Circular Log Engine**: Create a persistent binary logger that writes fixed 64-byte records to disk with `O_APPEND`. If the file reaches 1 MB, roll over to a new generation atomically using `rename()`.
8. **Thread-Safe Concurrent Pread/Pwrite Block Pool**: Implement a block storage layer where multiple threads read and write 512-byte blocks at arbitrary block indices simultaneously without mutex contention using `pread()` and `pwrite()`.
9. **Zero-Copy File Splicing Wrapper**: Implement a fallback pipeline transferring data between two descriptors using direct kernel page manipulation where possible (`sendfile()` on Linux) or chunked 64 KB memory alignment buffers.

<details>
<summary>Solutions (Hard)</summary>

```c
/* Exercise 8: Concurrent Block Storage Layer */
#include <unistd.h>
#include <fcntl.h>
#include <stdint.h>
#include <errno.h>

#define BLOCK_SIZE 512

typedef struct {
    int fd;
} BlockStore;

int blockstore_init(BlockStore *bs, const char *path) {
    bs->fd = open(path, O_RDWR | O_CREAT, 0644);
    return bs->fd < 0 ? -1 : 0;
}

int blockstore_read_block(BlockStore *bs, uint64_t block_idx, void *out_buf) {
    off_t offset = (off_t)(block_idx * BLOCK_SIZE);
    ssize_t n = pread(bs->fd, out_buf, BLOCK_SIZE, offset);
    return (n == BLOCK_SIZE) ? 0 : -1;
}

int blockstore_write_block(BlockStore *bs, uint64_t block_idx, const void *in_buf) {
    off_t offset = (off_t)(block_idx * BLOCK_SIZE);
    ssize_t n = pwrite(bs->fd, in_buf, BLOCK_SIZE, offset);
    return (n == BLOCK_SIZE) ? 0 : -1;
}

void blockstore_close(BlockStore *bs) {
    if (bs->fd >= 0) {
        close(bs->fd);
        bs->fd = -1;
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet
- **File Descriptors**: Small non-negative integers indexing a process-private kernel descriptor table.
- **Flags**:
  - `O_RDONLY`, `O_WRONLY`, `O_RDWR`: File access permissions.
  - `O_CREAT | O_EXCL`: Atomic file creation (fails with `EEXIST` if file exists).
  - `O_APPEND`: Moves file pointer to end before write atomically.
  - `O_NONBLOCK`: Makes calls return immediately with `EAGAIN` if not ready.
  - `O_CLOEXEC`: Prevents descriptor leaks across `execve()`.
- **System Calls**:
  - `open(path, flags, mode)`: Open or create file.
  - `read(fd, buf, count)`: Fetch up to `count` bytes (returns bytes read, 0 on EOF, -1 on error).
  - `write(fd, buf, count)`: Emit up to `count` bytes.
  - `lseek(fd, offset, whence)`: Move file pointer (`SEEK_SET`, `SEEK_CUR`, `SEEK_END`).
  - `dup2(oldfd, newfd)`: Atomic descriptor duplication for I/O redirection.

---

## 13. Further Reading
- W. Richard Stevens, *Advanced Programming in the UNIX Environment (APUE)*, Chapters 3 & 4.
- Michael Kerrisk, *The Linux Programming Interface (TLPI)*, Chapters 4 & 5 (File I/O).
- Linux Manual Pages: `man 2 open`, `man 2 read`, `man 2 write`, `man 2 lseek`, `man 2 dup`.

---

[← Previous Module: Advanced](../04-advanced/README.md) | [Module Index](README.md) | [Next Note: Processes (fork, exec, wait)](./02-processes-fork-exec-wait.md)
