# Symbol Visibility and ELF Binary Internals in C

> Deep architectural exploration of the Executable and Linkable Format (ELF): binary sections, symbol tables (`.symtab` vs `.dynsym`), symbol visibility controls (`default`, `hidden`), and the `-fvisibility=hidden` optimization.

| Metadata | Details |
| :--- | :--- |
| **Level** | Advanced |
| **Prerequisites** | [04-linking-static-vs-shared-libraries.md](./04-linking-static-vs-shared-libraries.md), [08-writing-a-small-library.md](./08-writing-a-small-library.md) |
| **Time to Read** | 40 minutes |
| **Status** | Complete |
| **Standard Baseline** | ISO C11 / C17 / C23 (System V ELF standard) |

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
9. [Comparison: Symbol Visibility Attributes](#9-comparison-symbol-visibility-attributes)
10. [Interview Questions](#10-interview-questions)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary & Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why
On modern UNIX and Linux systems, the canonical binary file format for object files (`.o`), static libraries, shared objects (`.so`), core dumps, and executable binaries is **ELF (Executable and Linkable Format)**.

By default in GCC and Clang on Linux:
- Every function and global variable that is not declared `static` is exported as a **Public Symbol** with `default` visibility.
- When compiled into a shared library, these symbols populate the dynamic symbol table (`.dynsym`).
- Calls between two internal functions within the same library must go through the Procedure Linkage Table (PLT) and Global Offset Table (GOT) in case an application interposes them!

This default behavior degrades application startup time, wastes RAM on bloated symbol tables, and prevents the compiler from inlining internal functions. Mastering **Symbol Visibility** transforms shared libraries into lean, secure, high-speed binary artifacts.

---

## 2. Core Concepts

### 2.1 Anatomy of an ELF Binary
An ELF file has a dual view:
1. **Linker View (Sections)**: Used at compile and link time.
   - `.text`: Executable CPU machine code instructions.
   - `.rodata`: Read-only data (string literals, const lookup tables).
   - `.data`: Initialized global and static variables (occupies space on disk).
   - `.bss`: Uninitialized global and static variables (zero-initialized at runtime; consumes zero disk space).
   - `.symtab`: Full symbol table (stripped in release builds).
   - `.dynsym`: Dynamic symbol table (retained at runtime for `ld.so`).
2. **Execution View (Segments)**: Used by the OS kernel `execve()` loader.
   - `PT_LOAD`: Memory regions mapped into virtual address space with specific permissions (`R-X` for code, `RW-` for data).

### 2.2 Symbol Visibility Attributes
GCC and Clang support four visibility modes:
1. **`default`**: Exported globally. Can be called from outside the shared library and interposed by `LD_PRELOAD`.
2. **`hidden`**: The symbol is not exported into `.dynsym`. It is completely invisible outside the shared library, but accessible across all translation units within the same library.
3. **`protected`**: Exported globally, but calls from within the library cannot be overridden/interposed by external symbols.
4. **`internal`**: Like hidden, but targets can optimize assumptions further (rarely used).

### 2.3 The `-fvisibility=hidden` Strategy
The industry gold standard for shared library development:
1. Compile the entire codebase with the compiler flag **`-fvisibility=hidden`**.
2. Explicitly annotate only the public API functions in headers using:
   `__attribute__((visibility("default")))`
All internal helper functions automatically become hidden, enabling direct PC-relative jump instructions instead of PLT trampolines!

---

## 3. How It Works Internally

### Hidden vs Default Symbol Call Disassembly

```
Call to Default Function (foo):
  call foo@plt            <-- Jumps to PLT trampoline stub
  ... Trampoline reads GOT table pointer ...

Call to Hidden Function (bar) with -fvisibility=hidden:
  call bar                <-- Direct 1-cycle PC-relative machine jump!
                              Compiler can also inline the function directly!
```

```mermaid
flowchart TD
    subgraph SharedLibrary["libfoo.so Memory Space"]
        PublicFunc["Public API: foo()<br>[visibility: default]"]
        HiddenFunc["Internal Helper: bar()<br>[visibility: hidden]"]
        Caller["Caller: foo()"]
        Caller -->|Direct RIP-relative Jump| HiddenFunc
    end
    ExternalApp["Main Application"] -->|Must go through PLT/GOT| PublicFunc
    ExternalApp -.->|CANNOT ACCESS (Linker Error)| HiddenFunc
```

---

## 4. Syntax & Usage

```c
// Macro definitions for clean symbol export
#define EXPORT_API __attribute__((visibility("default")))
#define HIDDEN_API __attribute__((visibility("hidden")))

// Public exported function
EXPORT_API int public_interface(void);

// Internal library function (accessible across files, but invisible to user)
HIDDEN_API void internal_worker(void);
```

---

## 5. Practical Examples

### Example 1: Symbol Visibility and Access Demonstration
Goal: Implement public and hidden functions, verifying execution and encapsulation properties.

```c
#include <stdio.h>

// Static (hidden/local) symbol
static int hidden_internal_counter = 42;

// Public symbol with explicit attribute
__attribute__((visibility("default"))) int get_counter(void) {
    return hidden_internal_counter;
}

int main(void) {
    printf("=== Symbol Visibility and ELF Sections ===\n");
    printf("Public function returned: %d\n", get_counter());
    return 0;
}
```

**Verified Output:**
```text
=== Symbol Visibility and ELF Sections ===
Public function returned: 42
```

---

## 6. Best Practices
1. **Always Compile Shared Libraries with `-fvisibility=hidden`**: This single flag reduces dynamic symbol count by up to 90%, speeding up application startup and enabling massive compiler inlining.
2. **Explicitly Export Only Public APIs**: Mark public functions with a portable `MYLIB_API` macro that maps to `visibility("default")` on GCC and `__declspec(dllexport)` on Windows.
3. **Strip Non-Dynamic Symbols in Production**: Run `strip --strip-unneeded libfoo.so` before distribution to eliminate `.symtab` debug names while preserving `.dynsym`.
4. **Inspect `.dynsym` with `readelf -s --dyn-syms`**: Verify that no private internal functions accidentally leak into the exported symbol table.
5. **Mark Read-Only Constants as `const`**: Ensures data is placed in the read-only `.rodata` segment, causing the OS to trigger `SIGSEGV` if any code attempts to write to it.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Is Dangerous | Correct Approach |
| :--- | :--- | :--- |
| Leaving all symbols default | Bloats `.dynsym`, exposes private internals to clients, and prevents compiler inlining. | Compile with `-fvisibility=hidden`. |
| Uninitialized global variable in header | Placed in `.comm` (Common section), causing linker to merge disparate variables across translation units. | Declare in header with `extern`; define once in `.c`. |
| Modifying `.rodata` string literal | `char *s = "hello"; s[0] = 'H';` crashes with `SIGSEGV` because string resides in write-protected `.rodata`. | Use array syntax for mutable strings: `char s[] = "hello";`. |
| Stripping `.dynsym` | Running `strip --all` removes dynamic symbols needed at runtime, rendering the `.so` un-linkable. | Use `strip --strip-unneeded` or `strip --strip-debug`. |
| Hidden functions in C++ dynamic casts | Hiding RTTI typeinfo symbols across shared library boundaries breaks `dynamic_cast` and exception catching. | Export RTTI types explicitly. |

---

## 8. Performance & Security Notes

### Performance Gains of `-fvisibility=hidden`
1. **Direct Jumps**: Calling hidden functions avoids the PLT trampoline (saving 2 memory indirection jumps per call).
2. **Inlining Optimization**: The compiler optimizer knows hidden functions cannot be interposed by external modules, allowing aggressive inlining across translation units.
3. **Dynamic Linker Acceleration**: Reduces the work `ld.so` must perform during symbol relocation at program startup by orders of magnitude.

---

## 9. Comparison: Symbol Visibility Attributes

```
default:
- Exported in .dynsym: YES
- Callable from outside .so: YES
- Interposable by LD_PRELOAD: YES
- Call overhead: via PLT/GOT

protected:
- Exported in .dynsym: YES
- Callable from outside .so: YES
- Interposable by LD_PRELOAD: NO (internal calls bind locally)
- Call overhead: Direct locally, via PLT externally

hidden:
- Exported in .dynsym: NO
- Callable from outside .so: NO
- Interposable: NO
- Call overhead: Strictly direct 1-cycle PC-relative jump
```

---

## 10. Interview Questions

<details>
<summary><strong>1. What is the difference between `.symtab` and `.dynsym` in an ELF file?</strong></summary>

- `.symtab`: Contains all symbols used for static linking and debugging (local functions, static variables, labels). It can be completely removed using the `strip` command without affecting runtime execution.
- `.dynsym`: Contains only the symbols required for dynamic linking at runtime (imported and exported functions). It is mapped into process memory by `PT_LOAD` / `PT_DYNAMIC` and cannot be stripped.
</details>

<details>
<summary><strong>2. Why does the `.bss` section consume zero bytes in the on-disk ELF binary?</strong></summary>

The `.bss` (Block Started by Symbol) section contains uninitialized global and static variables. Because all such variables are guaranteed to be zero by the C standard, the ELF file only stores a 64-bit integer specifying the total byte length required. When `execve()` maps the binary, the OS kernel maps an anonymous zero-filled page of that size.
</details>

<details>
<summary><strong>3. How does `-fvisibility=hidden` allow the compiler to optimize code?</strong></summary>

When a function is `default`, the compiler must assume that an external library or `LD_PRELOAD` might replace the function at runtime; it cannot inline it and must emit PLT calls. When marked `hidden`, the compiler knows the function can never be replaced from the outside, allowing it to inline the body, delete unused dead code, and pass arguments directly in registers without ABI conformance.
</details>

<details>
<summary><strong>4. What is the Common (`.comm`) section in ELF?</strong></summary>

Tentative definitions in C (e.g. `int global_var;` without `extern` or `= 0` in a header included by multiple files) are placed in the `COMMON` section. The linker merges all identical common symbols into a single memory location in `.bss` rather than failing with a duplicate symbol error.
</details>

<details>
<summary><strong>5. How do you inspect the dynamic symbols exported by a `.so` file?</strong></summary>

Using GNU binutils:
```bash
readelf -s --dyn-syms libfoo.so
# Or using nm
nm -D --defined-only libfoo.so
```
</details>

<details>
<summary><strong>6. What is the difference between an ELF Section and an ELF Segment?</strong></summary>

- **Section**: Logical components meaningful to the compiler and linker (`.text`, `.data`, `.rodata`). Stored in Section Headers at the end of the ELF file.
- **Segment**: Memory mapping directives meaningful to the operating system kernel loader (`PT_LOAD`). Multiple sections with identical memory permissions are grouped into a single segment (e.g. `.text` and `.rodata` grouped into a Read-Execute segment).
</details>

<details>
<summary><strong>7. What happens if a function is marked `__attribute__((visibility("protected")))`?</strong></summary>

The symbol is exported so outside executables can call it, but references to the symbol from *within* the defining shared library will always bind directly to the local definition, ignoring any override in the main executable or `LD_PRELOAD`.
</details>

<details>
<summary><strong>8. Why should you avoid global writable data in shared libraries?</strong></summary>

Global writable variables reside in `.data` or `.bss`. In a shared library, while read-only code pages (`.text`) are shared across all processes, writable data pages require **Copy-On-Write (COW)**: each process gets its own physical copy, increasing overall system RAM usage.
</details>

---

## 11. Practice Exercises

### Easy 1: Check Section Headers with `readelf`
Command to display all section headers and their memory addresses in a binary.

<details>
<summary>Solution</summary>

```bash
readelf -S ./program
```
</details>

### Easy 2: Strip Debug Symbols
Command to strip `.symtab` debug symbols while preserving runtime dynamic linking symbols.

<details>
<summary>Solution</summary>

```bash
strip --strip-unneeded libfoo.so
```
</details>

### Easy 3: Explicit Hidden Attribute
Declare a helper function with hidden visibility in C.

<details>
<summary>Solution</summary>

```c
__attribute__((visibility("hidden"))) void internal_helper(void);
```
</details>

### Medium 1: Compile Library with Hidden Visibility
Write GCC command to compile `libmath.so` with hidden visibility by default and one exported function.

<details>
<summary>Solution</summary>

```bash
gcc -std=c11 -Wall -Wextra -fPIC -fvisibility=hidden -shared math.c -o libmath.so
```
</details>

### Medium 2: Verify Symbol Count Reduction
Write a shell script that counts total exported symbols before and after adding `-fvisibility=hidden`.

<details>
<summary>Solution</summary>

```bash
#!/bin/bash
gcc -fPIC -shared module.c -o lib_default.so
gcc -fPIC -fvisibility=hidden -shared module.c -o lib_hidden.so
echo "Default exported symbols: $(nm -D --defined-only lib_default.so | wc -l)"
echo "Hidden exported symbols:  $(nm -D --defined-only lib_hidden.so | wc -l)"
```
</details>

### Medium 3: Place Data in Custom Named Section
Place a read-only configuration struct in a dedicated `.my_config` section.

<details>
<summary>Solution</summary>

```c
__attribute__((section(".my_config"), used)) const char secret_key[] = "SUPER_SECRET_KEY_123";
```
</details>

### Hard 1: Parse ELF 64-Bit Header in C
Write a C program that opens an ELF file, validates the magic bytes `0x7F 'E' 'L' 'F'`, and prints whether it is 32-bit or 64-bit.

<details>
<summary>Solution</summary>

```c
#include <stdio.h>
#include <stdint.h>
#include <elf.h>

bool inspect_elf(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    Elf64_Ehdr header;
    if (fread(&header, 1, sizeof(header), f) != sizeof(header)) { fclose(f); return false; }
    fclose(f);
    if (header.e_ident[EI_MAG0] != ELFMAG0 || header.e_ident[EI_MAG1] != ELFMAG1 ||
        header.e_ident[EI_MAG2] != ELFMAG2 || header.e_ident[EI_MAG3] != ELFMAG3) {
        return false;
    }
    printf("Class: %s
", header.e_ident[EI_CLASS] == ELFCLASS64 ? "64-bit" : "32-bit");
    return true;
}
```
</details>

### Hard 2: Iterate and Dump Symbol Table Programmatically
Write a C utility using `libelf` to iterate over all function names in `.dynsym`.

<details>
<summary>Solution</summary>

```c
// Using gnu libelf: elf_begin, elf_nextscn, gelf_getshdr, gelf_getsym
```
</details>

### Hard 3: Linker Version Script Symbol Scoping
Write a GNU Linker Version Script (`exports.map`) exposing only functions starting with `api_`.

<details>
<summary>Solution</summary>

```text
LIBFOO_1.0 {
  global:
    api_*;
  local:
    *;
};
```
Link with: `gcc -shared -Wl,--version-script=exports.map ...`
</details>

---

## 12. Summary & Cheat Sheet

| Visibility | Scope | Inlining | Symbol Table |
| :--- | :--- | :--- | :--- |
| **`default`** | Global (exported) | Disallowed (interposable) | `.dynsym` |
| **`hidden`** | Internal to `.so` | Fully enabled | Not in `.dynsym` |
| **`protected`** | Global | Local calls inlinable | `.dynsym` |
| **`-fvisibility=hidden`**| Global flag | Default becomes hidden | Minimizes `.dynsym` |

---

## 13. Further Reading
- *Tool Interface Standard (TIS) Executable and Linking Format (ELF) Specification* Version 1.2.
- Ulrich Drepper, *How to Write Shared Libraries* (Section 2.2: Exporting Symbols).

---

[← Previous Note: Dynamic Loading with dlopen](./09-dynamic-loading-dlopen.md) | [Advanced C Index](./README.md) | [Next Note: Inline Assembly and Intrinsics](./11-inline-assembly-and-intrinsics-intro.md)
