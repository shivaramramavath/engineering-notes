# Module 02: Core Concepts

> The definitive architectural foundation of the C language: pointers, memory management, composites, preprocessor mechanics, file streams, and memory layouts.

| Metadata | Details |
| :--- | :--- |
| **Module Code** | `02-core-concepts` |
| **Prerequisites** | [00-setup](../00-setup/README.md), [01-fundamentals](../01-fundamentals/README.md) |
| **Duration** | ~12 hours |
| **Status** | 100% Complete (25 of 25 Notes) |
| **Standard Baseline**| ISO C11 / C17 / C23 |

---

## Overview & Learning Outcomes
Module 02 transitions from elementary syntax to deep systems engineering. In this module, you will master:
1. **Contiguous & Non-Contiguous Memory**: Arrays, row-major matrices, strings, pointer arithmetic, and cache lines.
2. **Pointers & Indirection**: Virtual memory addressing, double pointers, strict aliasing, and the Clockwise/Spiral rule.
3. **User-Defined Aggregates**: Struct alignment padding, trailing padding, offset calculations, discriminated unions, and flexible array members.
4. **Dynamic Memory Disciplines**: Heap allocator internals, defensive `realloc` growth, amortized $O(1)$ dynamic arrays, and leak-free cleanups.
5. **System Streams & CLI**: Standard I/O buffering strategies, binary serialization, and robust argument vector parsing.

---

## Module Table of Contents

| Note | Title | Level | Status | Focus Areas |
| :--- | :--- | :--- | :--- | :--- |
| **01** | [Arrays](./01-arrays.md) | Core | Complete | Contiguous layout, decay, VLA pitfalls |
| **02** | [Strings](./02-strings.md) | Core | Complete | NTBS, `.rodata` literals, bounds safety |
| **03** | [Pointers](./03-pointers.md) | Core | Complete | Addresses, indirection, C23 `nullptr` |
| **04** | [Declarations & Typedef](./04-declarations-typedef.md) | Core | Complete | Spiral rule, complex function types |
| **05** | [Type Qualifiers](./05-type-qualifiers.md) | Core | Complete | `const`, `volatile`, `restrict`, `_Atomic` |
| **06** | [Structs, Unions & Enums](./06-structs-unions-enums.md) | Core | Complete | Padding, `offsetof`, tagged variants |
| **07** | [Scope & Storage Classes](./07-scope-storage-classes.md) | Core | Complete | Lexical scope, linkage, static duration |
| **08** | [Memory Management](./08-memory-management.md) | Core | Complete | `malloc`, `realloc`, heap chunks, safety |
| **09** | [Preprocessor](./09-preprocessor.md) | Core | Complete | Macros, `do-while(0)`, C23 `#embed` |
| **10** | [Command-Line Arguments](./10-command-line-arguments.md) | Core | Complete | `argc`, `argv`, `strtol` validation |
| **11** | [File I/O](./11-file-io.md) | Core | Complete | Streams, `setvbuf`, binary serialization |
| **12** | [Pointer Arithmetic](./12-pointer-arithmetic.md) | Core | Complete | `ptrdiff_t`, bounds rules, scaling |
| **13** | [Multi-Dimensional Arrays](./13-multi-dimensional-arrays.md) | Core | Complete | Row-major layout, flat striding, cache |
| **14** | [Dynamic Arrays & Realloc](./14-dynamic-arrays-and-realloc.md) | Core | Complete | Vectors, geometric growth, amortized $O(1)$ |
| **15** | [Safe String Handling](./15-string-h-functions-and-safe-alternatives.md) | Core | Complete | `snprintf`, truncation detection, BSD |
| **16** | [Bit-Fields](./16-bit-fields.md) | Core | Complete | Memory packing, endianness, hardware MMIO |
| **17** | [Enums & Flags](./17-enums-and-flags.md) | Core | Complete | Bitmasks, logic operators, C23 fixed enums |
| **18** | [Designated Initializers](./18-designated-initializers-and-compound-literals.md) | Core | Complete | Compound literals, sparse arrays, C23 `{}` |
| **19** | [Struct Padding & Alignment](./19-struct-padding-and-alignment.md) | Core | Complete | Natural alignment, reordering, `alignas` |
| **20** | [Process Memory Layout](./20-memory-layout-stack-heap-data-bss.md) | Core | Complete | `.text`, `.data`, `.bss`, heap, stack |
| **21** | [Flexible Array Members](./21-flexible-array-members.md) | Core | Complete | Single allocation variable records |
| **22** | [Restrict & Aliasing](./22-restrict-and-aliasing-intro.md) | Core | Complete | Strict aliasing, `char*` exception, SIMD |
| **23** | [Pointers to Pointers](./23-pointers-to-pointers-and-arrays-of-pointers.md) | Core | Complete | Out parameters, ragged tables, indirection |
| **24** | [Complex Declarations](./24-complex-declarations-reading-guide.md) | Core | Complete | Clockwise rule, signal signatures, `typeof` |
| **25** | [Linked Structures Intro](./25-linked-structures-with-pointers-intro.md) | Core | Complete | Self-referential structs, list mechanics |

---

## Verified Code Companion
All code examples in this module are located in [`examples/`](./examples/) and have been compiled and verified with:
```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -Werror *.c -o output.exe
```
Real execution outputs are recorded in:
- `real_outputs_batch6.json` (Notes 01–04)
- `real_outputs_batch7.json` (Notes 05–08)
- `real_outputs_batch8.json` (Notes 09–12)
- `real_outputs_batch9.json` (Notes 13–17)
- `real_outputs_batch10.json` (Notes 18–25)

---

[← Previous Module: 01-fundamentals](../01-fundamentals/README.md) | [Up: C Curriculum Index](../README.md) | [Next Module: 03-intermediate →](../03-intermediate/README.md)
