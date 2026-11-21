# Systems Programming and POSIX Technical Interview Guide (`06-systems-qa`)

> Master low-level OS systems interview topics: process lifecycles, signal handling, async-signal safety, file descriptors, multiplexing, and concurrency.

| Metadata | Details |
| :--- | :--- |
| **Guide** | `06-systems-qa` |
| **Level** | Advanced |
| **Standard Baseline** | C11 / POSIX.1-2008 / Linux / BSD |

---

## 1. Process Management and Lifecycles

<details>
<summary><strong>Q1: What is the exact mechanism of `fork()`, and how does Copy-on-Write (COW) optimize it?</strong></summary>

`fork()` creates an exact clone of the calling process:
1. The kernel allocates a new task struct and PID for the child.
2. The child duplicates the parent's file descriptor table (incrementing open file description reference counts).
3. **Copy-on-Write (COW):** Rather than copying all physical memory pages immediately, the kernel maps the child's virtual memory pages to the parent's existing physical pages, marking them Read-Only (`RO`).
4. Only when either parent or child attempts a write operation does the CPU trigger a hardware page fault, prompting the kernel to allocate a new physical page frame and copy the 4KB data block.
</details>

<details>
<summary><strong>Q2: What is the difference between a Zombie process and an Orphan process, and how are they reaped?</strong></summary>

- **Zombie Process:** A child process that has terminated (`exit()`), but whose parent has not yet read its exit status via `wait()` or `waitpid()`. It holds an entry in the OS process table.
  - *Remedy:* Parent must call `waitpid(-1, &status, WNOHANG)` or set `signal(SIGCHLD, SIG_IGN)` to auto-reap.
- **Orphan Process:** A child process whose parent terminated before the child finished.
  - *Resolution:* The orphan is immediately adopted by `init` (PID 1) or a system subreaper (`systemd`), which automatically reaps it upon termination.
</details>

---

## 2. Signals and Async-Signal Safety

<details>
<summary><strong>Q3: What makes a function "async-signal-safe", and why is `printf()` banned inside signal handlers?</strong></summary>

An async-signal-safe function can be invoked safely from within a signal handler without causing race conditions or deadlocks if interrupted during another function call.
`printf()` is **strictly prohibited** in signal handlers because:
1. It acquires internal I/O mutex locks (`flockfile`). If a thread is interrupted by a signal while holding this mutex and the handler invokes `printf()`, the process deadlocks immediately.
2. It uses internal non-reentrant static formatting buffers.
Inside signal handlers, only POSIX-specified safe calls (like `write()`, `_exit()`) or writes to `volatile sig_atomic_t` variables are permitted.
</details>

<details>
<summary><strong>Q4: Why should `sigaction()` always be preferred over legacy `signal()`?</strong></summary>

`signal()` has non-portable, implementation-dependent semantics:
- On System V, `signal()` resets the handler to `SIG_DFL` after the first trigger, requiring unsafe re-registration.
- `sigaction()` provides deterministic POSIX semantics: explicit signal masking (`sa_mask`) to block signals during handler execution, and explicit control over whether interrupted system calls restart (`SA_RESTART`).
</details>

---

## 3. I/O Multiplexing and IPC

<details>
<summary><strong>Q5: Compare `select()`, `poll()`, and `epoll` in Linux.</strong></summary>

| Feature | `select()` | `poll()` | `epoll` |
| :--- | :--- | :--- | :--- |
| **Data Structure** | Bitmask (`fd_set`) | Array of `struct pollfd` | Red-Black Tree in kernel |
| **Max Descriptors** | Fixed (`FD_SETSIZE = 1024`) | Unlimited | Unlimited |
| **Time Complexity** | $O(N)$ scanning | $O(N)$ scanning | $O(1)$ event ready list |
| **Buffer Copying** | Re-copied on every call | Re-copied on every call | Reused via kernel state |
| **Best Suited For** | Legacy portability | Small number of FDs | 10,000+ concurrent network connections |
</details>

<details>
<summary><strong>Q6: How does `dup2(oldfd, newfd)` redirect standard output in shell pipelines?</strong></summary>

`dup2()` closes `newfd` (e.g., `STDOUT_FILENO = 1`) if it was open, then atomically duplicates `oldfd` (e.g., write end of a pipe) into slot `newfd`. Any subsequent `write()` or `printf()` calls to stdout now route directly into the pipe buffer.
</details>

---

[← Data Structures Coding](./05-data-structures-coding-problems.md) | [Interview Index](./README.md) | [Next: C vs C++ Differences →](./07-c-vs-cpp.md)
