# Memory Mapping (mmap) and Shared Memory

> Master virtual memory page mappings in C: POSIX `mmap()`, `munmap()`, `msync()`, shared vs private copy-on-write mappings (`MAP_SHARED` vs `MAP_PRIVATE`), anonymous memory mapping, and zero-copy IPC via POSIX shared memory (`shm_open`).

| Metadata | Details |
| :--- | :--- |
| **Level** | Systems / Advanced |
| **Prerequisites** | [01-file-descriptors-and-posix-io.md](./01-file-descriptors-and-posix-io.md), [04-advanced/01-custom-allocators-and-arenas.md](../04-advanced/01-custom-allocators-and-arenas.md) |
| **Time to read** | 35 minutes |
| **Status** | Complete |
| **Standard Baseline**| POSIX.1-2008 / ISO C11 (Linux kernel, glibc, Win32 CreateFileMapping contrast) |

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
Standard file I/O using `read()` and `write()` involves multiple levels of memory copies: data is transferred from physical storage into the operating system kernel's page cache, and then copied across the user/kernel privilege boundary into a user-space memory buffer. For high-throughput databases, search engines, and multi-gigabyte file manipulation, this double-buffering introduces severe memory bus contention and CPU overhead.

**Memory Mapping (`mmap`)** eliminates this overhead by instructing the CPU's Memory Management Unit (MMU) to map file data or kernel memory directly into the process's virtual address space. Once mapped, the file can be accessed directly using standard C pointers and array indexing (`ptr[i]`). Reads and writes bypass user-space system calls entirely, backed directly by the hardware virtual memory subsystem.

---

## 2. Core Concepts
- **Virtual Page Allocation**: `mmap()` assigns a contiguous range of virtual addresses aligned to the system page size (typically 4096 bytes).
- **Mapping Types**:
  - `MAP_SHARED`: Updates made to the memory region are shared with all other processes mapping the same region and are flushed to the underlying file.
  - `MAP_PRIVATE`: Creates a Copy-on-Write (COW) mapping. Modifications are private to the calling process and are never written back to the underlying file on disk.
- **Anonymous Memory (`MAP_ANONYMOUS`)**: A mapping backed by pure RAM rather than a filesystem file descriptor. This is the primary primitive used by custom allocators and `malloc()` to allocate large memory heaps directly from the kernel.
- **POSIX Shared Memory (`shm_open`)**: Allocates named memory-mapped regions in a dedicated tmpfs virtual filesystem (`/dev/shm` on Linux), providing the fastest possible zero-copy IPC mechanism between independent processes.
- **Page Fault Mechanics**: `mmap()` does not read the entire file into RAM at once; memory pages are loaded into physical RAM on-demand by the kernel when the pointer is first dereferenced (lazy loading).

---

## 3. How It Works Internally

### Hardware MMU Page Translation
```
Process Virtual Address Space (e.g. 0x7F001000)
+-------------------------------------------------------+
| Virtual Page 0 | Virtual Page 1 | Virtual Page 2      |
+-------------------------------------------------------+
        |                 |                |
        v (Hardware MMU Page Table Walk)   v (Lazy / Unmapped)
+-----------------------------------+   [Page Fault Exception Triggered]
| Physical Frame in RAM Page Cache  |   Kernel reads 4 KB block from disk
| Shared across all mapping procs   |   Updates page table -> Resumes proc
+-----------------------------------+
        ^
        |
+-------------------------------------------------------+
| Underlying Disk File (Blocks on ext4 / XFS / NVMe)    |
+-------------------------------------------------------+
```

1. When `mmap()` is called, the kernel allocates a `vm_area_struct` representing the virtual address range, but allocates **no physical RAM** for the file contents.
2. When the program executes `char c = ptr[0];`, the CPU MMU attempts to translate the virtual address.
3. Because the page table entry is marked non-present, the CPU triggers a **Minor/Major Page Fault** interrupt.
4. The kernel intercepts the fault, reads the required 4 KB block from the storage device into the system page cache, updates the process page table entry with the physical frame address and access permissions (`PROT_READ | PROT_WRITE`), and resumes the instruction transparently.

