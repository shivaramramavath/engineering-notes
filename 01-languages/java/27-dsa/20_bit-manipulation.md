# Bit Manipulation

> Low-level binary arithmetic, two's complement mechanics, hardware bitwise intrinsics, and branch-free algorithmic patterns.

| Metadata | Value |
|---|---|
| **Level** | Intermediate |
| **Prerequisites** | [01_arrays-and-strings.md](01_arrays-and-strings.md) |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **Java Version** | Java 8+ (Verified on Java 25 LTS) |

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
9. [Comparison / When to Use What](#9-comparison--when-to-use-what)
10. [Interview Questions](#10-interview-questions)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary / Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

In Java, all integer types (`byte`, `short`, `int`, `long`) are stored in memory using **Two's Complement** binary representation.

**Bit Manipulation** refers to performing operations directly on the individual binary digits (bits) of integers.
- **Extreme Speed**: Bitwise operations execute in a single CPU clock cycle (sub-nanosecond latency).
- **Space Efficiency**: A 32-bit integer can store a set of 32 booleans without any heap allocation or pointer overhead.
- **Branch-Free Programming**: Eliminates costly CPU branch mispredictions in critical loops.

---

## 2. Core Concepts

### 1. Java Bitwise Operators
- **AND (`&`)**: `1 & 1 = 1`, all other combinations yield `0`. Used for masking and clearing.
- **OR (`|`)**: `0 | 0 = 0`, all other combinations yield `1`. Used for setting bits.
- **XOR (`^`)**: `1 ^ 0 = 1, 0 ^ 1 = 1`, while `1 ^ 1 = 0, 0 ^ 0 = 0`. Crucial for parity checks and toggling.
- **NOT (`~`)**: Inverts all bits (`~x = -(x + 1)` in two's complement).
- **Left Shift (`<<`)**: Shifts bits left, padding with zeros (`x << k = x * 2^k`).
- **Signed Right Shift (`>>`)**: Shifts bits right, preserving the sign bit (arithmetic shift).
- **Unsigned Right Shift (`>>>`)**: Shifts bits right, padding with zeros regardless of sign (logical shift).

### 2. Fundamental Bit Tricks
| Goal | Bit Operation | Mathematical Meaning |
|---|---|---|
| **Check if $k$-th bit is set** | `(x & (1 << k)) != 0` | Returns boolean |
| **Set $k$-th bit** | `x | (1 << k)` | Activates bit $k$ |
| **Clear $k$-th bit** | `x & ~(1 << k)` | Deactivates bit $k$ |
| **Toggle $k$-th bit** | `x ^ (1 << k)` | Inverts bit $k$ |
| **Isolate lowest set bit** | `x & -x` | Retains only the least significant 1-bit |
| **Clear lowest set bit** | `x & (x - 1)` | Removes the least significant 1-bit |
| **Check if power of 2** | `x > 0 && (x & (x - 1)) == 0` | Exactly one 1-bit exists |

---

## 3. How It Works Internally

### HotSpot JVM Intrinsics and Hardware Vectorization
The HotSpot JIT compiler detects standard Java library methods and replaces them directly with native CPU instructions:
```
Java API Call                  x86-64 Instruction       ARM64 Instruction
Integer.bitCount(x)        ->  POPCNT                   CNT
Integer.numberOfLeadingZeros(x) -> LZCNT / BSR          CLZ
Integer.numberOfTrailingZeros(x) -> TZCNT / BSF         RBIT + CLZ
Integer.highestOneBit(x)   ->  BSR + SHL                CLZ + ROR
```
Handwritten bit-counting loops take 32 cycles; `Integer.bitCount()` compiles to a single `POPCNT` instruction running in 1 cycle!

### Two's Complement Negation (`-x`)
In two's complement, `-x = ~x + 1`.
Consider $x = 12$ (`00001100`):
- `~x` = `11110011`
- `~x + 1` = `11110100` (which is `-12`)
- Now compute `x & -x`:
  ```
    00001100 (12)
  & 11110100 (-12)
  ----------
    00000100 (4 = lowest set bit isolated!)
  ```

---

## 4. Syntax & Usage

### Standard Bit Utilities in Java

```java
public class BitManipulationBasics {
    public static void main(String[] args) {
        int x = 12; // Binary: 00001100

        // 1. Isolate lowest set bit (4)
        int lowestSetBit = x & -x;
        System.out.println("Lowest set bit: " + lowestSetBit);

        // 2. Clear lowest set bit (8)
        int cleared = x & (x - 1);
        System.out.println("Cleared lowest bit: " + cleared);

        // 3. Hardware intrinsics
        System.out.println("Population count: " + Integer.bitCount(x)); // 2
        System.out.println("Trailing zeros: " + Integer.numberOfTrailingZeros(x)); // 2
        System.out.println("Leading zeros: " + Integer.numberOfLeadingZeros(x)); // 28
    }
}
```

---

## 5. Practical Examples

### Example 1: Single Number I, II, III (LeetCode 136, 137, 260)

```java
public class SingleNumberVariants {
    // Single Number I: Every element appears twice except one
    public int singleNumberI(int[] nums) {
        int xor = 0;
        for (int num : nums) xor ^= num;
        return xor; // Duplicate pairs cancel out: x ^ x = 0
    }

    // Single Number II: Every element appears 3 times except one
    public int singleNumberII(int[] nums) {
        int ones = 0, twos = 0;
        for (int num : nums) {
            ones = (ones ^ num) & ~twos;
            twos = (twos ^ num) & ~ones;
        }
        return ones;
    }

    // Single Number III: Exactly two distinct numbers appear once
    public int[] singleNumberIII(int[] nums) {
        int xorAll = 0;
        for (int num : nums) xorAll ^= num;

        // Isolate lowest differing bit between the two unique numbers
        int diffBit = xorAll & -xorAll;

        int a = 0, b = 0;
        for (int num : nums) {
            if ((num & diffBit) == 0) {
                a ^= num;
            } else {
                b ^= num;
            }
        }

        return new int[]{a, b};
    }
}
```

### Example 2: Number of 1 Bits (LeetCode 191, Brian Kernighan)

```java
public class HammingWeight {
    public int hammingWeight(int n) {
        int count = 0;
        while (n != 0) {
            n &= (n - 1); // Clears the lowest set bit in O(1)
            count++;
        }
        return count; // Loop runs strictly equal to number of set bits!
    }
}
```

### Example 3: Counting Bits (LeetCode 338, DP with Lowest Bit Transition)

```java
public class CountingBits {
    public int[] countBits(int n) {
        int[] ans = new int[n + 1];
        for (int i = 1; i <= n; i++) {
            // ans[i] = 1 + ans[number with lowest set bit cleared]
            ans[i] = ans[i & (i - 1)] + 1;
        }
        return ans;
    }
}
```

### Example 4: Reverse Bits (LeetCode 190)

```java
public class ReverseBits {
    public int reverseBits(int n) {
        int result = 0;
        for (int i = 0; i < 32; i++) {
            result = (result << 1) | (n & 1);
            n >>>= 1; // Unsigned shift to avoid sign extension
        }
        return result;
    }
}
```

### Example 5: Bitwise AND of Numbers Range (LeetCode 201)

```java
public class RangeBitwiseAnd {
    public int rangeBitwiseAnd(int left, int right) {
        // The bitwise AND of all numbers in [left, right] is their common prefix!
        int shiftCount = 0;
        while (left < right) {
            left >>= 1;
            right >>= 1;
            shiftCount++;
        }
        return left << shiftCount;
    }
}
```

---

## 6. Best Practices

1. **Use HotSpot Intrinsics for Bit Counting**: Always call `Integer.bitCount(x)` instead of looping. It compiles directly into the single-cycle hardware `POPCNT` instruction.
2. **Use Unsigned Shift `>>>` on Binary Data**: When treating integers as raw bit strings (e.g. hashing, cryptography, bitmaps), always use `>>>` instead of `>>` to prevent negative sign extension.
3. **Always Enclose Bitwise Expressions in Parentheses**: Because `&`, `|`, and `^` have lower precedence than `==`, `!=`, `<`, and `+`, write: `if ((x & 1) == 0)` never `if (x & 1 == 0)`.
4. **Guard Long Shifts with `1L << k`**: Shifting a 32-bit `1 << 40` evaluates as `1 << (40 % 32) = 1 << 8`! To shift beyond 31 bits, use `1L << k`.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Fix |
|---|---|---|
| `x & 1 == 0` without parentheses | Evaluates as `x & (1 == 0)` which is a compile error or wrong result | Always write `(x & 1) == 0`. |
| Shifting by $\ge 32$ on `int` | In Java, shift distance is masked with `0x1F` (modulo 32) | Use `long` with `1L << k`. |
| Using `>>` instead of `>>>` | Preserves sign bit, filling top bits with 1s for negative numbers | Use `>>>` for raw binary logic. |
| Assuming `x > 0` is not needed for power of 2 | For $x = 0$ or negative numbers, `(x & (x-1)) == 0` can misfire | Use `x > 0 && (x & (x - 1)) == 0`. |

---

## 8. Performance & Security Notes

- **Time Complexity**: All primitive bitwise operations (`&`, `|`, `^`, `~`, `<<`, `>>`, `>>>`) run in strictly $O(1)$ single-cycle time.
- **Constant-Time Cryptography**: Branch-free bit manipulation prevents timing side-channel attacks by ensuring the execution path is identical regardless of input values.
- **Register Utilization**: Bit manipulation operates entirely within CPU registers, requiring zero memory loads or cache line fills.

---

## 9. Comparison / When to Use What

| Technique | Time Complexity | Space Complexity | Best For |
|---|---|---|---|
| **`HashSet<Integer>`** | $O(1)$ average | High ($32$ bytes/node) | Arbitrary sparse IDs |
| **`BitSet`** | $O(1)$ | Compact (1 bit/id) | Dense collections up to $10^7$ IDs |
| **Primitive Bitmask (`int`/`long`)** | $O(1)$ hardware | Zero ($4$ or $8$ bytes) | Sets with up to 64 items |

---

## 10. Interview Questions

<details>
<summary>1. How does Brian Kernighan's algorithm count set bits in $O(\text{set bits})$ time?</summary>

The expression `x & (x - 1)` always clears the lowest set bit of `x`. By repeatedly applying `x = x & (x - 1)` until `x == 0`, the loop executes strictly as many times as there are 1-bits in `x`, bypassing all intermediate 0-bits.
</details>

<details>
<summary>2. Why does `1 << 35` equal `8` in Java?</summary>

The Java Language Specification dictates that for 32-bit `int` shifts, only the lowest 5 bits of the shift distance are used (`shift % 32`). Since $35 \pmod{32} = 3$, `1 << 35` evaluates to `1 << 3 = 8`. For 64-bit `long` shifts, the lowest 6 bits are used (`shift % 64`).
</details>

<details>
<summary>3. How do you swap two variables without using a temporary variable using XOR?</summary>

```java
a = a ^ b;
b = a ^ b; // b becomes (a ^ b) ^ b = a
a = a ^ b; // a becomes (a ^ b) ^ a = b
```
Note: In production code, standard temporary variables are preferred because the XOR trick can trigger undefined behavior when `a` and `b` reference the same memory address (`a ^ a = 0`).
</details>

<details>
<summary>4. What is Gray Code and how is it generated using bitwise operations?</summary>

A Gray Code is a binary numeral system where two successive values differ in only one bit. The $i$-th Gray code can be generated in $O(1)$ using:
`gray = i ^ (i >> 1);`
</details>

<details>
<summary>5. How do you compute the absolute value of an integer without branching?</summary>

```java
int mask = x >> 31; // 0 if x >= 0, -1 (all 1s) if x < 0
int abs = (x + mask) ^ mask;
```
If $x < 0$, `mask = -1`: $(x - 1) \oplus -1 = -x$. If $x \ge 0$, `mask = 0`: $x \oplus 0 = x$.
</details>

<details>
<summary>6. How do you find the parity of a 64-bit integer in $O(1)$?</summary>

```java
int parity = Long.bitCount(x) & 1;
```
If the population count of 1-bits is odd, parity is 1; if even, parity is 0.
</details>

<details>
<summary>7. What is the difference between Arithmetic Shift and Logical Shift?</summary>

- **Arithmetic Shift (`>>`)**: Preserves the sign bit by shifting in copies of the most significant bit (MSB). Shifting a negative number right preserves its negative sign.
- **Logical Shift (`>>>`)**: Fills the vacant high-order bits with zeros unconditionally, treating the number as an unsigned bitfield.
</details>

<details>
<summary>8. How do you determine if two integers have opposite signs without branching?</summary>

```java
boolean oppositeSigns = (x ^ y) < 0;
```
The sign bit of a number is 1 if negative and 0 if positive. If $x$ and $y$ have opposite signs, their XOR will have a 1 in the sign bit, making the result negative ($< 0$).
</details>

---

## 11. Practice Exercises

### Exercise 1: Missing Number (LeetCode 268, XOR cancellation)

<details>
<summary>Solution</summary>

```java
public class MissingNumber {
    public int missingNumber(int[] nums) {
        int xor = nums.length;
        for (int i = 0; i < nums.length; i++) {
            xor ^= i ^ nums[i];
        }
        return xor; // Numbers present cancel out; missing number remains!
    }
}
```
</details>

### Exercise 2: Sum of Two Integers Without `+` or `-` (LeetCode 371)

<details>
<summary>Solution</summary>

```java
public class SumOfTwoIntegers {
    public int getSum(int a, int b) {
        while (b != 0) {
            int carry = (a & b) << 1; // Carry occurs where both bits are 1
            a = a ^ b;                // Sum without carry
            b = carry;                // Loop until carry is fully resolved
        }
        return a;
    }
}
```
</details>

### Exercise 3: Subsets (LeetCode 78, Bitmask Generation)

<details>
<summary>Solution</summary>

```java
import java.util.*;

public class SubsetsBitmask {
    public List<List<Integer>> subsets(int[] nums) {
        int n = nums.length;
        int totalSubsets = 1 << n;
        List<List<Integer>> result = new ArrayList<>(totalSubsets);

        for (int mask = 0; mask < totalSubsets; mask++) {
            List<Integer> sub = new ArrayList<>();
            for (int i = 0; i < n; i++) {
                if ((mask & (1 << i)) != 0) {
                    sub.add(nums[i]);
                }
            }
            result.add(sub);
        }

        return result;
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **Core Primitives**: `x & (x - 1)` clears lowest 1-bit; `x & -x` isolates lowest 1-bit.
- **XOR Identity**: $x \oplus x = 0$ and $x \oplus 0 = x$.
- **HotSpot Intrinsics**: Always use `Integer.bitCount()` and `Integer.numberOfTrailingZeros()`.
- **Precedence Guard**: Always write `(x & mask) != 0`.
- **Shift Modulo**: 32-bit integers mask shift by 31; use `1L << k` for 64-bit masks.

---

## 13. Further Reading

- [Henry S. Warren Jr.: Hacker's Delight (2nd Edition)](https://en.wikipedia.org/wiki/Hacker%27s_Delight)
- [OpenJDK Source Code: `java.lang.Integer`](https://github.com/openjdk/jdk/blob/master/src/java.base/share/classes/java/lang/Integer.java)
- [Baeldung: Bitwise Operators in Java](https://www.baeldung.com/java-bitwise-operators)
- [LeetCode Bit Manipulation Tag](https://leetcode.com/tag/bit-manipulation/)
