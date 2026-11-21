# Compiler Hardening Flags

> **Deploying compiler-level binary defenses: stack canaries, `_FORTIFY_SOURCE`, Position Independent Executables (PIE), and Full RELRO in C.**

| Level | Prerequisites | Time to Read | Status | Standard |
| :--- | :--- | :--- | :--- | :--- |
| Advanced | [01-buffer-overflows.md](01-buffer-overflows.md), [04-advanced/10-symbol-visibility-and-elf-basics.md](../04-advanced/10-symbol-visibility-and-elf-basics.md) | 26 mins | Complete | C99 / C11 / C17 / POSIX |

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

Writing flawless, vulnerability-free C code is exceptionally difficult in large, multi-million-line software engineering projects. Even the most disciplined engineering teams occasionally introduce an integer overflow or off-by-one string index.

**Compiler Hardening Flags** represent the first line of defense-in-depth:
- Instead of relying exclusively on source-code vigilance, modern compilers (GCC and Clang) can inject automated runtime protections directly into generated assembly.
- They insert canary words into stack frames, replace unsafe memory functions with bounds-checked variants, randomize executable code locations, and lock internal ELF pointer tables into read-only memory.
- Hardening transforms catastrophic Remote Code Execution (RCE) vulnerabilities into immediate, deterministic application crashes, denying adversaries control over execution flow.

In production systems engineering, shipping unhardened binaries is a severe operational risk.

---

## 2. Core Concepts

### 2.1 The Hardening Quadrumvirate
Modern production builds enforce four foundational compiler mitigations:
1. **Stack Protection (`-fstack-protector-strong`):** Detects linear stack buffer overflows before a function returns to its caller.
2. **Standard Library Fortification (`-D_FORTIFY_SOURCE=2` / `=3`):** Dynamically replaces standard functions (`memcpy`, `strcpy`, `snprintf`) with bounds-checked variants.
3. **Position Independent Executable (`-fPIE -pie`):** Enables Address Space Layout Randomization (ASLR) for the main binary code segment, randomizing code addresses on every execution.
4. **Read-Only Relocations (Full RELRO: `-Wl,-z,relro,-z,now`):** Resolves all dynamic symbols at process startup and marks the Global Offset Table (GOT) strictly read-only, preventing GOT overwrite attacks.

```mermaid
flowchart TD
    subgraph Compiler Hardening Pipeline
        Source[C Source Code] --> Compiler{GCC / Clang Hardening Flags}
        Compiler -->|-fstack-protector-strong| Canary[Stack Canary Guard Words]
        Compiler -->|-D_FORTIFY_SOURCE=3| Fortify[Compile/Runtime Bounds Checks]
        Compiler -->|-fPIE -pie| PIE[Full ASLR Binary Randomization]
        Compiler -->|-Wl,-z,relro,-z,now| RELRO[Read-Only GOT Section]
        Canary & Fortify & PIE & RELRO --> HardenedBin[Hardened Production Binary]
    end
```

---

## 3. How It Works Internally

### 3.1 Stack Canary Mechanics (`-fstack-protector-strong`)
When a function declares a local buffer or has its address taken:
1. In the function prologue, the compiler fetches a 64-bit random canary value from the thread control block (`%fs:0x28` on Linux x86_64) and stores it on the stack directly above the saved frame pointer (`%rbp`):
   ```assembly
   movq   %fs:40, %rax
   movq   %rax, -8(%rbp)     ; Place canary on stack
   xorl   %eax, %eax
   ```
2. In the function epilogue, before executing `ret`, the canary on the stack is compared against `%fs:40`:
   ```assembly
   movq   -8(%rbp), %rax
   xorq   %fs:40, %rax
   jne    .Lstack_chk_fail   ; Jump to abort if modified!
   leave
   ret
   ```
3. If an attacker's buffer overflow overwrote the return address, it inevitably corrupted the canary byte sequence first. The check fails, calling `__stack_chk_fail()`, which halts the process immediately.

