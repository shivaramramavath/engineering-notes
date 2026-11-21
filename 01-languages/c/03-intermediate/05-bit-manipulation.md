# 05 Bit Manipulation

> Master low-level binary manipulation, bitwise operators, masking idioms, bit-twiddling hacks, and modern C23 `<stdbit.h>` utilities for high-performance systems engineering.

| Metadata | Details |
| :--- | :--- |
| **Level** | Intermediate |
| **Prerequisites** | [01-fundamentals/03-integer-representation.md](../01-fundamentals/03-integer-representation.md), [01-fundamentals/05-operators.md](../01-fundamentals/05-operators.md) |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **Standard** | C11 / C17 / C23 |

---

## Table of Contents
1. [1. What & Why](#1-what--why)
2. [2. Core Concepts](#2-core-concepts)
3. [3. How It Works Internally](#3-how-it-works-internally)
4. [4. Syntax & Usage](#4-syntax--usage)
5. [5. Practical Examples](#5-practical-examples)
6. [6. Best Practices](#6-best-practices)
7. [7. Common Mistakes & Pitfalls](#7-common-mistakes--pitfalls)
8. [8. Performance & Security Notes](#8-performance--security-notes)
9. [9. Comparison / When to Use What](#9-comparison--when-to-use-what)
10. [10. Interview Questions](#10-interview-questions)
11. [11. Practice Exercises](#11-practice-exercises)
12. [12. Summary / Cheat Sheet](#12-summary--cheat-sheet)
13. [13. Further Reading](#13-further-reading)

---

## 1. What & Why

Bit manipulation is the practice of inspecting, modifying, and transforming individual binary digits (bits) directly within hardware registers and memory cells. In modern computing, byte-addressable architecture groups bits into 8-bit octets, yet countless systems engineering problems require granular single-bit control:
- **Hardware Interaction**: Memory-mapped I/O (MMIO) registers represent peripheral control lines (GPIO pins, UART status flags, interrupt enable masks) as discrete bit fields.
- **Network Protocols**: IP, TCP, and UDP headers pack boolean flags (`SYN`, `ACK`, `FIN`) and fractional integer values (Quality of Service, Header Length) into single 16-bit or 32-bit words to minimize bandwidth.
- **Data Compression & Cryptography**: Huffman encoding, cryptographic hash functions (SHA-256, AES), and pseudo-random generators rely on bitwise rotations, XOR blending, and bit permutations.
- **Space-Efficient Algorithms**: Bitmaps and Bloom filters pack millions of boolean presence states into minimal memory, reducing cache misses and footprint.

C provides direct access to CPU arithmetic logic unit (ALU) bit instructions through dedicated bitwise operators, allowing developers to write high-throughput code that maps directly to single-cycle machine instructions.

---

## 2. Core Concepts

### 2.1 The Six Bitwise Operators
C defines six operators that act bit-by-bit on integer operands:
1. `&` (Bitwise AND): Yields 1 if and only if both corresponding bits are 1.
2. `|` (Bitwise OR): Yields 1 if either corresponding bit is 1.
3. `^` (Bitwise XOR): Yields 1 if the corresponding bits are distinct (different).
4. `~` (Bitwise NOT / Complement): Inverts all bits (unary operator).
5. `<<` (Left Shift): Shifts bits toward more significant positions, shifting in zeros on the right.
6. `>>` (Right Shift): Shifts bits toward less significant positions.

### 2.2 Unsigned vs. Signed Integers
Bitwise manipulation should virtually **always** be performed on unsigned integer types (`uint8_t`, `uint16_t`, `uint32_t`, `uint64_t`).
- **Right Shift of Signed Types**: Right-shifting a negative signed integer produces **implementation-defined** behavior (typically arithmetic shift, copying the sign bit, but not guaranteed by ISO C).
- **Left Shift of Signed Types**: In C11/C17, left-shifting a signed integer where the result overflows `INT_MAX` invokes **Undefined Behavior (UB)**. C23 formalizes two's complement representation, but unsigned arithmetic remains the only branch free of overflow UB.
- **Integer Promotion**: Operands smaller than `int` (such as `uint8_t` or `uint16_t`) are automatically promoted to `signed int` before applying bitwise operators. Always use unsigned literals (e.g., `1u << 31`) to prevent accidental sign overflow.

### 2.3 The Four Fundamental Mask Idioms
- **Set Bit $N$**: `val |= (1u << n);`
- **Clear Bit $N$**: `val &= ~(1u << n);`
- **Toggle Bit $N$**: `val ^= (1u << n);`
- **Test Bit $N$**: `bool is_set = (val & (1u << n)) != 0;`

---

## 3. How It Works Internally

Bitwise operations translate directly to hardware ALU instructions. Unlike division, multiplication, or function calls, bitwise operations execute in a single clock cycle on almost all modern CPU microarchitectures (x86 `AND`, `OR`, `XOR`, `NOT`, `SHL`, `SHR`, `SAR`).

```
Bitwise Operations ALU Mapping:
┌─────────────────┬──────────────┬──────────────────┐
│ C Operation     │ x86 Assembly │ ARM Assembly     │
├─────────────────┼──────────────┼──────────────────┤
│ a & b           │ and eax, edx │ AND w0, w0, w1   │
│ a | b           │ or  eax, edx │ ORR w0, w0, w1   │
│ a ^ b           │ xor eax, edx │ EOR w0, w0, w1   │
│ ~a              │ not eax      │ MVN w0, w0       │
│ a << n          │ shl eax, cl  │ LSL w0, w0, w1   │
│ (unsigned) >> n │ shr eax, cl  │ LSR w0, w0, w1   │
│ (signed) >> n   │ sar eax, cl  │ ASR w0, w0, w1   │
└─────────────────┴──────────────┴──────────────────┘
```

### Memory Layout & Shift Overflow Mechanics
When shifting an $N$-bit value by $S$ positions:
- If $S < 0$ or $S \ge N$, ISO C states the behavior is **undefined**. In x86 hardware, the CPU masks shift counts to 5 bits (`shift_count & 31`), meaning `val << 32` effectively executes `val << 0`, leaving the operand unchanged rather than clearing it to 0.

```
Logical Shift vs Arithmetic Shift:
Logical Right Shift (Unsigned):
Input:    1101 0010  (0xD2)
>> 1   :  0110 1001  (0x69) -> Zero shifted in at MSB

Arithmetic Right Shift (Signed Negative):
Input:    1101 0010  (-46 in 8-bit two's complement)
>> 1   :  1110 1001  (-23)  -> Sign bit duplicated at MSB
```

---

## 4. Syntax & Usage

### 4.1 Mask Creation and Field Extraction
```c
#include <stdint.h>

#define FIELD_MASK  0x0Fu
#define FIELD_SHIFT 4u

/* Extract 4-bit field at bits [7:4] */
uint32_t extract_field(uint32_t reg) {
    return (reg >> FIELD_SHIFT) & FIELD_MASK;
}

/* Pack 4-bit field into bits [7:4] */
uint32_t pack_field(uint32_t reg, uint32_t val) {
    reg &= ~(FIELD_MASK << FIELD_SHIFT);      /* Clear existing bits */
    reg |= ((val & FIELD_MASK) << FIELD_SHIFT); /* Inject new value */
    return reg;
}
```

### 4.2 C23 Standard Bit Utilities (`<stdbit.h>`)
ISO C23 introduced standardized, type-generic bit operations that map directly to hardware instructions (such as `POPCNT`, `CLZ`, `CTZ`, `BSR`):
- `stdc_count_ones(x)`: Number of set bits (popcount).
- `stdc_leading_zeros(x)`: Number of consecutive zero bits starting from the MSB.
- `stdc_trailing_zeros(x)`: Number of consecutive zero bits starting from the LSB.
- `stdc_has_single_bit(x)`: Returns true if `x` is an exact power of two.
- `stdc_bit_ceil(x)` / `stdc_bit_floor(x)`: Next / previous power of two.

---

## 5. Practical Examples

### Comprehensive Example: Bit Flags, Twiddling Hacks, and Popcount
The following complete program demonstrates state flags manipulation, power-of-two checks, lowest-set-bit extraction, and Brian Kernighan's population count algorithm.

```c
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

/* Basic operations: Set, Clear, Toggle, Test */
static inline uint32_t bit_set(uint32_t val, unsigned int bit) {
    return val | (1u << bit);
}

static inline uint32_t bit_clear(uint32_t val, unsigned int bit) {
    return val & ~(1u << bit);
}

static inline uint32_t bit_toggle(uint32_t val, unsigned int bit) {
    return val ^ (1u << bit);
}

static inline bool bit_test(uint32_t val, unsigned int bit) {
    return (val & (1u << bit)) != 0;
}

/* Bit twiddling hacks */
static inline bool is_power_of_two(uint32_t val) {
    return (val != 0) && ((val & (val - 1)) == 0);
}

static inline uint32_t isolate_lowest_set_bit(uint32_t val) {
    /* 2's complement negation -x is (~x + 1) */
    return val & (~val + 1u);
}

static inline unsigned int count_set_bits_kernighan(uint32_t val) {
    unsigned int count = 0;
    while (val != 0) {
        val &= (val - 1); /* Clears the lowest set bit */
        count++;
    }
    return count;
}

int main(void) {
    printf("=== Bit Manipulation Demonstrations ===\n");
    uint32_t flags = 0x00000000;

    /* Bit manipulation operations */
    flags = bit_set(flags, 3);
    flags = bit_set(flags, 7);
    printf("Flags after setting bits 3 and 7: 0x%08X (bit 3: %s, bit 4: %s)\n",
           (unsigned int)flags,
           bit_test(flags, 3) ? "set" : "clear",
           bit_test(flags, 4) ? "set" : "clear");

    flags = bit_toggle(flags, 3);
    printf("Flags after toggling bit 3: 0x%08X (bit 3: %s)\n",
           (unsigned int)flags,
           bit_test(flags, 3) ? "set" : "clear");

    flags = bit_clear(flags, 7);
    printf("Flags after clearing bit 7: 0x%08X\n", (unsigned int)flags);

    /* Bit hacks */
    uint32_t nums[] = {0, 1, 2, 14, 16, 40};
    size_t n_nums = sizeof(nums) / sizeof(nums[0]);
    for (size_t i = 0; i < n_nums; i++) {
        uint32_t x = nums[i];
        printf("x=%2u | power_of_two: %-5s | lowest_bit: 0x%02X | popcount: %u\n",
               (unsigned int)x,
               is_power_of_two(x) ? "true" : "false",
               (unsigned int)isolate_lowest_set_bit(x),
               count_set_bits_kernighan(x));
    }

    return 0;
}
```

#### Compilation & Verified Execution Output
```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -Werror 05-bit-manipulation.c -o 05-bit-manipulation.exe
./05-bit-manipulation.exe
```

```text
=== Bit Manipulation Demonstrations ===
Flags after setting bits 3 and 7: 0x00000088 (bit 3: set, bit 4: clear)
Flags after toggling bit 3: 0x00000080 (bit 3: clear)
Flags after clearing bit 7: 0x00000000
x= 0 | power_of_two: false | lowest_bit: 0x00 | popcount: 0
x= 1 | power_of_two: true  | lowest_bit: 0x01 | popcount: 1
x= 2 | power_of_two: true  | lowest_bit: 0x02 | popcount: 1
x=14 | power_of_two: false | lowest_bit: 0x02 | popcount: 3
x=16 | power_of_two: true  | lowest_bit: 0x10 | popcount: 1
x=40 | power_of_two: false | lowest_bit: 0x08 | popcount: 2
```

---

## 6. Best Practices

1. **Always Use Unsigned Types for Bitfields**: Prefer `uint32_t`, `uint64_t`, or `size_t`. Signed integers introduce implementation-defined right shifts and undefined behavior on signed overflow.
2. **Always Suffix Literal Masks with `u` or `ULL`**: Writing `1 << 31` creates a signed `int` literal shifted into the sign bit (undefined behavior). Always write `1u << 31` or `1ULL << 63`.
3. **Parenthesize Bitwise Expressions in Macros**: Operators `&`, `|`, and `^` have lower precedence than relational operators (`==`, `!=`, `<`, `>`). Always wrap operands and macro bodies: `#define TEST(x, b) (((x) & (1u << (b))) != 0)`.
4. **Guard Shift Quantities**: Ensure the shift count is strictly less than the width of the promoted integer type (`shift < 32` for `uint32_t`).
5. **Prefer Compiler Builtins or `<stdbit.h>` for Popcount/CLZ**: Modern processors provide hardware instructions (`POPCNT`, `LZCNT`). Use `__builtin_popcount()` (GCC/Clang), `__popcnt` (MSVC), or C23 `stdc_count_ones()` rather than manual software loops.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It's Wrong | The Fix |
| :--- | :--- | :--- |
| `if (flags & FLAG_A == FLAG_A)` | `==` has higher precedence than `&`. Evaluates as `flags & (FLAG_A == FLAG_A)`. | Add explicit parentheses: `if ((flags & FLAG_A) == FLAG_A)`. |
| `1 << 31` | `1` is a signed 32-bit `int`. Shifting into bit 31 triggers signed integer overflow (UB). | Use unsigned literal: `1u << 31`. |
| `uint32_t x = 1; x <<= 32;` | Shifting by a count $\ge$ width of type is Undefined Behavior in ISO C. | Ensure shift amount is between `0` and `31`. |
| `int32_t x = -8; x >>= 1;` expecting `0x7FFFFFFC` | Right shift on signed negative integer is implementation-defined (arithmetic shift preserves sign). | Cast to `uint32_t` before shifting for logical shift. |
| Using `~` on promoted types: `uint8_t b = ~0x01;` | `0x01` is promoted to `int` (32-bit). `~0x01` is `0xFFFFFFFE`. When compared to `uint8_t`, it can lead to subtle logic errors. | Mask explicitly: `uint8_t b = (uint8_t)~0x01u;`. |
| Modifying variable multiple times in unsequenced bit operations: `x ^= y; y ^= x; x ^= y;` as single-line macro | If variables point to the same memory location, self-XOR zeroes the value (`x ^= x` results in 0). | Use a temporary variable for swapping values safely. |

---

## 8. Performance & Security Notes

- **Zero-Branch Algorithms**: Bit manipulation eliminates conditional branches (`if`/`else`), preventing branch misprediction penalties in tight execution loops. For example, branchless conditional select: `result = (a & mask) | (b & ~mask);`.
- **Constant-Time Cryptography**: In cryptographic implementations (AES, ChaCha20, RSA), conditional branches based on secret keys introduce timing side-channel attacks. Bitwise logic executes in strictly constant time across all CPU operands.
- **Endianness Independence**: Bitwise shifts operate purely on **numeric values**, not memory byte representations. `0x1234u << 8` results in `0x123400u` on both Little-Endian (x86, ARM) and Big-Endian (SPARC, PowerPC) architectures. Memory byte-swapping (`htons`, `ntohl`) is only necessary when serializing to byte buffers.

---

## 9. Comparison / When to Use What

| Technique | Readability | Portability | Performance | Best Use Case |
| :--- | :--- | :--- | :--- | :--- |
| **Bitwise Operators (`&`, `|`, `^`)** | Moderate | High (Universal ISO C) | Optimal (Single CPU cycle) | Hardware registers, protocol flags, high-speed masks |
| **Bit-Fields (`unsigned int f : 3;`)** | High | Low (Compiler layout unspecified) | Moderate (Masking emitted by compiler) | Internal debug structures, non-serialized memory |
| **Compiler Builtins (`__builtin_popcount`)** | High | Low (GCC/Clang specific) | Optimal (Single assembly instruction) | High-performance indexing, chess engines |
| **C23 `<stdbit.h>`** | High | High (Standard C23 compliant) | Optimal (Hardware mapped) | Modern C23 codebases needing portable bit counting |
| **Boolean Arrays (`bool flags[32]`)** | Very High | High | Low (32 bytes vs 4 bytes, poor cache locality) | Non-performance-critical logic |

---

## 10. Interview Questions

<details>
<summary><strong>1. Why is shifting a 32-bit integer by 32 positions undefined behavior in C?</strong></summary>

According to the ISO C standard (6.5.7), if the value of the right operand is negative or is greater than or equal to the width of the promoted left operand, the behavior is undefined. At the hardware level, x86 instructions `SHL` and `SHR` mask the count operand to 5 bits (`count & 0x1F`). Therefore, a shift by 32 becomes a shift by 0 on x86, whereas on ARM or other architectures it may clear the register to 0. To avoid compiler-to-compiler discrepancies, the standard categorizes this as undefined behavior.
</details>

<details>
<summary><strong>2. How does `x & (x - 1)` clear the lowest set bit?</strong></summary>

Subtracting 1 from an integer borrows from the least significant set bit. This turns the lowest set bit from 1 to 0, and flips all lower trailing zero bits to 1 (e.g., `0b10100 - 1 = 0b10011`). When this result is bitwise ANDed with the original number `x`, the bits above the lowest set bit remain identical, while the lowest set bit and all bits below it evaluate to 0 (`0b10100 & 0b10011 = 0b10000`).
</details>

<details>
<summary><strong>3. Are bitwise shift operations endian-dependent?</strong></summary>

No. Bitwise operators in C operate on mathematical integer values, not their physical storage in RAM. Shifting `1u << 8` multiplies the integer value by 256 regardless of whether the target architecture stores the least significant byte first (Little-Endian) or most significant byte first (Big-Endian). Endianness only affects how integers are ordered in memory when cast to byte pointers (`uint8_t *`) or transmitted across network interfaces.
</details>

<details>
<summary><strong>4. Why should you avoid bitwise operations on signed integers?</strong></summary>

Signed integer representation historically allowed one's complement, sign-magnitude, or two's complement. Shifting signed integers can invoke implementation-defined behavior (right-shifting negative values) or undefined behavior (left-shifting that overflows `INT_MAX`). Furthermore, integer promotions automatically convert smaller unsigned types like `uint8_t` to `signed int`, which can introduce unexpected sign bits upon bitwise inversion (`~`).
</details>

<details>
<summary><strong>5. Explain how to swap two integers without a temporary variable using XOR, and why it can be dangerous.</strong></summary>

```c
x ^= y;
y ^= x;
x ^= y;
```
Each bitwise XOR toggles bits that differ. However, if `x` and `y` reside at the same memory address (e.g., in a sorting algorithm where an element is swapped with itself: `swap(&arr[i], &arr[j])` where `i == j`), the first statement `*x ^= *x` zeroes the memory cell, destroying the value permanently. In modern compilers, using a standard temporary variable compiles to identical register moves without aliasing hazards.
</details>

<details>
<summary><strong>6. What is the difference between logical and arithmetic right shift?</strong></summary>

A logical right shift shifts in zeroes at the most significant bit (MSB), which is standard for unsigned integers. An arithmetic right shift replicates the sign bit (MSB) into the newly vacant positions, preserving the negative sign of two's complement signed values. In C, right-shifting an unsigned type is guaranteed to be a logical shift; right-shifting a signed negative type is implementation-defined.
</details>

<details>
<summary><strong>7. How do you round up an unsigned integer to the next power of two using bit manipulation?</strong></summary>

By progressively propagating set bits downward:
```c
uint32_t round_up_power_of_two(uint32_t v) {
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v++;
    return v;
}
```
This smears the highest set bit across all lower bit positions, resulting in a continuous run of 1s (`0b00011111`), which when incremented by 1 rolls over to `0b00100000`.
</details>

<details>
<summary><strong>8. What is the advantage of C23 `<stdbit.h>` over historical idioms?</strong></summary>

Historical software loops (like Kernighan's popcount) require variable CPU cycles dependent on the number of set bits. C23 `<stdbit.h>` introduces standardized, type-generic functions (`stdc_count_ones`, `stdc_leading_zeros`, `stdc_first_leading_one`) that allow compilers to directly emit single-cycle hardware instructions (`POPCNT`, `LZCNT`, `CLZ`) portably across x86, ARM, RISC-V, and MIPS without relying on compiler-specific builtins.
</details>

---

## 11. Practice Exercises

### Easy: Check if an Integer is Even or Odd
Write a function `bool is_odd(uint32_t n)` using bitwise operations instead of modulo `%`.

<details>
<summary>Solution</summary>

```c
#include <stdint.h>
#include <stdbool.h>

bool is_odd(uint32_t n) {
    return (n & 1u) != 0;
}
```
</details>

### Medium: Reverse the Bits of a 32-bit Unsigned Integer
Write a function `uint32_t reverse_bits(uint32_t n)` that reverses the binary sequence (bit 0 becomes bit 31, bit 1 becomes bit 30, etc.) using divide-and-conquer bit swaps.

<details>
<summary>Solution</summary>

```c
#include <stdint.h>

uint32_t reverse_bits(uint32_t n) {
    n = ((n >> 1) & 0x55555555u) | ((n & 0x55555555u) << 1);
    n = ((n >> 2) & 0x33333333u) | ((n & 0x33333333u) << 2);
    n = ((n >> 4) & 0x0F0F0F0Fu) | ((n & 0x0F0F0F0Fu) << 4);
    n = ((n >> 8) & 0x00FF00FFu) | ((n & 0x00FF00FFu) << 8);
    n = (n >> 16) | (n << 16);
    return n;
}
```
</details>

### Hard: Implement a 1024-bit Fixed Size Bitset
Implement a memory-efficient bitset supporting `set_bit`, `clear_bit`, and `test_bit` operations across 1024 discrete boolean states using an array of `uint32_t`.

<details>
<summary>Solution</summary>

```c
#include <stdint.h>
#include <stdbool.h>

#define BITSET_BITS 1024
#define WORDS (BITSET_BITS / 32)

typedef struct {
    uint32_t words[WORDS];
} Bitset1024;

void bitset_init(Bitset1024 *bs) {
    for (int i = 0; i < WORDS; i++) bs->words[i] = 0;
}

void bitset_set(Bitset1024 *bs, unsigned int bit) {
    if (bit < BITSET_BITS) {
        bs->words[bit / 32] |= (1u << (bit % 32));
    }
}

void bitset_clear(Bitset1024 *bs, unsigned int bit) {
    if (bit < BITSET_BITS) {
        bs->words[bit / 32] &= ~(1u << (bit % 32));
    }
}

bool bitset_test(const Bitset1024 *bs, unsigned int bit) {
    if (bit >= BITSET_BITS) return false;
    return (bs->words[bit / 32] & (1u << (bit % 32))) != 0;
}
```
</details>

---

## 12. Summary / Cheat Sheet

| Goal | Bitwise Expression | Note |
| :--- | :--- | :--- |
| Set bit $n$ | `x |= (1u << n)` | Turns bit on |
| Clear bit $n$ | `x &= ~(1u << n)` | Turns bit off |
| Toggle bit $n$ | `x ^= (1u << n)` | Inverts bit state |
| Test bit $n$ | `(x & (1u << n)) != 0` | Returns boolean |
| Clear lowest set bit | `x & (x - 1)` | Yields 0 if $x$ was power of two |
| Isolate lowest set bit | `x & (-x)` | Mask containing only lowest 1 |
| Divide by $2^k$ | `x >> k` | Fast integer division (unsigned) |
| Multiply by $2^k$ | `x << k` | Fast integer multiplication |
| Modulo $2^k$ | `x & ((1u << k) - 1)` | Valid only for powers of 2 |

---

## 13. Further Reading

- ISO/IEC 9899:2018 (C17) §6.5.7 (Bitwise shift operators), §6.5.10–§6.5.12 (Bitwise logic).
- ISO/IEC 9899:2024 (C23) §7.18 (`<stdbit.h>` Bit and byte utilities).
- Henry S. Warren, Jr., *Hacker's Delight*, 2nd Edition (Addison-Wesley) — The definitive reference on bit-twiddling algorithms.
- SEI CERT C Coding Standard: INT13-C (Use bitwise operators only on unsigned operands).
- SEI CERT C Coding Standard: INT34-C (Do not shift an expression by a negative number of bits or greater than or equal to the width of the promoted expression).

---

[← Previous](04-assertions-defensive-coding.md) | [Section Index](README.md) | [Next →](06-variadic-functions.md)
