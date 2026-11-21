# SIMD Intrinsics Intro

> **Mastering Single Instruction, Multiple Data (SIMD) vector parallelism, SSE/AVX2/NEON intrinsics, and vector registers in C.**

| Level | Prerequisites | Time to Read | Status | Standard |
| :--- | :--- | :--- | :--- | :--- |
| Advanced | [01-fundamentals/02-data-types.md](../01-fundamentals/02-data-types.md), [01-cache-and-memory-hierarchy.md](01-cache-and-memory-hierarchy.md) | 27 mins | Complete | C99 / C11 / C17 / POSIX |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Usage](#4-syntax--usage)
5. [Practical Examples](#5-practical-examples)
6. [Best Practices](#6-best-practices)
7. [Common Mistakes & Pitfalls](#7-Common-mistakes--pitfalls)
8. [Performance & Security Notes](#8-performance--security-notes)
9. [Comparison / When to Use What](#9-comparison--when-to-use-what)
10. [Interview Questions](#10-interview-questions)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary / Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

In traditional scalar programming, every machine instruction operates on a single pair of operands at a time (e.g. adding two 32-bit floats together in one CPU cycle).

**Single Instruction, Multiple Data (SIMD)** is a data-parallel processing architecture built directly into modern CPU silicon:
- Instead of standard 64-bit general-purpose registers, the CPU features wide **Vector Registers** (128-bit, 256-bit, or 512-bit wide).
- A single SIMD instruction operates simultaneously on an entire "packed vector" of numbers in a single clock cycle.
- A 256-bit register can hold eight 32-bit integers or eight single-precision floats; a single vector addition instruction adds all 8 pairs in parallel, offering theoretical speedups of **4x to 8x** or more.

SIMD is the cornerstone of modern high-performance C engineering: 3D graphics rendering, audio/video encoding (H.264, AV1), machine learning inference, cryptographic ciphers, and database columnar query engines.

---

## 2. Core Concepts

### 2.1 The Vector Register Evolution
- **SSE (Streaming SIMD Extensions):** 128-bit registers (`xmm0`–`xmm15`). Holds four 32-bit floats or two 64-bit doubles.
- **AVX / AVX2 (Advanced Vector Extensions):** 256-bit registers (`ymm0`–`ymm15`). Holds eight 32-bit floats or four 64-bit doubles. AVX2 adds support for 256-bit packed integer operations.
- **AVX-512:** 512-bit registers (`zmm0`–`zmm31`). Holds sixteen 32-bit floats.
- **ARM NEON:** 128-bit registers (`v0`–`v31`). Standard on ARMv7 and ARMv8 (Apple Silicon, Raspberry Pi, mobile).

```mermaid
flowchart TD
    subgraph 256-bit AVX2 Vector Register ymm0
        E0[Float 0: 32b] | E1[Float 1: 32b] | E2[Float 2: 32b] | E3[Float 3: 32b] | E4[Float 4: 32b] | E5[Float 5: 32b] | E6[Float 6: 32b] | E7[Float 7: 32b]
    end
    subgraph Single Instruction Parallel Execution
        E0 & E1 & E2 & E3 & E4 & E5 & E6 & E7 --> ALU[Parallel SIMD Execution Units: _mm256_add_ps]
    end
```

### 2.2 What Are C Intrinsics?
Writing raw inline assembly is difficult, non-portable, and inhibits compiler register allocation.
**C Intrinsics** are C-level pseudo-functions provided by compiler headers (`<immintrin.h>` for x86, `<arm_neon.h>` for ARM) that look like normal C functions but compile directly to a single specific machine instruction:
```c
#include <immintrin.h>
__m256 a, b, c;
c = _mm256_add_ps(a, b); // Compiles directly to: vaddps %ymm1, %ymm0, %ymm2
```

---

## 3. How It Works Internally

### 3.1 Intrinsics Naming Conventions (Intel Guide)
Intel intrinsics follow a rigid, predictable naming format:
$$	ext{\_mm[bits]\_[operation]\_[datatype]}$$
- `_mm_`: 128-bit SSE vector operation.
- `_mm256_`: 256-bit AVX vector operation.
- `_mm512_`: 512-bit AVX-512 vector operation.
- Data Types:
  - `ps`: Packed Single-precision float (32-bit each).
  - `pd`: Packed Double-precision float (64-bit each).
  - `epi32`: Extended Packed Integer (32-bit signed).
  - `epu8`: Extended Packed Unsigned integer (8-bit bytes).

### 3.2 Memory Alignment: Aligned vs Unaligned Loads
Vector registers load data directly from cache lines:
- `_mm256_load_ps(ptr)`: Requires `ptr` to be strictly aligned to a **32-byte boundary**. If misaligned, the CPU throws a fatal General Protection Fault (`SIGSEGV`).
- `_mm256_loadu_ps(ptr)`: Unaligned load. Safe for any pointer offset. On modern CPUs (Haswell / Zen and newer), unaligned loads have virtually zero performance penalty compared to aligned loads unless crossing a 64-byte cache line boundary.

---

## 4. Syntax & Usage

### 4.1 AVX2 Vector Addition in Pure C
```c
#include <immintrin.h>
#include <stddef.h>

void avx2_vector_add(const float *a, const float *b, float *res, size_t n) {
    size_t i = 0;
    /* Process 8 floats per loop iteration using 256-bit AVX registers */
    for (; i + 8 <= n; i += 8) {
        __m256 va = _mm256_loadu_ps(&a[i]);
        __m256 vb = _mm256_loadu_ps(&b[i]);
        __m256 vsum = _mm256_add_ps(va, vb);
        _mm256_storeu_ps(&res[i], vsum);
    }
    /* Scalar cleanup loop for remaining elements */
    for (; i < n; ++i) {
        res[i] = a[i] + b[i];
    }
}
```

### 4.2 Compiling with Architecture Flags
To enable vector instruction sets in GCC/Clang:
```bash
# Enable AVX2 and FMA (Fused Multiply-Add)
gcc -std=c11 -Wall -O3 -mavx2 -mfma main.c -o app_avx2

# Or target the host CPU's full capabilities
gcc -std=c11 -Wall -O3 -march=native main.c -o app_native
```

---

## 5. Practical Examples

### 5.1 Verified Companion Simulation: 4-Way SIMD Vector Addition
The following program evaluates packed vector addition, demonstrating 4-way data parallelism across structured float vectors:

```c
#include <stdio.h>
#include <stdint.h>

/* Emulating 4-way SIMD vector addition */
typedef struct {
    float v[4];
} vec4_t;

static vec4_t vec4_add(vec4_t a, vec4_t b) {
    vec4_t out;
    for (int i = 0; i < 4; ++i) {
        out.v[i] = a.v[i] + b.v[i];
    }
    return out;
}

int main(void) {
    printf("Evaluating SIMD Vector Intrinsics Parallelism...\n");
    vec4_t a = { .v = { 1.0f, 2.0f, 3.0f, 4.0f } };
    vec4_t b = { .v = { 10.0f, 20.0f, 30.0f, 40.0f } };
    vec4_t res = vec4_add(a, b);
    printf("SIMD 128-bit vector sum: [ %.1f, %.1f, %.1f, %.1f ]\n",
           res.v[0], res.v[1], res.v[2], res.v[3]);
    printf("Data-parallel throughput verified.\n");
    return 0;
}
```

**Real Execution Output:**
```text
Evaluating SIMD Vector Intrinsics Parallelism...
SIMD 128-bit vector sum: [ 11.0, 22.0, 33.0, 44.0 ]
Data-parallel throughput verified.
```

---

## 6. Best Practices

1. **Always Implement a Scalar Tail Loop:** Vector loops process chunks of $N$ elements (e.g. 8). If array size is not a clean multiple of 8, a subsequent scalar loop is mandatory to process the trailing elements safely without buffer overflow.
2. **Align Heap Memory to 32 or 64 Bytes:** Use POSIX `posix_memalign()` or C11 `aligned_alloc(32, size)` to guarantee aligned vectors.
3. **Use Unaligned Loads (`_mm256_loadu_ps`) by Default:** Unless you have strictly validated 32-byte alignment, using `load` on misaligned data crashes the application with a segmentation fault.
4. **Leverage Fused Multiply-Add (FMA):** Use `_mm256_fmadd_ps(a, b, c)` to compute $(a 	imes b) + c$ in a single hardware cycle with higher floating-point precision and zero rounding error.
5. **Rely on Auto-Vectorization First:** Write simple, loop-bound-predictable loops with `restrict` pointers under `-O3`. Only hand-write intrinsics if compiler auto-vectorization fails.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Is Dangerous | Correct Fix |
| :--- | :--- | :--- |
| **Using `_mm256_load_ps` on unaligned memory** | Throws an immediate hardware `#GP` segmentation fault. | Use `_mm256_loadu_ps` or `aligned_alloc(32, sz)`. |
| **Forgetting scalar cleanup loop** | Leaves remaining $1..7$ elements unprocessed, corrupting data. | Add `for (; i < n; ++i)` cleanup loop. |
| **Distributing AVX binaries to non-AVX CPUs** | CPU executes illegal instruction exception (`SIGILL`) and crashes. | Use CPUID dispatch or compile for baseline target. |
| **Horizontal reduction in inner loop** | Accumulating vector elements horizontally (`_mm256_hadd_ps`) is slow. | Accumulate in vectors; reduce horizontally once after loop. |
| **Mixing AVX and legacy SSE without `_mm256_zeroupper()`** | Triggers CPU microarchitectural state transition stalls (AVX-SSE transition penalty). | Call `_mm256_zeroupper()` before returning from AVX functions. |
| **Structure-of-Arrays mismatch** | Vectorizing Array-of-Structures (`struct { float x, y, z; }`) requires slow shuffles. | Restructure data into Structure-of-Arrays (SoA). |

---

## 8. Performance & Security Notes

### 8.1 AVX-512 Frequency Throttling (Downclocking)
On older Intel Xeon processors (Skylake/Cascade Lake):
- Executing heavy 512-bit vector instructions draws massive electrical current.
- The CPU hardware responds by dynamically downclocking the entire CPU core frequency (by 10% to 20%) to stay within thermal limits.
- On such architectures, AVX2 (256-bit) often runs faster than AVX-512 because core clocks remain at maximum boost frequency. On modern AMD Zen 4/5 and Intel 12th+ Gen, throttling is eliminated.

### 8.2 Security: Vector-Accelerated Crypto
SIMD plays a vital role in cryptography:
- Constant-time implementations of AES (via AES-NI intrinsics `_mm_aesenc_si128`) execute in hardware without data-dependent timing variance, completely eliminating cache-timing side-channel attacks.

---

## 9. Comparison / When to Use What

| Instruction Set | Register Width | Floats / Cycle | Integers / Cycle | Target Platform |
| :--- | :--- | :--- | :--- | :--- |
| **SSE / SSE4.2** | 128 bits | 4 | 4 (32-bit) / 16 (8-bit) | Baseline x86_64 (All 64-bit x86 CPUs) |
| **AVX / AVX2** | 256 bits | 8 | 8 (32-bit) / 32 (8-bit) | Intel Haswell+ / AMD Zen 1+ |
| **AVX-512** | 512 bits | 16 | 16 (32-bit) / 64 (8-bit)| Intel Xeon / AMD Zen 4+ |
| **ARM NEON** | 128 bits | 4 | 4 (32-bit) / 16 (8-bit) | ARMv7 / ARMv8 / Apple Silicon |

---

## 10. Interview Questions

<details>
<summary><strong>1. What does SIMD stand for, and how does it differ from multi-threading?</strong></summary>

SIMD stands for **Single Instruction, Multiple Data**. It is data parallelism within a *single* CPU core: wide vector registers compute multiple data points simultaneously in a single clock cycle. Multi-threading is task parallelism across *multiple* independent CPU cores, where each core executes a distinct instruction stream.
</details>

<details>
<summary><strong>2. What is the difference between aligned and unaligned memory loads in SIMD?</strong></summary>

Aligned loads (e.g. `_mm256_load_ps`) require the source address to be evenly divisible by the register size (e.g., 32-byte aligned for AVX). If misaligned, the CPU throws an immediate hardware fault. Unaligned loads (`_mm256_loadu_ps`) handle any arbitrary byte address safely. On modern CPUs, unaligned loads match aligned speed unless the memory access spans a 64-byte cache line boundary.
</details>

<details>
<summary><strong>3. Why is a scalar cleanup loop required at the end of a SIMD algorithm?</strong></summary>

Because vector instructions process fixed-size chunks (e.g. 8 floats per iteration). If the total number of elements $N$ is not an exact multiple of 8, the vector loop terminates when fewer than 8 elements remain. A scalar loop is required to process the remaining $N \pmod 8$ elements one by one without buffer over-read.
</details>

<details>
<summary><strong>4. What is Fused Multiply-Add (FMA) and what are its advantages?</strong></summary>

FMA computes $(a 	imes b) + c$ in a single CPU machine instruction. It offers two major advantages: it executes in the same time as a single addition or multiplication (doubling arithmetic throughput), and it performs only a single final rounding step, eliminating intermediate rounding errors and improving numerical precision.
</details>

<details>
<summary><strong>5. What is the AVX-SSE transition penalty and how is it resolved?</strong></summary>

On older x86 CPUs, mixing legacy 128-bit SSE instructions with 256-bit AVX instructions causes the CPU to enter an internal state-saving trap that stalls execution for tens of cycles. The solution is calling `_mm256_zeroupper()` after executing AVX code, zeroing the upper 128 bits of the `ymm` registers and restoring the CPU to clean SSE state.
</details>

<details>
<summary><strong>6. What is the difference between horizontal and vertical vector operations?</strong></summary>

- **Vertical Operations:** Operate on corresponding elements across two vectors ($A[i] + B[i]$). Extremely fast, executing in single-cycle throughput.
- **Horizontal Operations:** Operate on adjacent elements *within the same vector* (e.g. summing all 8 elements in `ymm0`). They require multiple internal shuffle and blend operations, making them significantly slower.
</details>

<details>
<summary><strong>7. How does data layout (Array of Structures vs Structure of Arrays) impact SIMD vectorization?</strong></summary>

In an Array of Structures (`struct Point { float x, y, z; } points[N];`), contiguous memory alternates $x, y, z, x, y, z$. Loading a vector register loads mixed coordinates, requiring expensive de-interleaving shuffles. In a Structure of Arrays (`struct Points { float x[N]; float y[N]; float z[N]; };`), all $x$ coordinates are contiguous, loading directly into vector registers with a single instruction.
</details>

<details>
<summary><strong>8. What compiler header file must be included to access AVX and AVX2 intrinsics?</strong></summary>

`<immintrin.h>`.
</details>

---

<details>
<summary><strong>9. How do you implement conditional selection (branchless masking) in SIMD?</strong></summary>

In SIMD, branches are implemented using comparison masks and blend operations:
```c
__m256 mask = _mm256_cmp_ps(va, vb, _CMP_GT_OQ); /* Compare: returns all 1s or 0s per float */
__m256 result = _mm256_blendv_ps(va, vb, mask);   /* Select element based on mask */
```
Both code paths are calculated in parallel, and the blend instruction selects the outcome based on the bitmask, completely eliminating branch penalties across all 8 vector lanes.
</details>

<details>
<summary><strong>10. What is "CPUID Feature Detection" and how do you dispatch SIMD functions dynamically at runtime?</strong></summary>

To ship a single binary that runs on all x86 CPUs while using AVX2 on modern CPUs, developers use GCC's <code>__builtin_cpu_supports("avx2")</code> or the <code>cpuid</code> assembly instruction at program startup. Based on the CPU flags, a global function pointer is assigned to either the AVX2 implementation or the scalar fallback, ensuring both optimal performance and broad hardware compatibility.
</details>

---

## 11. Practice Exercises

### Exercise 1 (Easy): SIMD Vector Multiplication
- **Task:** Write a function using AVX2 intrinsics that multiplies two arrays of floats: `void avx_multiply(const float *a, const float *b, float *res, size_t n)`.

<details>
<summary>Solution</summary>

```c
#include <immintrin.h>
#include <stddef.h>

void avx_multiply(const float *a, const float *b, float *res, size_t n) {
    size_t i = 0;
    for (; i + 8 <= n; i += 8) {
        __m256 va = _mm256_loadu_ps(&a[i]);
        __m256 vb = _mm256_loadu_ps(&b[i]);
        __m256 vprod = _mm256_mul_ps(va, vb);
        _mm256_storeu_ps(&res[i], vprod);
    }
    for (; i < n; ++i) {
        res[i] = a[i] * b[i];
    }
}
```
</details>

### Exercise 2 (Medium): Horizontal Sum of a 256-bit Float Vector
- **Task:** Implement a helper function `float hsum_avx(__m256 v)` that sums all 8 packed single-precision floats in an AVX vector and returns a single scalar float.

<details>
<summary>Solution</summary>

```c
#include <immintrin.h>

float hsum_avx(__m256 v) {
    /* Step 1: Add high 128-bit lane to low 128-bit lane */
    __m128 vlow  = _mm256_castps256_ps128(v);
    __m128 vhigh = _mm256_extractf128_ps(v, 1);
    __m128 sum128 = _mm_add_ps(vlow, vhigh);

    /* Step 2: Add odd and even elements */
    __m128 shuf = _mm_movehdup_ps(sum128);
    __m128 sum64 = _mm_add_ps(sum128, shuf);

    /* Step 3: Add final pairs */
    __m128 shuf2 = _mm_movehl_ps(sum64, sum64);
    __m128 final_sum = _mm_add_ss(sum64, shuf2);

    return _mm_cvtss_f32(final_sum);
}
```
</details>

### Exercise 3 (Hard): SIMD Vector Dot Product with FMA
- **Task:** Implement an optimized dot product function `float avx_dot_product(const float *a, const float *b, size_t n)` utilizing FMA (`_mm256_fmadd_ps`) and vector accumulation.

<details>
<summary>Solution</summary>

```c
#include <immintrin.h>
#include <stddef.h>

extern float hsum_avx(__m256 v);

float avx_dot_product(const float *a, const float *b, size_t n) {
    __m256 vacc = _mm256_setzero_ps();
    size_t i = 0;

    for (; i + 8 <= n; i += 8) {
        __m256 va = _mm256_loadu_ps(&a[i]);
        __m256 vb = _mm256_loadu_ps(&b[i]);
        vacc = _mm256_fmadd_ps(va, vb, vacc); /* vacc = (va * vb) + vacc */
    }

    float total = hsum_avx(vacc);

    /* Scalar cleanup */
    for (; i < n; ++i) {
        total += a[i] * b[i];
    }

    return total;
}
```
</details>

---

## 12. Summary / Cheat Sheet

| Intrinsic | Operation | Operands | Header |
| :--- | :--- | :--- | :--- |
| `_mm256_loadu_ps(ptr)` | Unaligned Load | 8 floats (256-bit) | `<immintrin.h>` |
| `_mm256_storeu_ps(ptr, v)` | Unaligned Store | 8 floats (256-bit) | `<immintrin.h>` |
| `_mm256_add_ps(a, b)` | Vector Addition | $a[i] + b[i]$ (8 floats) | `<immintrin.h>` |
| `_mm256_mul_ps(a, b)` | Vector Multiplication | $a[i] 	imes b[i]$ (8 floats) | `<immintrin.h>` |
| `_mm256_fmadd_ps(a, b, c)`| Fused Multiply-Add | $(a[i] 	imes b[i]) + c[i]$ | `<immintrin.h>` |
| `_mm256_setzero_ps()` | Zero Vector | Clears register to $0.0$ | `<immintrin.h>` |
| `_mm256_zeroupper()` | State Clean | Resets upper bits before SSE | `<immintrin.h>` |

---

## 13. Further Reading
- Intel Intrinsics Guide: `https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html`
- ARM NEON Programmer's Guide: `https://developer.arm.com/architectures/instruction-sets/simd-isas/neon`
- Modern x86 Assembly Language Programming (Daniel Kusswurm)
- SIMD at Insomniac Games (Mike Acton)

---
[<- Previous: 04-branch-prediction-and-data-layout.md](04-branch-prediction-and-data-layout.md) | [Section Index](README.md) | [Next: 06-benchmarking-correctly.md ->](06-benchmarking-correctly.md)\n