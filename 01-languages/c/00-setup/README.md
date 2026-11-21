# 00 · Setup & Toolchain

> Comprehensive guide to establishing a modern, professional, and portable C development environment across Linux, macOS, and Windows.

| Metric | Details |
|---|---|
| **Module Level** | Beginner → Professional Foundation |
| **Prerequisites** | Terminal basics, command-line familiarity |
| **Status** | Complete |
| **Standards Covered** | ISO C11 / C17 / C23 |

---

## Scope & Overview
C has no built-in runtime manager, package repository, or virtual machine. The developer's environment—the compiler, binutils, debugger, build automation, and language server—directly governs build performance, code diagnostics, binary security, and portability. 

This module guides you from configuring compilers (GCC, Clang) and standards compliance flags, through editor integration (`clangd`, compilation databases), to dissecting the multi-stage compilation pipeline, mastering GDB debugging, building reproducible Makefiles, interpreting compiler assembly, and structuring scalable modular projects.

---

## Contents

| # | Note | Level | Status | Focus / Key Takeaway |
|---|---|---|---|---|
| 01 | [Toolchain](./01-toolchain.md) | Beginner | Complete | Compilers (GCC/Clang), binutils, sanitizers, package management |
| 02 | [Editor Setup](./02-editor-setup.md) | Beginner | Complete | LSP (`clangd`), compilation databases (`compile_commands.json`), formatting |
| 03 | [Compilation Process](./03-compilation-process.md) | Intermediate | Complete | Preprocessing, compilation, assembly, linking, ELF inspection |
| 04 | [First Program](./04-first-program.md) | Beginner | Complete | `main` anatomy, exit codes, I/O streams, C runtime startup |
| 05 | [C Standards & History](./05-c-standards-history.md) | Beginner | Complete | Evolution from K&R to C89, C99, C11, C17, and C23 |
| 06 | [Debugging with GDB](./06-debugging-with-gdb.md) | Intermediate | Complete | Breakpoints, stepping, watchpoints, stack backtraces, core dumps |
| 07 | [Makefile Basics](./07-makefile-basics.md) | Beginner | Complete | Rules, targets, prerequisites, pattern rules, automatic variables, `-MMD -MP` |
| 08 | [Compiler Flags & Warnings](./08-compiler-flags-and-warnings.md) | Intermediate | Complete | Warning hygiene (`-Wall -Wextra -Wpedantic -Werror`), optimization, ASan |
| 09 | [Reading Assembly (Godbolt)](./09-reading-assembly-godbolt.md) | Intermediate | Complete | x86-64 assembly basics, registers, System V ABI calling conventions, Godbolt |
| 10 | [C Project Layout](./10-c-project-layout.md) | Intermediate | Complete | Modular architecture, `include/` vs `src/`, out-of-tree builds, opaque pointers |
| 11 | [Headers & Include Guards](./11-headers-and-include-guards-intro.md) | Beginner | Complete | Idempotent guards, `#pragma once`, IGO, forward declarations, ODR |

---

## Learning Outcomes
By completing this module, you will:
1. Understand the exact role of each tool in the C ecosystem (compiler driver, frontend, backend optimizer, assembler, linker).
2. Configure automated code completion, linting, and formatting using `clangd` and `clang-format`.
3. Inspect intermediate compilation artifacts (`.i`, `.s`, `.o`) and disassemble binaries using `objdump`, `readelf`, and `nm`.
4. Implement standard-compliant entry points, handle exit codes portably, and leverage `atexit` cleanup hooks.
5. Trace and diagnose memory corruption, crashes, and logic bugs using GDB breakpoints and hardware watchpoints.
6. Design robust GNU Makefiles that track dynamic header dependencies automatically.
7. Map high-level C logic to underlying x86-64 CPU registers, calling conventions, and instruction streams.
8. Organize enterprise-grade C projects with strict public API encapsulation and out-of-tree compilation.

---

## Navigation
- **Next Topic:** [01 · Toolchain](./01-toolchain.md)
- **Module Index:** [C Curriculum Overview](../README.md)
- **Next Module:** [01 · Fundamentals](../01-fundamentals/README.md)
