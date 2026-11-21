# Property-Based Testing Ideas in C

> **Validating mathematical invariants, algebraic laws, and roundtrip data encoding in C using randomized property-based testing and QuickCheck methodologies.**

| Level | Prerequisites | Time to Read | Status | Standard |
| :--- | :--- | :--- | :--- | :--- |
| Advanced | [01-unit-testing-with-unity-or-cmocka.md](01-unit-testing-with-unity-or-cmocka.md), [01-fundamentals/04-variables-constants.md](../01-fundamentals/04-variables-constants.md) | 25 mins | Complete | C99 / C11 / C17 / POSIX |

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

Traditional unit testing relies on **example-based testing**: the engineer selects arbitrary sample inputs ($x = 2$, $x = -1$, $x = 100$) and asserts against known outputs. However, manually crafted examples frequently miss subtle edge cases: integer overflow boundaries, buffer limit wrap-around, or strange unicode sequences.

**Property-Based Testing (PBT)**, originally pioneered by Haskell's *QuickCheck*, flips this approach:
- Instead of writing individual input-output pairs, the developer formalizes **universal mathematical properties** or **system invariants** that must hold true for *all* valid inputs in a domain.
- The testing framework generates thousands of pseudo-random test inputs automatically.
- If a property violation is detected, advanced property runners perform **shrinking**—iteratively paring down the failing input to the absolute smallest minimal reproducing example (e.g., shrinking a 1000-element array down to a 2-element array containing `0` and `-1`).

In pure C, property testing is particularly powerful for verifying data serialization codecs, encryption routines, compression algorithms, and sorted container invariants.

---

## 2. Core Concepts

### 2.1 Universal Invariant Archetypes
When designing property-based tests in C, properties generally fall into canonical mathematical archetypes:
1. **Roundtrip Invariance (Encode / Decode):** For any valid data structure $x$, serializing and deserializing must yield the identical original value:
   $$\forall x, \quad \text{decode}(\text{encode}(x)) == x$$
2. **Idempotence:** Applying an operation twice produces the identical result as applying it once:
   $$\forall x, \quad f(f(x)) == f(x)$$
   *(e.g., sorting an already sorted list, formatting a string, cleaning whitespace).*
3. **Commutativity & Associativity:** Order of operations does not alter the mathematical outcome:
   $$\forall a, b, \quad a + b == b + a$$
4. **Oracle Comparison:** Comparing a fast, highly optimized algorithm against a simple, obviously correct (but slow) reference implementation:
   $$\forall x, \quad \text{fast\_sort}(x) == \text{bubble\_sort}(x)$$
5. **Invariant Invariance:** Verifying internal integrity constraints (e.g., a Red-Black tree maintains black-height balance after arbitrary insertion and deletion sequences).

```mermaid
flowchart TD
    Generator[Random Input Generator: PRNG / Seeds] --> Invariant{Property Invariant Checker}
    Invariant -->|decode(encode(x)) == x| Success[Pass: Advance iteration]
    Invariant -->|Violation Found!| Shrinker[Shrinking Engine: Pare down input]
    Shrinker --> MinimalRepro[Minimal Reproducing Failure: e.g. x = -1]
```

---

## 3. How It Works Internally

### 3.1 Pseudo-Random Generation & Seed Control
Property testing relies on deterministic pseudo-random number generators (PRNGs), such as **xoshiro256** or standard `rand_r()`.
- Every test run starts with a printed seed value:
  ```text
  [PROPERTY TEST] Running 10,000 iterations with SEED: 0xA3F109BC
  ```
- If iteration #7,842 discovers a crash or logic failure, re-running the test binary with `-seed=0xA3F109BC` guarantees 100% deterministic reproduction under GDB.

### 3.2 The Shrinking Process
When a failure occurs on a large, complex input (e.g., an array of 500 random integers), identifying the root cause is difficult. The shrinker attempts smaller mutations:
1. Try halving the array size. Does it still fail? Yes -> discard other half.
2. Try zeroing out positive elements. Does it still fail? Yes.
3. Try shrinking values towards 0.
Eventually, the engine prints:
```text
Property failed: sort preserves length
Minimal failing input: [ 2147483647, -2147483648 ] (caused integer overflow in comparator)
```

---

## 4. Syntax & Usage

