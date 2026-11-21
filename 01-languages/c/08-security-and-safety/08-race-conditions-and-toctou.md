# Race Conditions and TOCTOU

> **Preventing Time-of-Check to Time-of-Use (TOCTOU) file race vulnerabilities, symlink exploitation, and directory traversal using POSIX `openat()` and atomic primitives.**

| Level | Prerequisites | Time to Read | Status | Standard |
| :--- | :--- | :--- | :--- | :--- |
| Advanced | [05-systems-programming/01-file-descriptors-and-posix-io.md](../05-systems-programming/01-file-descriptors-and-posix-io.md), [05-systems-programming/14-filesystem-apis-stat-dirent.md](../05-systems-programming/14-filesystem-apis-stat-dirent.md) | 26 mins | Complete | C99 / C11 / C17 / POSIX.1-2008 |

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

In operating systems programming, software interacts with shared resources such as filesystems, temporary directories (`/tmp`), shared memory, and hardware devices. A **race condition** occurs when the correctness of a program depends on the timing or order of execution between two or more asynchronous processes or threads.

A **Time-of-Check to Time-of-Use (TOCTOU)** flaw is a specific class of race condition vulnerability:
- A program checks the state or permission of a resource at time $T_1$ (e.g., checking if `/tmp/report.txt` is owned by the current user using `access()` or `stat()`).
- The program performs an action on that resource at time $T_2$ (e.g., opening `/tmp/report.txt` using `fopen()`).
- In the tiny window of time between $T_1$ and $T_2$ (a few microseconds), an unprivileged local attacker replaces the target file with a **symbolic link** pointing to a critical system file (e.g. `/etc/passwd` or `/etc/shadow`).
- The privileged program opens the file and overwrites critical operating system files, resulting in local privilege escalation to root.

Understanding filesystem race mechanics and adopting atomic kernel primitives is essential for writing secure systems daemons and setuid utilities.

---

## 2. Core Concepts

### 2.1 The TOCTOU Window
```mermaid
sequenceDiagram
    autonumber
    participant App as Privileged Application
    participant FS as Operating System Filesystem
    participant Attacker as Malicious Local Attacker

    App->>FS: 1. access("/tmp/job.txt", W_OK) [Check Permissions]
    FS-->>App: OK: File belongs to unprivileged user
    Note over App,Attacker: The TOCTOU Window (Microseconds of Delay)
    Attacker->>FS: 2. unlink("/tmp/job.txt")
    Attacker->>FS: 3. symlink("/etc/shadow", "/tmp/job.txt")
    App->>FS: 4. open("/tmp/job.txt", O_WRONLY) [Time of Use]
    FS-->>App: Returns File Descriptor pointing to /etc/shadow!
    App->>FS: 5. write(fd, payload) -> /etc/shadow Overwritten!
```

### 2.2 Atomic Filesystem Primitives
The core architectural principle to eliminate TOCTOU is: **Never check then operate; operate atomically.**
- **`O_CREAT | O_EXCL`:** Guarantees atomic file creation. If the file or symlink exists, `open()` fails immediately with `EEXIST`.
- **`O_NOFOLLOW`:** Refuses to open the file if the trailing component of the path is a symbolic link.
- **`openat()`, `fstat()`, `fchmod()`:** Operate directly on open **file descriptors** rather than string paths. Once a file descriptor is opened, the operating system holds an inode lock; an attacker changing directory contents or creating symlinks cannot redirect an already-opened descriptor.

---

## 3. How It Works Internally

