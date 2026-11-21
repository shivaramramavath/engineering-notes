# Process Memory Layout: Stack, Heap, Data, BSS, and Text

> Comprehensive architectural analysis of a C program's virtual address space: segment organization, object mapping, runtime growth directions, and memory inspection tools.

| Metadata | Details |
| :--- | :--- |
| **Level** | Core Concepts |
| **Prerequisites** | [Compilation Process](../00-setup/03-compilation-process.md), [Scope and Storage Classes](./07-scope-storage-classes.md), [Memory Management](./08-memory-management.md) |
| **Time to Read** | 30 minutes |
| **Status** | Complete |
| **Standard** | ISO C11 / C17 / C23 |

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
9. [Comparison: When to Use What](#9-comparison-when-to-use-what)
10. [Interview Questions](#10-interview-questions)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary & Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

When you run a compiled C binary, the operating system kernel creates a **process** and assigns it a private, isolated **Virtual Address Space**. Rather than exposing physical RAM directly, the CPU's Memory Management Unit (MMU) maps this virtual space to physical memory pages.

Inside every C process, memory is rigorously partitioned into distinct logical segments, each possessing specific access permissions (Read, Write, Execute) and storage lifecycles:
1. **`.text` Segment**: Machine code instructions (Read-Only, Executable).
2. **`.rodata` Segment**: Constant literals and read-only tables (Read-Only).
3. **`.data` Segment**: Initialized global and static variables (Read-Write).
4. **`.bss` Segment**: Uninitialized global and static variables, zero-initialized by the OS (Read-Write).
5. **Heap Segment**: Dynamically allocated memory via `malloc()`, growing upward toward higher addresses (Read-Write).
6. **Memory Mapping Segment**: Shared libraries and memory-mapped files (`mmap`).
7. **Stack Segment**: Function call frames and automatic variables, growing downward toward lower addresses (Read-Write).

Understanding this process memory layout is fundamental to debugging segmentation faults, diagnosing stack overflows, optimizing cache locality, and comprehending binary security mechanisms like Address Space Layout Randomization (ASLR).

---

## 2. Core Concepts

### Memory Segment Taxonomy
- **Text (`.text`)**: Houses compiled CPU instructions. It is marked Read-Only and Executable (`RX`). Any attempt to modify code at runtime triggers a Segmentation Fault (`SIGSEGV`).
- **Read-Only Data (`.rodata`)**: Stores string literals (`"Hello"`) and global `const` objects. Marked Read-Only (`R`).
- **Initialized Data (`.data`)**: Stores global and static variables with non-zero initializers: `int g_count = 10;`. Allocated in the executable file on disk and loaded into memory on startup.
- **Block Started by Symbol (`.bss`)**: Stores uninitialized (or zero-initialized) global and static variables: `int g_buffer[1000];`. Takes zero space in the binary file on disk; the OS loader allocates anonymous pages and zeros them upon execution.
- **Heap**: Managed by the libc allocator (`malloc`, `free`). Starts immediately above `.bss` and expands upward via `brk()` / `sbrk()` system calls or anonymous `mmap()` mappings.
- **Stack**: Stores function parameters, local automatic variables, and return instruction pointers. Managed automatically by CPU stack pointer registers (`RSP` / `ESP`). Grows downward.

---

## 3. How It Works Internally

### Standard Linux x86-64 Process Virtual Address Space

```
 high  0x7FFFFFFFFFFF  ┌──────────────────────────────────────────────┐
                       │ Kernel Space (Inaccessible to User Space)    │
                       ├──────────────────────────────────────────────┤
                       │ Environment Variables & argv Strings         │
                       ├──────────────────────────────────────────────┤
                       │ Stack Segment (Grows DOWNWARD)               │
                       │   - Function frames, local auto variables    │
                       │   - RSP register points to top of stack      │
                       │                    ↓↓                        │
                       ├──────────────────────────────────────────────┤
                       │ Memory Mapping Segment                       │
                       │   - Shared dynamic libraries (.so, DLLs)     │
                       │   - mmap anonymous memory                    │
                       ├──────────────────────────────────────────────┤
                       │                    ↑↑                        │
                       │ Heap Segment (Grows UPWARD)                  │
                       │   - malloc() / calloc() / realloc()          │
                       │   - Managed via brk / sbrk                   │
                       ├──────────────────────────────────────────────┤
                       │ .bss Segment (Uninitialized Data, Zeroed)    │
                       ├──────────────────────────────────────────────┤
                       │ .data Segment (Initialized Globals/Statics)  │
                       ├──────────────────────────────────────────────┤
                       │ .rodata Segment (String literals, consts)    │
                       ├──────────────────────────────────────────────┤
  low  0x000000400000  │ .text Segment (Executable Machine Code)      │
       0x000000000000  └──────────────────────────────────────────────┘
                       (Null Pointer Trap Zone - Unmapped, causes SEGV)
```

---

## 4. Syntax & Usage

```c
#include <stdio.h>
#include <stdlib.h>

/* .data segment: Initialized non-zero */
int g_counter = 100;

/* .bss segment: Zero-initialized by kernel loader */
int g_raw_buffer[4096];

/* .rodata segment: Read-only */
const char g_version[] = "v2.5.0";

void inspect_addresses(void) {
    /* Stack: automatic duration */
    int local_stack = 42;

    /* Heap: dynamic allocation */
    int *dynamic_heap = malloc(sizeof(int));

    printf("Stack Address: %p\n", (void*)&local_stack);
    printf("Heap Address:  %p\n", (void*)dynamic_heap);

    free(dynamic_heap);
}
```

> [!NOTE]
> ### Origin of the Name "BSS"
> "BSS" stands for **Block Started by Symbol**, an assembly pseudo-operator from the IBM 704 assembler (1950s). In modern ELF binaries, `.bss` allows reserving gigabytes of memory space without increasing the compiled binary file size on disk.

---

## 5. Practical Examples

### Complete Demonstration Program
The following program prints the exact virtual addresses of objects in `.text`, `.rodata`, `.data`, `.bss`, heap, and stack, verifying the architectural segment hierarchy.

```c
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

/* Initialized global -> .data segment */
int g_initialized = 42;

/* Uninitialized global -> .bss segment (zero-initialized) */
int g_uninitialized;

int main(void) {
    printf("=== C Process Memory Layout Demo ===\n");

    /* Local variable -> Stack */
    int stack_var = 100;

    /* Dynamic allocation -> Heap */
    int *heap_ptr = malloc(sizeof(int));
    if (!heap_ptr) return 1;
    *heap_ptr = 200;

    /* String literal -> .rodata */
    const char *rodata_str = "Read Only String";

    printf(".text (main fn):       %p\n", (void*)(uintptr_t)main);
    printf(".rodata (string lit):  %p\n", (void*)rodata_str);
    printf(".data (global init):   %p (val: %d)\n", (void*)&g_initialized, g_initialized);
    printf(".bss (global uninit):  %p (val: %d)\n", (void*)&g_uninitialized, g_uninitialized);
    printf("Heap (malloc block):   %p\n", (void*)heap_ptr);
    printf("Stack (local var):     %p\n", (void*)&stack_var);

    free(heap_ptr);
    return 0;
}
```

```text
=== C Process Memory Layout Demo ===
.text (main fn):       00401489
.rodata (string lit):  0040b08a
.data (global init):   0040a004 (val: 42)
.bss (global uninit):  0040ea24 (val: 0)
Heap (malloc block):   00c61dc8
Stack (local var):     0061ff14
```

### Real-World Example 2: Inspecting Live Process Virtual Memory via `/proc/self/maps`
On Linux, processes can inspect their own live virtual address page mappings, permissions, and backing files directly by reading `/proc/self/maps`:

```c
#include <stdio.h>

void print_process_memory_maps(void) {
    FILE *f = fopen("/proc/self/maps", "r");
    if (!f) {
        perror("Unable to open /proc/self/maps (non-Linux OS)");
        return;
    }
    char line[256];
    printf("--- /proc/self/maps Virtual Page Table ---\n");
    while (fgets(line, sizeof line, f)) {
        printf("%s", line);
    }
    fclose(f);
}
```
*Sample `/proc/self/maps` output snippet:*
```text
00400000-00452000 r-xp 00000000 08:02 1234567    /usr/bin/my_app    (.text)
00651000-00652000 r--p 00051000 08:02 1234567    /usr/bin/my_app    (.rodata)
00652000-00653000 rw-p 00052000 08:02 1234567    /usr/bin/my_app    (.data)
00653000-00674000 rw-p 00000000 00:00 0          [heap]             (Heap)
7ffc3a100000-7ffc3a121000 rw-p 00000000 00:00 0  [stack]            (Stack)
```


*Note: Observe how the addresses increase logically from `.text` (`00401489`) up to the `.data`/`.bss` segments, into the Heap (`00c61dc8`), with the Stack residing at a high address range (`0061ff14`).*

---

## 6. Best Practices

1. **Keep Large Buffers Off the Stack**: Default thread stack limits are small (often 8 MB on Linux, 1 MB on Windows). Declaring huge local arrays `int big[1000000]` causes silent stack overflow crashes. Allocate large buffers on the heap via `malloc()`.
2. **Take Advantage of `.bss` Zero-Initialization**: Global and static variables are guaranteed by standard ISO C to be initialized to zero before `main()` executes. You do not need to write manual initialization loops for global buffers.
3. **Inspect Binaries with `size` and `readelf`**: Use `size my_program` to inspect the `.text`, `.data`, and `.bss` footprints of your application during CI builds.
4. **Never Return Addresses of Stack Objects**: Stack memory is invalidated when the function returns. The address becomes an immediate dangling pointer.
5. **Always Free Heap Blocks Exactly Once**: Stack memory cleans up automatically; heap memory requires explicit release via `free()`.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It's Wrong | The Fix |
| :--- | :--- | :--- |
| **Stack Overflow via huge local array** | Exceeds operating system stack memory limit (typically 1–8 MB), crashing with `SIGSEGV`. | Allocate on the heap (`malloc`) or declare at file scope (`static`). |
| **Writing to string literals** | String literals reside in read-only `.rodata`. Writing `char *s = "hi"; s[0]='H';` triggers `SIGSEGV`. | Use a mutable stack array: `char s[] = "hi";`. |
| **Assuming stack variables are zeroed** | Automatic stack variables contain whatever arbitrary garbage was left by previous function calls. | Always initialize local variables explicitly: `int x = 0;`. |
| **Stack pointer escaping function scope** | Returning `&local_var` returns an address of an invalidated stack frame. | Return by value or allocate dynamically. |
| **Redundant explicit zero-init in `.data`** | Initializing global buffers with `{0}` or non-zero values bloats the binary disk footprint. | Leave uninitialized to place in `.bss` (takes 0 bytes on disk). |
| **Confusing Stack vs Heap allocation costs** | Stack allocation is a single register subtraction instruction ($O(1)$); heap allocation traverses free lists. | Use stack for small, short-lived variables. |

---

## 8. Performance & Security Notes

### Stack vs. Heap Allocation Performance
- **Stack Allocation**: Consists of a single CPU instruction modifying the stack pointer register (e.g., `sub rsp, 32`). Deallocation is equally instantaneous (`add rsp, 32`). Allocation time is $O(1)$ and takes less than 1 nanosecond.
- **Heap Allocation**: Invokes libc heap management code, searches segregated free lists (tcache, fastbins), locks mutexes in multithreaded programs, and may execute kernel system calls (`brk`/`mmap`). Allocation time can take hundreds of nanoseconds.

### Security: ASLR and Stack Smashing
1. **Address Space Layout Randomization (ASLR)**: Modern operating systems randomize the base addresses of the stack, heap, and shared libraries on every execution run. This prevents attackers from hardcoding fixed return addresses in shellcode exploits.
2. **Stack Smashing and Stack Canaries**: Compilers with `-fstack-protector-strong` place a random guard integer (canary) between local arrays and the saved return instruction pointer. Before returning, the function verifies the canary; if overwritten by a buffer overflow, the process aborts immediately.

---

## 9. Comparison: When to Use What

| Segment | Lifetime | Size Limit | Allocation Speed | Thread Safe? |
| :--- | :--- | :--- | :--- | :--- |
| **Stack** | Function Block | Small (1–8 MB) | Instant (1 CPU cycle) | Yes (Private per thread) |
| **Heap** | Manual (`free`) | Large (RAM limits) | Slower (Allocator logic) | Needs locks / arenas |
| **.bss** | Entire Program | Large (RAM limits) | Free at load time | Shared (Requires sync) |
| **.data** | Entire Program | Large (Disk + RAM) | Loaded from binary | Shared (Requires sync) |
| **.rodata** | Entire Program | Large (Disk + RAM) | Loaded from binary | Safe (Read-only) |

---

## 10. Interview Questions

<details>
<summary><strong>1. What is the fundamental difference between the <code>.data</code> and <code>.bss</code> segments?</strong></summary>

Both `.data` and `.bss` store variables with static storage duration (globals and `static` locals).
`.data` contains variables that are initialized with non-zero values; these values are stored inside the compiled executable file on disk and loaded into memory on startup.
`.bss` contains variables that are uninitialized or initialized to zero; it occupies zero bytes in the executable file on disk, with the operating system allocating and zeroing the memory pages at load time.
</details>

<details>
<summary><strong>2. In which direction do the stack and heap grow on modern x86 architectures?</strong></summary>

On x86 and most modern architectures, the **stack grows downward** (from higher memory addresses toward lower memory addresses), while the **heap grows upward** (from lower memory addresses toward higher memory addresses).
</details>

<details>
<summary><strong>3. What happens when a process experiences a Stack Overflow?</strong></summary>

When function call frames or large local arrays exceed the allocated stack segment boundary, the stack pointer touches an unmapped **guard page** placed by the operating system immediately below the stack. The CPU MMU triggers a page fault, which the kernel translates into a Segmentation Fault (`SIGSEGV`), terminating the process immediately.
</details>

<details>
<summary><strong>4. Where does a <code>static</code> local variable inside a function reside?</strong></summary>

A `static` local variable does not reside on the function call stack. It resides in the **`.data` segment** (if initialized with a non-zero value) or the **`.bss` segment** (if uninitialized or initialized to zero). Its lifetime persists across the entire duration of the program.
</details>

<details>
<summary><strong>5. What tool can you use to inspect the size of memory segments in a compiled C program?</strong></summary>

The standard GNU binutils command **`size`**:
`size my_program` outputs the exact byte counts for the `text`, `data`, and `bss` sections. Detailed sections can be inspected using `readelf -S my_program` or `objdump -h my_program`.
</details>

<details>
<summary><strong>6. Why is a string literal placed in <code>.rodata</code> rather than the stack?</strong></summary>

String literals are constant byte arrays intended to be shared across identical occurrences in the translation unit (string pooling). Placing them in the read-only `.rodata` segment allows the OS to share the physical memory page across multiple instances and protect it from accidental mutation with hardware memory write protection.
</details>

<details>
<summary><strong>7. How do multiple threads in the same process interact with memory segments?</strong></summary>

All threads in a single process share the exact same `.text`, `.rodata`, `.data`, `.bss`, and heap segments. However, **each thread is allocated its own private, isolated stack segment** to manage its independent call frames and local variables.
</details>

<details>
<summary><strong>8. What is the purpose of the memory mapping segment between the heap and the stack?</strong></summary>

The memory mapping segment is used by the OS to load dynamic shared libraries (`.so` on Linux, `.dll` on Windows) and to fulfill large anonymous memory allocations requested via `mmap()` (such as `malloc` requests exceeding 128 KB).
</details>

---

## 11. Practice Exercises

### Exercise 1: Segment Footprint Inspector (Easy)
Declare an uninitialized global array of 100,000 integers. Check the compiled binary size on disk. Then initialize it with non-zero values and observe how the file size expands.

<details>
<summary>View Solution</summary>

```c
/* Version 1: .bss segment (Zero bytes added to disk binary) */
int large_buffer[100000];
int main(void) { return large_buffer[0]; }

/* Version 2: .data segment (Adds ~400 KB directly to binary file on disk!) */
/* int large_buffer[100000] = { 1, 2, 3 }; */
```
Inspect with: `gcc v1.c -o v1 && size v1` vs `gcc v2.c -o v2 && size v2`.
</details>

### Exercise 2: Stack Growth Direction Detector (Medium)
Write a C program that determines whether the system stack grows upward or downward by comparing the memory addresses of two local variables in nested function calls.

<details>
<summary>View Solution</summary>

```c
#include <stdio.h>

void check_stack_growth(int *first_addr) {
    int second_var;
    if (&second_var < first_addr) {
        printf("Stack grows DOWNWARD (standard x86/ARM)\n");
    } else {
        printf("Stack grows UPWARD\n");
    }
}

int main(void) {
    int first_var;
    check_stack_growth(&first_var);
    return 0;
}
```
</details>

### Exercise 3: Runtime Heap Growth Tracker (Hard)
Write a program that allocates multiple small blocks using `malloc()` and inspects whether newly returned pointers are situated at increasing memory addresses, demonstrating heap expansion.

<details>
<summary>View Solution</summary>

```c
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

int main(void) {
    void *p1 = malloc(64);
    void *p2 = malloc(64);
    void *p3 = malloc(64);

    printf("Block 1: %p\n", p1);
    printf("Block 2: %p\n", p2);
    printf("Block 3: %p\n", p3);

    if ((uintptr_t)p2 > (uintptr_t)p1 && (uintptr_t)p3 > (uintptr_t)p2) {
        printf("Heap is expanding UPWARD into higher memory addresses.\n");
    }

    free(p1);
    free(p2);
    free(p3);
    return 0;
}
```
</details>

---

## 12. Summary & Cheat Sheet

```c
/* Process Segments Quick Map */
.text     /* Machine code instructions (RX) */
.rodata   /* String literals, const globals (R) */
.data     /* Non-zero initialized globals and static locals (RW) */
.bss      /* Zero-initialized globals and static locals (RW, 0 bytes on disk) */
Heap      /* Dynamic malloc() blocks, grows upward (RW) */
Stack     /* Local auto variables, function frames, grows downward (RW) */

/* CLI Inspection Tools */
size binary              /* View segment sizes in bytes */
readelf -S binary        /* View all ELF sections */
nm -S binary             /* View symbol names and sections */
cat /proc/<pid>/maps     /* Inspect live process virtual address mappings (Linux) */
```

---

## 13. Further Reading
- ISO/IEC 9899:2018 (C17 Standard), §6.2.4 "Storage durations of objects".
- *Computer Systems: A Programmer's Perspective* (3rd ed.) by Bryant & O'Hallaron (Chapter 7: "Linking", Chapter 9: "Virtual Memory").
- *The Linux Programming Interface* by Michael Kerrisk (Chapter 6: "Processes", Process Memory Layout).
- Ulrich Drepper: *ELF Handling For Symbol Versioning and Memory Layout*.

---

[← Previous: Struct Padding & Alignment](./19-struct-padding-and-alignment.md) | [Section Index](./README.md) | [Next: Flexible Array Members →](./21-flexible-array-members.md)