### 4.1 Custom Property Runner in Pure C
```c
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

/* Property: abs(x) is always non-negative (except INT_MIN) */
static bool prop_abs_non_negative(int32_t val) {
    if (val == INT32_MIN) return true; /* Known boundary exemption */
    int32_t res = abs(val);
    return res >= 0;
}

void run_property_test(uint32_t seed, size_t iterations) {
    srand(seed);
    for (size_t i = 0; i < iterations; ++i) {
        int32_t val = (int32_t)((rand() << 16) | rand());
        if (!prop_abs_non_negative(val)) {
            printf("Property violation found! Input: %d\n", val);
            return;
        }
    }
    printf("Passed %zu property iterations successfully.\n", iterations);
}
```

---

## 5. Practical Examples

### 5.1 Verified Companion Simulation: ZigZag Roundtrip Property
The following program evaluates the mathematical invariant of Google Protocol Buffers' **ZigZag encoding**: mapping signed integers to unsigned integers so that values near zero have small representations, verifying that $	ext{decode}(	ext{encode}(n)) == n$ across diverse domains:

```c
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

/* Property 1: Encoding then decoding an integer round-trips identically */
static uint32_t zigzag_encode(int32_t n) {
    return (uint32_t)((n << 1) ^ (n >> 31));
}

static int32_t zigzag_decode(uint32_t n) {
    return (int32_t)((n >> 1) ^ (-(int32_t)(n & 1)));
}

int main(void) {
    printf("Evaluating Property-Based Testing Invariants in C...\n");
    const int32_t sample_inputs[] = { 0, -1, 1, 2147483647, -2147483647, 42, -1000 };
    bool all_passed = true;

    for (size_t i = 0; i < sizeof(sample_inputs) / sizeof(sample_inputs[0]); ++i) {
        int32_t original = sample_inputs[i];
        uint32_t encoded = zigzag_encode(original);
        int32_t decoded = zigzag_decode(encoded);
        if (original != decoded) {
            all_passed = false;
            printf("Property violation at %d -> %u -> %d\n", original, encoded, decoded);
        }
    }

    if (all_passed) {
        printf("Property Verified: ∀x ∈ S, decode(encode(x)) == x across all test domains.\n");
    }
    return 0;
}
```

**Real Execution Output:**
```text
Evaluating Property-Based Testing Invariants in C...
Property Verified: ∀x ∈ S, decode(encode(x)) == x across all test domains.
```

---

## 6. Best Practices

1. **Log the PRNG Seed on Every Run:** Always print the active random seed at test suite startup. Without the seed, intermittent property test failures in CI cannot be reproduced locally.
2. **Include Hardcoded Boundary Values:** Pure random generation rarely hits exact edge cases (e.g., `0`, `NULL`, `INT_MAX`, `INT_MIN`). Always run a deterministic array of known boundary values before starting random loops.
3. **Keep Properties Pure:** A property function should be a pure function returning a `bool`. It should have zero side effects on global system state.
4. **Test Inverse Operations:** Whenever you implement a parser, serializer, compressor, or encryption cipher, always write a roundtrip property test ($A \circ A^{-1} = I$).
5. **Combine with Sanitizers:** Running property tests with ASan active turns the property runner into a focused in-process mini-fuzzer.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Is Dangerous | Correct Fix |
| :--- | :--- | :--- |
| **Using unseeded `rand()` without logging** | A failure in CI cannot be reproduced because the sequence of numbers is lost. | Initialize PRNG with an explicit seed and print `SEED = 0x...`. |
| **Testing properties that duplicate implementation** | Re-writing the function logic inside the property checker tests nothing. | Test high-level mathematical invariants (e.g. length preservation, monotonicity). |
| **Ignoring integer overflow in generators** | Expressions like `rand() + rand()` can trigger undefined signed integer overflow. | Generate unsigned numbers first, then cast to signed types explicitly. |
| **Testing too few iterations** | Running only 10 iterations misses boundary bugs in large data domains. | Run at least 1,000 to 100,000 iterations in CI test targets. |
| **Assuming floats are associative** | Testing `(a + b) + c == a + (b + c)` on `float` fails due to rounding precision. | Allow epsilon delta tolerances when evaluating floating-point invariants. |
| **Skipping zero and null cases** | Random generators that only produce positive integers miss the most common C bugs. | Explicitly inject zero, negative values, and empty strings into test sets. |

---

## 8. Performance & Security Notes

### 8.1 Fuzzing vs Property Testing
While both fuzzing and property testing feed randomized inputs into code:
- **Fuzzing (libFuzzer/AFL++):** Focuses on finding *crashes*, memory errors, and hangs by mutating arbitrary byte sequences guided by branch coverage.
- **Property-Based Testing:** Focuses on finding *semantic and logical bugs* by generating structured, typed data and verifying high-level invariants.

