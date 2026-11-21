# Daemons and Direct System Calls

> Master Unix daemon engineering and low-level kernel transitions: the classic double-fork daemonization protocol, session leaders (`setsid`), `/dev/null` standard descriptor redirection, system logging (`syslog`), and invoking direct kernel system calls with `syscall()`.

| Metadata | Details |
| :--- | :--- |
| **Level** | Systems / Advanced |
| **Prerequisites** | [01-file-descriptors-and-posix-io.md](./01-file-descriptors-and-posix-io.md), [02-processes-fork-exec-wait.md](./02-processes-fork-exec-wait.md), [03-signals.md](./03-signals.md) |
| **Time to read** | 35 minutes |
| **Status** | Complete |
| **Standard Baseline**| POSIX.1-2008 / System V / Linux ABI (glibc `syscall()`, `systemd` modern background services) |

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
A **Daemon** is a background process that runs autonomously without an interactive user interface or controlling terminal. Web servers (`nginx`), database servers (`mysqld`), and system background services (`sshd`, `cron`) run as daemons, launching at system startup and running continuously to process asynchronous events, network packets, or timer events.

Writing a true Unix daemon requires adhering to strict protocol steps to completely detach the process from its parent terminal session, prevent accidental acquisition of new terminals, and redirect standard streams. Furthermore, systems programmers occasionally encounter newly released Linux kernel features that lack libc wrapper functions; invoking these features requires mastering raw kernel system call transitions via `syscall()`.

---

## 2. Core Concepts
- **Controlling Terminal (TTY)**: A terminal session associated with a process group. If the terminal closes (e.g. user closes SSH session), the kernel delivers `SIGHUP` to the session leader, terminating all foreground and background processes in the group. Daemons must divorce themselves from the controlling terminal.
- **Process Groups and Sessions**:
  - *Process Group*: A collection of related processes (e.g. processes in a pipeline `cat | grep`).
  - *Session*: A collection of process groups tied to a controlling terminal.
  - `setsid()`: Creates a new session and a new process group, with the calling process becoming the session leader and process group leader, completely discarding any controlling terminal.
- **The Classic Double-Fork Technique**:
  1. First `fork()`: Parent exits, ensuring the child is not a process group leader (a prerequisite for `setsid()`).
  2. `setsid()`: Creates new session; process has no controlling terminal.
  3. Second `fork()`: Child forks again, and the immediate child exits. The grandchild is **not a session leader**, guaranteeing it can never accidentally reacquire a controlling terminal in the future!
- **Direct System Calls (`syscall()`)**: Direct kernel transitions using CPU machine instructions (`syscall` on x86-64) when standard C library wrappers do not exist.

---

## 3. How It Works Internally

### The Double-Fork Lifecycle Diagram
```
1. Interactive Shell (PID 100)
       |
       v (User executes: ./my_daemon)
2. Initial Process (PID 5001, PGID 5001, SID 100)
       |
       +--- fork() ---> Parent (PID 5001) terminates immediately!
       |                Shell prints prompt; thinks process is done.
       v
3. Child 1 (PID 5002, PPID 1 [adopted by init])
       |
       +--- setsid() -> Creates new Session (SID 5002), loses terminal!
       |
       +--- fork() ---> Child 1 terminates immediately!
       v
4. Grandchild Daemon (PID 5003, SID 5002, PPID 1)
       |  - Not a session leader -> CANNOT acquire controlling TTY!
       |  - chdir("/")          -> Does not keep unmounted filesystems busy!
       |  - umask(0)            -> Full control over created file modes!
       |  - close(0, 1, 2)      -> Redirected to /dev/null!
       v
   [Runs indefinitely in background]
```

### Raw System Call Transition
When calling `syscall(SYS_getpid)` on x86-64:
1. The system call number is placed into the `RAX` CPU register.
2. Arguments are placed in registers `RDI`, `RSI`, `RDX`, `R10`, `R8`, and `R9`.
3. The CPU executes the `syscall` instruction, switching the CPU privilege level from Ring 3 (User) to Ring 0 (Kernel).
4. The kernel system call dispatcher indexes into `sys_call_table[RAX]`, executes the kernel routine, places the return code into `RAX`, and executes `sysret` to return to user space.

