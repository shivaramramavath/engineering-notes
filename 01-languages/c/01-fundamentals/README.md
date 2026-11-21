# 01 · Fundamentals

> Comprehensive mastery of the core C language: program architecture, translation units, primitive data types, two's complement integer representation, storage durations, operator precedence hierarchies, type conversions, control flow, functions, standard I/O streams, non-deterministic behaviors, memory alignment, const-correctness, and value categories.

| Metric | Details |
|---|---|
| **Module Level** | Beginner → Intermediate Mastery |
| **Prerequisites** | [00 · Setup & Toolchain](../00-setup/README.md) |
| **Status** | Complete |
| **Standards Covered** | ISO C11 / C17 / C23 |

---

## Scope & Overview

This module establishes the bedrock foundation upon which all systems programming in C is constructed. While many programming languages hide the underlying machine representation, C operates in direct communion with hardware registers, physical memory addresses, and data bus widths.

Mastering this module guarantees that you understand:
1. Exactly how translation units compile and link via object file symbol tables.
2. How integer data types behave under two's complement and why signed overflow is undefined behavior.
3. The precise state machine of integer promotions and Usual Arithmetic Conversions.
4. How to format strings and parse inputs safely without buffer overflows.
5. How modern optimizing compilers exploit undefined behavior to delete dead code.
6. How hardware alignment and struct member ordering govern cache performance.

---

## Contents

| # | Note | Level | Status | Focus / Key Takeaway |
|---|---|---|---|---|
| 01 | [Program Structure](./01-program-structure.md) | Beginner | Complete | Translation units, external vs internal linkage (`static`/`extern`), symbol tables, CRT startup |
| 02 | [Data Types & Widths](./02-data-types.md) | Beginner | Complete | Primitive types, minimum widths, LP64 vs LLP64 data models, `<stdint.h>`, IEEE 754 floats |
| 03 | [Integer Representation](./03-integer-representation.md) | Intermediate | Complete | Two's complement, signed overflow UB vs unsigned modulo wrap, endianness, C23 `<stdckdint.h>` |
| 04 | [Variables & Constants](./04-variables-constants.md) | Beginner | Complete | Storage durations (auto/static/heap/thread), scopes, `const` vs `enum` vs C23 `constexpr` |
| 05 | [Operators & Precedence](./05-operators.md) | Beginner | Complete | 15 precedence tiers, bitwise masks, short-circuit `&&`/`\|\|`, sequenced comma operator |
| 06 | [Type Conversions](./06-type-conversions.md) | Intermediate | Complete | Integer promotion rules, Usual Arithmetic Conversions, sign comparison traps, strict aliasing |
| 07 | [Control Flow](./07-control-flow.md) | Beginner | Complete | Jump tables in `switch`, C23 `[[fallthrough]]`, Linux `goto cleanup` idiom, Duff's Device |
| 08 | [Functions](./08-functions.md) | Beginner | Complete | Pass-by-value semantics, stack frames, System V AMD64 ABI, function pointers, C23 `f(void)` |
| 09 | [Input & Output](./09-input-output.md) | Beginner | Complete | Standard streams, buffering (`_IOFBF`/`_IOLBF`/`_IONBF`), `snprintf` bounds safety, safe line reading |
| 10 | [Non-Deterministic Behaviors](./10-undefined-unspecified-implementation-defined.md) | Intermediate | Complete | The triad: Undefined vs Unspecified vs Implementation-Defined; optimizer UB exploitation |
| 11 | [Sizeof & Alignment](./11-sizeof-and-alignment-basics.md) | Intermediate | Complete | `sizeof` evaluation, hardware alignment, struct padding, member reordering, `alignas`/`alignof` |
| 12 | [Const-Correctness](./12-const-correctness-basics.md) | Intermediate | Complete | Read-only contracts, pointer-to-const vs const-pointer, clockwise rule, `.rodata` protection |
| 13 | [Promotions & Conversions](./13-integer-promotions-and-usual-arithmetic-conversions.md) | Intermediate | Complete | Conversion rank state machine, unsigned short promotion to signed int, arithmetic decision tree |
| 14 | [Sequence Points & Order](./14-sequence-points-and-evaluation-order.md) | Intermediate | Complete | Sequenced-before relations, unsequenced modification UB (`i = i++`), argument evaluation order |
| 15 | [Lvalues, Rvalues & Exprs](./15-lvalues-rvalues-and-expressions.md) | Intermediate | Complete | Value categories, modifiable lvalues, array decay mechanics, compound literals as lvalues |

---

## Learning Outcomes

By completing this module, you will:
1. Design multi-file C programs with strict symbol visibility and proper internal/external linkage.
2. Select exact-width integer types (`<stdint.h>`) that guarantee consistent memory layout across 32-bit and 64-bit systems.
3. Eliminate all integer truncation, signed overflow undefined behavior, and sign-comparison security flaws.
4. Eliminate uninitialized local variable reads and prevent stack buffer leaks across sockets.
5. Structure robust `switch` statements with verified jump tables and standard C23 `[[fallthrough]]` annotations.
6. Implement single-exit resource cleanup using the canonical Linux kernel `goto cleanup` pattern.
7. Implement bounds-checked formatting and input parsing using `snprintf` and `fgets`.
8. Order struct members by decreasing alignment to slash memory padding by up to 33%.
9. Enforce strict const-correctness across public library interfaces and protect data in `.rodata`.
10. Eliminate unsequenced evaluation bugs and distinguish modifiable lvalues from rvalues.

---

## Navigation
- **Previous Module:** [00 · Setup & Toolchain](../00-setup/README.md)
- **Next Module:** [02 · Core Concepts](../02-core-concepts/README.md)
- **Curriculum Index:** [C Master Curriculum Index](../README.md)