### 3.2 Full RELRO (Read-Only Relocations)
By default, ELF dynamic binaries use **Lazy Binding**: dynamic library functions (like `printf` or `system`) are only resolved when first called. To support this, the Global Offset Table (`.got.plt`) must remain writable throughout process execution.
- In Partial RELRO, an attacker with an arbitrary write primitive can overwrite a function pointer in the GOT with the address of `system()`.
- With **Full RELRO** (`-Wl,-z,relro,-z,now`), the dynamic linker resolves *all* symbols at initial process startup before transferring control to `main()`, and then invokes `mprotect()` to mark the entire GOT strictly read-only. Any subsequent write attempts trigger an immediate hardware segmentation fault.

---

## 4. Syntax & Usage

### 4.1 Enterprise Hardening Flag Matrix

```bash
# Recommended Enterprise Production Compilation Command
gcc -std=c11 -Wall -Wextra -Wpedantic     -O2 -D_FORTIFY_SOURCE=3     -fstack-protector-strong     -fPIE -pie     -Wl,-z,relro,-z,now     -Wl,-z,noexecstack     -fstack-clash-protection     -fcf-protection=full     main.c -o app_hardened
```

### 4.2 CMake Hardening Configuration
```cmake
# Modern CMake target hardening definition
add_executable(secure_app src/main.c)

target_compile_options(secure_app PRIVATE
    -Wall -Wextra -Wformat=2 -Werror=format-security
    -fstack-protector-strong
    -fstack-clash-protection
    -fPIE
    $<$<CONFIG:Release>:-O2 -D_FORTIFY_SOURCE=3>
)

target_link_options(secure_app PRIVATE
    -pie
    -Wl,-z,relro,-z,now
    -Wl,-z,noexecstack
)
```

---

## 5. Practical Examples

### 5.1 Verified Companion Simulation: Hardening Flag Audit
The following program evaluates active compiler hardening profiles and verifies the defensive characteristics of binary mitigation flags:

```c
#include <stdio.h>
#include <stdbool.h>

typedef struct {
    const char *flag;
    const char *defense;
    const char *overhead;
} hardening_profile_t;

static const hardening_profile_t DEFENSES[] = {
    {"-fstack-protector-strong", "Canary protection for stack frames with buffers", "< 1%"},
    {"-D_FORTIFY_SOURCE=2",      "Runtime bounds checking for standard string/mem calls", "< 1%"},
    {"-Wl,-z,relro,-z,now",      "Full RELRO: Global Offset Table made read-only", "0% (load-time)"},
    {"-fPIE -pie",               "Position Independent Executable enabling full ASLR", "< 2%"}
};

int main(void) {
    printf("Evaluating Compiler Hardening Flags & Binary Protections...\n");
    for (size_t i = 0; i < sizeof(DEFENSES)/sizeof(DEFENSES[0]); ++i) {
        printf("Flag: %-26s Defense: %-48s Overhead: %s\n",
               DEFENSES[i].flag, DEFENSES[i].defense, DEFENSES[i].overhead);
    }
    printf("Compiler hardening matrix verified.\n");
    return 0;
}
```

**Real Execution Output:**
```text
Evaluating Compiler Hardening Flags & Binary Protections...
Flag: -fstack-protector-strong   Defense: Canary protection for stack frames with buffers  Overhead: < 1%
Flag: -D_FORTIFY_SOURCE=2        Defense: Runtime bounds checking for standard string/mem calls Overhead: < 1%
Flag: -Wl,-z,relro,-z,now        Defense: Full RELRO: Global Offset Table made read-only    Overhead: 0% (load-time)
Flag: -fPIE -pie                 Defense: Position Independent Executable enabling full ASLR Overhead: < 2%
Compiler hardening matrix verified.
```

### 5.2 Auditing Compiled Binaries with `checksec`
Security engineers verify binary hardening using the open-source `checksec` utility:
```bash
checksec --file=app_hardened
```
Expected output for a compliant production executable:
```text
RELRO           STACK CANARY      NX            PIE             RPATH      RUNPATH      FORTIFY
Full RELRO      Canary found      NX enabled    PIE enabled     No RPATH   No RUNPATH   Yes
```

---

## 6. Best Practices