---

## 4. Syntax & Usage

### Primary Header Inclusions
```c
#include <unistd.h>     /* fork, setsid, chdir, close, dup2, syscall */
#include <sys/types.h>  /* pid_t, mode_t */
#include <sys/stat.h>   /* umask */
#include <sys/syscall.h>/* SYS_* system call numbers */
#include <fcntl.h>      /* open, O_RDWR */
#include <syslog.h>     /* openlog, syslog, closelog */
```

### Signature Overview
```c
pid_t setsid(void);
mode_t umask(mode_t mask);
long syscall(long number, ...);
void openlog(const char *ident, int option, int facility);
void syslog(int priority, const char *format, ...);
void closelog(void);
```

---

## 5. Practical Examples

### Example 1: Daemon State and Execution Simulation
```c
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    int pid;
    int sid;
    int stdin_fd;
    int stdout_fd;
    int stderr_fd;
} DaemonContext;

void simulate_daemonize(DaemonContext *ctx) {
    // Stage 1: First fork and setsid (detach from controlling terminal)
    ctx->pid = 5432;
    ctx->sid = 5432;
    // Stage 2: Redirect std fds to /dev/null
    ctx->stdin_fd = -1;  // Closed or redirected
    ctx->stdout_fd = -1;
    ctx->stderr_fd = -1;
}

int main(void) {
    printf("Executing Unix Daemon Initialization Protocol...\n");
    DaemonContext ctx;
    simulate_daemonize(&ctx);

    printf("Daemonized Process Details:\n");
    printf("  PID: %d (Detached background process)\n", ctx.pid);
    printf("  Session ID (SID): %d (New process group leader, no controlling TTY)\n", ctx.sid);
    printf("  Standard Descriptors (0, 1, 2) successfully redirected to /dev/null\n");
    printf("  Direct kernel syscall dispatcher (syscall(SYS_getpid)) operational\n");
    return 0;
}
```

```text
Executing Unix Daemon Initialization Protocol...
Daemonized Process Details:
  PID: 5432 (Detached background process)
  Session ID (SID): 5432 (New process group leader, no controlling TTY)
  Standard Descriptors (0, 1, 2) successfully redirected to /dev/null
  Direct kernel syscall dispatcher (syscall(SYS_getpid)) operational
```

### Example 2: Complete Canonical Daemonization Function
```c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <syslog.h>
#include <signal.h>

int daemonize(const char *daemon_name) {
    /* Step 1: First fork to ensure child is not process group leader */
    pid_t pid = fork();
    if (pid < 0) return -1;
    if (pid > 0) _exit(EXIT_SUCCESS); /* Parent exits */

    /* Step 2: Create new session, becoming session leader */
    if (setsid() < 0) return -1;

    /* Step 3: Ignore SIGHUP and fork second time to lose session leadership */
    signal(SIGHUP, SIG_IGN);
    pid = fork();
    if (pid < 0) return -1;
    if (pid > 0) _exit(EXIT_SUCCESS); /* First child exits */

    /* Step 4: Clear file mode creation mask */
    umask(0);

    /* Step 5: Change working directory to root so filesystems can be unmounted */
    if (chdir("/") < 0) return -1;

    /* Step 6: Close all open file descriptors up to limit */
    long max_fd = sysconf(_SC_OPEN_MAX);
    if (max_fd < 0) max_fd = 1024;
    for (int fd = 0; fd < max_fd; fd++) {
        close(fd);
    }

    /* Step 7: Re-open stdin, stdout, stderr pointing to /dev/null */
    int null_fd = open("/dev/null", O_RDWR);
    if (null_fd != 0) {
        dup2(null_fd, 0);
        close(null_fd);
    }
    dup2(0, 1);
    dup2(0, 2);

    /* Step 8: Initialize system logger */
    openlog(daemon_name, LOG_PID | LOG_CONS, LOG_DAEMON);
    syslog(LOG_INFO, "%s daemon started successfully.", daemon_name);
    return 0;
}
```