---

## 4. Syntax & Usage

### Primary Header Inclusions
```c
#include <sys/mman.h> /* mmap, munmap, msync, mprotect */
#include <sys/stat.h> /* fstat */
#include <fcntl.h>    /* open, O_RDWR, O_CREAT */
#include <unistd.h>   /* close, sysconf, _SC_PAGESIZE */
```

### Signature Overview
```c
void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset);
int munmap(void *addr, size_t length);
int msync(void *addr, size_t length, int flags);
int mprotect(void *addr, size_t length, int prot);
int shm_open(const char *name, int oflag, mode_t mode);
int shm_unlink(const char *name);
```

### Protection Flags (`prot`)
- `PROT_READ`: Pages may be read.
- `PROT_WRITE`: Pages may be written.
- `PROT_EXEC`: Pages may be executed as machine code.
- `PROT_NONE`: Pages may not be accessed (generates instant `SIGSEGV` on access; used for guard pages).

---

## 5. Practical Examples

### Example 1: POSIX Virtual Memory Mapping Simulation
```c
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define PAGE_SIZE 4096

typedef struct {
    uint8_t memory_block[PAGE_SIZE];
    size_t mapped_len;
} MmapVirtualPage;

int main(void) {
    printf("Simulating POSIX Virtual Memory Mapping (mmap / MAP_SHARED)...\n");
    MmapVirtualPage shared_page = {0};
    shared_page.mapped_len = PAGE_SIZE;

    const char *record = "RECORD_ID=90210;STATUS=COMMITTED;TX_ID=0xDEADBEEF";
    memcpy(shared_page.memory_block, record, strlen(record));

    printf("Mapped Virtual Page at %p (Length: %zu bytes)\n", (void *)shared_page.memory_block, shared_page.mapped_len);
    printf("Memory Contents via Direct Pointer Dereference: %s\n", (char *)shared_page.memory_block);
    printf("Synchronizing Page Cache via simulated msync(MS_SYNC)... OK\n");
    return 0;
}
```

```text
Simulating POSIX Virtual Memory Mapping (mmap / MAP_SHARED)...
Mapped Virtual Page at 0061ef18 (Length: 4096 bytes)
Memory Contents via Direct Pointer Dereference: RECORD_ID=90210;STATUS=COMMITTED;TX_ID=0xDEADBEEF
Synchronizing Page Cache via simulated msync(MS_SYNC)... OK
```

### Example 2: In-Place File Reversal via `mmap`
```c
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

int reverse_mapped_file(const char *filepath) {
    int fd = open(filepath, O_RDWR);
    if (fd < 0) return -1;

    struct stat sb;
    if (fstat(fd, &sb) < 0) { close(fd); return -1; }
    size_t len = (size_t)sb.st_size;
    if (len == 0) { close(fd); return 0; }

    char *data = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd); /* Safe to close fd immediately after mmap */
    if (data == MAP_FAILED) return -1;

    /* Reverse bytes in-place directly in memory */
    size_t i = 0, j = len - 1;
    while (i < j) {
        char tmp = data[i];
        data[i] = data[j];
        data[j] = tmp;
        i++; j--;
    }

    /* Force synchronous flush back to physical disk */
    msync(data, len, MS_SYNC);
    munmap(data, len);
    return 0;
}
```

---

