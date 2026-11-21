# The Complete C Systems Engineering Curriculum

> A rigorous, publication-grade reference curriculum and systems engineering manual spanning 14 comprehensive modules: from ISO C fundamentals and compiler internals to POSIX concurrency, hardware cache optimization, security hardening, and production reference implementations.

| Metadata | Details |
| :--- | :--- |
| **Track Code** | `01-languages/c` |
| **Scope** | 14 Modules / 150+ Comprehensive Concept Notes / 14 Production Projects |
| **Standard Baseline**| ISO C11 / ISO C17 / ISO C23 / POSIX.1-2008 / System V AMD64 ABI |
| **Status** | 100% Complete |
| **Author** | Technical Reviewer & Curriculum Engineer |

---

## Curriculum Philosophy

This curriculum is engineered for software engineers, systems programmers, and low-level architects who demand an uncompromising understanding of how software interfaces with hardware.

Every concept note in this repository adheres to a strict 13-section format:
1. **What & Why:** Motivation, real-world context, and industry necessity.
2. **Core Concepts:** Clear conceptual definitions and theoretical foundations.
3. **How It Works Internally:** Deep dive into CPU registers, assembly generation, stack frames, memory layouts, and compiler intermediate representations with diagrams.
4. **Syntax & Usage:** Idiomatic, modern C code patterns.
5. **Practical Examples:** Basic $\to$ Real-World $\to$ Edge-Case with verified outputs.
6. **Best Practices:** Production conventions and design guidelines.
7. **Common Mistakes & Pitfalls:** 6+ row diagnostic troubleshooting tables.
8. **Performance & Security Notes:** Cache locality, branch prediction, and vulnerability mitigation.
9. **Comparison Tables:** When to use what.
10. **Interview Questions:** In-depth technical questions with expandable `<details>` answers.
11. **Practice Exercises:** 3 hands-on coding challenges with full solutions.
12. **Summary / Cheat Sheet:** Rapid-review takeaways.
13. **Further Reading & Navigation:** Authoritative standard references and bi-directional module links.

---

## The 14 Curriculum Modules

```
01-languages/c/
├── 00-setup/                  # Toolchain, Compilers (GCC/Clang), Compilation Stages, Standards
├── 01-fundamentals/           # Program Structure, Data Types, Integers, Operators, Functions, I/O
├── 02-core-concepts/          # Arrays, Strings, Pointers, Memory, Structs, Unions, Preprocessor, Files
├── 03-intermediate/           # Multi-file, ADTs, Generics, Atomics, Sanitizers, Data Structures
├── 04-advanced/               # Memory Models, Inlines, Restrict, Volatile, Jumps, Metaprogramming
├── 05-systems-programming/    # POSIX Files, Processes, Signals, Threads, Atomics, Sockets, IPC
├── 06-build-and-tooling/      # GNU Make, Modern CMake, Clang-Tidy, Clang-Format, Gcov, CI/CD
├── 07-testing/                # Unity/CMocka, Mocking, Fuzzing (AFL/LibFuzzer), TDD, ASan/Valgrind
├── 08-security-and-safety/    # Buffer Overflows, Format Strings, CERT C, MISRA-C, Hardening, TOCTOU
├── 09-performance/            # CPU Caches, Profiling (perf), SIMD, SoA vs AoS, False Sharing, LTO/PGO
├── 10-design-and-patterns/    # Modular Opaque Handles, Vtables, GoF Patterns, Goto Cleanup, ABI Stability
├── 11-projects/               # 14 Full Reference Implementations (Shell, HTTP, KV, Allocator, JSON, etc.)
├── 12-interview/              # 9 Technical Interview Prep Tracks with 100+ Deep Q&A Pairs
└── 13-cheatsheets/            # 9 Quick Reference Cards (Syntax, Pointers, Stdlib, Formats, POSIX, C23)
```

---

## Module Directory Index