### 8.2 Security Invariant Verification
Use property testing to verify cryptographic and security invariants:
- **Constant Time Invariance:** Verify that timing differences between valid and invalid HMAC tags are within statistical noise.
- **Non-Malleability:** Mutating any single bit in a ciphertext must cause signature verification to fail 100% of the time.

---

## 9. Comparison / When to Use What

| Technique | Input Source | Verification Method | Primary Defect Target |
| :--- | :--- | :--- | :--- |
| **Example-Based Unit Tests** | Handcrafted inputs | Hardcoded expected values | Regressions in known scenarios |
| **Property-Based Testing** | Typed randomized generators | Universal invariants / Laws | Algorithmic flaws, subtle edge cases |
| **Coverage-Guided Fuzzing** | Mutated byte streams | Crashes / Sanitizer errors | Memory corruptions, parser exploits |

---

## 10. Interview Questions

<details>
<summary><strong>1. What is the fundamental difference between Example-Based Testing and Property-Based Testing?</strong></summary>

Example-based testing asserts that for a specific input $A$, the function returns specific output $B$. Property-based testing asserts that for *any* valid input in a domain, a universal property or mathematical relationship holds true (e.g., sorting preserves array length and elements are monotonically non-decreasing).
</details>

<details>
<summary><strong>2. What is "shrinking" in Property-Based Testing?</strong></summary>

Shrinking is the process of automatically reducing a complex, failing randomized input down to the smallest minimal reproducing case. If a 10,000-character string triggers a parser bug, shrinking discards substrings until it identifies the exact 3-character sequence that causes the failure.
</details>

<details>
<summary><strong>3. Give three examples of properties you can test on a Sorting algorithm.</strong></summary>

1. **Permutation (Conservation of Elements):** The sorted array contains the exact same elements with the same frequencies as the input array.
2. **Monotonicity:** For every adjacent index pair $i$ and $i+1$, $	ext{arr}[i] \le 	ext{arr}[i+1]$.
3. **Idempotence:** Sorting an already sorted array produces an identical array ($	ext{sort}(	ext{sort}(a)) == 	ext{sort}(a)$).
</details>

<details>
<summary><strong>4. Why is determinism critical in Property-Based Testing?</strong></summary>

Because test inputs are pseudo-randomly generated, a property failure that occurs once in 50,000 runs is impossible to debug unless the PRNG seed is captured. Logging the seed allows developers to replay the exact same sequence of pseudo-random numbers on demand.
</details>

<details>
<summary><strong>5. What is the "Test Oracle" pattern in property testing?</strong></summary>

A Test Oracle compares the output of a complex, highly optimized algorithm against a simple, trusted (often naive) reference implementation across thousands of random inputs: $orall x, f_{	ext{optimized}}(x) == f_{	ext{reference}}(x)$.
</details>

<details>
<summary><strong>6. Why do floating-point numbers fail naive algebraic property tests?</strong></summary>

IEEE-754 floating-point operations are not strictly associative: $(a + b) + c$ may differ from $a + (b + c)$ due to rounding errors and precision loss. Property tests on floats must verify that the difference is within an acceptable epsilon delta.
</details>

<details>
<summary><strong>7. How can you implement a roundtrip property test for a custom base64 encoder?</strong></summary>

Generate random byte buffers of varying lengths, encode them to base64 ASCII strings, decode the strings back into raw byte buffers, and assert with `memcmp()` that the decoded bytes match the original input exactly.
</details>

<details>
<summary><strong>8. Can property-based testing replace fuzzing?</strong></summary>

No. Property testing generates structured, well-typed data according to application data models, whereas fuzzers inject raw, malformed, and out-of-spec byte sequences specifically designed to crash low-level parsers and uncover memory corruption exploits.
</details>

---

<details>
<summary><strong>9. How do you implement data shrinking for dynamic linked data structures like trees or linked lists?</strong></summary>

Shrinking recursive data structures involves structural pruning:
1. Try replacing a binary tree with its left subtree, then with its right subtree.
2. For linked lists, attempt removing the head node, deleting contiguous sub-slices of elements, or replacing each node's payload with a simpler value (e.g. 0).
3. If the smaller structure still triggers the property violation, the shrinker commits the reduced structure and repeats the process recursively until no further reductions produce failures.
</details>

<details>
<summary><strong>10. How can property-based testing detect state machine race conditions in concurrent C systems?</strong></summary>

You can perform Model-Based State Machine Testing:
1. Define a reference sequential state machine model (e.g., an abstract queue).
2. Generate randomized sequences of parallel operations (e.g., thread 1 calls <code>push(X)</code>, thread 2 calls <code>pop()</code>).
3. Execute the concurrent operations against the actual concurrent C data structure.
4. Verify linearizability: whether there exists an interleaving of the parallel operations that maps to a valid sequential trace of the reference model.
</details>