## 6. Best Practices
1. **Offset Must Be Page-Aligned**: The `offset` argument passed to `mmap()` must always be a multiple of the system page size (`sysconf(_SC_PAGESIZE)`). Passing unaligned offsets results in `EINVAL`.
2. **Handle `SIGBUS` Signals**: If a mapped file is truncated by another process while you hold an active mapping, accessing memory beyond the truncated boundary triggers a `SIGBUS` (Bus Error) signal, terminating your process unless caught.
3. **Close the File Descriptor After Mapping**: Once `mmap()` returns a valid pointer, the kernel increments internal reference counts on the underlying file. You can (and should) `close(fd)` immediately to avoid holding open descriptor table slots.
4. **Use `msync()` for Persistence Guarantees**: Writes to `MAP_SHARED` modify the kernel page cache asynchronously. To guarantee non-volatile storage durability, call `msync(ptr, len, MS_SYNC)`.
5. **Always Check for `MAP_FAILED`**: On failure, `mmap()` returns `MAP_FAILED` (which is `(void *)-1`), **not** `NULL`. Checking `if (ptr == NULL)` will fail to detect errors!

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It's Wrong | Correct Fix |
| :--- | :--- | :--- |
| Checking `if (ptr == NULL)` for errors | On failure, `mmap()` returns `MAP_FAILED` (`(void *)-1`), not `NULL` | Always check `if (ptr == MAP_FAILED)` |
| Passing an offset that is not a multiple of page size | System calls fail immediately with `errno == EINVAL` | Align offset: `off & ~(sysconf(_SC_PAGESIZE) - 1)` |
| Writing to a `PROT_READ` mapping | Hardware MMU raises page protection fault, terminating process with `SIGSEGV` | Pass `PROT_READ \| PROT_WRITE` |
| Attempting to `mmap` a zero-byte file | Mapping 0 bytes is invalid and fails with `EINVAL` | Check file size via `fstat()`; handle zero-length files separately |
| Forgetting to call `ftruncate()` with `shm_open()` | Newly created shared memory objects have a size of 0; dereferencing triggers instant `SIGBUS` | Always call `ftruncate(fd, size)` immediately after `shm_open()` |
| Leaking address space with missing `munmap()` | In 32-bit processes, unreleased mappings fragment and exhaust the virtual address space | Always pair `mmap()` with `munmap()` |

---

## 8. Performance & Security Notes
- **Zero-Copy Performance**: For files exceeding 100 MB, `mmap` delivers up to a $3	imes$ speedup over `read()`/`write()` loops by eliminating kernel-to-user memory buffer copying.
- **Paging Advice via `madvise()`**: Inform the kernel's virtual memory subsystem about your intended memory access patterns using `madvise()`:
  - `MADV_SEQUENTIAL`: Doubles read-ahead page cache prefetching.
  - `MADV_RANDOM`: Disables read-ahead, saving memory bandwidth on B-tree index searches.
  - `MADV_DONTNEED`: Instructs kernel to drop physical pages immediately, freeing RAM.
- **Executable Memory Protection (W^X / DEP)**: Modern operating systems enforce "Write XOR Execute" (DEP/NX). Never map memory with both `PROT_WRITE` and `PROT_EXEC` simultaneously, as this opens security attack vectors (buffer overflow shellcode execution).

---

## 9. Comparison / When to Use What

| Feature | Standard I/O (`read`/`write`) | Memory Mapping (`mmap`) |
| :--- | :--- | :--- |
| **System Calls** | 1 syscall per buffer chunk | 1 syscall to map; 0 syscalls to read/write |
| **Memory Copies**| 2 copies (Disk $	o$ Page Cache $	o$ User Buffer) | **0 copies** (Direct pointer to Page Cache) |
| **Overhead** | Minimal for tiny files (< 4 KB) | Page allocation overhead; best for large files |
| **Sequential Reads**| Very fast with kernel readahead | Extremely fast |
| **Error Handling** | Returns error code (`-1`) | Triggers signals (`SIGSEGV`, `SIGBUS`) |

---

## 10. Interview Questions

<details>
<summary>1. What happens behind the scenes when a process calls `mmap()` on a 1 GB file?</summary>
The kernel updates the process's Virtual Memory Area (VMA) structures, assigning a 1 GB range of virtual addresses. Crucially, **no physical memory is allocated** and no disk blocks are read. The mapping is purely virtual. Physical memory pages are allocated and loaded from disk lazily via page faults only when the application actually dereferences pointers within that address range.
</details>