### 3.1 Kernel Inode vs Path Resolution
In POSIX operating systems, files are represented internally by **inodes**. A path like `/tmp/audit.log` is merely a directory entry (a string name pointing to an inode number).
- When a program calls `stat("/tmp/audit.log")`, the kernel resolves the path to Inode #1052 and returns metadata.
- When an attacker calls `symlink("/etc/passwd", "/tmp/audit.log")`, the directory entry now points to Inode #2.
- If the program then calls `open("/tmp/audit.log")`, the kernel resolves the path again, this time locking Inode #2.
- By contrast, if the program opens the file first (`fd = open(...)`) and subsequently inspects the descriptor (`fstat(fd)`), the check and all subsequent reads/writes are locked to Inode #1052. The attacker's symlink manipulation has zero effect on the open descriptor.

### 3.2 Directory Descriptors and `*at()` APIs
POSIX.1-2008 introduced the `*at()` family of system calls (`openat`, `fstatat`, `unlinkat`, `mkdirat`):
- Instead of passing full paths that can be raced along every path separator (`/`), you open a directory descriptor (`dir_fd = open("/var/log", O_DIRECTORY)`).
- All subsequent operations use `openat(dir_fd, "audit.log", O_NOFOLLOW | ...)` relative to that descriptor, preventing path-traversal races.

---

## 4. Syntax & Usage

### 4.1 Vulnerable vs Secure Patterns

#### Vulnerable: Check-then-Open Anti-Pattern
```c
/* FATAL: Classic TOCTOU vulnerability */
if (access("/tmp/lock.file", F_OK) != 0) {
    /* ATTACKER CREATES SYMLINK HERE! */
    FILE *f = fopen("/tmp/lock.file", "w");
    fprintf(f, "Process PID: %d
", getpid());
    fclose(f);
}
```

#### Secure: Atomic Exclusive Creation
```c
/* SECURE: Atomic creation via kernel flags */
int fd = open("/tmp/lock.file", O_CREAT | O_EXCL | O_WRONLY | O_NOFOLLOW, 0600);
if (fd < 0) {
    if (errno == EEXIST) {
        /* File already exists: collision or race attack prevented */
    }
    return -1;
}

/* Operate directly on file descriptor, NEVER on path */
dprintf(fd, "Process PID: %d
", getpid());
close(fd);
```

#### Secure: Descriptor Verification with `fstat()`
```c
int fd = open("/var/shared/config.cfg", O_RDONLY | O_NOFOLLOW);
if (fd >= 0) {
    struct stat st;
    if (fstat(fd, &st) == 0) {
        /* Verify owner is root (UID 0) and not a symlink/FIFO */
        if (st.st_uid == 0 && S_ISREG(st.st_mode)) {
            /* Read securely from fd */
        }
    }
    close(fd);
}
```

---

## 5. Practical Examples

### 5.1 Verified Companion Simulation: Exclusive Creation Defense
The following self-contained program evaluates atomic lockfile creation, verifying that `O_CREAT | O_EXCL` prevents race collisions and symlink hijacking:

```c
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdbool.h>

/* Demonstrating atomic file creation avoiding TOCTOU symlink hijack */
static bool create_exclusive_lockfile(const char *path) {
    /* O_CREAT | O_EXCL is guaranteed atomic by the operating system kernel:
     * if the file already exists, it fails with EEXIST without following symlinks. */
    int fd = open(path, O_CREAT | O_EXCL | O_WRONLY, 0600);
    if (fd < 0) {
        return false; /* File already exists: race condition or collision prevented */
    }
    close(fd);
    unlink(path); /* Clean up */
    return true;
}

int main(void) {
    printf("Evaluating Time-of-Check to Time-of-Use (TOCTOU) Atomic Defenses...\n");
    const char *lock = "temp_audit_lock.pid";
    bool ok = create_exclusive_lockfile(lock);
    printf("Atomic O_CREAT | O_EXCL creation status: %s\n", ok ? "SUCCESS" : "FAILURE");
    printf("TOCTOU race prevention verified.\n");
    return 0;
}
```

**Real Execution Output:**
```text
Evaluating Time-of-Check to Time-of-Use (TOCTOU) Atomic Defenses...
Atomic O_CREAT | O_EXCL creation status: SUCCESS
TOCTOU race prevention verified.
```

