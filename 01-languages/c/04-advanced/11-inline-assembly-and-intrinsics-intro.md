# Inline Assembly and Hardware Intrinsics in C

> Deep architectural exploration of GCC Extended Inline Assembly and hardware intrinsics in C: operand constraints, memory clobbers, CPU pipeline barriers, popcount, bit counting, and SIMD vector primitives.

| Metadata | Details |
| :--- | :--- |
| **Level** | Advanced |
| **Prerequisites** | [05-abi-and-calling-conventions.md](./05-abi-and-calling-conventions.md), [07-interfacing-with-assembly.md](./07-interfacing-with-assembly.md) |
| **Time to Read** | 40 minutes |
| **Status** | Complete |
| **Standard Baseline** | ISO C11 / C17 / C23 (GCC & Clang extended asm) |

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
9. [Comparison: Inline Assembly vs Compiler Intrinsics](#9-comparison-inline-assembly-vs-compiler-intrinsics)
10. [Interview Questions](#10-interview-questions)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary & Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why
While external assembly files (`.asm` / `.s`) are ideal for large subroutines, calling an external assembly function introduces function call overhead: pushing return addresses, executing `call` and `ret`, and spilling caller-saved registers.

For small, single-instruction CPU capabilities (e.g. reading the time-stamp counter `rdtsc`, atomic compare-and-swap, counting set bits, or memory fences), engineers use two techniques directly inside C source code:
1. **Extended Inline Assembly (`__asm__ __volatile__`)**: Embeds raw machine instructions directly into the compiler's instruction stream with explicit input/output constraint bindings.
2. **Compiler Built-in Intrinsics (`__builtin_*`)**: C-style pseudo-functions provided by GCC and Clang that the compiler translates directly into dedicated hardware machine instructions with zero overhead.

---

## 2. Core Concepts

### 2.1 The Extended Assembly Syntax
GCC extended assembly uses a 4-part colon-delimited structure:
```c
__asm__ __volatile__ (
    "instruction_template"
    : output_operands
    : input_operands
    : clobber_list
);
```
- **`__volatile__`**: Instructs the compiler optimizer not to delete, hoist, or reorder the assembly block even if its outputs appear unused.

### 2.2 Constraint Codes
- **`"r"`**: Allocate any general-purpose register for this operand.
- **`"m"`**: The operand is a memory address (no register load needed).
- **`"i"`**: Immediate integer constant known at compile time.
- **`"=r"`**: Write-only output register.
- **`"+r"`**: Read-modify-write operand.
- **`"&r"`** (Earlyclobber): Output register is modified before input operands are consumed.

### 2.3 The Clobber List
Tells the compiler which machine state was modified as a side-effect:
- **`"memory"`**: **Compiler Memory Barrier**. Tells the optimizer that arbitrary RAM has been read or written. The compiler flushes cached register values back to memory and does not reorder memory operations across this barrier.
- **`"cc"`**: Condition codes (CPU flags register `EFLAGS`) were modified by arithmetic.

### 2.4 Built-in Hardware Intrinsics
Whenever possible, **prefer compiler intrinsics over inline assembly**! Intrinsics allow the compiler optimizer to track types, reorder independent instructions, and perform register allocation optimally:
- `__builtin_popcount(x)`: Translates to hardware `POPCNT` instruction.
- `__builtin_clz(x)`: Count Leading Zeros (`LZCNT` / `BSR`).
- `__builtin_ctz(x)`: Count Trailing Zeros (`TZCNT` / `BSF`).
- `__builtin_bswap32(x)`: Hardware byte swap (`BSWAP`).

---

## 3. How It Works Internally

### Constraint Binding & Register Allocation

```
C Code:
int in_val = 10, out_val;
__asm__ ("add $5, %0" : "=r"(out_val) : "0"(in_val) : "cc");

Compiler Generation:
1. Allocates register (e.g. EAX) for in_val
2. Emits: add $5, %eax
3. Assigns out_val to EAX
```

```mermaid
flowchart LR
    CVar["C Variables (in, out)"] --> Bind["Compiler Constraint Matching ('r', '=r')"]
    Bind --> RegAlloc["Allocates Registers (e.g. %eax, %edx)"]
    RegAlloc --> Emit["Emits Inlined Machine Instructions"]
```

---

## 4. Syntax & Usage

```c
#include <stdint.h>

// Reading CPU Hardware Time-Stamp Counter using inline asm
static inline uint64_t rdtsc_asm(void) {
    uint32_t lo, hi;
    __asm__ __volatile__ (
        "rdtsc"
        : "=a" (lo), "=d" (hi)
    );
    return ((uint64_t)hi << 32) | lo;
}
```

---

## 5. Practical Examples

### Example 1: Hardware Intrinsics Demonstration
Goal: Demonstrate hardware bit-counting intrinsics for popcount and leading zeros.

```c
#include <stdio.h>
#include <stdint.h>

static int count_set_bits(uint32_t val) {
    return __builtin_popcount(val);
}

static int count_leading_zeros(uint32_t val) {
    return val ? __builtin_clz(val) : 32;
}

int main(void) {
    printf("=== Hardware Intrinsics & Assembly Operations ===\n");
    uint32_t num = 0x000F0000;
    printf("Popcount for 0x%08X: %d\n", num, count_set_bits(num));
    printf("Leading zeros for 0x%08X: %d\n", num, count_leading_zeros(num));
    return 0;
}
```

**Verified Output:**
```text
=== Hardware Intrinsics & Assembly Operations ===
Popcount for 0x000F0000: 4
Leading zeros for 0x000F0000: 12
```

---

## 6. Best Practices
1. **Prefer Intrinsics Over Inline Assembly**: Use `__builtin_*` or standard headers (`<immintrin.h>`, `<stdatomic.h>`). Compilers understand intrinsics and can optimize them across surrounding code.
2. **Always Clobber `"memory"` for Atomic and Device Operations**: If an assembly block modifies shared memory or reads memory mapped I/O (MMIO), add `"memory"` to prevent the compiler from caching memory reads across the block.
3. **Guard `__builtin_clz(0)` and `__builtin_ctz(0)`**: On x86, passing `0` to `__builtin_clz` invokes **undefined behavior** (the underlying `BSR` instruction leaves the destination register undefined). Always check `if (val == 0) return 32;`.
4. **Use Earlyclobber (`"&r"`) for Multi-Step Loops**: If an output register is written before all input registers are read, mark it with `"&"` so the compiler doesn't assign the same physical register to both an input and an output.
5. **Mark Side-Effect Assembly as `__volatile__`**: Pure calculations without side effects can omit `volatile` to allow the compiler to eliminate the block if its output is unused. Hardware instructions (`cpuid`, `outb`, `cli`) must be `volatile`.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Is Dangerous | Correct Approach |
| :--- | :--- | :--- |
| Missing clobber declarations | Compiler assumes unmentioned registers were unchanged, resulting in register corruption. | Explicitly list modified registers or flags (`"cc"`, `"memory"`). |
| Passing 0 to `__builtin_clz` | **Undefined Behavior**: Hardware instruction produces undefined CPU register value when input is 0. | Guard `if (val == 0) return 32;`. |
| Omitting `__volatile__` on hardware I/O | Compiler dead-code elimination deletes assembly blocks whose outputs are not referenced in C code. | Use `__asm__ __volatile__`. |
| Writing AT&T syntax with Intel flag | Compiling with `-masm=intel` breaks standard AT&T assembly blocks (`%eax` vs `eax`). | Maintain consistent syntax dialect or use `%k0` operand syntax. |
| Memory barriers without compiler barrier | Hardware CPU memory fences (`mfence`) do not stop the *compiler* from reordering instructions unless `"memory"` is clobbered. | Add `"memory"` to clobber list. |

---

## 8. Performance & Security Notes

### Compiler Optimization of Intrinsics
Unlike hand-written assembly strings which are black boxes to the compiler, built-in intrinsics participate directly in the compiler's Intermediate Representation (IR):
- Constant folding: `__builtin_popcount(7)` evaluates to `3` at compile time with zero runtime instructions!
- Vectorization: Loops using `__builtin_bswap32` are automatically vectorized into AVX2 `vpshufb` instructions.

---

## 9. Comparison: Inline Assembly vs Compiler Intrinsics

```
Compiler Intrinsics (__builtin_*):
- Optimization: Fully optimized, constant-folded, and vectorized by compiler.
- Safety: Type-checked; zero risk of register clobbering bugs.
- Portability: Portable across GCC and Clang.

Extended Inline Assembly (__asm__):
- Optimization: Compiler treats template as opaque black box; cannot optimize inside.
- Safety: Error-prone; programmer must specify exact constraints and clobbers.
- Portability: Target-architecture specific (x86_64 vs ARM64).
```

---

## 10. Interview Questions

<details>
<summary><strong>1. What does the `"memory"` clobber do in GCC inline assembly?</strong></summary>

It acts as a **Compiler Optimization Barrier**. It tells the compiler that the assembly block may read or write arbitrary memory locations through pointers unknown to the compiler. The compiler must flush all pending register values back to RAM before the assembly block, and reload variables from RAM after the block. (Note: it is a *compiler barrier*, not a hardware memory bus fence!).
</details>

<details>
<summary><strong>2. What is an Earlyclobber (`"&"`) operand constraint?</strong></summary>

By default, the compiler assumes all input operands are consumed before any output operand is written, so it may assign the same register (e.g. `eax`) to both an input and an output. If the assembly block writes to the output register *before* finishing with the input register, the input is destroyed. Adding `"&"` (e.g. `"=&r"`) tells the compiler to allocate a distinct register for the output.
</details>

<details>
<summary><strong>3. Why is `__builtin_clz(0)` undefined behavior?</strong></summary>

On x86 architectures, `__builtin_clz` maps to the `BSR` (Bit Scan Reverse) or `LZCNT` instruction. When the source operand is 0, the `BSR` instruction does not set the destination register and sets the Zero Flag (`ZF = 1`), leaving the destination register with unpredictable garbage.
</details>

<details>
<summary><strong>4. How do you implement a branchless minimum of two integers using inline assembly?</strong></summary>

Using the conditional move instruction `cmov`:
```c
int min_asm(int a, int b) {
    int res = a;
    __asm__ (
        "cmp %2, %1
	"
        "cmovg %2, %0"
        : "+r" (res)
        : "r" (a), "r" (b)
        : "cc"
    );
    return res;
}
```
</details>

<details>
<summary><strong>5. What is the difference between `volatile` on a C variable vs `__volatile__` on an `__asm__` block?</strong></summary>

- `volatile` on a variable: Every read or write must access memory directly; cannot be cached in CPU registers.
- `__volatile__` on `__asm__`: The assembly block cannot be moved, duplicated, or removed by dead code elimination, even if its outputs appear unused.
</details>

<details>
<summary><strong>6. How do you implement a CPU instruction pipeline barrier (hardware pause) for spinlocks?</strong></summary>

Use the `pause` instruction (`_mm_pause()` or `__asm__ __volatile__("pause")`). It tells the CPU pipeline that the thread is in a spinlock loop, preventing memory order pipeline violations and reducing power consumption.
</details>

<details>
<summary><strong>7. What header file provides Intel/AMD SIMD intrinsics?</strong></summary>

`<immintrin.h>` includes all x86 SIMD instruction sets (MMX, SSE, SSE2, SSE3, SSSE3, SSE4.1, SSE4.2, AVX, AVX2, AVX-512, FMA, AES).
</details>

<details>
<summary><strong>8. How can you read the CPU flags register (`EFLAGS`)?</strong></summary>

Using `pushf` (Push Flags to stack) followed by `pop` into a general-purpose register:
```c
uint64_t read_flags(void) {
    uint64_t flags;
    __asm__ __volatile__ ("pushfq; popq %0" : "=r"(flags));
    return flags;
}
```
</details>

---

## 11. Practice Exercises

### Easy 1: Hardware Byte Swap with Intrinsics
Implement `uint32_t bswap32(uint32_t x);` using GCC intrinsic.

<details>
<summary>Solution</summary>

```c
uint32_t bswap32(uint32_t x) {
    return __builtin_bswap32(x);
}
```
</details>

### Easy 2: Parity Check with Intrinsic
Write a function returning `1` if an integer has an odd number of set bits, `0` if even.

<details>
<summary>Solution</summary>

```c
int parity(unsigned int x) {
    return __builtin_parity(x);
}
```
</details>

### Easy 3: Compiler Memory Barrier Macro
Define a standard compiler reordering barrier macro.

<details>
<summary>Solution</summary>

```c
#define COMPILER_BARRIER() __asm__ __volatile__("" ::: "memory")
```
</details>

### Medium 1: Safe Count Leading Zeros
Implement `int safe_clz32(uint32_t x);` returning 32 when $x = 0$.

<details>
<summary>Solution</summary>

```c
int safe_clz32(uint32_t x) {
    return (x == 0) ? 32 : __builtin_clz(x);
}
```
</details>

### Medium 2: Atomic Exchange using Inline Assembly
Implement `int asm_atomic_xchg(int *ptr, int new_val);` using `xchg`.

<details>
<summary>Solution</summary>

```c
int asm_atomic_xchg(int *ptr, int new_val) {
    int old_val = new_val;
    __asm__ __volatile__ (
        "xchg %0, %1"
        : "+r" (old_val), "+m" (*ptr)
        :
        : "memory"
    );
    return old_val;
}
```
</details>

### Medium 3: Fast Integer Log2 using CLZ
Compute $\lfloor \log_2 N \rfloor$ in $O(1)$ time for positive 32-bit integers.

<details>
<summary>Solution</summary>

```c
int fast_log2_u32(uint32_t val) {
    return (val == 0) ? -1 : (31 - __builtin_clz(val));
}
```
</details>

### Hard 1: 128-Bit Multiplication via Inline Assembly
Multiply two 64-bit integers and produce a 128-bit result using `mulq`.

<details>
<summary>Solution</summary>

```c
void mul64_128(uint64_t a, uint64_t b, uint64_t *hi_out, uint64_t *lo_out) {
    uint64_t hi, lo;
    __asm__ (
        "mulq %3"
        : "=a" (lo), "=d" (hi)
        : "a" (a), "r" (b)
        : "cc"
    );
    *hi_out = hi;
    *lo_out = lo;
}
```
</details>

### Hard 2: Spinlock with `pause` and `test-and-set`
Implement an assembly-optimized spinlock using `__atomic_test_and_set` and `pause`.

<details>
<summary>Solution</summary>

```c
#include <stdatomic.h>

void spin_lock(atomic_flag *lock) {
    while (atomic_flag_test_and_set_explicit(lock, memory_order_acquire)) {
        __asm__ __volatile__("pause");
    }
}

void spin_unlock(atomic_flag *lock) {
    atomic_flag_clear_explicit(lock, memory_order_release);
}
```
</details>

### Hard 3: SIMD Vectorized Array Dot Product with AVX2
Implement dot product of two float arrays using `<immintrin.h>`.

<details>
<summary>Solution</summary>

```c
#include <immintrin.h>

float dot_product_avx(const float *a, const float *b, size_t n) {
    __m256 sum = _mm256_setzero_ps();
    size_t i = 0;
    for (; i + 8 <= n; i += 8) {
        __m256 va = _mm256_loadu_ps(a + i);
        __m256 vb = _mm256_loadu_ps(b + i);
        sum = _mm256_fmadd_ps(va, vb, sum);
    }
    float buf[8];
    _mm256_storeu_ps(buf, sum);
    float total = buf[0] + buf[1] + buf[2] + buf[3] + buf[4] + buf[5] + buf[6] + buf[7];
    for (; i < n; i++) total += a[i] * b[i];
    return total;
}
```
</details>

---

## 12. Summary & Cheat Sheet

| Mechanism | Code / Constraint | Use Case |
| :--- | :--- | :--- |
| **Volatile** | `__asm__ __volatile__` | Prevent dead code removal |
| **Compiler Fence**| `"" ::: "memory"` | Prevent compiler instruction reordering |
| **Output Reg** | `"=r" (val)` | Bind variable to register output |
| **Earlyclobber** | `"=&r" (val)` | Output modified before inputs read |
| **Intrinsics** | `__builtin_popcount` | Hardware instruction with IR optimization |

---

## 13. Further Reading
- GCC Documentation: *How to Use Inline Assembly Language with the GCC*.
- Intel 64 and IA-32 Architectures Software Developer's Manual (Volume 2: Instruction Set Reference).

---

[← Previous Note: Symbol Visibility & ELF Basics](./10-symbol-visibility-and-elf-basics.md) | [Advanced C Index](./README.md) | [Next Note: Object Files and the Linker](./12-object-files-and-the-linker-in-depth.md)