<details>
<summary>2. Why does `mmap()` return `MAP_FAILED` instead of `NULL` on error?</summary>
`NULL` (address 0) is technically a valid virtual memory address on some architectures and operating systems (e.g. mapping the zero page). To avoid ambiguity, POSIX defines `MAP_FAILED` as `((void *) -1)`.
</details>

<details>
<summary>3. What is the difference between `MAP_SHARED` and `MAP_PRIVATE`?</summary>
In `MAP_SHARED`, modifications to the memory are visible to all other processes mapping the same file and are eventually written back to the disk file. In `MAP_PRIVATE`, the kernel uses Copy-on-Write: reads are shared with the file, but any write creates a private copy of the modified page in RAM, visible only to the calling process and never flushed to disk.
</details>

<details>
<summary>4. What causes a `SIGBUS` signal when accessing an `mmap` pointer?</summary>
A `SIGBUS` occurs when a process attempts to access a valid virtual page that cannot be backed by the underlying physical storage. Common causes include:
1. The mapped file was truncated by another process to a smaller length.
2. The filesystem became full while the kernel attempted to allocate space for a written page.
3. A hardware I/O read error occurred while paging data from disk.
</details>

<details>
<summary>5. How does `MAP_ANONYMOUS` differ from file-backed mapping?</summary>
`MAP_ANONYMOUS` (or `MAP_ANON`) creates a mapping that is backed by system RAM and swap space rather than an actual file on disk. The `fd` argument is ignored (and should be passed as -1). The allocated pages are initialized to zero. This is the primary mechanism `malloc()` uses to allocate large blocks of heap memory.
</details>

<details>
<summary>6. How do you share memory between two completely unrelated processes using POSIX shared memory?</summary>
Process A calls `shm_open("/my_shm", O_CREAT | O_RDWR, 0660)` and sizes it with `ftruncate(fd, size)`. Process B calls `shm_open("/my_shm", O_RDWR, 0660)`. Both processes then call `mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0)`. The resulting pointer in both processes points to the exact same physical RAM pages, enabling zero-copy shared memory IPC.
</details>

<details>
<summary>7. What does `msync()` do, and what are its flags?</summary>
`msync()` synchronizes the dirty pages in the memory mapping with the physical storage device. Its flags are:
- `MS_ASYNC`: Schedules the write and returns immediately.
- `MS_SYNC`: Blocks until the data is physically committed to non-volatile storage.
- `MS_INVALIDATE`: Asks the kernel to invalidate cached copies of pages so subsequent reads fetch fresh data.
</details>

<details>
<summary>8. Why is it safe to close the file descriptor immediately after `mmap()`?</summary>
Because `mmap()` creates an active reference to the underlying kernel file description in the process's virtual memory subsystem. The kernel increments the reference count of the open file, so closing the integer file descriptor does not destroy the mapping or sever the connection to the disk file.
</details>

---

## 11. Practice Exercises

### Easy
1. **Page Size Query**: Write a small program that retrieves and prints the system page size using `sysconf(_SC_PAGESIZE)`.
2. **Anonymous Memory Allocator**: Use `mmap()` with `MAP_ANONYMOUS | MAP_PRIVATE` to allocate an array of 1000 integers, populate them, and unmap with `munmap()`.
3. **Read-Only File Printer**: Map a text file as `PROT_READ` and write its contents to `STDOUT_FILENO` using a single `write()` call.

<details>
<summary>Solutions (Easy)</summary>

```c
/* Exercise 1 */
#include <stdio.h>
#include <unistd.h>
void print_page_size(void) {
    printf("System Page Size: %ld bytes\n", sysconf(_SC_PAGESIZE));
}

/* Exercise 2 */
#include <sys/mman.h>
#include <stdio.h>
void alloc_anon(void) {
    size_t size = 1000 * sizeof(int);
    int *arr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (arr != MAP_FAILED) {
        arr[0] = 42;
        arr[999] = 100;
        munmap(arr, size);
    }
}
```
</details>

