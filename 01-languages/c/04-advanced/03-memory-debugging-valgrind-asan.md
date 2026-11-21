# Memory Debugging: Valgrind, ASan, and UBsan in C

> Deep architectural exploration of dynamic memory analysis in C: AddressSanitizer shadow memory mechanics, Valgrind Memcheck binary instrumentation, UndefinedBehaviorSanitizer (UBsan), and leak suppression.

| Metadata | Details |
| :--- | :--- |
| **Level** | Advanced |
| **Prerequisites** | [02-core-concepts/08-memory-management.md](../../02-core-concepts/08-memory-management.md), [03-intermediate/10-undefined-behavior-and-sanitizers.md](../../03-intermediate/10-undefined-behavior-and-sanitizers.md) |
| **Time to Read** | 45 minutes |
| **Status** | Complete |
| **Standard Baseline** | ISO C11 / C17 / C23 (GCC & Clang toolchains) |

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
9. [Comparison: ASan vs Valgrind Memcheck](#9-comparison-asan-vs-valgrind-memcheck)
10. [Interview Questions](#10-interview-questions)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary & Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why
Memory errors are the most dangerous and elusive category of bugs in systems software, accounting for $pprox 70\%$ of all critical security vulnerabilities (CVEs) across Chromium, Windows, and the Linux kernel.

Common memory safety violations:
- **Heap Buffer Overflow**: Writing past the boundary of a `malloc()` buffer.
- **Stack Buffer Overflow**: Overwriting local stack arrays and corrupting return addresses.
- **Use-After-Free (UAF)**: Dereferencing pointers after the target memory has been released.
- **Double Free**: Freeing the same heap pointer twice.
- **Uninitialized Memory Reads**: Branching or computing on uninitialized stack/heap garbage.
- **Memory Leaks**: Orphaned heap buffers never reclaimed with `free()`.

Modern tooling provides two distinct paradigms for detecting these bugs: **Compile-Time Instrumentation** (AddressSanitizer) and **Dynamic Binary Translation** (Valgrind Memcheck).

---

## 2. Core Concepts

### 2.1 AddressSanitizer (ASan)
ASan is a compiler-based instrumentation tool built into GCC and Clang (`-fsanitize=address`).
- **Shadow Memory**: Maps every 8 bytes of application virtual address space to 1 byte of shadow memory.
- **Redzones**: Inserts poisoned "redzone" guard buffers around stack, heap, and global variables.
- **Instrumentation**: Every memory read and write is intercepted with a check against shadow memory before execution.
- **Runtime Speed**: Incurs only $pprox 2\times$ CPU slowdown (fast enough for automated test suites and fuzzing).

### 2.2 Valgrind Memcheck
Valgrind runs unmodified binaries using a dynamic Just-In-Time (JIT) synthetic CPU emulator.
- **V-Bits (Validity)**: Tracks whether each bit of memory and CPU registers contains initialized data.
- **A-Bits (Addressability)**: Tracks whether each byte of memory is valid to read or write.
- **Zero Recompilation**: Works directly on existing binary executables.
- **Runtime Speed**: Incurs a heavy $10\times - 30\times$ CPU slowdown.

### 2.3 UndefinedBehaviorSanitizer (UBsan)
Compiles runtime checks for non-memory undefined behaviors (`-fsanitize=undefined`):
- Signed integer overflow.
- Misaligned pointer dereferencing.
- Bit shifts exceeding bit-width (`x << 32` on 32-bit int).
- Division by zero.

---

## 3. How It Works Internally

### Shadow Memory Architecture (AddressSanitizer)

```
Application Memory (8 Bytes):
[ B0 | B1 | B2 | B3 | B4 | B5 | B6 | B7 ]
                       |
Mapped via formula: (Addr >> 3) + 0x7fff8000
                       v
Shadow Memory Byte:
[ 0x00 ]: All 8 bytes are addressable.
[ 0x04 ]: First 4 bytes addressable; last 4 are in redzone!
[ 0xFA ]: Heap redzone (poisoned).
[ 0xFD ]: Freed heap memory (Use-After-Free detector).
```

```mermaid
flowchart TD
    Pointer["Memory Access at Ptr"] --> CalcShadow["Shadow = (Ptr >> 3) + Offset"]
    CalcShadow --> InspectShadow byte poisoned?
    Inspect -->|Yes| CrashReport["CRASH: ASan Address Error (SEGV/Heap-Buffer-Overflow)"]
    Inspect -->|No| Exec["Execute Instruction"]
```

---

## 4. Syntax & Usage

### Compiler Flags for GCC & Clang
```bash
# Debug build with full sanitizers enabled
gcc -std=c11 -Wall -Wextra -g -O1     -fsanitize=address     -fsanitize=undefined     -fsanitize=leak     -fno-omit-frame-pointer     program.c -o program

# Running under Valgrind
valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes ./program
```

---

## 5. Practical Examples

### Example 1: Clean Memory Lifecycle Verification
Goal: Demonstrate clean memory allocation, copy, and deallocation that passes ASan and Valgrind without warnings.

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void safe_string_copy(void) {
    const char *msg = "AddressSanitizer and Valgrind clean";
    size_t len = strlen(msg) + 1;
    char *buf = (char *)malloc(len);
    if (!buf) return;
    memcpy(buf, msg, len);
    printf("Safe buffer copy: %s\n", buf);
    free(buf);
}

int main(void) {
    printf("=== Memory Safety & Sanitizer Verification ===\n");
    safe_string_copy();
    printf("Zero leaks, zero out-of-bounds, zero use-after-free.\n");
    return 0;
}
```

**Verified Output:**
```text
=== Memory Safety & Sanitizer Verification ===
Safe buffer copy: AddressSanitizer and Valgrind clean
Zero leaks, zero out-of-bounds, zero use-after-free.
```

---

## 6. Best Practices
1. **Always Keep `-fno-omit-frame-pointer` with ASan**: Preserves the `rbp` register frame chain, enabling ASan to display accurate, readable stack traces on crashes.
2. **Combine ASan with UBsan in CI/CD**: Run test suites with `-fsanitize=address,undefined` enabled on every pull request.
3. **Use Suppression Files**: For false positives in external pre-compiled third-party libraries, provide a `suppressions.txt` file via `LSAN_OPTIONS=suppressions=supp.txt`.
4. **Use Quarantine Size Tuning**: In ASan, freed memory is quarantined before being returned to the allocator: `ASAN_OPTIONS=quarantine_size_mb=64` to maximize detection of delayed use-after-free bugs.
5. **Never Deploy Sanitizers in Release Builds**: Sanitizer checks increase binary size and double CPU latency. Never ship production binaries with `-fsanitize` enabled.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Is Dangerous | Correct Approach |
| :--- | :--- | :--- |
| Testing only with Valgrind | Valgrind misses stack buffer overflows and global out-of-bounds completely. | Use ASan to catch stack/global errors. |
| Compiling with `-O3` during debugging | Aggressive compiler inlining and instruction reordering scrambles crash line numbers. | Compile with `-g -O1` for sanitizer builds. |
| Ignoring uninitialized memory warnings | Reading uninitialized variables leads to non-deterministic production crashes. | Initialize all stack variables (`int x = 0;`). |
| Mixing ASan with Valgrind | Running an ASan-instrumented binary inside Valgrind causes conflicting shadow memory mappings and crash. | Use ASan or Valgrind independently, never together. |
| Forgetting `-fsanitize=undefined` | Memory sanitizers miss signed overflow, invalid bit shifts, and misaligned pointer casts. | Enable `-fsanitize=undefined` in tandem. |

---

## 8. Performance & Security Notes

### Overhead Comparison

| Tool | CPU Slowdown | Memory Overhead | Catches Stack Overflows? | Catches Uninitialized Reads? |
| :--- | :--- | :--- | :--- | :--- |
| **ASan** | $pprox 2	imes$ | $pprox 2	imes - 3	imes$ | **Yes** | No (Use MSan) |
| **MSan (MemorySanitizer)** | $pprox 2.5	imes$ | $pprox 3	imes$ | No | **Yes** |
| **Valgrind Memcheck** | $pprox 20	imes - 30	imes$ | $pprox 2	imes$ | No | **Yes** |
| **UBsan** | $pprox 1.05	imes - 1.2	imes$| Negligible | No | No |

---

## 9. Comparison: ASan vs Valgrind Memcheck

```
AddressSanitizer (ASan):
- Technique: Compiler instrumentation + Shadow memory.
- Speed: Fast (~2x). Suitable for fuzzing (AFL, libFuzzer).
- Strengths: Catches heap, stack, and global buffer overflows, use-after-free, double-free.
- Limitations: Requires recompilation with GCC/Clang.

Valgrind Memcheck:
- Technique: JIT binary translation (synthetic CPU).
- Speed: Slow (~20-30x).
- Strengths: Works on existing binaries; catches uninitialized reads and memory leaks.
- Limitations: Completely blind to stack and global buffer overflows!
```

---

## 10. Interview Questions

<details>
<summary><strong>1. How does AddressSanitizer map virtual memory to Shadow Memory?</strong></summary>

ASan divides memory into 8-byte blocks. The shadow byte address is computed via bitwise shift and addition:
$$\text{shadow\_address} = (\text{address} \gg 3) + \text{Offset}$$
On 64-bit Linux, `Offset` is `0x7fff8000`. If all 8 bytes are valid, the shadow byte is 0. If $k$ bytes ($1 \le k \le 7$) are valid, shadow byte is $k$. If the byte is in a poisoned redzone or freed quarantine, the shadow byte is negative/special ($> 0x7f$).
</details>

<details>
<summary><strong>2. Why can't Valgrind Memcheck detect Stack Buffer Overflows?</strong></summary>

Valgrind analyzes unmodified machine code at runtime. It does not know the intended layout or boundaries of local variables on the thread stack; to Valgrind, the entire stack frame allocated by `sub rsp, N` is valid addressable memory. If an array on the stack overflows into an adjacent local variable, Valgrind cannot detect it. ASan detects it because the compiler explicitly inserts redzones between local stack variables.
</details>

<details>
<summary><strong>3. What is MemorySanitizer (MSan) and why is it distinct from ASan?</strong></summary>

MSan (`-fsanitize=memory`) detects reads of uninitialized memory. It tracks shadow bitmasks for every bit of memory and register state. It is separate from ASan because combining both would incur prohibitive memory and CPU overhead.
</details>

<details>
<summary><strong>4. What is a Use-After-Return (UAR) bug and how can ASan catch it?</strong></summary>

UAR occurs when a function returns a pointer to a local stack variable, and the caller dereferences it. By default, ASan does not check stack use-after-return to save performance. Enabling `ASAN_OPTIONS=detect_stack_use_after_return=1` replaces stack allocations with fake heap-allocated stack frames that are poisoned upon function exit.
</details>

<details>
<summary><strong>5. What is the difference between "definitely lost", "indirectly lost", and "still reachable" in Valgrind?</strong></summary>

- **Definitely Lost**: No pointer points to the memory block. It is an undeniable memory leak.
- **Indirectly Lost**: The block is referenced only by another block that is itself definitely lost.
- **Still Reachable**: A pointer still points to the block at program termination (e.g. global caches not explicitly freed before `exit()`).
</details>

<details>
<summary><strong>6. How does ASan detect Use-After-Free bugs?</strong></summary>

When `free(ptr)` is called, ASan's custom allocator does not return the block to the OS immediately. Instead, it places the block into a **Quarantine Ring Buffer** and poisons its shadow memory bytes with `0xFD`. If any thread dereferences the pointer while it is quarantined, ASan intercepts the poisoned shadow access and crashes with a UAF report.
</details>

<details>
<summary><strong>7. How can you programmatically poison memory in custom allocators using ASan APIs?</strong></summary>

Include `<sanitizer/asan_interface.h>` and call:
- `ASAN_POISON_MEMORY_REGION(addr, size)`: marks memory invalid for access.
- `ASAN_UNPOISON_MEMORY_REGION(addr, size)`: marks memory valid.
This allows custom Arenas and Pools to leverage ASan's protection!
</details>

<details>
<summary><strong>8. What is ThreadSanitizer (TSan) and what does it detect?</strong></summary>

TSan (`-fsanitize=thread`) detects data races in multi-threaded code: when two threads access the same memory location concurrently without synchronization and at least one access is a write.
</details>

---

## 11. Practice Exercises

### Easy 1: Enable ASan in GCC Command Line
Write the exact GCC command line to compile `main.c` with AddressSanitizer and debug symbols.

<details>
<summary>Solution</summary>

```bash
gcc -std=c11 -Wall -Wextra -g -O1 -fsanitize=address -fno-omit-frame-pointer main.c -o main
```
</details>

### Easy 2: Valgrind Leak Check Command
Write the command to run an executable under Valgrind with detailed leak tracking.

<details>
<summary>Solution</summary>

```bash
valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes ./main
```
</details>

### Easy 3: ASan Environment Options
Set environment variable to halt on first error and produce core dump.

<details>
<summary>Solution</summary>

```bash
export ASAN_OPTIONS="halt_on_error=1:abort_on_error=1"
```
</details>

### Medium 1: Custom Arena ASan Poisoning
Add ASan poisoning hooks to the arena allocator from Note 01.

<details>
<summary>Solution</summary>

```c
#ifdef __SANITIZE_ADDRESS__
#include <sanitizer/asan_interface.h>
#define POISON_REGION(addr, sz) ASAN_POISON_MEMORY_REGION(addr, sz)
#define UNPOISON_REGION(addr, sz) ASAN_UNPOISON_MEMORY_REGION(addr, sz)
#else
#define POISON_REGION(addr, sz) ((void)0)
#define UNPOISON_REGION(addr, sz) ((void)0)
#endif
```
</details>

### Medium 2: Valgrind Suppression File Creation
Write a suppression rule to ignore leaks originating from an external library `libfoo.so`.

<details>
<summary>Solution</summary>

```text
{
   ignore_libfoo_leaks
   Memcheck:Leak
   match-leak-kinds: reachable
   ...
   obj:*/libfoo.so*
}
```
</details>

### Medium 3: Detecting Signed Integer Overflow with UBsan
Write code that intentionally overflows `INT_MAX` and compile with `-fsanitize=undefined` to observe UBsan diagnostics.

<details>
<summary>Solution</summary>

```c
#include <limits.h>
#include <stdio.h>

int main(void) {
    int x = INT_MAX;
    int y = x + 1; // UBsan intercepts signed integer overflow!
    printf("%d\n", y);
    return 0;
}
```
</details>

### Hard 1: Custom Malloc Redzone Wrapper
Implement a debugging `my_malloc()` that prepends and appends 16-byte canary redzones to detect buffer overflows without ASan.

<details>
<summary>Solution</summary>

```c
#define CANARY 0xDEADBEEFCAFEBABEULL

void *debug_malloc(size_t sz) {
    uint8_t *p = (uint8_t *)malloc(sz + 32);
    if (!p) return NULL;
    *(uint64_t *)p = CANARY;
    *(uint64_t *)(p + 8) = (uint64_t)sz;
    *(uint64_t *)(p + 16 + sz) = CANARY;
    return p + 16;
}
```
</details>

### Hard 2: Programmatic Stack Trace Dump on SEGV
Install a `SIGSEGV` signal handler that prints the call stack using `backtrace()` and `backtrace_symbols()`.

<details>
<summary>Solution</summary>

```c
#include <execinfo.h>
#include <signal.h>
#include <unistd.h>

void segv_handler(int sig) {
    void *array[32];
    size_t size = backtrace(array, 32);
    write(STDERR_FILENO, "Fatal SIGSEGV! Stacktrace:
", 27);
    backtrace_symbols_fd(array, size, STDERR_FILENO);
    _exit(1);
}
```
</details>

### Hard 3: CI/CD Multi-Sanitizer Pipeline Design
Design a shell script executing tests across ASan, UBsan, TSan, and Valgrind matrices.

<details>
<summary>Solution</summary>

```bash
#!/bin/bash
set -e
# 1. ASan + UBsan
gcc -g -O1 -fsanitize=address,undefined test.c -o test_asan && ./test_asan
# 2. TSan
gcc -g -O1 -fsanitize=thread test_threads.c -o test_tsan && ./test_tsan
# 3. Valgrind Memcheck
gcc -g -O0 test.c -o test_valgrind && valgrind --error-exitcode=1 ./test_valgrind
```
</details>

---

## 12. Summary & Cheat Sheet

| Sanitizer | Flag | What It Catches | Overhead |
| :--- | :--- | :--- | :--- |
| **AddressSanitizer** | `-fsanitize=address` | OOB heap/stack/global, UAF, double-free | $pprox 2	imes$ |
| **UBsan** | `-fsanitize=undefined` | Overflow, zero-divide, misaligned pointer | $pprox 5\%$ |
| **LeakSanitizer** | `-fsanitize=leak` | Memory leaks on exit | $pprox 5\%$ |
| **ThreadSanitizer** | `-fsanitize=thread` | Multi-threaded data races | $pprox 3	imes$ |
| **Valgrind** | `valgrind --leak-check=full` | Uninitialized reads, leaks (no compile flag) | $pprox 25	imes$ |

---

## 13. Further Reading
- Konstantin Serebryany et al., *AddressSanitizer: A Fast Address Sanity Checker*, USENIX ATC 2012.
- Julian Seward and Nicholas Nethercote, *Using Valgrind to detect undefined value errors with bit-precision*, USENIX 2005.

---

[← Previous Note: Memory Pools](./02-memory-pools.md) | [Advanced C Index](./README.md) | [Next Note: Linking: Static vs Shared](./04-linking-static-vs-shared-libraries.md)