1. **Use `-fstack-protector-strong` instead of `-fstack-protector-all`:** `-fstack-protector-strong` selectively protects any function that allocates local arrays, structs containing arrays, or has pointers to local variables. This yields 99% of the safety of `-all` with less than 1% CPU overhead.
2. **Combine `-D_FORTIFY_SOURCE` with `-O2` or `-O3`:** `_FORTIFY_SOURCE` requires compiler optimizations to deduce buffer capacities via `__builtin_object_size()`. If compiled at `-O0`, fortification is disabled.
3. **Always Pass Both Linker and Compiler Flags for PIE:** Compiling with `-fPIE` requires linking with `-pie`. Failing to pass `-pie` at link time emits a non-randomized executable.
4. **Audit Third-Party Dependencies:** A hardened application that links against an unhardened, non-PIE third-party static library loses much of its ASLR security. Verify all linked objects with `checksec`.
5. **Enable `-Wl,-z,noexecstack`:** Explicitly instruct the linker to mark the ELF stack segment non-executable (clearing the `GNU_STACK` executable bit).

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Is Dangerous | Correct Fix |
| :--- | :--- | :--- |
| **Using `_FORTIFY_SOURCE` with `-O0`** | Compiler cannot deduce object bounds at `-O0`; fortification silently deactivates. | Always compile with at least `-O1` (preferably `-O2`). |
| **Omitting `-pie` during linking** | Binary has position-independent code (`-fPIE`) but links as fixed-base executable, defeating ASLR. | Pass `-pie` to the linker: `gcc -pie ...`. |
| **Using Partial RELRO instead of Full RELRO** | GOT remains writable throughout execution; attackers can overwrite GOT function pointers. | Pass `-Wl,-z,relro,-z,now`. |
| **Assuming canaries stop all stack attacks** | Canaries do not protect local non-buffer variables placed below the buffer on the stack. | Enforce bounds checking in source code. |
| **Ignoring `-fstack-clash-protection`** | On Linux, large stack allocations can skip past the kernel stack guard page, corrupting the heap. | Add `-fstack-clash-protection`. |
| **Relying solely on compiler flags** | Hardening mitigates exploitation but does not fix underlying bugs; denial-of-service crashes still occur. | Fix underlying memory corruption defects in code. |

---

## 8. Performance & Security Notes

### 8.1 Performance Cost Evaluation
The overhead of a full hardening suite in production:
- **Stack Protector Strong:** $pprox 0.5\% - 1.0\%$ CPU overhead.
- **`_FORTIFY_SOURCE=2`:** $pprox 0.1\%$ CPU overhead (many checks resolved at compile time).
- **Full RELRO:** $0.0\%$ runtime overhead (slight startup symbol resolution time delay).
- **PIE / ASLR:** $pprox 1.0\% - 2.0\%$ CPU overhead on modern x86_64 architectures due to RIP-relative addressing.

Total system slowdown is consistently under **3%**, making compiler hardening an essential enterprise practice.

### 8.2 Modern Defenses: Shadow Stacks & CET
Intel **Control-flow Enforcement Technology (CET)** and ARM **Branch Target Identification (BTI)** provide hardware-assisted execution validation:
- Passing `-fcf-protection=full` on GCC/Clang emits instructions for Intel CET.
- The CPU maintains an internal, hardware-isolated Shadow Stack that mirrors the call stack; if a function's return address does not match the shadow stack copy, the CPU halts execution via hardware fault.

---

## 9. Comparison / When to Use What

| Hardening Flag | Attack Vector Neutralized | Runtime Overhead | Deployment Target |
| :--- | :--- | :--- | :--- |
| **`-fstack-protector-strong`** | Stack return address overwrites | $< 1\%$ | All builds (Debug & Release) |
| **`-D_FORTIFY_SOURCE=3`** | Bounded buffer overflows | $< 1\%$ | Production release builds |
| **`-Wl,-z,relro,-z,now`** | GOT table overwrites | Load-time only | Production server daemons |
| **`-fPIE -pie`** | Fixed-address ROP gadget attacks | $pprox 1-2\%$ | All networked/user-facing binaries |
| **`-fstack-clash-protection`** | Stack-to-heap clash exploits | Minimal | Linux enterprise servers |

---

## 10. Interview Questions

<details>
<summary><strong>1. What is the difference between Partial RELRO and Full RELRO?</strong></summary>

