# Arrays, Strings & Two-Pointer Patterns in JavaScript

> A production-grade algorithmic guide to arrays, strings, sliding windows, prefix sums, and two-pointer techniques in JavaScript, optimized for V8 memory representations and Unicode surrogate pairs.

| Metadata | Value |
| :--- | :--- |
| **Level** | Core / Comprehensive |
| **Prerequisites** | [01_complexity-and-big-o.md](file:///e:/Learning/engineering-notes/01-languages/javascript/24_dsa-in-javascript/01_complexity-and-big-o.md), [03_objects-and-arrays/02_array-methods-mutating.md](file:///e:/Learning/engineering-notes/01-languages/javascript/03_objects-and-arrays/02_array-methods-mutating.md) |
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

Arrays and strings represent the foundational linear data structures in computer science. In JavaScript, virtually every web application, data pipeline, and user interface involves manipulating arrays of records or parsing string payloads.

However, standard JavaScript arrays and strings have unique architectural characteristics:
- JavaScript arrays are not simple fixed-size C-style buffers; they are dynamically resizing, polymorphic heap objects managed by V8.
- JavaScript strings are immutable sequences of **UTF-16 code units**, which can split multi-byte Unicode characters (emojis, complex scripts) if indexed naively.
- Mastering algorithmic paradigms like **Two Pointers**, **Sliding Windows**, and **Prefix Sums** allows engineers to reduce naive $O(N^2)$ brute-force solutions to linear $O(N)$ operations.

---

## 2. Core Concepts

### 1. Two-Pointer Technique
Two pointers traverse linear data structures either:
- **Opposite Ends**: Left pointer starts at $0$, right pointer at $N-1$, moving inward toward each other (e.g. palindrome verification, sorted array two-sum).
- **Same Direction (Fast & Slow)**: Fast pointer explores candidates while slow pointer tracks valid elements or array write boundaries (e.g. removing duplicates in-place).

```
Opposite Ends:
[  1  ,  2  ,  4  ,  6  ,  8  ,  11  ]
   ▲                             ▲
  left                         right

Fast & Slow:
[  0  ,  1  ,  0  ,  3  ,  12  ]
   ▲     ▲
  slow  fast
```

### 2. Sliding Window Technique
Maintains a subset (window) of elements between a `left` and `right` index over an array or string:
- **Fixed Window ($K$)**: Window slides one element at a time; new element is added on the right, old element subtracted from the left ($O(1)$ window transition).
- **Dynamic Window**: The right boundary expands to satisfy a condition; the left boundary contracts when constraints are violated.

```
Window of size K = 3:
Step 1: [ 2 , 1 , 5 ] , 1 , 3 , 2  ──► Sum = 8
Step 2:   2 , [ 1 , 5 , 1 ] , 3 , 2  ──► Sum = 8 - 2 + 1 = 7
```

### 3. Prefix Sum Array
A pre-computed array where $P[i] = \sum_{j=0}^{i-1} A[j]$. Enables calculating the sum of any contiguous subarray from index $L$ to $R$ in **$O(1)$ constant time**:
$$\text{Sum}(L, R) = P[R + 1] - P[L]$$

---

## 3. How It Works Internally

### V8 String Representation: Flat vs ConsString (Ropes)
In V8, concatenating strings does not always copy characters immediately:
1. **SeqOneByteString / SeqTwoByteString**: Flat contiguous byte arrays in memory.
2. **ConsString (Rope Tree)**: When two large strings are concatenated (`strA + strB`), V8 creates a binary tree node with pointers to `left` and `right` without copying bytes.
3. **String Flattening**: When an index lookup (`str[i]`) or regular expression runs on a `ConsString`, V8 flattens the tree into a contiguous memory buffer.

```
ConsString Representation:
          [ ConsString ]
            /        \
    [ "Hello, " ]   [ "World!" ]
```

---

## 4. Syntax & API Reference

### Core Prefix Sum Class Implementation
```javascript
/**
 * Computes prefix sums for O(1) range queries.
 */
export class PrefixSum {
  constructor(nums) {
    this.prefix = new Int32Array(nums.length + 1);
    for (let i = 0; i < nums.length; i++) {
      this.prefix[i + 1] = this.prefix[i] + nums[i];
    }
  }

  /**
   * Returns sum of elements between indices left and right inclusive.
   * Time Complexity: O(1)
   */
  queryRange(left, right) {
    if (left < 0 || right >= this.prefix.length - 1 || left > right) {
      throw new RangeError('Invalid query bounds');
    }
    return this.prefix[right + 1] - this.prefix[left];
  }
}
```

---

## 5. Practical Examples

### Example 1: Valid Palindrome (Opposite-End Two Pointers)
```javascript
export function isPalindrome(s) {
  let left = 0;
  let right = s.length - 1;

  function isAlphanumeric(code) {
    return (
      (code >= 48 && code <= 57) || // 0-9
      (code >= 65 && code <= 90) || // A-Z
      (code >= 97 && code <= 122)   // a-z
    );
  }

  while (left < right) {
    while (left < right && !isAlphanumeric(s.charCodeAt(left))) {
      left++;
    }
    while (left < right && !isAlphanumeric(s.charCodeAt(right))) {
      right--;
    }

    if (s[left].toLowerCase() !== s[right].toLowerCase()) {
      return false;
    }

    left++;
    right--;
  }

  return true;
}
```

### Example 2: Maximum Subarray Sum of Size $K$ (Fixed Sliding Window)
```javascript
export function maxSubarraySum(nums, k) {
  if (nums.length < k || k <= 0) return 0;

  let currentSum = 0;
  // Compute initial window
  for (let i = 0; i < k; i++) {
    currentSum += nums[i];
  }

  let maxSum = currentSum;

  // Slide window across array: O(N) total time
  for (let i = k; i < nums.length; i++) {
    currentSum += nums[i] - nums[i - k]; // Add incoming, drop outgoing
    if (currentSum > maxSum) {
      maxSum = currentSum;
    }
  }

  return maxSum;
}
```

### Example 3: Longest Substring Without Repeating Characters (Dynamic Sliding Window)
```javascript
export function lengthOfLongestSubstring(s) {
  const lastSeenIndex = new Map();
  let maxLength = 0;
  let left = 0;

  for (let right = 0; right < s.length; right++) {
    const char = s[right];

    // If duplicate seen within current window, advance left pointer past previous occurrence
    if (lastSeenIndex.has(char) && lastSeenIndex.get(char) >= left) {
      left = lastSeenIndex.get(char) + 1;
    }

    lastSeenIndex.set(char, right);
    const windowLength = right - left + 1;
    if (windowLength > maxLength) {
      maxLength = windowLength;
    }
  }

  return maxLength;
}
```

### Example 4: Move Zeroes to End In-Place (Fast & Slow Pointers)
```javascript
export function moveZeroes(nums) {
  let slow = 0;

  // Fast pointer scans for non-zero elements
  for (let fast = 0; fast < nums.length; fast++) {
    if (nums[fast] !== 0) {
      // Swap elements without allocating temporary variables
      const temp = nums[slow];
      nums[slow] = nums[fast];
      nums[fast] = temp;
      slow++;
    }
  }

  return nums;
}
```

### Example 5: Unicode-Aware String Reversal (Surrogate Pair Safety)
```javascript
export function reverseUnicodeString(str) {
  // Using Intl.Segmenter correctly handles grapheme clusters (emojis, accent marks)
  if (typeof Intl !== 'undefined' && Intl.Segmenter) {
    const segmenter = new Intl.Segmenter(undefined, { granularity: 'grapheme' });
    const segments = Array.from(segmenter.segment(str), (s) => s.segment);
    return segments.reverse().join('');
  }

  // Fallback using Array.from which respects 32-bit surrogate pairs (code points)
  return Array.from(str).reverse().join('');
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **Unicode Surrogate Pairs (The Emoji Trap)**:
   JavaScript strings use UTF-16 encoding. Standard ASCII characters occupy 1 code unit (16 bits). Characters above `U+FFFF` (such as emojis `😀` or ancient scripts) require **two 16-bit code units** (a surrogate pair).
   ```javascript
   const emoji = '😀';
   console.log(emoji.length); // 2! (not 1)
   console.log(emoji[0]);      // '\uD83D' (lone surrogate, invalid Unicode character)
   ```
   Iterating character-by-character using `for (let i = 0; i < str.length; i++)` splits surrogate pairs. Use `for...of` or `Array.from(str)` to iterate by true Unicode Code Points.
2. **Off-by-One Errors in Window Size**:
   A sliding window between indices `left` and `right` inclusive contains `right - left + 1` elements, not `right - left`.
3. **Empty or Degenerate Inputs**:
   Always guard against empty arrays, arrays where $K > \text{length}$, or strings containing only whitespace.
4. **Mutating Array Lengths During Iteration**:
   Altering `arr.length` or calling `splice()` while iterating forward causes the index counter to skip elements. Always iterate backwards if deleting in-place, or use a two-pointer write index.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Algorithmic Consequence | Production Remediation |
| :--- | :--- | :--- |
| `str.split('').reverse().join('')` on emojis | Corrupts Unicode surrogate pairs and multi-codepoint emojis. | Use `Array.from(str).reverse().join('')` or `Intl.Segmenter`. |
| Re-summing subarrays in fixed window | Causes $O(N \times K)$ quadratic execution time. | Add incoming element and subtract outgoing element ($O(1)$ update). |
| `arr.splice(i, 1)` inside linear loops | Shifts memory on every deletion, degrading to $O(N^2)$. | Use fast/slow two-pointer in-place compaction. |
| Using string indexing `str[i]` on UTF-16 surrogate pairs | Retrieves broken half-surrogates. | Use `s.codePointAt(i)` or iterator protocol `for...of`. |
| Allocating new arrays inside hot loops | Triggers heavy GC churn and memory fragmentation. | Mutate in-place using two pointers or pre-allocate typed buffers. |
| Missing boundary check on empty arrays in prefix sums | Throws index out-of-bounds or produces `NaN`. | Allocate prefix array of size $N + 1$ with $P[0] = 0$. |

---

## 8. Performance Characteristics

- **Time Complexity of Array Methods**:
  - `push()` / `pop()`: $O(1)$ amortized.
  - `shift()` / `unshift()`: $O(N)$ (requires shifting all memory elements).
  - `slice()`: $O(K)$ where $K$ is slice length (allocates new array).
  - `splice()`: $O(N)$ (shifts elements and mutates buffer).
- **Two-Pointer Efficiency**:
  Two-pointer algorithms visit each element at most twice (once by right pointer, once by left pointer), guaranteeing strict **$O(N)$** linear time and **$O(1)$** auxiliary space.

---

## 9. Engine Internals

### V8 Smi Representation & Array Buffering
- **Small Integers (Smi)**: In 64-bit V8, 31-bit signed integers are stored directly within the pointer value (tagged with a least-significant bit of 0), requiring no heap allocation. An array of pure integers (`PACKED_SMI_ELEMENTS`) is stored as a flat contiguous C++ primitive array.
- **TypedArray Direct Memory**: `Int32Array` or `Uint8Array` allocate contiguous blocks directly in linear memory with zero pointer tagging or hidden class overhead, delivering performance equivalent to C arrays.

---

## 10. Best Practices & Design Guidelines

1. **Use Two Pointers for In-Place Modifications**: When asked to filter or reorder an array in-place, always reach for the fast/slow two-pointer pattern.
2. **Use Sliding Windows for Contiguous Subsequences**: If a problem mentions "continuous subarray", "substring", or "window of size K", sliding window is almost always the optimal $O(N)$ solution.
3. **Use Prefix Sums for Repeated Range Queries**: If an immutable array receives thousands of range sum requests, compute a prefix sum array once in $O(N)$ and answer all queries in $O(1)$.
4. **Use `Intl.Segmenter` for Human Text Processing**: Guarantee correct user-visible grapheme handling across international character sets.

---

## 11. Real-World Case Study

### High-Frequency Stock Trading Volume Spike Monitor
**Problem**: A financial analytics engine monitored live stock trade events. The platform needed to detect whether the total trading volume in any 1-minute window exceeded $10,000,000$ shares. Incoming trade volume was emitted as an array of 60,000 second-by-second records.
**Naive Implementation**:
On every new incoming second, a script re-summed the prior 60 seconds using `slice(i - 60, i).reduce((a, b) => a + b, 0)`. At 1,000 stocks, this consumed 100% CPU on Node.js due to 60,000 slice and reduce calls per second ($O(N \times K)$).

**Optimized Sliding Window**:
Replaced with a fixed sliding window maintaining a running sum:
```javascript
// O(1) per incoming second
runningVolume += newSecondVolume - windowBuffer[headIndex];
windowBuffer[headIndex] = newSecondVolume;
headIndex = (headIndex + 1) % 60;
if (runningVolume > THRESHOLD) triggerAlert();
```
**Outcome**:
- CPU consumption dropped from 98% down to 2.1%.
- Processing latency per price tick dropped from $4.2\text{ms}$ to $0.003\text{ms}$.

---

## 12. Interview Questions

<details>
<summary>1. When should you choose a Two-Pointer approach over a Hash Map?</summary>

Two Pointers should be chosen when:
1. The array is already **sorted** (or can be sorted in $O(N \log N)$ without violating constraints).
2. Space complexity must be strictly **$O(1)$** auxiliary space (a Hash Map requires $O(N)$ auxiliary memory).
3. Operations involve finding pairs, reversing elements, or compacting data in-place without copying.
</details>

<details>
<summary>2. Why does <code>"👨‍👩‍👧‍👦".length</code> equal 11 in JavaScript?</summary>

The family emoji (`👨‍👩‍👧‍👦`) is a complex sequence composed of four distinct Unicode character code points (Man, Woman, Girl, Boy) joined together by three invisible **Zero-Width Joiner (ZWJ)** characters (`\u200D`). Each individual person emoji is represented by a UTF-16 surrogate pair (2 code units $\times 4 = 8$), plus three 1-unit ZWJ characters ($8 + 3 = 11$ total code units). Because JavaScript's `.length` counts UTF-16 code units rather than visual graphemes, it reports 11.
</details>

<details>
<summary>3. What is the time and space complexity of calculating range sums using a Prefix Sum array?</summary>

- **Pre-computation**: $O(N)$ time to build the prefix sum array and $O(N)$ space to store the prefix sums.
- **Query Phase**: $O(1)$ constant time for each subsequent range query ($\text{prefix}[R+1] - \text{prefix}[L]$) and $O(1)$ auxiliary space.
</details>

<details>
<summary>4. How does a Dynamic Sliding Window guarantee $O(N)$ time complexity if it contains a nested while loop?</summary>

Although the code contains an outer `for` loop and an inner `while` loop, the `right` pointer increments at most $N$ times, and the `left` pointer also increments at most $N$ times across the entire algorithm execution. Because each pointer visits each element at most once, the total number of operations is bounded by $2N$, yielding a strict asymptotic time complexity of **$O(N)$**.
</details>

<details>
<summary>5. What is the difference between <code>str.charCodeAt()</code> and <code>str.codePointAt()</code>?</summary>

- `str.charCodeAt(i)` returns the 16-bit integer representation of the single UTF-16 **code unit** at index $i$ ($0$ to $65,535$).
- `str.codePointAt(i)` returns the full Unicode **code point** (up to 21 bits, $0$ to $1,114,111$). If index $i$ points to the start of a surrogate pair, it automatically combines the high and low surrogates and returns the complete 32-bit code point value.
</details>

<details>
<summary>6. How do you detect a cycle in a linear sequence using Two Pointers?</summary>

Using **Floyd's Cycle-Finding Algorithm (Tortoise and Hare)**. Initialize a `slow` pointer moving one step at a time and a `fast` pointer moving two steps at a time. If a cycle exists, the fast pointer will eventually lap and meet the slow pointer within the cycle in $O(N)$ time and $O(1)$ space. If fast reaches the end (`null`), no cycle exists.
</details>

<details>
<summary>7. Why is <code>Array.from(str)</code> safer for reversing strings than <code>str.split('')</code>?</summary>

`str.split('')` splits by individual 16-bit UTF-16 code units. If the string contains emojis or multi-byte characters, their surrogate pairs are split apart and reversed into an invalid sequence (e.g. low surrogate before high surrogate), corrupting the text. `Array.from(str)` uses the string's iterator protocol, which correctly groups UTF-16 surrogate pairs into single code point elements.
</details>

<details>
<summary>8. How can you remove duplicates from a sorted array in-place with $O(1)$ auxiliary memory?</summary>

Use a two-pointer approach: maintain a `writeIndex = 1`. Iterate with `i = 1` through `nums.length - 1`. Whenever `nums[i] !== nums[i - 1]`, copy `nums[writeIndex++] = nums[i]`. Finally, truncate `nums.length = writeIndex`. This operates in $O(N)$ time and $O(1)$ memory without allocating secondary arrays.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Container With Most Water
**Specification**: Given an integer array `height` representing vertical lines on a graph, find two lines that together with the x-axis form a container containing the most water. Return the maximum area in $O(N)$ time.

<details>
<summary>Solution</summary>

```javascript
export function maxArea(height) {
  let left = 0;
  let right = height.length - 1;
  let maxWater = 0;

  while (left < right) {
    const width = right - left;
    const currentHeight = Math.min(height[left], height[right]);
    const area = width * currentHeight;

    if (area > maxWater) {
      maxWater = area;
    }

    // Move the pointer pointing to the shorter vertical line inward
    if (height[left] < height[right]) {
      left++;
    } else {
      right--;
    }
  }

  return maxWater;
}
```
</details>

### Exercise 2: Minimum Size Subarray Sum
**Specification**: Given an array of positive integers `nums` and a positive integer `target`, return the minimal length of a contiguous subarray whose sum is greater than or equal to `target`. If there is no such subarray, return `0`. Must run in $O(N)$ time.

<details>
<summary>Solution</summary>

```javascript
export function minSubArrayLen(target, nums) {
  let left = 0;
  let currentSum = 0;
  let minLength = Infinity;

  for (let right = 0; right < nums.length; right++) {
    currentSum += nums[right];

    // Shrink window from the left as long as currentSum >= target
    while (currentSum >= target) {
      const windowLen = right - left + 1;
      if (windowLen < minLength) {
        minLength = windowLen;
      }
      currentSum -= nums[left];
      left++;
    }
  }

  return minLength === Infinity ? 0 : minLength;
}
```
</details>
