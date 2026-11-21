# Module 01: Java Fundamentals, Core Types, and Control Flow

> The architectural foundations of Java engineering: lexical grammar, program structure, primitive memory layouts, operator evaluation contracts, type casting, wrapper flyweights, modern switch expressions, loop optimizations, contiguous arrays, arbitrary-precision mathematics, and console stream I/O.

| Metadata | Value |
| :--- | :--- |
| **Track Section** | 01-languages / java |
| **Module Number** | 01 |
| **Prerequisites** | [00-setup](../00-setup/README.md) |
| **Baseline Runtime** | Oracle JDK 25 LTS / Eclipse Temurin 25 LTS |
| **Core Concepts** | Primitives, Two's Complement, IEEE 754, `BigDecimal`, Pattern Matching, SIMD Loops |

---

## 1. Architectural Overview & Conceptual Flow

```
[ 00 Program Structure ] ──► [ 01 Variables & Types ] ──► [ 02 Operators ]
  (Packages, Imports, AST)     (Stack vs Heap, Primitives)   (Precedence, Bitwise, Short-circuit)
           │
           ▼
[ 03 Type Casting ]      ──► [ 04 Wrappers & Autobox ] ──► [ 05 Conditionals ]
  (Widening/Narrowing,         (IntegerCache, Value-based,   (Pattern Matching Switch,
   checkcast, IEEE loss)        List.remove overload trap)    Guarded Patterns, Tableswitch)
           │
           ▼
[ 06 Loops & JIT ]       ──► [ 07 Arrays & Memory ]    ──► [ 08 Precision & Math ]
  (Unrolling, SIMD,            (Contiguous heap layout,      (IEEE 754, Two's complement,
   ConcurrentModification)      System.arraycopy, Covariance) BigDecimal, Banker's rounding)
           │
           ▼
[ 09 Console Stream I/O ]
  (System.in/out/err,
   Scanner newline trap, UTF-8)
```

---

## 2. Topic Directory & Navigation

| # | Topic Document | Primary Engineering Focus | Target Lines |
|---|---|---|---|
| 00 | [00_syntax-and-program-structure.md](./00_syntax-and-program-structure.md) | Lexical grammar, package hierarchy, Java 25 compact source files (`void main()`), module imports (`import module java.base;`), and Markdown Javadoc (`///`). | 507 |
| 01 | [01_variables-and-data-types.md](./01_variables-and-data-types.md) | 8 primitives, stack vs heap allocation, compressed OOPs, local variable type inference (`var`), and definite assignment. | 538 |
| 02 | [02_operators.md](./02_operators.md) | 15-level precedence hierarchy, binary numeric promotion, shift distance masking, compound assignment narrowing, and pattern matching `instanceof`. | 549 |
| 03 | [03_type-casting-and-conversion.md](./03_type-casting-and-conversion.md) | Widening vs narrowing primitive conversions, reference downcasting via `checkcast`, precision loss in `long` to `double`, and safe pattern extraction. | 522 |
| 04 | [04_wrapper-classes-and-autoboxing.md](./04_wrapper-classes-and-autoboxing.md) | Autoboxing bytecode desugaring (`valueOf`), `IntegerCache` flyweights, value-based classes, unboxing `NullPointerException`, and `List.remove` overload traps. | 533 |
| 05 | [05_conditionals.md](./05_conditionals.md) | Strict boolean `if/else`, modern switch expressions with arrow syntax (`->`), exhaustive sealed hierarchy matching, guarded patterns (`when`), and `tableswitch` vs `lookupswitch`. | 551 |
| 06 | [06_loops.md](./06_loops.md) | Standard, enhanced `for`, `while`, `do-while`, labeled breaks, fail-fast iterators (`ConcurrentModificationException`), loop unrolling, and SIMD vectorization. | 554 |
| 07 | [07_arrays.md](./07_arrays.md) | Contiguous heap memory layout, jagged arrays, array covariance traps (`ArrayStoreException`), vectorized `System.arraycopy()`, and `Arrays.mismatch()`. | 521 |
| 08 | [08_numeric-precision-and-math.md](./08_numeric-precision-and-math.md) | Two's complement integer overflow, `Math.*Exact`, IEEE 754 floating-point mantissa traps ($0.1+0.2 \ne 0.3$), `BigDecimal` scaling, and Banker's Rounding. | 513 |
| 09 | [09_console-input-output.md](./09_console-input-output.md) | Standard streams (`in`, `out`, `err`), formatted printing (`printf`), the `Scanner.nextInt()` newline skipping trap, secure password masking via `Console`, and high-throughput buffered reading. | 556 |

---

## 3. Core Engineering Competency Verification

You have mastered Java Fundamentals when you can definitively:
- [x] Explain the exact CPU instruction and binary reason why `(byte) 300` evaluates to `44` and `(byte) 130` evaluates to `-126`.
- [x] Diagnose and fix the silent `IntegerCache` comparison trap where `Integer a = 127, b = 127; a == b` is `true`, but `128` evaluates to `false`.
- [x] Prove mathematically why $0.1 + 0.2 \ne 0.3$ in IEEE 754 binary floating-point representation, and formulate exact calculations using `BigDecimal`.
- [x] Deconstruct sealed interface hierarchies and records using modern pattern matching `switch` expressions with zero fall-through and compile-time exhaustiveness.
- [x] Explain how HotSpot C2 vectorizes primitive array loops using SIMD instructions and eliminates array bounds checking via Range Check Elimination (RCE).
- [x] Implement safe console token extraction avoiding the classic `Scanner.nextInt()` trailing newline trap.

---

| [← Previous Module: 00-setup](../00-setup/README.md) | [Next Module: 02-methods →](../02-methods/README.md) |
