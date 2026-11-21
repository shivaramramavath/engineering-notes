# Reading Assembly and Using Compiler Explorer (Godbolt)

> A practical engineer's guide to mapping C constructs to x86-64 assembly: calling conventions (System V vs Microsoft x64), stack frames, compiler optimizations, vectorization, and Compiler Explorer (godbolt.org) workflows.

| Metadata | Details |
|---|---|
| **Level** | Intermediate |
| **Prerequisites** | [01-toolchain.md](./01-toolchain.md), [03-compilation-process.md](./03-compilation-process.md), [08-compiler-flags-and-warnings.md](./08-compiler-flags-and-warnings.md) |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **Standard Baseline** | x86-64 Architecture & System V AMD64 ABI |

---

## Table of Contents
- [1. What & Why](#1-what--why)
- [2. Core Concepts](#2-core-concepts)
- [3. How It Works Internally](#3-how-it-works-internally)
- [4. Syntax & Usage](#4-syntax--usage)
- [5. Practical Examples](#5-practical-examples)
- [6. Best Practices](#6-best-practices)
- [7. Common Mistakes & Pitfalls](#7-common-mistakes--pitfalls)
- [8. Performance & Security Notes](#8-performance--security-notes)
- [9. Comparison / When to Use What](#9-comparison--when-to-use-what)
- [10. Interview Questions](#10-interview-questions)
- [11. Practice Exercises](#11-practice-exercises)
- [12. Summary / Cheat Sheet](#12-summary--cheat-sheet)
- [13. Further Reading](#13-further-reading)

---

## 1. What & Why

In C, hardware abstraction is intentionally thin. Understanding assembly is not just for compiler engineers; it is an essential skill for all systems programmers:
1. **Performance Verification**: Knowing whether the compiler successfully vectorized a loop with AVX2/AVX-512 SIMD instructions or left it scalar.
2. **Diagnosing Optimization Failures**: Verifying whether function calls were successfully inlined, tail calls were optimized, or memory allocations were elided.
3. **Understanding UB Exploitation**: Observing how the compiler deletes "impossible" code branches when undefined behavior is detected in source code.

**Compiler Explorer (godbolt.org)** is the premier interactive tool for this analysis. It compiles C code in real time across dozens of compiler versions, color-coding high-level C expressions to their exact generated assembly instructions.

---

## 2. Core Concepts

### x86-64 Architectural Registers

Modern 64-bit x86 processors provide sixteen 64-bit general-purpose registers:

```
64-bit: %rax  %rbx  %rcx  %rdx  %rsi  %rdi  %rbp  %rsp  %r8  %r9  %r10  %r11  %r12  %r13  %r14  %r15
32-bit: %eax  %ebx  %ecx  %edx  %esi  %edi  %ebp  %esp  %r8d %r9d %r10d %r11d %r12d %r13d %r14d %r15d
16-bit: %ax   %bx   %cx   %dx   %si   %di   %bp   %sp   %r8w %r9w %r10w %r11w %r12w %r13w %r14w %r15w
 8-bit: %al   %bl   %cl   %dl   %sil  %dil  %bpl  %spl  %r8b %r9b %r10b %r11b %r12b %r13b %r14b %r15b
```

### The System V AMD64 ABI (Linux / macOS)

When calling functions in standard 64-bit Unix environments:
- **First 6 integer / pointer arguments**: Passed in `%rdi`, `%rsi`, `%rdx`, `%rcx`, `%r8`, `%r9`.
- **Floating-point arguments**: Passed in SSE registers `%xmm0` through `%xmm7`.
- **Return value**: Returned in `%rax` (integer/pointer) or `%xmm0` (float/double).
- **Callee-saved registers** (preserved across calls): `%rbx`, `%rsp`, `%rbp`, `%r12`, `%r13`, `%r14`, `%r15`.
- **Caller-saved registers** (scratch registers): `%rax`, `%rcx`, `%rdx`, `%rsi`, `%rdi`, `%r8`, `%r9`, `%r10`, `%r11`.

*Note on Windows x64 ABI*: Windows uses a different convention: arguments are passed in `%rcx`, `%rdx`, `%r8`, `%r9`, with mandatory 32-byte "shadow space" allocated on the stack.

---

### SIMD Vector Registers (SSE, AVX, AVX-512)

Modern CPUs provide vector extensions that operate on multiple data points simultaneously (Single Instruction, Multiple Data):
- **SSE (128-bit)**: %xmm0 through %xmm15. Can hold four 32-bit floats or two 64-bit doubles.
- **AVX / AVX2 (256-bit)**: %ymm0 through %ymm15. Lower 128 bits alias the corresponding %xmm registers. Can hold eight 32-bit floats or four 64-bit integers.
- **AVX-512 (512-bit)**: %zmm0 through %zmm31. Can process sixteen 32-bit floating-point numbers in a single clock cycle.

When inspecting Godbolt output, instructions prefixed with  (e.g. addps, mulss, movdqa) indicate VEX/EVEX encoded vector operations.

---

## 3. How It Works Internally

### Standard Stack Frame Anatomy

When a function executes, the CPU stack grows downward (from high memory to low memory):

```
High Memory
+------------------------------------+
| Caller's Stack Frame               |
| Arguments 7+ (if > 6 arguments)    |
+------------------------------------+
| Return Address (pushed by 'call')  |  <-- %rsp at function entry
+------------------------------------+
| Saved Old %rbp (pushed by callee)  |  <-- %rbp points here (frame pointer)
+------------------------------------+
| Local Variables & Spilled Regs     |
| (sub $N, %rsp)                     |
+------------------------------------+
| Red Zone (128 bytes below %rsp)    |  <-- System V ABI optimization
+------------------------------------+
Low Memory
```

### The 128-Byte "Red Zone"

Under the System V AMD64 ABI, the 128 bytes of memory immediately below the stack pointer `%rsp` are reserved as the **Red Zone**. Leaf functions (functions that do not call any other functions) can allocate local variables in this 128-byte region without adjusting `%rsp` via `sub $N, %rsp`, saving two CPU instructions per function invocation.

### Condition Codes and Flag Registers

The x86 CPU maintains an `EFLAGS` register that records the outcome of arithmetic and logical operations:
- **ZF (Zero Flag)**: Set if result is zero (`test eax, eax` sets ZF if `eax == 0`).
- **SF (Sign Flag)**: Set if the result is negative.
- **OF (Overflow Flag)**: Set if signed arithmetic overflow occurs.
- **CF (Carry Flag)**: Set if unsigned carry/borrow occurs.

Instructions like `je` (jump if equal), `jne` (jump if not equal), `jg` (jump if greater, signed), and `ja` (jump if above, unsigned) inspect these flag bits directly.

---

## 4. Syntax & Usage

### 1. Generating Assembly Locally
```bash
# Output AT&T syntax assembly (GCC default)
gcc -std=c11 -O2 -S main.c -o main.s

# Output Intel syntax assembly
gcc -std=c11 -O2 -S -masm=intel main.c -o main_intel.s

# Disassemble compiled object file with C source interleaved
objdump -d -S --no-show-raw-insn run_app
```

### 2. Dissecting Godbolt Assembly Output
Consider this C function:
```c
int add_and_scale(int a, int b) {
    return (a + b) * 4;
}
```

Generated x86-64 Assembly (Intel syntax):
```nasm
add_and_scale:
    lea     eax, [rdi + rsi]     ; eax = a + b (in parallel)
    shl     eax, 2               ; eax = eax << 2 (multiply by 4)
    ret
```
Notice how the compiler utilized `lea` (Load Effective Address) to perform addition without modifying condition flags, and replaced multiplication with a bitwise shift `shl`!

---

## 5. Practical Examples

### Assembly Mapping Target Program

```c
/* File: 01-languages/c/00-setup/examples/09-reading-assembly-godbolt.c */
#include <stdio.h>

/* A leaf calculation function designed for assembly inspection */
static int add_and_scale(int a, int b) {
    return (a + b) * 4;
}

int main(void) {
    printf("=== Assembly Translation Target ===\n");
    int x = 3;
    int y = 5;
    int res = add_and_scale(x, y);
    printf("add_and_scale(%d, %d) = %d\n", x, y, res);
    return 0;
}
```

#### Compilation & Execution Output
```bash
gcc -std=c11 -Wall -Wextra -O2 09-reading-assembly-godbolt.c -o run_09.exe
./run_09.exe
```
Output:
```text
=== Assembly Translation Target ===
add_and_scale(3, 5) = 32
```

---

## 6. Best Practices

1. **Use Compiler Explorer (`godbolt.org`) with Useful Flags**:
   - Filter out unused labels and library directives (`Intel syntax`, `Demangle`, `Filter unused labels`).
   - Pass production optimization flags: `-std=c17 -O2 -march=haswell`.
2. **Look for `lea` Exploitation**: Compilers use `lea` (Load Effective Address) for fast arithmetic: `lea eax, [rdi + rsi*4 + 12]` performs two additions and a multiplication in a single single-cycle CPU instruction.
3. **Check for Branchless Code (`cmov`)**: Modern compilers replace `if-else` branches with conditional moves (`cmovg`, `cmovne`) to eliminate branch mispredictions on modern out-of-order execution pipelines.
4. **Inspect Vectorization (SIMD)**: When compiling numerical loops, check if the compiler generated `ymm` or `zmm` registers (AVX/AVX-512) or fell back to scalar `xmm` instructions.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It's Wrong | Conforming Fix |
|---|---|---|
| Confusing AT&T and Intel syntax operand order | AT&T syntax uses `mov src, dst` while Intel syntax uses `mov dst, src`. Misreading this inverts data-flow direction! | Stick to one syntax; Intel syntax (`-masm=intel`) matches technical CPU datasheets. |
| Forgetting ABI argument passing registers | Assuming arguments are on the stack. In 64-bit x86, the first 6 integer arguments are in registers. | Remember the sequence: `%rdi`, `%rsi`, `%rdx`, `%rcx`, `%r8`, `%r9`. |
| Assuming multiplication is always `imul` | Compilers avoid `imul` for constant values, decomposing into sequences of `lea`, `shl`, and `sub`. | Recognize arithmetic decomposition patterns. |
| Overlooking the System V Red Zone in assembly | Modifying stack below `%rsp` in signal handlers or nested calls corrupts the Red Zone. | Ensure interrupts and signal handlers respect ABI Red Zone rules. |
| Reading assembly of `-O0` builds to judge efficiency | `-O0` generates excessive stack spills and reloads that do not represent real production performance. | Always inspect assembly at `-O2` or `-O3`. |
| Assuming non-inlined functions are always slow | Forcing inlining everywhere (`__attribute__((always_inline))`) bloats binary size and ruins CPU instruction cache locality. | Trust compiler inlining heuristics unless profiling proves otherwise. |
| Ignoring 32-bit register write zero-extension | Writing to a 32-bit subregister (e.g. `mov eax, 1`) automatically clears the upper 32 bits of `%rax`. Assuming upper bits persist causes subtle bugs. | Understand that 32-bit writes clear the top 32 bits of 64-bit registers in x86-64. |

---

## 8. Performance & Security Notes

- **Speculative Execution & Spectre**: Modern CPUs speculatively execute instructions ahead of branch resolution. Compilers insert `lfence` or retpoline sequences when building kernels to prevent speculative side-channel leaks.
- **Tail Call Optimization (TCO)**: When a function returns the direct result of calling another function (`return helper(x);`), the compiler emits a `jmp` instead of a `call`, reusing the current stack frame and converting recursion into a loop with zero stack overhead.

---

## 9. Comparison / When to Use What

| Architecture ABI | First 4 Integer Registers | Stack Shadow Space | Return Register |
|---|---|---|---|
| **System V AMD64 (Linux/macOS)** | `%rdi`, `%rsi`, `%rdx`, `%rcx` | None (Has 128-byte Red Zone) | `%rax` |
| **Microsoft x64 (Windows)** | `%rcx`, `%rdx`, `%r8`, `%r9` | 32 bytes reserved above return | `%rax` |
| **ARM64 (AArch64)** | `x0`, `x1`, `x2`, `x3` | None | `x0` |

---

## 10. Interview Questions

<details>
<summary><strong>1. What is the fundamental syntactic difference between AT&T and Intel assembly syntax?</strong></summary>

Operand ordering and sigils. In Intel syntax: `instruction destination, source` (e.g. `mov eax, 1`). In AT&T syntax: `instruction source, destination` (e.g. `movl $1, %eax`). AT&T also prefixes registers with `%` and immediate values with `$`.
</details>

<details>
<summary><strong>2. What is the purpose of the `lea` instruction, and why do compilers prefer it over `add`?</strong></summary>

`lea` (Load Effective Address) computes memory addresses using CPU hardware address generation units (`[base + index * scale + disp]`). Compilers exploit this for general arithmetic: it can perform an addition, multiplication by 2/4/8, and offset addition in a single instruction **without altering CPU condition flags**.
</details>

<details>
<summary><strong>3. Which registers are used to pass the first six integer/pointer arguments in the System V AMD64 ABI?</strong></summary>

In order: `%rdi`, `%rsi`, `%rdx`, `%rcx`, `%r8`, and `%r9`. Any arguments beyond six are pushed onto the stack in reverse order.
</details>

<details>
<summary><strong>4. What is a "callee-saved" register? Name three in the x86-64 System V ABI.</strong></summary>

A callee-saved (or non-volatile) register is one whose value must be preserved by any function that modifies it. If a function uses it, the function must save it (e.g. `push %rbx`) and restore it before returning. Examples: `%rbx`, `%rbp`, `%r12`, `%r13`, `%r14`, `%r15`.
</details>

<details>
<summary><strong>5. What is the "Red Zone" in the x86-64 System V ABI?</strong></summary>

A 128-byte region below the current stack pointer `%rsp` that is guaranteed not to be modified by signal handlers or interrupts. Leaf functions can store local data in this zone without allocating stack space via `sub %rsp`.
</details>

<details>
<summary><strong>6. What is Tail Call Optimization (TCO)? What does it look like in assembly?</strong></summary>

TCO occurs when a function's final action is returning the result of calling a function. The compiler replaces the `call` and subsequent `ret` with a single `jmp` instruction, reusing the caller's stack frame and avoiding stack frame allocation.
</details>

<details>
<summary><strong>7. How does the compiler transform an integer division by a constant (e.g. `n / 3`)?</strong></summary>

Hardware integer division (`idiv`) is extremely slow (20-80 CPU cycles). Compilers replace division by a known constant with a multiplication by a "magic number" (the fixed-point reciprocal) followed by a bitwise right shift, computing the division in 1-3 cycles.
</details>

<details>
<summary><strong>8. What is the difference between `test` and `cmp` instructions in x86?</strong></summary>

`cmp a, b` performs `a - b` and sets CPU status flags without storing the result. `test a, b` performs bitwise AND (`a & b`) and sets status flags (zero flag, sign flag). `test eax, eax` is the canonical idiom for checking if a value is zero.
</details>

<details>
<summary><strong>9. What happens to the upper 32 bits of a 64-bit register when a 32-bit operation executes in x86-64?</strong></summary>

In x86-64 architecture, any operation that writes to a 32-bit subregister (such as `mov eax, 5` or `add edx, 1`) **automatically zero-extends** the result into the entire 64-bit register (`%rax` or `%rdx`), setting the upper 32 bits to zero.
</details>

<details>
<summary><strong>10. What is a "leaf function", and how does the compiler optimize its stack frame?</strong></summary>

A leaf function is a function that does not call any other functions. Because it makes no nested calls, the compiler can omit saving `%rbp` (frame pointer elimination), omit adjusting `%rsp` by using the 128-byte Red Zone, and avoid spilling scratch registers.
</details>

---

<details>
<summary><strong>11. How does the compiler implement a switch statement in assembly?</strong></summary>

The compiler chooses between three strategies based on case density and quantity:
1. **Linear search (cmp/je sequence)**: Used for small numbers of cases (e.g. 1-3 cases).
2. **Binary search tree of branches**: Used for sparse, non-sequential case values.
3. **Jump Table (jmp [table + index*8])**: Used when case values are dense or sequential. A contiguous array of code addresses is placed in the .rodata section; the switch variable is bounds-checked and used as an index for an indirect branch in (1)$ constant time.
</details>

<details>
<summary><strong>12. What are the calling convention rules for floating-point and SIMD vector arguments?</strong></summary>

Under the System V AMD64 ABI, the first eight floating-point or vector arguments (float, double, __m128, __m256) are passed in the SSE/AVX registers %xmm0 through %xmm7. Subsequent vector/float arguments are pushed onto the stack. Return values of float/double type are returned in %xmm0 (or %xmm0 and %xmm1 for complex numbers).
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Identifying the Return Register</strong></summary>

**Goal**: Look at this disassembled function and determine what value it returns and how:

```nasm
mystery:
    mov     eax, edi
    imul    eax, esi
    ret
```

**Solution**:
It accepts two integer arguments (passed in `%edi` and `%esi`), multiplies them together, and stores the result in `%eax`. Because `%eax` is the return register for 32-bit integers, it returns `a * b`.
</details>

<details>
<summary><strong>Exercise 2 (Medium): Reading a Loop in Assembly</strong></summary>

**Goal**: Reverse-engineer this assembly loop back into C:

```nasm
sum_array:
    xor     eax, eax
    test    esi, esi
    jle     .done
    xor     edx, edx
.loop:
    add     eax, DWORD PTR [rdi+rdx*4]
    inc     rdx
    cmp     esi, edx
    jne     .loop
.done:
    ret
```

**Solution**:
It sums an array of `n` integers:
```c
int sum_array(const int *arr, int n) {
    int sum = 0;
    for (int i = 0; i < n; ++i) {
        sum += arr[i];
    }
    return sum;
}
```
</details>

<details>
<summary><strong>Exercise 3 (Hard): Recognizing Auto-Vectorization in Godbolt</strong></summary>

**Goal**: Compile an array addition function with `gcc -O3 -mavx2` on godbolt.org, and identify the vector instructions.

**Solution**:
```c
void add_arrays(float *restrict a, const float *restrict b, int n) {
    for (int i = 0; i < n; ++i) {
        a[i] += b[i];
    }
}
```
In the assembly output, look for `vmovups` (vector move unaligned packed single-precision) and `vaddps ymm0, ymm0, YMMWORD PTR [...]`, demonstrating that the loop is processing 8 floats simultaneously per AVX2 instruction.
</details>

---

## 12. Summary / Cheat Sheet

- **System V AMD64 Calling Convention**: `%rdi`, `%rsi`, `%rdx`, `%rcx`, `%r8`, `%r9`.
- **Return Value**: `%rax` (integers/pointers), `%xmm0` (floats).
- **`lea`**: Arithmetic powerhouse for pointer and integer arithmetic without modifying flags.
- **`test reg, reg`**: Standard idiom for checking if a variable is zero (`NULL`).
- **TCO**: Function calls transformed into `jmp` jumps.
- **Godbolt**: Always analyze with `-O2` or `-O3` to see real optimizer output.

---

## 13. Further Reading

- Bryant & O'Hallaron: *Computer Systems: A Programmer's Perspective* (Chapter 3: Machine-Level Representation of Programs).
- Michael Matz et al.: *System V Application Binary Interface AMD64 Architecture Processor Supplement*.
- Compiler Explorer (godbolt.org) documentation and guides.

---

[← Previous: 08-compiler-flags-and-warnings.md](./08-compiler-flags-and-warnings.md) | [Section Index](./README.md) | [Next: 10-c-project-layout.md](./10-c-project-layout.md) →
