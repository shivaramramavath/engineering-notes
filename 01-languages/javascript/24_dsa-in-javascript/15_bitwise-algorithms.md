# Bitwise Algorithms & Bit Manipulation in JavaScript

> A production-grade algorithmic guide to Bitwise Manipulation in JavaScript, covering 32-bit two's complement coercion, bitmasking, Brian Kernighan's algorithm, XOR properties, and RBAC permissions.

| Metadata | Value |
| :--- | :--- |
| **Level** | Core / Comprehensive |
| **Prerequisites** | [01_complexity-and-big-o.md](file:///e:/Learning/engineering-notes/01-languages/javascript/24_dsa-in-javascript/01_complexity-and-big-o.md), [01_fundamentals/05_operators.md](file:///e:/Learning/engineering-notes/01-languages/javascript/01_fundamentals/05_operators.md) |
| **Standard Baseline** | ECMAScript 2024 / Node.js 20+ LTS / V8 Engine |
| **Status** | Production Ready |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & API Reference](#4-syntax--api-reference)
5. [Practical Examples](#5-practical-examples)
6. [Edge Cases, Pitfalls & Gotchas](#6-edge-cases-pitfalls--gotchas)
7. [Common Mistakes & Anti-Patterns](#7-common-mistakes--anti-patterns)
8. [Performance Characteristics](#8-performance-characteristics)
9. [Engine Internals](#9-engine-internals)
10. [Best Practices & Design Guidelines](#10-best-practices--design-guidelines)
11. [Real-World Case Study](#11-real-world-case-study)
12. [Interview Questions](#12-interview-questions)
13. [Exercises & Challenges](#13-exercises--challenges)

---

## 1. What & Why

All digital computer hardware operates fundamentally on binary digits (bits: 0 and 1). While high-level JavaScript code typically deals with abstract data structures (objects, arrays, strings), operating directly at the bit level unlocks profound speed and memory efficiencies.

In JavaScript, bitwise operations are essential for:
- **High-Performance Flags & Bitmasks**: Compressing dozens of boolean permission settings into a single 32-bit integer.
- **Cryptographic & Hashing Routines**: Implementing SHA-256, MurmurHash, or UUID generators.
- **Algorithmic Optimizations**: Finding unique elements, calculating powers of two, and generating subsets in **$O(1)$ or $O(N)$** time without allocating heap memory.

---

## 2. Core Concepts

### Bitwise Operators in JavaScript
| Operator | Name | Syntax | Description |
| :--- | :--- | :--- | :--- |
| `&` | AND | `a & b` | 1 if both bits are 1; else 0 |
| `\|` | OR | `a \| b` | 1 if either bit is 1; else 0 |
| `^` | XOR | `a ^ b` | 1 if bits differ; 0 if bits match |
| `~` | NOT | `~a` | Inverts all bits (two's complement: `~x === -(x + 1)`) |
| `<<` | Left Shift | `a << b` | Shifts bits left, filling with 0 ($a \times 2^b$) |
| `>>` | Sign-Preserving Right Shift | `a >> b` | Shifts right, preserving sign bit ($\lfloor a / 2^b \rfloor$) |
| `>>>` | Zero-Fill Right Shift | `a >>> b` | Shifts right, pushing 0s from left (unsigned result) |

```
Bitwise Logic:
  1010 (10)       1010 (10)       1010 (10)
& 1100 (12)     | 1100 (12)     ^ 1100 (12)
───────────     ───────────     ───────────
  1000 (8)        1110 (14)       0110 (6)
```

### Essential Properties of XOR ($\oplus$)
1. **Self-Inverse**: $x \oplus x = 0$
2. **Identity**: $x \oplus 0 = x$
3. **Commutative & Associative**: $a \oplus b \oplus c = b \oplus c \oplus a$
Any pair of duplicate numbers cancels out to 0, leaving unique values isolated!

### Brian Kernighan’s Clearing Trick
The bitwise expression `n & (n - 1)` **clears the lowest set bit (rightmost 1)** of $n$:
```
  n     = 12 = 1100
  n - 1 = 11 = 1011
  n & (n - 1) = 1000 (Lowest 1-bit cleared!)
```
- If `n & (n - 1) === 0` (and $n > 0$), $n$ is guaranteed to be a **Power of Two**!

---

## 3. How It Works Internally

### The V8 32-Bit Integer Coercion (`ToInt32`)
JavaScript numbers are represented internally as 64-bit IEEE-754 double-precision floating-point numbers. When a bitwise operator is applied:
1. V8 calls the internal abstract operation `ToInt32(val)`.
2. The 64-bit float is truncated to a **32-bit signed two's complement integer**.
3. The bitwise operation executes in a single CPU instruction.
4. The result is converted back into a JavaScript number.

```
64-bit Float Number (e.g. 5.9)
       │
       ▼  ToInt32 Coercion
32-bit Two's Complement Integer: 00000000 00000000 00000000 00000101 (5)
       │
       ▼  Bitwise Operation (e.g. 5 << 1)
32-bit Result:                   00000000 00000000 00000000 00001010 (10)
```

---

## 4. Syntax & API Reference

### Core Bitmasking Helper Functions
```javascript
/**
 * Bitmask manipulation utility library.
 */
export const Bitmask = {
  // Checks if the k-th bit is set (0-indexed)
  isSet: (n, k) => (n & (1 << k)) !== 0,

  // Sets the k-th bit to 1
  set: (n, k) => n | (1 << k),

  // Clears the k-th bit to 0
  clear: (n, k) => n & ~(1 << k),

  // Toggles the k-th bit (0 -> 1 or 1 -> 0)
  toggle: (n, k) => n ^ (1 << k),

  // Clears the lowest set bit
  clearLowestSetBit: (n) => n & (n - 1),

  // Extracts the lowest set bit alone
  getLowestSetBit: (n) => n & -n,

  // Checks if n is a positive power of two
  isPowerOfTwo: (n) => n > 0 && (n & (n - 1)) === 0,
};
```

---

## 5. Practical Examples

### Example 1: Single Number (Find Unique in Pairs using XOR)
```javascript
/**
 * In an array where every element appears twice except one, finds the unique element.
 * Time Complexity: O(N) | Space Complexity: O(1)
 */
export function singleNumber(nums) {
  let unique = 0;
  for (let i = 0; i < nums.length; i++) {
    unique ^= nums[i]; // All duplicate pairs cancel out (x ^ x === 0)
  }
  return unique;
}

// console.log(singleNumber([4, 1, 2, 1, 2])); // 4
```

### Example 2: Number of 1 Bits / Hamming Weight (Brian Kernighan)
```javascript
/**
 * Counts set bits in O(set bits) time rather than O(32) iterations.
 */
export function hammingWeight(n) {
  let count = 0;
  let num = n;

  while (num !== 0) {
    num = num & (num - 1); // Clears rightmost set bit
    count++;
  }

  return count;
}

// console.log(hammingWeight(11)); // 11 is 1011 in binary -> 3 set bits
```

### Example 3: Missing Number in Array $[0 \dots N]$
```javascript
export function missingNumber(nums) {
  let xor = 0;
  const n = nums.length;

  // XOR all indices 0..n
  for (let i = 0; i <= n; i++) {
    xor ^= i;
  }

  // XOR all elements in nums; duplicate values cancel, leaving missing number
  for (let i = 0; i < n; i++) {
    xor ^= nums[i];
  }

  return xor;
}

// console.log(missingNumber([3, 0, 1])); // 2
```

### Example 4: Subsets Generation via Binary Counter (Bitmasking)
```javascript
export function subsetsBitmask(nums) {
  const n = nums.length;
  const totalSubsets = 1 << n; // 2^n
  const results = [];

  for (let mask = 0; mask < totalSubsets; mask++) {
    const subset = [];
    for (let i = 0; i < n; i++) {
      // If i-th bit of mask is set, include nums[i]
      if ((mask & (1 << i)) !== 0) {
        subset.push(nums[i]);
      }
    }
    results.push(subset);
  }

  return results;
}
```

### Example 5: In-Place Value Swapping Without Temporary Variable
```javascript
export function swapInPlace(arr, i, j) {
  if (i === j) return; // Must not be identical index or xor zeroes the value!

  arr[i] = arr[i] ^ arr[j];
  arr[j] = arr[i] ^ arr[j];
  arr[i] = arr[i] ^ arr[j];
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **32-Bit Integer Truncation (The $> 2^{31}$ Overflow)**:
   All bitwise operators in JavaScript coerce numbers to 32-bit signed integers. If you operate on numbers larger than $2^{31} - 1 = 2,147,483,647$:
   ```javascript
   console.log(2147483648 | 0); // -2147483648 (wraps around to negative!)
   console.log(1 << 31);         // -2147483648
   ```
   **Fix**: For 64-bit bitwise operations, use native **BigInt** (`1n << 60n`).
2. **Operator Precedence Traps**:
   Bitwise operators have **lower precedence than equality operators** (`==`, `===`):
   ```javascript
   // CRITICAL BUG: Evaluates as (n & (1 === 0)) which is (n & false) === 0
   if (n & 1 === 0) { ... }

   // CORRECT: Use explicit parentheses
   if ((n & 1) === 0) { ... }
   ```
3. **Signed Right Shift (`>>`) vs. Zero-Fill Right Shift (`>>>`)**:
   `-10 >> 1` propagates the sign bit, returning `-5`. In contrast, `-10 >>> 1` treats the number as unsigned 32-bit, returning `2147483643`! Always use `>>>` when working with raw binary buffers or unsigned integers.
4. **XOR Self-Swap Destruction**:
   Executing the XOR swap on identical array indices (`swapInPlace(arr, 0, 0)`) evaluates `arr[0] ^= arr[0]`, zeroing out the element!

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Operational Defect | Production Remediation |
| :--- | :--- | :--- |
| `n & 1 === 0` without parentheses | Equality executes first, completely corrupting condition logic. | Always parenthesize: `(n & 1) === 0`. |
| Bitwise operations on numbers $> 2^{31}$ | Coerces to 32-bit two's complement, producing negative values. | Use native `BigInt` (`1n << 40n`). |
| Using XOR swap on identical indices | `x ^ x = 0` sets the element permanently to zero. | Guard with `if (i !== j)` before running XOR swap. |
| Neglecting negative numbers in `isPowerOfTwo` | Negative powers of two produce false positives. | Guard with `n > 0 && (n & (n - 1)) === 0`. |
| Using `Math.pow(2, n)` for integer powers | Slower than bit shift; potential floating point inaccuracies. | Use `1 << n` (for $n < 31$) or `1n << BigInt(n)`. |
| Storing 50 separate boolean properties | Bloats V8 object shape and memory footprint. | Pack booleans into a single 32-bit bitmask integer. |

---

## 8. Performance Characteristics

- **Instruction Execution**: Bitwise operators compile directly into single-cycle CPU instructions (`AND`, `OR`, `XOR`, `SHL`, `SHR`), executing in $<0.5\text{ns}$ on modern silicon.
- **Memory Density**: A single 32-bit integer stores 32 distinct boolean flags, consuming 4 bytes of memory compared to 32 separate JavaScript object properties ($\approx 1.2\text{KB}$ with hidden class overhead).

---

## 9. Engine Internals

### V8 Smi (Small Integer) Bit Tagging
In 64-bit V8, numbers that fit in 31 bits are stored as **Smis (Small Integers)**:
- V8 uses the least significant bit (LSB) as a tag: `0` means Smi, `1` means HeapObject pointer.
- Smi integers do not live on the garbage-collected heap; they reside directly inside the pointer register.
- Bitwise operators take advantage of this representation, avoiding heap dereferencing entirely.

---

## 10. Best Practices & Design Guidelines

1. **Always Use Parentheses**: Guard bitwise operations against operator precedence pitfalls: `(mask & flag) !== 0`.
2. **Use Bitmasks for Permission Schemes**: Model Role-Based Access Control (RBAC) flags using bit powers of two.
3. **Use Brian Kernighan for Bit Counting**: Iterates only as many times as there are 1-bits ($O(\text{set bits})$) rather than checking all 32 bits.
4. **Use `BigInt` for Large Bitfields**: Exceed 32 bits safely using `1n << 45n`.

---

## 11. Real-World Case Study

### High-Performance RBAC Permission Validator
**Scenario**: An enterprise multi-tenant API validated user permissions on every HTTP request. Users had combinations of 24 distinct permissions (`READ_DOCS`, `EDIT_DOCS`, `DELETE_DOCS`, `ADMIN_BILLING`, etc.).
**Naive Implementation**: Stored permissions as an array of strings: `user.permissions = ['READ_DOCS', 'EDIT_DOCS']`. Each authorization check executed `user.permissions.includes(perm)` ($O(N)$ linear string comparison).

**Bitmask Architecture**:
```javascript
export const Permissions = {
  READ:   1 << 0, // 0001 (1)
  WRITE:  1 << 1, // 0010 (2)
  DELETE: 1 << 2, // 0100 (4)
  ADMIN:  1 << 3, // 1000 (8)
};

// Assign permissions: user has READ + WRITE
const userPerms = Permissions.READ | Permissions.WRITE; // 3

// Check authorization in O(1) single-cycle CPU operation
const canWrite = (userPerms & Permissions.WRITE) !== 0;  // true
const canDelete = (userPerms & Permissions.DELETE) !== 0; // false
```
**Outcome**:
- Memory per user record dropped from 480 bytes to 4 bytes.
- Permission validation time dropped from $120\text{ns}$ to $0.4\text{ns}$ (300x faster).

---

## 12. Interview Questions

<details>
<summary>1. What happens when a bitwise operator is applied to a floating-point number in JavaScript?</summary>

JavaScript numbers are 64-bit IEEE-754 double-precision floats. When a bitwise operator (e.g. `|`, `&`, `^`, `<<`) is encountered, the V8 engine invokes the abstract `ToInt32` operation. This truncates the fractional digits and converts the number into a 32-bit signed two's complement integer. The bitwise operation is performed on this 32-bit integer, and the result is converted back into a JavaScript number. Any bits beyond 32 are discarded.
</details>

<details>
<summary>2. How does <code>n & (n - 1)</code> clear the lowest set bit?</summary>

Subtracting 1 from an integer $n$ flips all the bits to the right of the lowest set bit (including the lowest set bit itself from 1 to 0), while leaving all bits to the left unchanged. Performing a bitwise AND between $n$ and $n - 1$ results in 0 for all bits starting at the lowest set bit to the right, effectively clearing the lowest 1-bit while preserving all higher bits.
</details>

<details>
<summary>3. Why must bitwise operations be wrapped in parentheses when combined with equality operators?</summary>

In JavaScript's operator precedence table, equality operators (`==`, `!=`, `===`, `!==`) have precedence level 9, whereas bitwise AND (`&`) has precedence level 8 and bitwise OR (`|`) has level 6. Writing `if (n & 1 === 0)` evaluates `1 === 0` first (false / 0), resulting in `n & 0 === 0`, which is always falsy. Adding parentheses `(n & 1) === 0` forces the bitwise operation to evaluate before the comparison.
</details>

<details>
<summary>4. What is the difference between <code>>></code> and <code>>>></code>?</summary>

- `>>` is the **Sign-Propagating Right Shift**: It preserves the sign bit (the leftmost bit in two's complement). If the number is negative, it fills the shifted positions with 1s (`-8 >> 1 === -4`).
- `>>>` is the **Zero-Fill Right Shift**: It does not preserve the sign bit; it pushes 0s in from the left, treating the 32-bit representation strictly as an **unsigned 32-bit integer**. Consequently, shifting a negative number with `>>>` converts it into a large positive integer (`-8 >>> 1 === 2147483644`).
</details>

<details>
<summary>5. How can you find the single non-duplicate number in an array where every other element appears twice?</summary>

By initializing `unique = 0` and XORing all elements in the array: `for (const x of nums) unique ^= x;`.
Because XOR is commutative and associative ($a \oplus b \oplus a = b \oplus (a \oplus a) = b \oplus 0 = b$), every pair of identical numbers cancels out to 0. After XORing the entire array, the only remaining value is the unique number. Time complexity is $O(N)$ and auxiliary space is $O(1)$.
</details>

<details>
<summary>6. How do you check if an integer is a Power of Two using bitwise operations?</summary>

An integer $n$ is a power of two if and only if it is positive ($n > 0$) and contains exactly one set bit in its binary representation. Using Brian Kernighan's trick: `(n > 0) && (n & (n - 1)) === 0`. If clearing its only set bit results in zero, it is a power of two.
</details>

<details>
<summary>7. How do you implement a bitwise toggle operation?</summary>

Using the XOR (`^`) operator with a bitmask containing a 1 at the desired position:
`mask = mask ^ (1 << bitIndex);`
- If the bit was 1: $1 \oplus 1 = 0$ (cleared).
- If the bit was 0: $0 \oplus 1 = 1$ (set).
</details>

<details>
<summary>8. How can you perform bitwise operations on values exceeding 32 bits in JavaScript?</summary>

By using ECMAScript's native **BigInt** type. BigInt supports arbitrary-precision integers and implements bitwise operators (`&`, `|`, `^`, `~`, `<<`, `>>`) without truncating to 32 bits:
```javascript
const largeBitmask = 1n << 50n; // Bit 50 safely set without 32-bit overflow!
```
Note: BigInt operands cannot be mixed with standard Numbers without explicit conversion.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Counting Bits
**Specification**: Given an integer `n`, return an array `ans` of length `n + 1` such that for each `i` ($0 \le i \le n$), `ans[i]` is the number of `1`'s in the binary representation of `i`. Implement in $O(N)$ time using dynamic programming with bitwise transition.

<details>
<summary>Solution</summary>

```javascript
export function countBits(n) {
  const ans = new Int32Array(n + 1);

  // ans[i] = ans[i >> 1] + (i & 1)
  // Number of set bits equals set bits of i / 2 plus the least significant bit
  for (let i = 1; i <= n; i++) {
    ans[i] = ans[i >> 1] + (i & 1);
  }

  return Array.from(ans);
}
```
</details>

### Exercise 2: Reverse Bits
**Specification**: Reverse the bits of a given 32-bit unsigned integer and return the resulting integer.

<details>
<summary>Solution</summary>

```javascript
export function reverseBits(n) {
  let result = 0;

  for (let i = 0; i < 32; i++) {
    // Shift result left by 1 and append lowest bit of n
    result = (result << 1) | (n & 1);
    // Unsigned right shift n by 1
    n = n >>> 1;
  }

  // Convert to unsigned 32-bit integer using >>> 0
  return result >>> 0;
}
```
</details>
