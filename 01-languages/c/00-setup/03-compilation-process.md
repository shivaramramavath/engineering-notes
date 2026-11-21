# Compilation Process

> The four stages of C compilation: preprocessing, compilation to assembly, assembling to relocatable object code, and linking into an executable binary.

| Metadata | Details |
|---|---|
| **Level** | Intermediate |
| **Prerequisites** | [01 · Toolchain](./01-toolchain.md) |
| **Time to Read** | 25 minutes |
| **Status** | Complete |

---

## Table of Contents
- [1. What & Why](#1-what--why)
- [2. Core Concepts](#2-core-concepts)
- [3. How It Works Internally](#3-how-it-works-internally)
- [4. Syntax & Usage](#4-syntax--usage)
- [5. Practical Examples](#5-practical-examples)
- [6. Best Practices](#6-best-practices)
- [7. Common Mistakes & Pitfalls](#7-common-mistakes--pitfalls)
- [8. Performance & Security Notes](#8-performance--security-notes)
- [9. Comparison / When to Use What](#9-comparison--when-to-use-what)
- [10. Interview Questions](#10-interview-questions)
- [11. Practice Exercises](#11-practice-exercises)
- [12. Summary / Cheat Sheet](#12-summary--cheat-sheet)
- [13. Further Reading](#13-further-reading)

---

## 1. What & Why

When you run `gcc main.c -o app`, it appears as a single atomic command. In reality, C compilation is a four-stage pipeline of distinct programs, each transforming your code from one representation to another:

$$\text{Source Code (.c)} \xrightarrow{\text{cpp}} \text{Preprocessed (.i)} \xrightarrow{\text{cc1}} \text{Assembly (.s)} \xrightarrow{\text{as}} \text{Object Code (.o)} \xrightarrow{\text{ld}} \text{Executable Binary}$$

Understanding what happens at each stage is essential for:
- Diagnosing compiler errors vs linker errors (e.g. why `implicit declaration of function` happens in stage 2, but `undefined reference to function` happens in stage 4).
- Debugging preprocessor macro expansion bugs and header collision issues.
- Inspecting assembly output to verify compiler optimizations (vectorization, register allocation).
- Understanding how variables map into physical binary sections (`.text`, `.data`, `.bss`, `.rodata`).

---

## 2. Core Concepts

### The Four Compilation Stages

```
+---------------------------------------------------------------------------------------+
|                                C COMPILATION PIPELINE                                 |
+---------------------------------------------------------------------------------------+
|  Stage 1: Preprocessing (cpp)   | Expands macros, strips comments, inlines headers    |
|  Stage 2: Compilation (cc1)     | Parses C into AST, checks types, emits assembly     |
|  Stage 3: Assembly (as)         | Translates assembly to machine opcodes (.o)         |
|  Stage 4: Linking (ld)          | Combines .o files, resolves addresses & symbols     |
+---------------------------------------------------------------------------------------+
```

### Stage 1: Preprocessing (`cpp`)
The preprocessor is a text-transformation engine running before semantic compilation. It:
1. Strips all C comments (`/* ... */` and `// ...`).
2. Evaluates conditional compilation directives (`#ifdef`, `#ifndef`, `#if`, `#endif`).
3. Recursively inlines `#include` header files by copying their entire textual contents into the translation unit.
4. Performs macro expansion (`#define`).
5. Generates line control markers (e.g. `# 1 "main.c"`) so the compiler can trace error messages back to the original source line.

### Stage 2: Compilation (`cc1`)
The compiler takes the single preprocessed text stream (`.i`) and performs lexical analysis, parsing, semantic type-checking, optimization, and code generation. It translates high-level C logic into target-specific assembly instructions (`.s`).

### Stage 3: Assembly (`as`)
The assembler converts human-readable assembly instructions (mnemonics like `mov`, `add`, `call`) into raw binary machine code opcodes, packaging them into a **Relocatable Object File** (`.o` on Linux, `.obj` on Windows).

### Stage 4: Linking (`ld` / `collect2`)
An object file cannot run on its own because external function calls (like `printf` or functions in your other files) have placeholder addresses. The linker:
1. Merges identical sections across all object files (`.text` with `.text`, `.data` with `.data`).
2. Resolves symbol references: matches symbol uses to symbol definitions.
3. Fixes up relocated memory addresses.
4. Adds C runtime startup code (`crt1.o`) that sets up the stack, environment, and invokes `main()`.

---

## 3. How It Works Internally

### ELF Binary Sections & Memory Mapping

On Linux and modern Unix systems, object files and executables use the **ELF** (Executable and Linkable Format) format. When the operating system's `execve` system call executes a binary, the kernel reads the ELF Program Headers and maps sections into the process's virtual memory address space:

```
VIRTUAL MEMORY ADDRESS SPACE (High Memory: 0x7FFFFFFFFFFF)
+-------------------------------------------------------------+
| Stack (Grows Downward)    | Local automatic variables, frames|
|            │                                                |
|            ▼                                                |
|                                                             |
|            ▲                                                |
|            │                                                |
| Heap (Grows Upward)       | Dynamic memory via malloc/free  |
+-------------------------------------------------------------+
| .bss Section (RW)         | Zero-initialized global/static  |
| .data Section (RW)        | Initialized global/static vars  |
+-------------------------------------------------------------+
| .rodata Section (RO)      | String literals, const data     |
| .text Section (RX)        | Executable machine instructions |
+-------------------------------------------------------------+
(Low Memory: 0x00400000)
```

| Section | Permissions | Role | Consumes Disk Space? |
|---|---|---|---|
| **`.text`** | Read + Execute (RX) | Machine code instructions for all compiled functions. | Yes |
| **`.rodata`** | Read-Only (RO) | String literals (`"Hello"`), `const` lookup tables. | Yes |
| **`.data`** | Read + Write (RW) | Global and `static` variables initialized to non-zero. | Yes |
| **`.bss`** | Read + Write (RW) | Global and `static` variables initialized to zero or uninitialized. | **No** (Only records total byte size needed; kernel zero-fills on page fault). |

---

## 4. Syntax & Usage

You can stop GCC at any intermediate compilation stage using specific command-line flags:

### 1. Stop After Preprocessing (`-E`)
```bash
gcc -E main.c -o main.i
```
Generates the preprocessed text file `main.i`. Open it to inspect how macros were expanded or which header files were inlined.

### 2. Stop After Assembly Code Generation (`-S`)
```bash
gcc -S -O2 -masm=intel main.c -o main.s
```
Generates human-readable assembly instructions in `main.s`. The `-masm=intel` flag produces clean Intel syntax (destination first) rather than AT&T syntax.

### 3. Stop After Assembling Object Code (`-c`)
```bash
gcc -c -O2 main.c -o main.o
```
Produces the binary relocatable object file `main.o`. This is what build tools (like `make`) do for incremental compilation.

### 4. Link Relocatable Objects into Final Executable
```bash
gcc main.o utils.o -o my_app
```
Invokes the linker to combine object files and standard library archives into `my_app`.

### 5. Inspecting Binary Artifacts with Binutils
```bash
# View ELF section headers (addresses, sizes, flags):
readelf -S main.o

# View symbol table (functions, variables, linkage):
nm -C main.o

# Disassemble .text machine code back to assembly:
objdump -d -M intel main.o

# Print memory footprint summary:
size my_app
```

---

## 5. Practical Examples

### Diagnostic Program: Memory Segment Address Inspection
The compilable source file is available at [`examples/03-compilation-process.c`](./examples/03-compilation-process.c).

#### Goal
Inspect at runtime the exact memory addresses of variables located in `.text`, `.rodata`, `.data`, `.bss`, and the Stack, proving their physical separation into distinct ELF segments.

#### Code
```c
#include <stdio.h>
#include <stdint.h>

/* Global initialized variable: stored in .data section */
int global_counter = 42;

/* Global uninitialized variable: stored in .bss section */
int uninitialized_buffer[64];

/* Constant literal pointer: string literal stored in .rodata section */
static const char *const rodata_message = "Hello from .rodata!";

/* Function code: stored in .text section */
static void inspect_memory_segments(int stack_arg) {
    int local_stack_var = 100;

    printf("=== Process Memory Segment Inspection ===\\n");
    printf(".text (Code Segment)       at address: %p\\n", (void *)(uintptr_t)inspect_memory_segments);
    printf(".rodata (Read-Only Data)   at address: %p\\n", (void *)rodata_message);
    printf(".data (Initialized Global) at address: %p (value = %d)\\n", (void *)&global_counter, global_counter);
    printf(".bss (Uninitialized Block) at address: %p\\n", (void *)uninitialized_buffer);
    printf("Stack (Parameter Address)  at address: %p (value = %d)\\n", (void *)&stack_arg, stack_arg);
    printf("Stack (Local Variable)     at address: %p (value = %d)\\n", (void *)&local_stack_var, local_stack_var);
}

int main(void) {
    inspect_memory_segments(7);
    return 0;
}
```

#### Compilation & Execution
```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -Werror examples/03-compilation-process.c -o run_03_compilation
./run_03_compilation
```

#### Real Verified Output
```text
=== Process Memory Segment Inspection ===
.text (Code Segment)       at address: 00401489
.rodata (Read-Only Data)   at address: 0040b064
.data (Initialized Global) at address: 0040a004 (value = 42)
.bss (Uninitialized Block) at address: 0040ea40
Stack (Parameter Address)  at address: 0061ff10 (value = 7)
Stack (Local Variable)     at address: 0061fefc (value = 100)
```

#### Explanation of Memory Offsets
Notice the address groupings:
1. `.text` (`0x00401489`), `.rodata` (`0x0040b064`), `.data` (`0x0040a004`), and `.bss` (`0x0040ea40`) are clustered in low virtual memory (`0x0040xxxx`), mapped directly from the ELF segments.
2. The Stack addresses (`0x0061ff10` and `0x0061fefc`) live in an entirely different, higher region of virtual memory.
3. Notice that `local_stack_var` (`0x0061fefc`) is at a **lower address** than `stack_arg` (`0x0061ff10`), directly demonstrating that the call stack grows downward towards lower addresses on x86 architectures.

---

## 6. Best Practices

1. **Compile in Two Steps for Multi-File Projects:** Compile individual `.c` files to `.o` with `gcc -c`, then link `.o` files together. This enables incremental builds where modifying one file does not trigger recompilation of the whole codebase.
2. **Never Define Variables in Header Files:** Defining a variable in a header (`int count = 0;`) causes every `.c` file that includes it to emit a `.data` symbol, causing `duplicate symbol` or `multiple definition` linker errors. Declare as `extern int count;` in the header, and define `int count = 0;` in exactly one `.c` file.
3. **Always Guard Header Files:** Use standard include guards (`#ifndef MY_HEADER_H ... #endif`) or `#pragma once` to prevent redundant preprocessor expansion and circular compilation dependencies.
4. **Leverage Link-Time Optimization (LTO):** In release builds, compile and link with `-flto`. LTO preserves intermediate representation across object files, allowing the compiler to inline functions across completely separate `.c` files.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Manifestation Stage | Cause & Resolution |
|---|---|---|
| `fatal error: foo.h: No such file or directory` | Stage 1 (Preprocessing) | Preprocessor cannot locate header. Pass `-Ipath/to/headers` to include search path. |
| `error: implicit declaration of function 'bar'` | Stage 2 (Compilation) | Compiler encountered a function call without a prior prototype declaration. Include the correct header. |
| `undefined reference to 'pow'` | Stage 4 (Linking) | Function was declared, but linker cannot find the compiled machine code in any library. Link math library with `-lm`. |
| `multiple definition of 'global_var'` | Stage 4 (Linking) | Variable was defined (allocated memory) in a header included by multiple translation units. Use `extern`. |
| Circular `#include` dependencies | Stage 1 (Preprocessing) | Two headers include each other. Use forward struct declarations (`struct Node;`) in headers instead of full `#include`. |

---

## 8. Performance & Security Notes

- **The Zero-Cost Nature of `.bss`:** Uninitialized global arrays (e.g. `int buffer[10000000];`) do NOT inflate the binary executable size on disk. The ELF header simply stores an 8-byte integer indicating that $40\text{ MB}$ of memory must be allocated by the kernel upon process initialization. Conversely, initializing `int buffer[10000000] = {1};` forces the binary to write $40\text{ MB}$ of literal values directly to the file on disk (`.data`).
- **Binary Hardening:**
  - `strip --strip-all binary`: Strips symbol and debugging tables, reducing binary size and making reverse engineering harder.
  - `-Wl,-z,relro,-z,now`: Marks Global Offset Tables (GOT) as completely read-only after dynamic link resolution, preventing GOT overwrite exploitation.

---

## 9. Comparison / When to Use What

### Static Linking vs Dynamic Linking

| Attribute | Static Library (`.a` / `.lib`) | Dynamic / Shared Library (`.so` / `.dll`) |
|---|---|---|
| **Linking Phase** | Fully copied into executable at link time (`ld`). | Symbol stubs linked; code loaded by dynamic linker (`ld.so`) at runtime. |
| **Executable Size** | Large (contains all library object code). | Small (contains only symbol references). |
| **Dependencies** | Self-contained; runs anywhere with compatible ABI. | Requires target machine to have matching `.so` in `LD_LIBRARY_PATH`. |
| **Memory Footprint** | Duplicate code in memory if 10 processes run it. | Shared: OS kernel maps the single `.text` page into all 10 processes. |
| **Updates / Patches** | Requires recompiling and relinking the executable. | Simply replace `.so` on disk; bugfix takes effect on next execution. |

---

## 10. Interview Questions

<details>
<summary><strong>1. Why does <code>int buffer[1000000];</code> result in a tiny binary file, while <code>int buffer[1000000] = {1};</code> results in a massive binary?</strong></summary>

This is due to the difference between the **`.bss`** and **`.data`** ELF sections:
- `int buffer[1000000];` is uninitialized, so it is placed in the `.bss` section. The ELF format does not store zeros on disk; it simply records a 4MB size reservation in the section header. When the OS loads the binary into RAM, it allocates zero-filled pages on demand.
- `int buffer[1000000] = {1};` is initialized to non-zero values, placing it in the `.data` section. The compiler is obligated to write the actual binary representation (4MB of data) into the executable file on disk so the OS can load it literally.
</details>

<details>
<summary><strong>2. What is the difference between an "undefined reference" error and an "implicit declaration" warning?</strong></summary>

- **"Implicit declaration of function"** occurs during **Stage 2 (Compilation)**. The compiler driver reaches a function call without having seen a prototype declaration. In C89/C99, the compiler assumed the function returns `int`; in C11/C17/C23, this is an error under strict flags (`-Werror`).
- **"Undefined reference"** occurs during **Stage 4 (Linking)**. The code compiled successfully into `.o`, but when the linker tried to resolve the function's external symbol name to an actual machine code memory address across all object files and libraries, no definition existed.
</details>

<details>
<summary><strong>3. How does the dynamic linker (<code>ld.so</code>) resolve symbols at runtime using the Global Offset Table (GOT) and Procedure Linkage Table (PLT)?</strong></summary>

Shared libraries (`.so`) are loaded at arbitrary memory addresses due to ASLR (Address Space Layout Randomization). To allow code to call shared library functions without rewriting code pages:
1. Function calls jump to a stub in the **PLT (Procedure Linkage Table)**, which resides in the executable `.text`.
2. The PLT stub reads a target address from the **GOT (Global Offset Table)** in `.data`.
3. On the first call (lazy binding), the GOT points back into the dynamic linker (`ld.so`), which resolves the function's real address in the shared library, overwrites the GOT entry with that address, and executes the function.
4. Subsequent calls jump through the PLT directly to the resolved GOT address with zero dynamic linker overhead.
</details>

---

## 11. Practice Exercises

### Exercise 1 (Easy): Generating and Inspecting Assembly Output
**Goal:** Write a simple function that adds two numbers. Compile it to assembly (`.s`) using Intel syntax and locate the CPU registers (`eax`, `edi`, `esi` on x86-64) used to receive parameters and return the result.

<details>
<summary>View Solution</summary>

```c
// add.c
int add(int a, int b) {
    return a + b;
}
```
Compile: `gcc -S -O2 -masm=intel add.c -o add.s`
Inspection of `add.s`:
```assembly
add:
    lea eax, [rdi+rsi]    # rdi = first arg (a), rsi = second arg (b)
    ret                   # return value in eax
```
</details>

### Exercise 2 (Medium): Inspecting Object Symbols with nm
**Goal:** Compile a file with static functions, global functions, and uninitialized variables. Use `nm` to classify symbols as local (`t`/`b`), global (`T`/`B`), or undefined (`U`).

<details>
<summary>View Solution</summary>

```c
// symbols.c
#include <stdio.h>

static int private_var = 1;     // 'd' (local data)
int public_var = 2;             // 'D' (global data)
int uninit_var;                 // 'B' (global bss)

static void helper(void) {}     // 't' (local text)
void public_func(void) {        // 'T' (global text)
    helper();
    printf("%d\\n", public_var); // 'U' (undefined reference to printf)
}
```
Run:
```bash
gcc -c symbols.c -o symbols.o
nm symbols.o
```
*Output will clearly show `U printf`, `T public_func`, `t helper`, `D public_var`, `d private_var`.*
</details>

### Exercise 3 (Hard): Resolving Linker Collisions
**Goal:** Create two `.c` files that both define `int counter = 0;`. Observe the linker error, then fix the code using `extern` without altering runtime access to the shared counter.

<details>
<summary>View Solution</summary>

**Failing State:**
- `file1.c`: `int counter = 0;`
- `file2.c`: `int counter = 0;`
- Link: `gcc file1.c file2.c -o app`
- *Linker Error:* `multiple definition of 'counter'`

**Fixed State:**
- `counter.h`: `extern int counter;` (declaration only)
- `file1.c`: `#include "counter.h"` followed by definition `int counter = 0;`
- `file2.c`: `#include "counter.h"` (accesses `counter` through `extern` declaration)
- Link: `gcc file1.c file2.c -o app` compiles and links cleanly.
</details>

---

## 12. Summary / Cheat Sheet

```bash
# Complete 4-stage manual compilation pipeline
gcc -E main.c -o main.i           # Stage 1: Preprocess
gcc -S -O2 -masm=intel main.i -o main.s # Stage 2: Compile to assembly
gcc -c main.s -o main.o           # Stage 3: Assemble to machine code
gcc main.o -o main.exe            # Stage 4: Link into executable

# View binary sections and sizes
size main.o main.exe

# Disassemble with source code interleaved
gcc -g -O2 -c main.c -o main.o
objdump -S -M intel main.o

# Identify undefined symbols requiring external libraries
nm -u main.o
```

---

## 13. Further Reading

- Levine, John R. *Linkers and Loaders*. Morgan Kaufmann, 1999.
- Bryant, Randal E., and O'Hallaron, David R. *Computer Systems: A Programmer's Perspective (CS:APP)*, Chapter 7: "Linking".
- Tool Interface Standard (TIS): *Executable and Linking Format (ELF) Specification Version 1.2*.
- Ulrich Drepper: *How To Write Shared Libraries*, Red Hat, 2011.

---

## Navigation
- **Previous:** [02 · Editor Setup](./02-editor-setup.md)
- **Section Index:** [00 · Setup Index](./README.md)
- **Next:** [04 · First Program](./04-first-program.md)