### Medium
4. **Fast File Grep via Mmap**: Write a program that maps a large log file and searches for a substring using `memmem()`, benchmarking against standard `fgets()` loops.
5. **Shared Memory Counter IPC**: Write two programs (`shm_producer.c` and `shm_consumer.c`) that communicate over `/dev/shm` using `shm_open()` and a shared atomic integer.
6. **Memory Protection with `mprotect`**: Map an anonymous page, write to it, use `mprotect(ptr, size, PROT_READ)` to make it read-only, and verify that reads still succeed.

<details>
<summary>Solutions (Medium)</summary>

```c
/* Exercise 6: mprotect Read-Only Protection */
#include <sys/mman.h>
#include <stdio.h>
#include <unistd.h>

void test_mprotect(void) {
    size_t psize = sysconf(_SC_PAGESIZE);
    int *ptr = mmap(NULL, psize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    ptr[0] = 12345;

    /* Transition page to Read-Only */
    mprotect(ptr, psize, PROT_READ);

    /* Reading succeeds */
    printf("Read value: %d\n", ptr[0]);

    /* Note: ptr[0] = 999; would trigger SIGSEGV! */
    munmap(ptr, psize);
}
```
</details>

### Hard
7. **Zero-Copy Memory-Mapped Ring Buffer**: Implement a circular buffer where a single underlying virtual memory block is mapped twice contiguously using two adjacent `mmap()` calls to eliminate ring buffer wrap-around branching!
8. **Append-Only Memory-Mapped Database**: Implement a miniature key-value database engine that maps a 1 GB file. As records are appended, resize the mapping dynamically using Linux `mremap()` or file pre-allocation.
9. **Shared Memory Mutex Synchronization**: Set up a shared memory segment containing a `pthread_mutex_t` initialized with `PTHREAD_PROCESS_SHARED` to synchronize critical sections across two independent processes.

<details>
<summary>Solutions (Hard)</summary>

```c
/* Exercise 9: Inter-Process Shared Mutex */
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>

typedef struct {
    pthread_mutex_t lock;
    int shared_counter;
} SharedRegion;

SharedRegion *setup_shared_mutex(const char *name) {
    int fd = shm_open(name, O_CREAT | O_RDWR, 0660);
    ftruncate(fd, sizeof(SharedRegion));

    SharedRegion *r = mmap(NULL, sizeof(SharedRegion), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);

    /* Configure mutex for inter-process sharing */
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&r->lock, &attr);
    pthread_mutexattr_destroy(&attr);

    return r;
}
```
</details>

---

## 12. Summary / Cheat Sheet
- **mmap**: Maps disk file or RAM directly into process address space.
- **Error Check**: Always verify `ptr != MAP_FAILED` (`(void *)-1`).
- **Flags**:
  - `MAP_SHARED`: Changes committed to file; shared with other processes.
  - `MAP_PRIVATE`: Copy-on-Write; changes stay in RAM and are never written to file.
  - `MAP_ANONYMOUS`: Backed by RAM, not a file.
- **Sync**: `msync(ptr, len, MS_SYNC)` flushes page cache to physical disk.
- **POSIX SHM**: `shm_open()` + `ftruncate()` + `mmap()` = fastest zero-copy IPC.

---

## 13. Further Reading
- W. Richard Stevens, *Advanced Programming in the UNIX Environment*, Chapter 14 (Memory-Mapped I/O).
- Michael Kerrisk, *The Linux Programming Interface*, Chapters 49 & 54 (mmap and POSIX Shared Memory).
- Linux Manual Pages: `man 2 mmap`, `man 2 munmap`, `man 3 shm_open`.

---

[← Previous Note: I/O Multiplexing (select, poll, epoll)](./10-io-multiplexing-select-poll-epoll.md) | [Module Index](README.md) | [Next Note: Daemons and Direct System Calls](./12-daemons-and-system-calls.md)