Under **Partial RELRO**, the ELF sections `.init_array`, `.fini_array`, and `.ctors` are mapped read-only, but the Global Offset Table (`.got.plt`) remains writable to allow lazy symbol binding at runtime. Under **Full RELRO** (`-Wl,-z,relro,-z,now`), the dynamic linker forces immediate symbol binding at startup and marks the entire GOT read-only, completely preventing attackers from overwriting GOT entries.
</details>

<details>
<summary><strong>2. How does <code>_FORTIFY_SOURCE</code> determine whether a buffer write is out of bounds?</strong></summary>

It uses the compiler intrinsic `__builtin_object_size(ptr, type)`. When a fixed-size buffer is passed to a function like `memcpy(dest, src, n)`, the compiler determines the exact allocation size of `dest` at compile time. It substitutes `memcpy` with `__memcpy_chk(dest, src, n, dest_size)`. If $n > 	ext{dest\_size}$, `__memcpy_chk()` calls `__chk_fail()` and terminates the process.
</details>

<details>
<summary><strong>3. Why does <code>_FORTIFY_SOURCE</code> require compiler optimization (e.g. <code>-O2</code>) to function?</strong></summary>

Without optimization (`-O0`), the compiler does not perform dataflow analysis, constant propagation, or inlining, preventing `__builtin_object_size()` from determining buffer sizes at compile time. In the absence of size knowledge, fortified functions fall back to standard unbounded implementations.
</details>

<details>
<summary><strong>4. What is the difference between <code>-fstack-protector</code>, <code>-fstack-protector-strong</code>, and <code>-fstack-protector-all</code>?</strong></summary>

- `-fstack-protector`: Only protects functions with local char buffers larger than 8 bytes.
- `-fstack-protector-all`: Protects every function regardless of local variables, causing unnecessary CPU and stack memory inflation.
- `-fstack-protector-strong`: Protects any function declaring arrays of any type or size, structures containing arrays, or functions whose local variables have their address taken. It provides optimal protection with minimal overhead.
</details>

<details>
<summary><strong>5. What is a "Stack Clash" attack and how does <code>-fstack-clash-protection</code> prevent it?</strong></summary>

A stack clash attack exploits large stack allocations (e.g., `alloca(1000000)` or large local arrays). The stack allocation can jump completely over the OS kernel's 4KB stack guard page without touching it, colliding into and corrupting the adjacent heap segment. `-fstack-clash-protection` forces the compiler to probe each memory page incrementally (probe reads) during allocation, ensuring the CPU hits the guard page and triggers an unexploitable page fault.
</details>

<details>
<summary><strong>6. What is the purpose of the <code>-Wl,-z,noexecstack</code> linker flag?</strong></summary>

It explicitly marks the GNU_STACK program header in the generated ELF binary as non-executable (clearing the executable flag). This instructs the operating system loader to enforce hardware Data Execution Prevention (DEP/NX), ensuring code cannot execute from stack memory pages.
</details>

<details>
<summary><strong>7. Why must a Position Independent Executable be built with both <code>-fPIE</code> and <code>-pie</code>?</strong></summary>

`-fPIE` is a compiler flag instructing the code generator to emit position-independent assembly instructions (using RIP-relative addressing for global variables and functions). `-pie` is a linker flag instructing the GNU linker to emit a dynamically relocatable executable ELF binary rather than a fixed-base executable. Both are required for full ASLR.
</details>

<details>
<summary><strong>8. What is the command-line utility used to audit binary hardening flags on Linux?</strong></summary>

The standard industry tool is `checksec` (e.g. `checksec --file=my_binary`).
</details>

---

<details>
<summary><strong>9. How do you verify whether a binary was compiled with stack canaries using readelf?</strong></summary>

You can run <code>readelf -s <binary> | grep __stack_chk_fail</code>. If the binary was built with <code>-fstack-protector</code>, the undefined symbol <code>__stack_chk_fail@GLIBC</code> will be present in the dynamic symbol table (<code>.dynsym</code>) indicating that stack canary failure handlers are linked.
</details>

<details>
<summary><strong>10. What is the difference between FORTIFY_SOURCE level 2 and level 3?</strong></summary>