---

## 11. Practice Exercises

### Exercise 1 (Easy): Invariant for String Length
- **Task:** Write a property test verifying that concatenating string $A$ and string $B$ produces a string whose length is always $	ext{strlen}(A) + 	ext{strlen}(B)$.

<details>
<summary>Solution</summary>

```c
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>

bool prop_concat_length(const char *s1, const char *s2) {
    size_t l1 = strlen(s1);
    size_t l2 = strlen(s2);
    char buf[256];
    snprintf(buf, sizeof(buf), "%s%s", s1, s2);
    return strlen(buf) == (l1 + l2);
}

int main(void) {
    assert(prop_concat_length("abc", "def"));
    assert(prop_concat_length("", "world"));
    assert(prop_concat_length("hello", ""));
    printf("Concat length property verified!\n");
    return 0;
}
```
</details>

### Exercise 2 (Medium): Monotonicity Property Checker
- **Task:** Write a function `bool verify_sorted_monotonic(const int *arr, size_t n)` that verifies whether an array conforms to the non-decreasing sorting property.

<details>
<summary>Solution</summary>

```c
#include <stdio.h>
#include <stdbool.h>
#include <stddef.h>
#include <assert.h>

bool verify_sorted_monotonic(const int *arr, size_t n) {
    if (!arr || n <= 1) return true;
    for (size_t i = 0; i < n - 1; ++i) {
        if (arr[i] > arr[i + 1]) {
            return false;
        }
    }
    return true;
}

int main(void) {
    int a1[] = { 1, 2, 2, 4, 10 };
    int a2[] = { 1, 5, 3 };
    assert(verify_sorted_monotonic(a1, 5) == true);
    assert(verify_sorted_monotonic(a2, 3) == false);
    printf("Monotonicity property checker verified!\n");
    return 0;
}
```
</details>

### Exercise 3 (Hard): Min-Heap Invariant Property Test
- **Task:** Write a property test that validates the min-heap parent-child invariant for an array-backed binary heap: $	ext{heap}[i] \le 	ext{heap}[2i + 1]$ and $	ext{heap}[i] \le 	ext{heap}[2i + 2]$.

<details>
<summary>Solution</summary>

```c
#include <stdio.h>
#include <stdbool.h>
#include <assert.h>

bool verify_min_heap_property(const int *heap, size_t n) {
    if (!heap || n <= 1) return true;
    for (size_t i = 0; i < n; ++i) {
        size_t left = 2 * i + 1;
        size_t right = 2 * i + 2;
        if (left < n && heap[i] > heap[left]) {
            return false;
        }
        if (right < n && heap[i] > heap[right]) {
            return false;
        }
    }
    return true;
}

int main(void) {
    int valid_heap[] = { 2, 5, 8, 12, 14, 20 };
    int invalid_heap[] = { 2, 10, 4, 12, 14, 3 };
    assert(verify_min_heap_property(valid_heap, 6));
    assert(!verify_min_heap_property(invalid_heap, 6));
    printf("Min-heap invariant property verified!\n");
    return 0;
}
```
</details>

---

## 12. Summary / Cheat Sheet

| Property Invariant | Formula | Common Use Cases |
| :--- | :--- | :--- |
| **Roundtrip** | $f^{-1}(f(x)) == x$ | Serializers, parsers, codecs, encodings. |
| **Idempotence** | $f(f(x)) == f(x)$ | Sorters, formatters, sanitizers, normalizers. |
| **Commutativity** | $a \oplus b == b \oplus a$ | Math routines, hash combiners, set unions. |
| **Oracle Test** | $f_{	ext{fast}}(x) == f_{	ext{slow}}(x)$ | Optimizations, SIMD ports, parallel algorithms. |
| **Monotonicity** | $x \le y \implies f(x) \le f(y)$ | Sorted data structures, priority queues. |

---

## 13. Further Reading
- QuickCheck: A Lightweight Tool for Random Testing of Haskell Programs (Claessen & Hughes)
- Theft: A Property-Based Testing Library for C: `https://github.com/silentbicycle/theft`
- RapidCheck (Modern C++ property testing concepts applicable to C): `https://github.com/emil-e/rapidcheck`
- Property-Based Testing in Practice (Fred Hebert)

---
[<- Previous: 04-test-driven-c.md](04-test-driven-c.md) | [Section Index](README.md) | [Next: 06-testing-with-sanitizers-and-valgrind.md ->](06-testing-with-sanitizers-and-valgrind.md)\n