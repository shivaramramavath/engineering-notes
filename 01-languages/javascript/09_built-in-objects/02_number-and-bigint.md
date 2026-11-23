# JavaScript Number & BigInt: IEEE-754, Precision Limits, Arbitrary Precision & Engine Internals

> Comprehensive technical reference on ECMAScript `Number` (IEEE-754 64-bit double), floating-point precision, `Number.EPSILON`, safe integer boundaries, `BigInt` arbitrary precision, and V8 Smi / HeapNumber architectures.

| Metadata | Details |
| :--- | :--- |
| **Level** | Core / Engine Internals |
| **Prerequisites** | [01_fundamentals/02_data-types.md](../01_fundamentals/02_data-types.md), [01_fundamentals/05_operators.md](../01_fundamentals/05_operators.md) |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **ECMAScript Baseline** | ES2015 (Number static methods, EPSILON) / ES2020 (BigInt) / ES2021 (Numeric Separators) |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts: IEEE-754 Double Precision & BigInt](#2-core-concepts-ieee-754-double-precision--bigint)
3. [How It Works: Bitwise Layouts & Limits](#3-how-it-works-bitwise-layouts--limits)
4. [Syntax & API Reference](#4-syntax--api-reference)
5. [Practical Examples](#5-practical-examples)
6. [Edge Cases, Pitfalls & Gotchas](#6-edge-cases-pitfalls--gotchas)
7. [Common Mistakes & Anti-Patterns](#7-common-mistakes--anti-patterns)
8. [Performance Characteristics](#8-performance-characteristics)
9. [Engine Internals: V8 Smi (Small Integer) Tagging & HeapNumber](#9-engine-internals-v8-smi-small-integer-tagging--heapnumber)
10. [Best Practices & Design Guidelines](#10-best-practices--design-guidelines)
11. [Real-World Case Study: Financial Ledger & Micro-Cents Accounting System](#11-real-world-case-study-financial-ledger--micro-cents-accounting-system)
12. [Interview Questions & Deep Dives](#12-interview-questions--deep-dives)
13. [Exercises & Challenges](#13-exercises--challenges)

---

## 1. What & Why

In JavaScript, all numbers historically belonged to a single primitive type: **`Number`**. Standardized as an **IEEE-754 64-bit double-precision binary floating-point value (binary64)**, this single type handles integers, fractions, scientific notation, and special sentinels (`Infinity`, `-Infinity`, `NaN`).

However, representing numbers in base-2 floating point creates two fundamental engineering challenges:
1. **Fractional Precision Errors:** Base-10 fractions (such as $0.1$ and $0.2$) have infinite recurring representations in base-2 binary, leading to infamous rounding discrepancies like `0.1 + 0.2 === 0.30000000000000004`.
2. **The Safe Integer Limit ($2^{53} - 1$):** Because binary64 allocates 53 bits to significand precision, integers beyond $9,007,199,254,740,991$ lose precision. Large Twitter/Discord 64-bit Snowflake IDs or database primary keys get corrupted when parsed as standard Numbers.

To solve the safe integer boundary without breaking backward compatibility, ECMAScript 2020 introduced **`BigInt`**: a true arbitrary-precision integer primitive capable of representing integers of unbounded size.

---

## 2. Core Concepts: IEEE-754 Double Precision & BigInt

### 2.1 The IEEE-754 64-Bit Binary Layout
A standard JavaScript `Number` occupies exactly 64 bits (8 bytes) structured as follows:

```
 1 bit         11 bits                       52 bits
+------+---------------------+----------------------------------------------------+
| Sign | Exponent (biased)   | Mantissa / Fraction (Significand)                  |
+------+---------------------+----------------------------------------------------+
  [S]         [E0 - E10]                          [F0 - F51]
```
- **Sign (1 bit):** `0` for positive, `1` for negative (including signed zero: `+0` and `-0`).
- **Exponent (11 bits):** Biased by $1023$, representing powers of two from $2^{-1022}$ to $2^{1023}$.
- **Fraction / Mantissa (52 bits):** Provides 53 bits of precision due to an implicit leading bit ($1.f$).

### 2.2 Numerical Constants & Boundaries
- `Number.MAX_SAFE_INTEGER` ($2^{53} - 1 = 9,007,199,254,740,991$): The largest integer that can be safely represented without loss of precision.
- `Number.MIN_SAFE_INTEGER` ($-(2^{53} - 1) = -9,007,199,254,740,991$).
- `Number.MAX_VALUE` ($\approx 1.7976931348623157 \times 10^{308}$): Largest positive representable float before overflowing to `Infinity`.
- `Number.MIN_VALUE` ($\approx 5 \times 10^{-324}$): Smallest positive non-zero denormalized float.
- `Number.EPSILON` ($2^{-52} \approx 2.220446049250313 \times 10^{-16}$): The difference between $1$ and the smallest floating point number greater than $1$.

### 2.3 BigInt: Arbitrary Precision Integers
A `BigInt` is created by appending an `n` suffix to an integer literal or calling `BigInt()`:
```javascript
const large = 9007199254740991n + 2n;
console.log(large); // 9007199254740993n (Exact precision preserved!)
console.log(typeof large); // "bigint"
```
**Key Invariant:** `BigInt` and `Number` **cannot be mixed implicitly** in arithmetic operations. Mixing them throws a `TypeError` to prevent unintentional precision truncation.

---

## 3. How It Works: Bitwise Layouts & Limits

### 3.1 Why $0.1 + 0.2 \neq 0.3$
In decimal (base 10), $\frac{1}{3}$ is $0.33333\dots$ (infinite recurring).
In binary (base 2), $\frac{1}{10}$ ($0.1$) is an infinite repeating sequence:
$$\text{Binary } 0.1 = 0.0001100110011001100110011001100110011001100110011001101\dots$$
Because the 64-bit float can only store 53 bits of mantissa, the number is rounded. When $0.1$ and $0.2$ are added, the sum of their truncated binary fractions yields:
$$0.3000000000000000444089209850062616169452667236328125$$
Hence, `0.1 + 0.2 === 0.3` evaluates to `false`.

### 3.2 Floating-Point Comparison via `Number.EPSILON`
To compare floats safely, test whether their absolute difference is less than machine epsilon:
```javascript
function areFloatsEqual(a, b, epsilon = Number.EPSILON) {
  return Math.abs(a - b) < epsilon;
}

console.log(areFloatsEqual(0.1 + 0.2, 0.3)); // true
```

---

## 4. Syntax & API Reference

### 4.1 Number Static Verification Methods
Modern ES2015 static methods on `Number` do **not** perform loose type coercion, unlike global functions:

```javascript
// Strict verification (ES2015):
Number.isNaN(NaN);        // true
Number.isNaN("NaN");      // false (strict!)
isNaN("NaN");             // true (legacy global coerces string to NaN!)

Number.isFinite(100);     // true
Number.isFinite("100");   // false (strict!)
isFinite("100");          // true (legacy global coerces string)

Number.isInteger(42);     // true
Number.isInteger(42.0);   // true (in IEEE-754, 42.0 is identical to 42)
Number.isInteger(42.5);   // false

Number.isSafeInteger(9007199254740991);  // true
Number.isSafeInteger(9007199254740992);  // false (outside safe integer range)
```

### 4.2 Number Formatting Methods
```javascript
const pi = 3.14159265;

console.log(pi.toFixed(2));       // "3.14" (returns string rounded to decimal places)
console.log(pi.toPrecision(4));   // "3.142" (returns string with 4 significant digits)
console.log((123456).toExponential(2)); // "1.23e+5"

// Localized formatting:
console.log((1234567.89).toLocaleString("de-DE")); // "1.234.567,89"
```

### 4.3 BigInt Static & Instance Methods
```javascript
const b = 100n;

// BigInt wrapping to 64-bit signed/unsigned (for binary protocols):
const signed = BigInt.asIntN(64, 0xFFFFFFFFFFFFFFFFn);
console.log(signed); // -1n

const unsigned = BigInt.asUintN(64, -1n);
console.log(unsigned); // 18446744073709551615n (0xFFFFFFFFFFFFFFFFn)
```

---

## 5. Practical Examples

### Example 1: Snowflake 64-Bit ID Extraction via BigInt
```javascript
// Discord / Twitter 64-bit Snowflake ID parser
class Snowflake {
  static EPOCH = 1420070400000n; // Discord epoch (2015-01-01)

  static parse(snowflakeString) {
    const id = BigInt(snowflakeString);
    const timestamp = (id >> 22n) + Snowflake.EPOCH;
    const workerId = (id >> 17n) & 0x1Fn;
    const processId = (id >> 12n) & 0x1Fn;
    const sequence = id & 0xFFFn;

    return {
      timestamp: new Date(Number(timestamp)),
      workerId: Number(workerId),
      processId: Number(processId),
      sequence: Number(sequence)
    };
  }
}

const parsed = Snowflake.parse("175928847299117063");
console.log(parsed.timestamp.toISOString()); // 2016-04-30T11:18:25.796Z
console.log("Worker:", parsed.workerId);     // 0
```

### Example 2: Safe High-Precision Currency Computation
```javascript
// Convert decimal dollars to integer cents/micros to avoid IEEE-754 drift
class Money {
  constructor(micros) {
    this.micros = BigInt(micros); // 1 USD = 1,000,000 micros
  }

  static fromDollars(dollars) {
    // Round to avoid float parsing errors
    const micros = Math.round(dollars * 1_000_000);
    return new Money(micros);
  }

  add(other) {
    return new Money(this.micros + other.micros);
  }

  multiply(quantity) {
    return new Money(this.micros * BigInt(quantity));
  }

  format() {
    const totalMicros = this.micros;
    const dollars = totalMicros / 1_000_000n;
    const remainder = totalMicros % 1_000_000n;
    const cents = remainder / 10_000n;
    return `$${dollars}.${cents.toString().padStart(2, "0")}`;
  }
}

const item = Money.fromDollars(19.99);
const tax = Money.fromDollars(1.60);
const total = item.add(tax).multiply(3);
console.log("Total invoice:", total.format()); // "$64.77"
```

### Example 3: 32-Bit Bitwise Truncation vs BigInt Bitwise
```javascript
// Number bitwise operators truncate to signed 32-bit int:
const hugeNum = 0x80000000; // 2147483648
console.log(hugeNum | 0);    // -2147483648 (Overflows 32-bit signed int!)

// BigInt preserves full arbitrary precision in bitwise ops:
const hugeBig = 0x80000000n;
console.log(hugeBig | 0n);   // 2147483648n (Exact!)
```

### Example 4: Distinguishing `+0`, `-0`, and `NaN`
```javascript
function inspectNumberSign(val) {
  if (Object.is(val, -0)) return "Negative Zero (-0)";
  if (Object.is(val, +0)) return "Positive Zero (+0)";
  if (Object.is(val, NaN)) return "Not-a-Number (NaN)";
  return val > 0 ? "Positive" : "Negative";
}

console.log(inspectNumberSign(-0)); // "Negative Zero (-0)"
console.log(inspectNumberSign(+0)); // "Positive Zero (+0)"
console.log(inspectNumberSign(0 / 0)); // "Not-a-Number (NaN)"

// Division by zero preserves sign:
console.log(1 / +0); // Infinity
console.log(1 / -0); // -Infinity
```

### Example 5: Custom JSON Serialization for BigInt
```javascript
// JSON.stringify natively throws TypeError on BigInt
const payload = {
  id: 1234567890123456789n,
  name: "Ledger-Record"
};

const jsonString = JSON.stringify(payload, (key, value) => {
  return typeof value === "bigint" ? value.toString() + "n" : value;
});

console.log("Serialized:", jsonString);
// Output: Serialized: {"id":"1234567890123456789n","name":"Ledger-Record"}

// Reviving BigInt on parse:
const parsed = JSON.parse(jsonString, (key, value) => {
  if (typeof value === "string" && /^\d+n$/.test(value)) {
    return BigInt(value.slice(0, -1));
  }
  return value;
});
console.log(parsed.id); // 1234567890123456789n
```

---

## 6. Edge Cases, Pitfalls & Gotchas

### 6.1 Mixing Number and BigInt Throws TypeError
Arithmetic operations between `Number` and `BigInt` do not perform implicit coercion:
```javascript
const num = 10;
const big = 20n;

// Throws TypeError: Cannot mix BigInt and other types:
// const sum = num + big;

// Explicit conversion required:
const sum = num + Number(big); // 30 (Potential precision loss if big > 2^53 - 1)
const bigSum = BigInt(num) + big; // 30n (Safe!)
```

### 6.2 Comparison Between Number and BigInt
Loose equality (`==`) coerces across types, but strict equality (`===`) checks type:
```javascript
console.log(0n == 0);   // true
console.log(0n === 0);  // false (bigint !== number)
console.log(1n < 2);    // true (relational operators work naturally)
```

### 6.3 Loss of Precision When Converting Large Strings to Number
Parsing a 64-bit integer string with `Number()` or `parseInt()` silently corrupts the value:
```javascript
const raw = "9007199254740993"; // MAX_SAFE_INTEGER + 2
console.log(Number(raw));       // 9007199254740992 (Corrupted!)
console.log(BigInt(raw));       // 9007199254740993n (Accurate)
```

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Bad Practice | Idiomatic Solution | Reason |
| :--- | :--- | :--- | :--- |
| **Direct Float Equality** | `if (total === 0.3)` | `if (Math.abs(total - 0.3) < Number.EPSILON)` | Floating-point rounding makes direct equality fail. |
| **Using `new Number()`** | `const n = new Number(42);` | `const n = 42;` or `Number("42")` | Instantiates a wrapper object; causes truthy bugs (`if (new Number(0))` is true). |
| **Using global `isNaN()`** | `isNaN("hello")` returns `true` | `Number.isNaN(val)` | Global `isNaN` coerces strings; `Number.isNaN` checks for actual `NaN` type. |
| **Parsing 64-bit IDs with `parseInt`** | `const id = parseInt(req.body.id);` | `const id = BigInt(req.body.id);` | Values $> 2^{53}-1$ lose precision silently. |
| **Bitwise Operations on Large Numbers** | `const rounded = num \| 0;` | `Math.trunc(num)` | Bitwise `\| 0` truncates numbers to 32 bits, destroying values $> 2^{31}-1$. |
| **Serializing BigInt with `JSON.stringify`** | `JSON.stringify({ id: 10n })` | Provide custom `toJSON` or replacer function | `JSON.stringify` throws `TypeError: Do not know how to serialize a BigInt`. |

---

## 8. Performance Characteristics

1. **V8 Smi Representation (Fast Path):**
   - Integers that fit into 31 bits (on 32-bit architectures) or 32 bits (on 64-bit architectures) are stored as **Smi (Small Integers)** directly inside the pointer register. They require $0$ bytes of heap memory and no garbage collection overhead.
2. **HeapNumber Allocation (Slow Path):**
   - Non-integer floating point numbers or numbers exceeding the Smi range are boxed in a heap-allocated `HeapNumber` object (~16–24 bytes).
3. **BigInt Allocation Overhead:**
   - A `BigInt` is always allocated on the heap as an array of 64-bit words (digits). Arithmetic on `BigInt` cannot use single-cycle CPU floating-point registers and is significantly slower than Smi math.

---

## 9. Engine Internals: V8 Smi (Small Integer) Tagging & HeapNumber

In 64-bit V8, pointers are tagged at the lowest bit to differentiate heap object pointers from immediate integers:

```
               64-Bit Word Representation in V8
+----------------------------------------------------------------+
| Value (32 bits)                       | 31 bits padding | Tag  |
+----------------------------------------------------------------+
                                                             |
                                            +----------------+
                                            |
                         Tag bit = 0: Smi (Small Integer)
                         Tag bit = 1: HeapObject Pointer
```

### The Smi Fast Path:
1. When a variable holds an integer within $[-2^{31}, 2^{31}-1]$, the least significant bit is set to `0`. The CPU executes integer addition (`ADD`) directly on the tagged register without dereferencing memory.
2. If an operation causes an integer overflow or introduces a decimal fraction, V8 automatically transitions the variable from a Smi to a pointer pointing to a newly allocated `HeapNumber`.
3. TurboFan JIT tracks type feedback. If an arithmetic loop stays strictly within Smi bounds, TurboFan generates raw assembly instructions without boxing.

---

## 10. Best Practices & Design Guidelines

1. **Store Financial Figures as Integers or BigInt:** Never store monetary amounts as floating-point dollars/euros. Store them in base currency units (cents, mills, or satoshis) using `BigInt`.
2. **Always Use `Number.isFinite()` and `Number.isNaN()`:** Replace all legacy calls to global `isNaN()` and `isFinite()` to avoid implicit string coercion bugs.
3. **Use Numeric Separators for Readability:** Write `1_000_000` instead of `1000000` to enhance code clarity.
4. **Use String Types for Transporting 64-Bit IDs:** In JSON REST APIs and GraphQL schemas, serialize 64-bit database IDs, snowflakes, and timestamps as Strings rather than Numbers.

---

## 11. Real-World Case Study: Financial Ledger & Micro-Cents Accounting System

### Problem
A subscription billing engine charged customers fractional usage costs. Summing floating-point fees (`0.0015 + 0.0035 + 0.0010`) resulted in an accumulated rounding discrepancy of $3,400 per month across 10 million transactions, causing compliance failures during external financial audits.

### Solution
Migrate the entire pricing calculation pipeline to a fixed-point `MicroCents` accounting model powered by `BigInt`:

```javascript
class LedgerEngine {
  // 1 Cent = 10,000 Micro-Cents (allowing calculations down to $0.000001)
  static SCALE = 10_000n;

  static toMicroCents(amountString) {
    const [dollars, cents = "00", fractionalCents = "00"] = amountString.split(/[.]/);
    const wholeCents = BigInt(dollars) * 100n + BigInt(cents.padEnd(2, "0").slice(0, 2));
    const micro = BigInt(fractionalCents.padEnd(4, "0").slice(0, 4));
    return wholeCents * LedgerEngine.SCALE + micro;
  }

  static formatCurrency(microCents) {
    const totalCents = microCents / LedgerEngine.SCALE;
    const remainder = microCents % LedgerEngine.SCALE;
    const dollars = totalCents / 100n;
    const cents = totalCents % 100n;
    return `$${dollars}.${cents.toString().padStart(2, "0")} (micro: ${remainder})`;
  }
}

// Processing thousands of micro-transactions:
const fees = [
  LedgerEngine.toMicroCents("0.0015"),
  LedgerEngine.toMicroCents("0.0035"),
  LedgerEngine.toMicroCents("0.0010"),
  LedgerEngine.toMicroCents("19.9900")
];

const totalBalance = fees.reduce((acc, fee) => acc + fee, 0n);

console.log("Raw micro-cents total:", totalBalance); // 20006000n
console.log("Audited statement:", LedgerEngine.formatCurrency(totalBalance));
// Output: "$20.00 (micro: 6000)" -- Exact to 6 decimal places, zero drift!
```

---

## 12. Interview Questions & Deep Dives

<details>
<summary><strong>Q1: Why does <code>Number.MAX_SAFE_INTEGER</code> equal $2^{53} - 1$ instead of $2^{64} - 1$?</strong></summary>

JavaScript uses IEEE-754 double-precision 64-bit binary floating-point numbers. Out of the 64 bits, 1 bit is dedicated to the sign and 11 bits to the exponent, leaving 52 bits for the mantissa (fraction). With the normalized implicit leading bit, the mantissa provides 53 bits of precision. Therefore, $2^{53} - 1$ is the largest integer that can be uniquely distinguished from its adjacent values.
</details>

<details>
<summary><strong>Q2: What is the difference between <code>Number.isNaN(val)</code> and global <code>isNaN(val)</code>?</strong></summary>

Global `isNaN(val)` first coerces its argument to a Number using the internal `ToNumber` abstract operation. If `val` is `"hello"`, it coerces to `NaN`, and `isNaN("hello")` returns `true`. In contrast, `Number.isNaN(val)` strictly verifies that `typeof val === "number"` and that the value is specifically `NaN`. `Number.isNaN("hello")` returns `false`.
</details>

<details>
<summary><strong>Q3: How does JavaScript differentiate between <code>+0</code> and <code>-0</code>, and how can you detect it?</strong></summary>

Under IEEE-754, zero has a sign bit: `0` (positive) and `1` (negative). While `+0 === -0` evaluates to `true`, they produce different results in division: `1 / +0 === Infinity`, whereas `1 / -0 === -Infinity`. They can be reliably differentiated using `Object.is(+0, -0)`, which returns `false`.
</details>

<details>
<summary><strong>Q4: Why does <code>JSON.stringify()</code> throw a TypeError when given a BigInt?</strong></summary>

The JSON standard (RFC 8259) does not define an arbitrary-precision integer type; all numbers in JSON are parsed according to standard JSON numeric grammar. Serializing a `BigInt` into a JSON number would risk silent precision corruption when consumed by parsers adhering to 64-bit float limits. To prevent data corruption, TC39 mandated that `JSON.stringify` throws by default.
</details>

<details>
<summary><strong>Q5: What are V8 Small Integers (Smis), and what is their performance benefit?</strong></summary>

Smis are 31-bit or 32-bit signed integers stored directly inside pointer-sized registers using pointer tagging (lowest bit set to 0). Because Smis are not allocated on the garbage-collected heap, operations on Smis execute with near-native CPU register efficiency without causing GC pressure.
</details>

<details>
<summary><strong>Q6: How do bitwise operators like <code>\|</code>, <code>&</code>, and <code>~</code> behave on standard Numbers?</strong></summary>

When applied to Numbers, JavaScript bitwise operators convert their operands to 32-bit signed integers via the `ToInt32` abstract operation. Any fractional parts are discarded, and values exceeding 32 bits wrap around using two's complement arithmetic (e.g., `0x80000000 \| 0` evaluates to `-2147483648`).
</details>

<details>
<summary><strong>Q7: What is <code>Number.EPSILON</code>, and how is it used in production?</strong></summary>

`Number.EPSILON` ($2^{-52} \approx 2.22 \times 10^{-16}$) represents the smallest measurable difference between 1 and the next representable 64-bit float. It is used as a tolerance threshold when comparing floating-point calculations to avoid false negatives caused by rounding errors (`Math.abs(a - b) < Number.EPSILON`).
</details>

<details>
<summary><strong>Q8: What happens when you perform integer division with BigInt?</strong></summary>

Division on `BigInt` using the `/` operator truncates fractions towards zero (equivalent to `Math.trunc()`), exactly like integer division in C/C++ or Java: `5n / 2n === 2n` and `-5n / 2n === -2n`.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Safe Floating-Point Precision Summation
Write a function `sumFloats(...numbers)` that adds an arbitrary list of floating-point numbers without precision drift up to 10 decimal places.

<details>
<summary><strong>Solution</strong></summary>

```javascript
function sumFloats(...numbers) {
  const FACTOR = 1e10;
  const intSum = numbers.reduce((acc, n) => {
    return acc + Math.round(n * FACTOR);
  }, 0);
  return intSum / FACTOR;
}

console.log(0.1 + 0.2); // 0.30000000000000004
console.log(sumFloats(0.1, 0.2)); // 0.3
console.log(sumFloats(1.005, 2.005, 3.005)); // 6.015
```
</details>

### Exercise 2: Arbitrary Precision Factorial with BigInt
Implement a factorial function that calculates $N!$ for large values of $N$ (e.g., $50!$) without integer overflow.

<details>
<summary><strong>Solution</strong></summary>

```javascript
function factorial(n) {
  let result = 1n;
  const limit = BigInt(n);
  for (let i = 2n; i <= limit; i++) {
    result *= i;
  }
  return result;
}

console.log(factorial(20).toString()); 
// 2432902008176640000

console.log(factorial(50).toString());
// 30414093201713378043612608166064768844377641568960512000000000000
```
</details>

### Exercise 3: Safe Parsing of Large JSON Numbers
Write a JSON parser preprocessor that wraps numbers larger than `Number.MAX_SAFE_INTEGER` in quotes so they can be parsed as `BigInt` without corruption.

<details>
<summary><strong>Solution</strong></summary>

```javascript
function parseLargeJson(rawJson) {
  // Regex matches unquoted numbers with 16 or more digits:
  const preprocessed = rawJson.replace(/:\s*(-?\d{16,})/g, ': "$1"');
  
  return JSON.parse(preprocessed, (key, value) => {
    if (typeof value === "string" && /^-?\d{16,}$/.test(value)) {
      return BigInt(value);
    }
    return value;
  });
}

const raw = '{"userId": 9007199254740999, "count": 42}';
const parsed = parseLargeJson(raw);

console.log(typeof parsed.userId); // "bigint"
console.log(parsed.userId === 9007199254740999n); // true
console.log(typeof parsed.count);  // "number"
```
</details>