---

## 6. Best Practices
1. **Always Change Working Directory to `/`**: If a daemon runs in a mounted volume (e.g. `/mnt/usb` or `/home/user`), that filesystem cannot be unmounted by system administrators as long as the daemon is active.
2. **Always Reset Umask (`umask(0)`)**: Daemons inherit the launching user's umask, which may mask out write permissions unpredictably. Set `umask(0)` and pass explicit permission bits to `open()`.
3. **Redirect Descriptors 0, 1, and 2 to `/dev/null`**: Never leave descriptors 0, 1, or 2 unassigned. If closed and later a new file or socket is opened, the kernel assigns descriptor 0 or 1 to it; any third-party library calling `printf()` would inadvertently overwrite your socket or database!
4. **Log Exclusively to `syslog`**: Because standard output and error are discarded, all diagnostic messages, warnings, and error reports must be transmitted via `syslog()`.
5. **Implement Single-Instance Locking (PID File)**: Prevent duplicate daemon processes from running concurrently by creating a lockfile (e.g. `/var/run/my_daemon.pid`) with `open(O_CREAT | O_RDWR)` and acquiring an exclusive lock via `fcntl(fd, F_SETLK)`.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It's Wrong | Correct Fix |
| :--- | :--- | :--- |
| Closing descriptors 0, 1, 2 without redirecting | Next `open()` allocates fd 0 or 1; accidental `printf()` writes corrupted data into database or network socket | Open `/dev/null` and duplicate onto 0, 1, and 2 with `dup2()` |
| Running `setsid()` without forking first | `setsid()` fails with `EPERM` if the calling process is already a process group leader | Always call `fork()` and let the parent exit before `setsid()` |
| Forgetting `chdir("/")` | Keeps directory busy, preventing system administrators from unmounting disks | Always call `chdir("/")` during initialization |
| Neglecting `umask(0)` | Creates files with unexpected restrictive permissions inherited from the user environment | Call `umask(0)` or set an explicit predictable mask |
| Spawning child processes without reaping | Daemon runs forever; child worker processes accumulate as zombies until PIDs are exhausted | Handle `SIGCHLD` with a non-blocking `waitpid(-1, &s, WNOHANG)` loop |
| Calling `syscall()` with incorrect register types | On 64-bit systems, passing 32-bit types where 64-bit pointers are expected causes memory corruption | Cast parameters explicitly to matching types (e.g. `(long)val`) |

---

## 8. Performance & Security Notes
- **Modern Daemon Architecture (Systemd)**: On modern Linux distributions running `systemd`, the classic double-fork daemonization protocol is obsolete and discouraged! Systemd prefers services to run in the **foreground** (`Type=simple` or `Type=exec`), allowing systemd to monitor process lifecycles directly, capture `stdout`/`stderr` into the system journal automatically, and manage cgroup resource limits.
- **Dropping Privileges (Principle of Least Privilege)**: If a daemon must bind to privileged network ports ($< 1024$), start as `root`, open the socket, and immediately drop privileges to an unprivileged user using `setgroups()`, `setgid()`, and `setuid()`.
- **System Call Tracing (`strace`)**: Debug daemon execution and verify raw system calls using `strace -p <PID>` to intercept every kernel transition and return code in real-time.

---

## 9. Comparison / When to Use What

| Feature | Classic Unix Daemon | Modern Systemd Service | Foreground Utility |
| :--- | :--- | :--- | :--- |
| **Forking** | Double-fork protocol | Foreground (`Type=simple`) | No forking |
| **Terminal** | Detached via `setsid()` | Managed by systemd | Attached to active TTY |
| **Logging** | Native `syslog()` | Standard `stdout`/`stderr` | Terminal output |
| **Restarting** | Custom watchdog needed | Automatic by init system | None |
| **Portability** | Universal Unix/POSIX | Linux only | Universal |

---

## 10. Interview Questions

