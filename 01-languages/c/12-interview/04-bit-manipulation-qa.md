# Bit Manipulation Technical Interview Guide (`04-bit-manipulation-qa`)

> Master essential low-level bit twiddling algorithms: masking, bitwise tricks, endianness detection, population count, and branchless operations.

| Metadata | Details |
| :--- | :--- |
| **Guide** | `04-bit-manipulation-qa` |
| **Level** | Advanced |
| **Standard Baseline** | C11 / C17 / C23 |

---

## 1. Fundamental Bit Tricks

<details>
<summary><strong>Q1: How do you determine if an unsigned integer is a power of two in $O(1)$?</strong></summary>

```c
bool is_power_of_two(uint32_t x) {
    return (x != 0) && ((x & (x - 1)) == 0);
}
```
**Why:** A power of two has exactly one set bit (e.g., `0b00010000`). Subtracting 1 flips that bit and sets all lower bits (`0b00001111`). Bitwise AND between them yields exactly zero.
</details>

<details>
<summary><strong>Q2: How do you isolate the lowest set bit of an integer?</strong></summary>

```c
uint32_t lowest_set_bit(uint32_t x) {
    return x & (-x);
}
```
**Why:** Under two's complement arithmetic, `-x = (~x) + 1`. Inverting `x` and adding 1 preserves the lowest set bit while inverting all higher bits. ANDing `x` with `-x` leaves only the lowest set bit.
</details>

<details>
<summary><strong>Q3: How do you swap two integers without a temporary variable, and why is it discouraged?</strong></summary>

```c
void xor_swap(int *a, int *b) {
    if (a != b) { /* Mandatory guard against aliasing! */
        *a ^= *b;
        *b ^= *a;
        *a ^= *b;
    }
}
```
**Why discouraged:**
1. If `a == b` (same memory address), `*a ^= *a` zeroes the memory!
2. Modern superscalar CPUs execute standard temporary register swaps faster due to register renaming, whereas sequential XOR operations introduce serial data dependencies.
</details>

---

## 2. Advanced Bit Algorithms

<details>
<summary><strong>Q4: How does Brian Kernighan's algorithm count set bits (Popcount)?</strong></summary>

```c
int count_set_bits(uint32_t n) {
    int count = 0;
    while (n > 0) {
        n &= (n - 1); /* Clears the lowest set bit */
        count++;
    }
    return count;
}
```
Runs in $O(K)$ time where $K$ is the number of set bits, rather than 32 loop iterations.
In C23, use standard `<stdbit.h>` `stdc_count_ones(n)` (which compiles to single CPU instruction `popcnt`).
</details>

<details>
<summary><strong>Q5: How do you detect system endianness at runtime in pure C?</strong></summary>

```c
bool is_little_endian(void) {
    uint16_t test = 0x0001;
    uint8_t *byte = (uint8_t*)&test;
    return (*byte == 0x01);
}
```
If the least significant byte (`0x01`) is stored at the lowest memory address, the machine is Little-Endian (x86, ARM). If `0x00` is at the lowest address, it is Big-Endian.
</details>

<details>
<summary><strong>Q6: How do you compute the branchless absolute value of a signed 32-bit integer?</strong></summary>

```c
int32_t branchless_abs(int32_t x) {
    int32_t mask = x >> 31; /* Arithmetic shift: 0 if positive, -1 (all 1s) if negative */
    return (x + mask) ^ mask;
}
```
Eliminates branch prediction misses on random numerical datasets.
</details>

---

[← Output Puzzles](./03-output-based-questions.md) | [Interview Index](./README.md) | [Next: Data Structures Coding Problems →](./05-data-structures-coding-problems.md)
