# Module 03: Intermediate C

> Transitioning from core language syntax to robust systems programming: function pointers, callbacks, multi-file translation units, rigorous error propagation, assertions, and bit manipulation.

| Metadata | Details |
| :--- | :--- |
| **Module Code** | `03-intermediate` |
| **Prerequisites** | [00-setup](../00-setup/README.md), [01-fundamentals](../01-fundamentals/README.md), [02-core-concepts](../02-core-concepts/README.md) |
| **Duration** | ~15 hours |
| **Status** | In Progress (Batches 11–16) |
| **Standard Baseline**| ISO C11 / C17 / C23 |

---

## Overview & Learning Outcomes
Module 03 elevates your C programming from single-file scripts to modular, production-grade systems architectures. You will master:
1. **Dynamic Dispatch & Callbacks**: Function pointers, callback context patterns (`void *ctx`), reentrant API design, and table-driven state machines.
2. **Modular Multi-File Translation Units**: Header interface design, compilation units, link-time symbol resolution, and preventing circular dependencies.
3. **Defensive Error Handling**: Status codes, thread-local `errno`, cascading cleanup via `goto cleanup`, and compile-time/runtime assertions.
4. **Low-Level Bitwise Systems**: Bit manipulation, bitmasks, bit hacks, endianness conversions, and variadic functions.
5. **Undefined Behavior & Sanitizers**: Catching non-deterministic flaws with UBSan, ASan, and Valgrind.
6. **Data Structures**: Self-contained, robust implementations of lists, stacks, queues, trees, hash tables, heaps, and graphs under `data-structures/`.

---

## Module Table of Contents

### Core Intermediate Notes
| Note | Title | Level | Status | Focus Areas |
| :--- | :--- | :--- | :--- | :--- |
| **01** | [Function Pointers](./01-function-pointers.md) | Intermediate | Complete | Callbacks, `void *ctx`, `qsort`, dispatch tables |
| **02** | [Multi-File Programs](./02-multi-file-programs.md) | Intermediate | Complete | Headers, translation units, include guards, linkage |
| **03** | [Error Handling](./03-error-handling.md) | Intermediate | Complete | Status codes, `errno`, `strerror_r`, `goto cleanup` |
| **04** | [Assertions & Defensive Coding](./04-assertions-defensive-coding.md) | Intermediate | Complete | `assert`, `NDEBUG`, `static_assert`, invariants |
| **05** | [Bit Manipulation](./05-bit-manipulation.md) | Intermediate | Complete | Bitwise operators, bitmask flags, bit hacks |
| **06** | [Variadic Functions](./06-variadic-functions.md) | Intermediate | Complete | `va_list`, `va_start`, `va_arg`, `vsnprintf` |
| **07** | [Text Processing & Parsing](./07-text-processing-parsing.md) | Intermediate | Complete | Tokenizers, lexers, AST parsing, regex intro |
| **08** | [Unicode & Locale](./08-unicode-locale.md) | Intermediate | Complete | UTF-8, multi-byte encodings, `wchar_t` pitfalls |
| **09** | [Time and Randomness](./09-time-and-random.md) | Intermediate | Complete | `time.h`, high-res clocks, PRNGs, seeding |
| **10** | [Undefined Behavior & Sanitizers](./10-undefined-behavior-and-sanitizers.md) | Intermediate | Complete | **Slot 10 Filled!** ASan, UBSan, TSan, compiler traps |
| **11** | [Standard Library Overview](./11-standard-library.md) | Intermediate | Planned | POSIX vs ISO standard library catalog |
| **12** | [Generic Programming](./12-generic-programming.md) | Intermediate | Planned | `_Generic` macros, type-agnostic containers |
| **13** | [Opaque Pointers & ADTs](./13-opaque-pointers-and-adts.md) | Intermediate | Planned | Encapsulation, opaque handles, information hiding |
| **14** | [Callbacks & Dispatch Tables](./14-callbacks-and-function-pointer-tables.md) | Intermediate | Planned | Jump tables, event loops, plugin systems |
| **15** | [setjmp & longjmp](./15-setjmp-longjmp.md) | Intermediate | Planned | Non-local jumps, coroutine basics, exception emulation |
| **16** | [Inline, Restrict, Volatile](./16-inline-restrict-volatile.md) | Intermediate | Planned | Optimization qualifiers deep dive |
| **17** | [X-Macros & Advanced Preprocessor](./17-x-macros-and-advanced-preprocessor.md) | Intermediate | Planned | Code generation, enum synchronization |
| **18** | [Strict Aliasing & Type Punning](./18-strict-aliasing-and-type-punning.md) | Intermediate | Planned | TBAA rules, union punning, fast bit reinterpretation |
| **19** | [Integer Overflow & Checked Arithmetic](./19-integer-overflow-and-checked-arithmetic.md) | Intermediate | Planned | `<stdckdint.h>`, saturation, wrap-around guards |
| **20** | [Floating-Point Pitfalls](./20-floating-point-pitfalls.md) | Intermediate | Planned | IEEE-754 precision, epsilon comparisons, NaNs |
| **21** | [Signals Basics](./21-signals-basics-portable.md) | Intermediate | Planned | POSIX signals, `sig_atomic_t`, async-signal safety |
| **22** | [stdint, inttypes & Limits](./22-stdint-inttypes-and-limits.md) | Intermediate | Planned | Exact-width integers, format specifiers |
| **23** | [Locale & Wide Chars](./23-locale-and-wide-chars-deep-dive.md) | Intermediate | Planned | Internationalization, code pages, UTF-8 conversion |

### Sub-Module: Data Structures (`data-structures/`)
Complete reference implementations with tests under [`data-structures/`](./data-structures/):
- `01-linked-lists.md`, `02-stacks-queues.md`, `03-trees.md`, `04-hash-tables.md`, `05-graphs.md`
- `06-dynamic-array.md`, `07-queues-circular-buffer.md`, `08-heaps-and-priority-queues.md`, `09-binary-search-trees.md`
- `10-tries.md`, `11-union-find.md`, `12-sorting-algorithms.md`, `13-searching-algorithms.md`, `14-algorithm-complexity.md`
- `15-skip-lists.md`, `16-bloom-filters.md`, `17-lru-cache-structure.md`, `18-graph-algorithms.md`, `19-string-algorithms.md`

---

## Verified Code Companion
All examples are compiled and verified with:
```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -Werror *.c -o output.exe
```

---

[← Previous Module: 02-core-concepts](../02-core-concepts/README.md) | [Up: C Curriculum Index](../README.md) | [Next Module: 04-advanced →](../04-advanced/README.md)