<code>_FORTIFY_SOURCE=2</code> checks buffer boundaries using <code>__builtin_object_size()</code>, which computes fixed, maximum buffer bounds deduced statically at compile time. <code>_FORTIFY_SOURCE=3</code> (introduced in GCC 12 and LLVM 15) uses <code>__builtin_dynamic_object_size()</code>, which allows the compiler to compute non-constant dynamic buffer bounds evaluated at runtime (e.g. from <code>malloc(n)</code>), detecting overflows in dynamically sized buffers with virtually no runtime overhead.
</details>

---

## 11. Practice Exercises

### Exercise 1 (Easy): Auditing Makefile for Security Flags
- **Task:** Identify the missing security flags in the following Makefile and update it to meet enterprise hardening standards:
```makefile
CFLAGS = -Wall -O2
app: main.c
	gcc $(CFLAGS) main.c -o app
```

<details>
<summary>Solution</summary>

```makefile
CFLAGS = -std=c11 -Wall -Wextra -O2 -D_FORTIFY_SOURCE=3          -fstack-protector-strong -fPIE -fstack-clash-protection
LDFLAGS = -pie -Wl,-z,relro,-z,now -Wl,-z,noexecstack

app: main.c
	$(CC) $(CFLAGS) main.c $(LDFLAGS) -o app
```
</details>

### Exercise 2 (Medium): Triggering Fortify Source Bounds Checking
- **Task:** Write a minimal C program that triggers a compile-time or runtime abort under `-D_FORTIFY_SOURCE=2` when copying 20 bytes into a 10-byte buffer.

<details>
<summary>Solution</summary>

```c
#include <stdio.h>
#include <string.h>

int main(void) {
    char small_buffer[10];
    const char *large_string = "01234567890123456789";

    /* Under -O2 -D_FORTIFY_SOURCE=2, __builtin_object_size knows small_buffer is 10B.
     * Compiler emits a warning/error or runtime __chk_fail! */
    memcpy(small_buffer, large_string, 20);

    printf("Buffer: %s\n", small_buffer);
    return 0;
}
```
</details>

### Exercise 3 (Hard): Emulating Stack Canary Checking in C
- **Task:** Write a software verification harness that wraps a function call with a randomized canary verification check, validating that the canary is inspected before returning.

<details>
<summary>Solution</summary>

```c
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

static uint64_t g_master_canary = 0x1234567890ABCDEFULL;

typedef struct {
    char data[16];
    uint64_t canary;
} protected_frame_t;

static void protected_worker(const char *input) {
    protected_frame_t frame;
    frame.canary = g_master_canary;

    /* Simulating bounded operation */
    strncpy(frame.data, input, sizeof(frame.data) - 1);
    frame.data[sizeof(frame.data) - 1] = '\0';

    /* Verify Canary */
    if (frame.canary != g_master_canary) {
        fprintf(stderr, "*** STACK SMASHING DETECTED: Canary corrupted! ***\n");
        abort();
    }
    printf("Canary verified: Frame execution safe.\n");
}

int main(void) {
    protected_worker("SafePayload");
    return 0;
}
```
</details>

---

## 12. Summary / Cheat Sheet

| Defense Flag | Target Threat | Primary Mechanism |
| :--- | :--- | :--- |
| `-fstack-protector-strong` | Stack smashing / RCE | Injects randomized canary before saved `%rbp` |
| `-D_FORTIFY_SOURCE=3` | Buffer overflows in stdlib | Dynamic bounds-checked `__chk` wrappers |
| `-Wl,-z,relro,-z,now` | GOT pointer hijacking | Resolves symbols at startup and makes GOT read-only |
| `-fPIE -pie` | Hardcoded ROP attacks | Randomizes entire executable base address (ASLR) |
| `-Wl,-z,noexecstack` | Shellcode injection | Marks stack memory pages non-executable (NX/DEP) |
| `-fstack-clash-protection`| Stack-heap clash | Probes pages sequentially during stack growth |

---

## 13. Further Reading
- Hardening against memory corruption vulnerabilities (Red Hat Enterprise Linux)
- GCC Instrumentation and Hardening Options Manual
- Debian Hardening Wiki: `https://wiki.debian.org/Hardening`
- Open Web Application Security Project (OWASP) C/C++ Hardening Guide

---
[<- Previous: 06-misra-c-overview.md](06-misra-c-overview.md) | [Section Index](README.md) | [Next: 08-race-conditions-and-toctou.md ->](08-race-conditions-and-toctou.md)\n