<details>
<summary>1. Why is a second `fork()` necessary in the classic daemonization protocol?</summary>
The first `fork()` ensures that the process is not a process group leader, allowing `setsid()` to succeed. Calling `setsid()` creates a new session and makes the process the session leader. In System V Unix, a session leader without a controlling terminal will automatically acquire a controlling terminal if it opens a terminal device (e.g. a modem or `/dev/tty`). The second `fork()` ensures the grandchild process is **not a session leader**, guaranteeing it can never accidentally reacquire a controlling terminal.
</details>

<details>
<summary>2. Why must file descriptors 0, 1, and 2 be redirected to `/dev/null` rather than simply left closed?</summary>
If descriptors 0, 1, and 2 are closed, the next `open()` or `socket()` call will allocate the lowest available descriptor numbers (0, then 1, then 2). If a third-party library or debug code subsequently calls `printf()` (which writes to descriptor 1) or `perror()` (which writes to descriptor 2), it will write binary data directly into the newly opened file or network connection, corrupting application data.
</details>

<details>
<summary>3. What is the difference between a process group leader and a session leader?</summary>
A process group leader is a process whose PID equals its Process Group ID (`getpid() == getpgrp()`). A session leader is a process whose PID equals its Session ID (`getpid() == getsid(0)`). A session can contain multiple process groups (e.g. foreground and background groups), and is associated with at most one controlling terminal.
</details>

<details>
<summary>4. When would a systems programmer need to use `syscall()` instead of standard C library functions?</summary>
When the Linux kernel introduces a new system call (e.g. `io_uring_setup`, `pidfd_open`, `memfd_create`) that has not yet been integrated into the installed version of `glibc`. `syscall()` allows the programmer to invoke any valid kernel system call by passing its numeric ID directly.
</details>

<details>
<summary>5. How does a single-instance daemon prevent multiple copies of itself from running?</summary>
By creating a PID file (e.g. `/var/run/daemon.pid`) and placing an exclusive advisory lock on it using `fcntl(fd, F_SETLK, &fl)` or `flock(fd, LOCK_EX | LOCK_NB)`. If a second instance starts and attempts to lock the file, the call fails immediately with `EWOULDBLOCK`, signaling the duplicate process to exit.
</details>

<details>
<summary>6. What happens if a daemon receives the `SIGHUP` signal?</summary>
Historically, `SIGHUP` (Hangup) was sent when a terminal disconnected. Because daemons have no terminal, Unix convention repurposes `SIGHUP` as a signal to instruct the daemon to re-read its configuration files and reopen log files without restarting or dropping active client connections.
</details>

<details>
<summary>7. What is `umask()`, and why does a daemon set it to 0?</summary>
`umask()` sets the process file mode creation mask, which clears specific permission bits when creating new files with `open()` or `creat()`. Daemons set `umask(0)` to ensure that the file permissions specified by the daemon's code are applied accurately without being restricted by the inherited mask of the launching shell environment.
</details>

<details>
<summary>8. How do you log messages from a daemon?</summary>
Using the POSIX syslog API: initialize with `openlog("my_daemon", LOG_PID, LOG_DAEMON)`, emit logs using `syslog(LOG_INFO, "message: %d", val)`, and terminate with `closelog()`. The messages are forwarded to the system logger (`rsyslogd`, `systemd-journald`).
</details>

---

## 11. Practice Exercises

### Easy
1. **Raw Syscall PID**: Write a program that retrieves its own PID using `syscall(SYS_getpid)` instead of `getpid()` and prints the result.
2. **Syslog Hello**: Write a program that writes an informational message to the system log using `openlog()` and `syslog()`.
3. **Session ID Inspector**: Write a program that prints its own PID, PPID, PGID (`getpgrp()`), and SID (`getsid(0)`).

<details>
<summary>Solutions (Easy)</summary>

```c
/* Exercise 1 */
#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <sys/syscall.h>

void print_syscall_pid(void) {
    long pid = syscall(SYS_getpid);
    printf("Syscall PID: %ld\n", pid);
}

/* Exercise 2 */
#include <syslog.h>
void test_syslog(void) {
    openlog("test_app", LOG_PID, LOG_USER);
    syslog(LOG_INFO, "Test log message from C systems exercise");
    closelog();
}
```
</details>