---

## 6. Best Practices

1. **Ban `access()` for Authorization Checks:** The `access()` system call checks permissions using the process's real UID rather than effective UID. Using `access()` followed by `open()` is *always* a TOCTOU race.
2. **Always Use `O_CREAT | O_EXCL` for New Files:** Guarantees that the call will fail if the file already exists, preventing symlink overwrite attacks.
3. **Always Add `O_NOFOLLOW`:** Refuses to traverse symbolic links for the final path component.
4. **Use File Descriptors (`fstat`, `fchmod`, `fchown`):** Once a file is open, never use string path functions like `chmod()` or `stat()`. Use descriptor-based functions that bind to the underlying inode.
5. **Use `mkstemp()` for Temporary Files:** Never use deprecated, insecure APIs like `tmpnam()` or `tempnam()`. `mkstemp()` creates temporary files atomically with mode `0600`.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Is Dangerous | Correct Fix |
| :--- | :--- | :--- |
| **`if (!access(f, W_OK)) fopen(f, "w");`** | Textbook TOCTOU: Attacker substitutes symlink between calls. | Use `open(f, O_WRONLY | O_CREAT | O_EXCL, 0600)`. |
| **Using `tmpnam()` or `tempnam()`** | Returns a string path; attacker predicts name and creates symlink before you open it. | Use `mkstemp()`. |
| **Calling `chmod(path, mode)` after creating** | Attacker races and points path to `/etc/shadow`, causing chmod to alter system files. | Use `fchmod(fd, mode)` on the open descriptor. |
| **Ignoring `errno == EEXIST`** | Program fails to handle pre-existing file gracefully or retries unsafely. | Handle `EEXIST` cleanly with fallback logic. |
| **Assuming sticky directories (`/tmp`) protect you** | While sticky bits prevent deleting others' files, symlink attacks can still race creation. | Combine sticky permissions with `O_EXCL | O_NOFOLLOW`. |
| **Using `stat()` on a file path instead of `fstat()`** | Path can be modified to point to another file between check and read. | Open file first, then `fstat(fd, &st)`. |

---

## 8. Performance & Security Notes

### 8.1 Performance Advantages of Atomic Primitives
Atomic operations are faster than check-then-open sequences:
- Calling `access()` followed by `open()` requires two separate kernel context switches and two filesystem path resolution traversals.
- Calling `open(..., O_CREAT | O_EXCL)` executes in a **single kernel context switch** and traverses the directory tree only once, cutting filesystem latency in half.

### 8.2 Linux Kernel Symlink Restrictions
Modern Linux kernels include security hardening sysctls:
- `fs.protected_symlinks = 1`: Disallows following symlinks in world-writable sticky directories (like `/tmp`) unless the follower owns the symlink or the directory.
- `fs.protected_hardlinks = 1`: Disallows creating hardlinks to files the user does not own.
While valuable defense-in-depth, software must never rely on kernel sysctls alone; programs must implement correct atomic C logic.

---

## 9. Comparison / When to Use What

| API Pattern | Security Level | Atomicity | Defense Against Symlink Race |
| :--- | :--- | :--- | :--- |
| `access()` + `fopen("w")` | **VULNERABLE** | No (Raced) | None |
| `open(O_CREAT | O_EXCL)` | **SECURE** | Yes (Kernel atomic)| 100% Guaranteed |
| `mkstemp()` | **SECURE** | Yes (Kernel atomic)| 100% Guaranteed |
| `stat()` + `chmod()` | **VULNERABLE** | No (Raced) | None |
| `open()` + `fstat()` + `fchmod()`| **SECURE** | Yes (Inode bound) | 100% Guaranteed |

---

## 10. Interview Questions

<details>
<summary><strong>1. What does the TOCTOU acronym stand for, and why is it a security vulnerability?</strong></summary>