| Module | Name | Notes Count | Key Highlights |
| :--- | :--- | :--- | :--- |
| **[00-setup](./00-setup/README.md)** | Setup & Toolchains | 11 Notes | GCC, Clang, MSVC, ASTs, Preprocessor, Linker, C99/C11/C17/C23 standards |
| **[01-fundamentals](./01-fundamentals/README.md)** | Fundamentals | 15 Notes | Two's complement, promotions, sequence points, control flow, functions, I/O |
| **[02-core-concepts](./02-core-concepts/README.md)** | Core Concepts | 25 Notes | Pointer arithmetic, strings, structs, unions, `malloc`/`free`, file I/O |
| **[03-intermediate](./03-intermediate/README.md)** | Intermediate & ADTs | 23 Notes + Submodule | Slot 10 UB gap filled, generics (`_Generic`), opaque pointers, 19 data structures |
| **[04-advanced](./04-advanced/README.md)** | Advanced C | 12 Notes | Memory models, `restrict`, `volatile`, `setjmp`/`longjmp`, bit twiddling |
| **[05-systems-programming](./05-systems-programming/README.md)**| Systems Programming | 17 Notes | Fork, exec, signals, epoll, pthreads, atomics, shared memory, sockets |
| **[06-build-and-tooling](./06-build-and-tooling/README.md)** | Build & Tooling | 11 Notes | GNU Make DAGs, Modern CMake 3.x, Clang-Tidy, Sanitizer CI pipelines |
| **[07-testing](./07-testing/README.md)** | Testing & Quality | 7 Notes | Unity, CMocka test doubles, AFL fuzzing, Valgrind, branch coverage |
| **[08-security-and-safety](./08-security-and-safety/README.md)**| Security & Safety | 11 Notes | Stack canaries, ASLR, CERT C, MISRA-C rules, crypto pitfalls, TOCTOU |
| **[09-performance](./09-performance/README.md)** | Performance & Tuning | 10 Notes | L1/L2/L3 caches, `perf`, SIMD intrinsics, SoA vs AoS, False Sharing, LTO/PGO |
| **[10-design-and-patterns](./10-design-and-patterns/README.md)**| Design & Architecture | 7 Notes | Opaque pointers, Vtables, GoF patterns in C, goto cleanup, ABI stability |
| **[11-projects](./11-projects/README.md)** | Production Projects | 14 Projects | Shell, HTTP server, KV store, Allocator, JSON parser, Thread pool, VM |
| **[12-interview](./12-interview/README.md)** | Interview Guide | 9 Tracks | 100+ deep Q&As with memory layouts, UB puzzles, systems questions |
| **[13-cheatsheets](./13-cheatsheets/README.md)** | Cheatsheets | 9 Sheets | Syntax, spiral rule, stdlib, printf/scanf, gcc/gdb, posix, C23, bit tricks |

---

## Suggested Learning Roadmaps

### Path A: Computer Systems Engineering Track
1. Modules 00, 01, 02 (Language Foundations)
2. Module 05 (Systems Programming & POSIX)
3. Module 09 (Performance & Memory Hierarchy)
4. Module 10 (Design & Patterns)
5. Projects: `01-tiny-shell`, `02-http-server`, `04-memory-allocator`, `07-thread-pool`

### Path B: Embedded Systems & Firmware Track
1. Modules 00, 01, 02 (Foundations)
2. Module 03 (Bit manipulation & Incomplete Types)
3. Module 08 (Safety, MISRA-C, Hardening)
4. Module 09 (Cache Locality & Memory Alignment)
5. Module 12 (`09-embedded-c-qa.md`)
6. Projects: `13-ring-buffer-logger`, `12-file-compression-huffman`, `09-tiny-vm`

### Path C: High-Performance Infrastructure Track
1. Modules 02, 03 (Core Memory & ADTs)
2. Module 06 (Build Systems & Sanitizers)
3. Module 07 (Testing & Fuzzing)
4. Module 09 (SIMD, SoA vs AoS, False Sharing, LTO/PGO)
5. Projects: `03-key-value-store`, `08-lru-cache`, `10-dynamic-string-library`

---

## Quick Navigation

- [Alphabetical & Topic Index](./INDEX.md)
- [Systems Programming Glossary](./GLOSSARY.md)
- [Contributing Guidelines](./CONTRIBUTING.md)
- [Concept Note Template](./_templates/concept-template.md)
- [Project Template](./_templates/project-template.md)