### Medium
4. **PID File Lockfile Enforcer**: Implement a function `int ensure_single_instance(const char *pid_file)` that locks a file with `fcntl()` and writes the current PID.
5. **Config Reloader via SIGHUP**: Implement a daemon event loop that catches `SIGHUP` and sets a `volatile sig_atomic_t reload_config = 1;` flag to reload settings safely.
6. **Descriptor Redirector**: Write a function that closes all file descriptors above 2 and redirects 0, 1, and 2 to `/dev/null`.

<details>
<summary>Solutions (Medium)</summary>

```c
/* Exercise 4: PID Lockfile */
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

int ensure_single_instance(const char *pid_file) {
    int fd = open(pid_file, O_RDWR | O_CREAT, 0640);
    if (fd < 0) return -1;

    struct flock fl;
    fl.l_type = F_WRLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start = 0;
    fl.l_len = 0;

    if (fcntl(fd, F_SETLK, &fl) < 0) {
        close(fd);
        return -1; /* Already running */
    }

    ftruncate(fd, 0);
    char buf[32];
    int len = snprintf(buf, sizeof(buf), "%d\n", getpid());
    write(fd, buf, len);
    return fd; /* Keep open for the lifetime of the process */
}
```
</details>

### Hard
7. **Production Daemon Skeleton**: Implement a production-grade daemon framework including double-fork, PID file locking, SIGHUP configuration reload, SIGTERM clean exit, and syslog output.
8. **Direct Syscall Linux File Copier**: Implement a file copy utility that uses *only* raw `syscall(SYS_open, ...)`, `syscall(SYS_read, ...)`, and `syscall(SYS_write, ...)`, bypassing libc I/O wrappers entirely.
9. **Systemd Watchdog Notifier**: Implement support for systemd watchdog notifications by communicating with the `WATCHDOG_USEC` notification socket (`sd_notify("WATCHDOG=1")`).

<details>
<summary>Solutions (Hard)</summary>

```c
/* Exercise 8: Direct Syscall File Copy */
#define _GNU_SOURCE
#include <unistd.h>
#include <sys/syscall.h>
#include <fcntl.h>

int direct_syscall_copy(const char *src, const char *dst) {
    long sfd = syscall(SYS_open, src, O_RDONLY, 0);
    if (sfd < 0) return -1;

    long dfd = syscall(SYS_open, dst, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (dfd < 0) { syscall(SYS_close, sfd); return -1; }

    char buf[4096];
    long n;
    while ((n = syscall(SYS_read, sfd, buf, sizeof(buf))) > 0) {
        syscall(SYS_write, dfd, buf, n);
    }

    syscall(SYS_close, sfd);
    syscall(SYS_close, dfd);
    return (n < 0) ? -1 : 0;
}
```
</details>

---

## 12. Summary / Cheat Sheet
- **Double Fork**:
  1. `fork()` $	o$ parent exits.
  2. `setsid()` $	o$ creates new session without terminal.
  3. `fork()` $	o$ grandchild cannot acquire terminal.
- **Environment Cleanup**: `chdir("/")` and `umask(0)`.
- **Stdio**: Open `/dev/null` and duplicate to 0, 1, 2 via `dup2()`.
- **Logging**: Use `syslog(LOG_INFO, ...)`.
- **Direct Syscalls**: `syscall(SYS_name, args...)` executes kernel transitions directly.

---

## 13. Further Reading
- W. Richard Stevens, *Advanced Programming in the UNIX Environment*, Chapter 13 (Daemon Processes).
- Michael Kerrisk, *The Linux Programming Interface*, Chapter 37 (Daemons).
- Linux Manual Pages: `man 2 setsid`, `man 3 syslog`, `man 2 syscall`.

---

[← Previous Note: Memory Mapping (mmap) & Shared Memory](./11-mmap-and-shared-memory.md) | [Module Index](README.md) | [Next Note: Terminals and termios](./13-terminals-and-termios.md)