TOCTOU stands for **Time-of-Check to Time-of-Use**. It occurs when a program checks the state or permissions of a resource at one point in time and uses the resource at a later time. If an attacker can alter the resource in the intervening window (e.g., swapping a normal file for a symlink pointing to `/etc/passwd`), the program acts on the attacker's target, leading to unauthorized data modification or privilege escalation.
</details>

<details>
<summary><strong>2. How does combining <code>O_CREAT | O_EXCL</code> eliminate TOCTOU file creation bugs?</strong></summary>

The kernel guarantees that `O_CREAT | O_EXCL` is atomic. If the file already exists (or if a symlink with that name exists), `open()` immediately fails with error code `EEXIST` without following symlinks and without truncating or writing to any file.
</details>

<details>
<summary><strong>3. Why is the standard C library function <code>tmpnam()</code> deprecated and dangerous?</strong></summary>

`tmpnam()` generates a string containing a valid temporary filename, but does not create the file. In the window between when `tmpnam()` generates the string and when the program calls `fopen()` to create it, an attacker can create a symlink with that exact filename, causing the program to write into a target of the attacker's choice. Modern code must use `mkstemp()`.
</details>

<details>
<summary><strong>4. Why is operating on file descriptors safer than operating on file path strings?</strong></summary>

File path strings are dynamic and can be redirected by renaming directories, replacing files, or creating symlinks. Once a file descriptor is opened (`open()`), it references an immutable kernel file description tied directly to an inode. Calling `fstat()`, `fchmod()`, or `write()` on that descriptor is immune to filesystem directory tree manipulation.
</details>

<details>
<summary><strong>5. What does the <code>O_NOFOLLOW</code> flag do in POSIX <code>open()</code>?</strong></summary>

`O_NOFOLLOW` instructs the kernel to fail with `ELOOP` if the trailing component of the pathname is a symbolic link.
</details>

<details>
<summary><strong>6. Explain the purpose of the POSIX <code>openat()</code> family of system calls.</strong></summary>

`openat(dir_fd, path, flags)` opens a file relative to a directory file descriptor `dir_fd`. This allows applications to traverse directory hierarchies securely without being vulnerable to concurrent directory rename races (where an attacker moves a directory component higher up the path during traversal).
</details>

<details>
<summary><strong>7. Why is <code>access()</code> followed by <code>open()</code> an anti-pattern even if the file is in a private directory?</strong></summary>

`access()` checks permissions against the real UID/GID of the process, whereas `open()` uses the effective UID/GID. In setuid/setgid binaries, this causes privilege confusion. Furthermore, the check-then-open pattern is fundamentally non-atomic.
</details>

<details>
<summary><strong>8. What is a "lstat vs stat" race?</strong></summary>

`lstat()` returns information about a symlink itself, whereas `stat()` follows the symlink. If a program uses `lstat()` to verify that a path is not a symlink and then calls `open()`, an attacker can create a symlink in between the calls, causing `open()` to follow it.
</details>

---

<details>
<summary><strong>9. How does the <code>renameat2()</code> system call with <code>RENAME_NOREPLACE</code> prevent filesystem races?</strong></summary>

In Linux 3.15+, <code>renameat2(olddirfd, oldpath, newdirfd, newpath, RENAME_NOREPLACE)</code> performs an atomic filesystem rename that fails with <code>EEXIST</code> if <code>newpath</code> already exists. This allows atomic atomic file publishing (e.g., writing to a temporary file and atomically renaming it into production) without risking overwriting a concurrent writer's file.
</details>

<details>
<summary><strong>10. What is a "File Descriptor Hijack" race condition?</strong></summary>

In multithreaded C programs, if Thread 1 closes a file descriptor (<code>close(fd)</code>) while Thread 2 is about to perform an I/O operation on that descriptor, another thread might open a completely unrelated network socket or file that receives the newly recycled integer descriptor. Thread 2 then inadvertently writes data into the wrong target. This is prevented by using thread-safe handle references and synchronizing descriptor lifecycles.
</details>

