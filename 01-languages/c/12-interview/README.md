# Module 12: C Technical Interview Mastery Guide

> A rigorous, enterprise-grade technical interview preparation curriculum covering C language fundamentals, pointer arithmetic, memory models, tricky output prediction puzzles, bit twiddling, systems programming, C vs C++ contrasts, undefined behavior traps, and embedded systems architecture.

| Metadata | Details |
| :--- | :--- |
| **Module Code** | `12-interview` |
| **Prerequisites** | Modules 00 through 11 (Full C Curriculum) |
| **Duration** | ~30 hours |
| **Status** | Complete |
| **Standard Baseline**| C99 / C11 / C17 / C23 / POSIX.1-2008 / System V AMD64 ABI |

---

## Overview & Interview Strategy

Engineering interviews evaluating C proficiency do not test superficial trivia; they evaluate a candidate's mental model of:
1. **Physical Hardware Reality:** How variables, structs, stack frames, and pointers map to registers, cache lines, and virtual memory pages.
2. **Standard Guarantees vs Compiler Exploitation:** Distinguishing strictly conforming C from Undefined Behavior, and understanding how modern optimizing compilers exploit UB axioms.
3. **Defensive Systems Programming:** Resource leak prevention, concurrency safety, alignment constraints, and boundary condition validation.

This module provides exhaustive, deep-dive interview preparation categorized across 9 specialized guides.

---

## Interview Preparation Tracks

| # | Guide | Topic & Scope | Focus Areas |
| :--- | :--- | :--- | :--- |
| **01** | [01-c-basics-qa.md](./01-c-basics-qa.md) | Language Fundamentals | Types, storage classes, scopes, linkage, sequence points, preprocessor macros |
| **02** | [02-pointers-and-memory-qa.md](./02-pointers-and-memory-qa.md) | Pointers & Memory | Pointer arithmetic, multidimensional arrays, heap allocators, aliasing, alignment |
| **03** | [03-output-based-questions.md](./03-output-based-questions.md) | Output Prediction | Promotion quirks, precedence traps, evaluation order, side-effect expressions |
| **04** | [04-bit-manipulation-qa.md](./04-bit-manipulation-qa.md) | Bit Twiddling | Masks, parity, bit hacks, endianness, popcount, branchless arithmetic |
| **05** | [05-data-structures-coding-problems.md](./05-data-structures-coding-problems.md) | Coding Problems | Intrusive lists, binary trees, ring buffers, hash tables, heaps in pure C |
| **06** | [06-systems-qa.md](./06-systems-qa.md) | Systems & POSIX | Fork/exec, signals, file descriptors, sockets, pthreads, atomics, IPC |
| **07** | [07-c-vs-cpp.md](./07-c-vs-cpp.md) | C vs C++ Differences | Incompatibilities, ABI linkage (`extern "C"`), struct tags, casts, RAII |
| **08** | [08-undefined-behavior-puzzles.md](./08-undefined-behavior-puzzles.md) | UB Traps & Puzzles | Signed overflow, strict aliasing, null pointer optimizations, data races |
| **09** | [09-embedded-c-qa.md](./09-embedded-c-qa.md) | Embedded & Bare-Metal | `volatile`, memory-mapped I/O, interrupt handlers (ISRs), linker scripts, alignment |

---

[← Previous Module: Projects](../11-projects/README.md) | [C Language Track Index](../README.md) | [Next Module: Cheatsheets](../13-cheatsheets/README.md)