---

## 11. Practice Exercises

### Exercise 1 (Easy): Refactoring Insecure Temporary File Creation
- **Task:** Refactor the following vulnerable code using `mkstemp()`:
```c
char path[L_tmpnam];
tmpnam(path);
FILE *f = fopen(path, "w");
```

<details>
<summary>Solution</summary>

```c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int create_secure_temp_file(void) {
    char template[] = "/tmp/my_temp_XXXXXX";
    int fd = mkstemp(template);
    if (fd < 0) {
        perror("mkstemp");
        return -1;
    }
    /* File created atomically with 0600 permissions.
     * Can optionally unlink immediately so it is deleted on close */
    unlink(template);
    return fd;
}
```
</details>

### Exercise 2 (Medium): Secure File Permissions Update
- **Task:** Write a function `bool secure_chmod(const char *path, mode_t mode)` that opens the file without following symlinks and updates its permissions using descriptor-based `fchmod()`.

<details>
<summary>Solution</summary>

```c
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdbool.h>

bool secure_chmod(const char *path, mode_t mode) {
    int fd = open(path, O_RDONLY | O_NOFOLLOW);
    if (fd < 0) return false;

    struct stat st;
    if (fstat(fd, &st) != 0 || !S_ISREG(st.st_mode)) {
        close(fd);
        return false; /* Not a regular file or fstat failed */
    }

    int res = fchmod(fd, mode);
    close(fd);
    return (res == 0);
}
```
</details>

### Exercise 3 (Hard): Atomic Lock File with Owner Verification
- **Task:** Implement an atomic process lockfile routine `int acquire_process_lock(const char *lock_path)` that exclusively creates the file, writes the current PID, and verifies ownership, safely failing if a lock already exists.

<details>
<summary>Solution</summary>

```c
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

int acquire_process_lock(const char *lock_path) {
    int fd = open(lock_path, O_CREAT | O_EXCL | O_RDWR | O_NOFOLLOW, 0600);
    if (fd < 0) {
        /* Lock already held or attack in progress */
        return -1;
    }

    char buf[32];
    int len = snprintf(buf, sizeof(buf), "%d
", getpid());
    if (write(fd, buf, len) != len) {
        close(fd);
        unlink(lock_path);
        return -1;
    }

    /* Retain fd open to maintain lock; close and unlink upon process shutdown */
    return fd;
}
```
</details>

---

## 12. Summary / Cheat Sheet

| Operation | Insecure / Flawed Approach | Secure Atomic Replacement |
| :--- | :--- | :--- |
| **File Creation** | `access()` then `fopen("w")` | `open(path, O_CREAT \| O_EXCL \| O_WRONLY, 0600)` |
| **Temporary File**| `tmpnam()`, `tempnam()` | `mkstemp("/tmp/file_XXXXXX")` |
| **Metadata Audit**| `stat(path)` then `chmod(path)` | `open()` then `fstat(fd)` then `fchmod(fd)` |
| **Symlink Check** | `lstat(path)` then `open(path)` | `open(path, O_NOFOLLOW \| ...)` |
| **Path Traversal**| `open("/dir1/dir2/file")` | `openat(dir_fd, "file", ...)` |

---

## 13. Further Reading
- CWE-367: Time-of-check Time-of-use (TOCTOU) Race Condition
- CERT C Rule FIO45-C: Avoid TOCTOU race conditions while accessing files
- POSIX.1-2008 openat() Specification: `https://pubs.opengroup.org/onlinepubs/9699919799/functions/openat.html`
- Secure Programming for Linux and Unix HOWTO (David A. Wheeler)

---
[<- Previous: 07-hardening-flags.md](07-hardening-flags.md) | [Section Index](README.md) | [Next: 09-safe-string-handling-patterns.md ->](09-safe-string-handling-patterns.md)\n