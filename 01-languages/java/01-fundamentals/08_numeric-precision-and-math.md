# Numeric Precision, Floating-Point IEEE 754, and Arbitrary-Precision Math

> Hardware-level numerical computation in Java involves two's complement integer overflow, IEEE 754 floating-point rounding anomalies, and exact arbitrary-precision decimal modeling via `BigDecimal` and `BigInteger`.

| Metadata | Value |
| :--- | :--- |
| **Level** | Intermediate |
| **Prerequisites** | [01_variables-and-data-types.md](file:///E:/Learning/engineering-notes/01-languages/java/01-fundamentals/01_variables-and-data-types.md) |
| **Time to Read** | 30 minutes |
| **Status** | Complete |
| **Java LTS Baseline** | Java 25 LTS (with `Math.*Exact` and modern `BigDecimal`) |

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

### Problem Statement
Computers operate on discrete physical registers (32-bit and 64-bit words). Consequently, computer arithmetic is not equivalent to real-world mathematics:
- Finite integer ranges wrap around upon overflow ($2^{31}-1 + 1 = -2^{31}$).
- Decimal fractions (such as $0.1$ or $0.2$) cannot be represented accurately in binary base-2 floating-point notation.
- Rounding errors accumulate across repeated calculations, creating discrepancies in financial ledgers, tax withholdings, scientific simulations, and cryptographic hashing.

### Historical Context & Java's Design
Java introduced explicit guarantees for mathematical reproducibility:
- **Strictly Specified Bit Widths:** Unlike C/C++ (where `int` or `long` varies between 16, 32, or 64 bits depending on the compiler and operating system), Java mandates exact bit widths across all hardware.
- **Safe Integer Arithmetic (Java 8+):** Added `Math.addExact()`, `Math.multiplyExact()`, and `Math.toIntExact()` which verify overflow and throw `ArithmeticException`.
- **Arbitrary-Precision Standard Library:** Built-in `java.math.BigDecimal` and `java.math.BigInteger` provide exact mathematical decimal and integer calculations to any required precision.

### Real-World Relevance
Neglecting numeric precision causes devastating bugs: financial calculations losing fractions of cents on currency exchanges, flight trajectory tracking drift, or integer overflows permitting unauthorized overdrafts in banking software.

---

## 2. Core Concepts

### Integer Overflow & Two's Complement
All signed integers in Java (`byte`, `short`, `int`, `long`) use binary **two's complement** representation:
- The most significant bit (MSB) acts as the sign bit (`0` = positive, `1` = negative).
- In two's complement, there is one more negative number than positive numbers (e.g., `int` ranges from $-2,147,483,648$ to $+2,147,483,647$).
- **The Absolute Value Trap:** `Math.abs(Integer.MIN_VALUE)` returns $-2,147,483,648$ because $+2,147,483,648$ cannot fit in a 32-bit signed integer!

### IEEE 754 Floating-Point Standard
`float` (32-bit) and `double` (64-bit) conform strictly to IEEE 754:

```
64-bit double:
[ Sign: 1 bit ] [ Biased Exponent: 11 bits ] [ Significand / Mantissa: 52 bits ]
```
- **Special Values:**
  - `Double.NaN`: "Not a Number" (result of $0.0 / 0.0$ or $\sqrt{-1}$). `NaN == NaN` evaluates to `false`!
  - `Double.POSITIVE_INFINITY` ($1.0 / 0.0$) and `Double.NEGATIVE_INFINITY` ($-1.0 / 0.0$).
  - Negative zero (`-0.0`): Distinct bit representation from `+0.0`, though `-0.0 == +0.0` evaluates to `true`.

### Arbitrary-Precision Math (`BigDecimal` & `BigInteger`)
- **`BigInteger`:** Supports arbitrarily large signed integers, limited only by available JVM heap RAM.
- **`BigDecimal`:** Represents immutable arbitrary-precision signed decimal numbers. Composed of an unscaled `BigInteger` and a 32-bit integer `scale`:
  $$\text{Value} = \text{unscaledValue} \times 10^{-\text{scale}}$$

---

## 3. How It Works Internally

### Why $0.1 + 0.2 \ne 0.3$ in Binary Floating-Point
In base 10, a fraction can be represented with terminating decimals only if its denominator's prime factors are $2$ and $5$ (e.g., $1/2 = 0.5$, $1/4 = 0.25$, $1/10 = 0.1$). Fractions like $1/3 = 0.3333\dots$ repeat indefinitely.

In binary (base 2), a fraction can terminate only if its denominator is a power of 2 ($1/2$, $1/4$, $1/8$, $1/16$). The number $0.1$ ($1/10$) in binary is an infinite repeating fraction:
$$0.0001100110011001100110011\dots_2$$
When stored in a 64-bit `double`, the mantissa is rounded at 52 bits. Adding the rounded approximations of $0.1$ and $0.2$ produces:
`0.30000000000000004`

### `BigDecimal` Memory Internals
`BigDecimal` achieves exact representation by avoiding binary floating point altogether:
- For small numbers (up to 18 digits), `BigDecimal` stores the unscaled value in a primitive 64-bit `long compactVal` field, bypassing object allocations.
- For numbers exceeding 18 digits, it allocates a `BigInteger` object (storing an `int[]` magnitude array).
- The `scale` represents how many digits appear to the right of the decimal point:
  - `BigDecimal.valueOf(12345, 2)` represents $123.45$ (unscaled value = $12345$, scale = $2$).

### `Math` vs. `StrictMath`
- **`java.lang.Math`:** Delegates transcendental functions (`sin`, `cos`, `log`, `pow`) to native hardware instructions (x87 FPU or AVX instructions). Results may vary by 1–2 Ulp (Units in the Last Place) across different CPU architectures.
- **`java.lang.StrictMath`:** Guarantees bit-for-bit identical results on every computer by executing standard software algorithms ported from the OpenBSD `fdlibm` library, sacrificing slight performance for 100% mathematical determinism.

---

## 4. Syntax & Usage

### Safe Arithmetic with `Math.*Exact`
```java
// Throws ArithmeticException instead of silent overflow:
int safeSum = Math.addExact(Integer.MAX_VALUE - 10, 5);
int safeProduct = Math.multiplyExact(100_000, 50_000);
int safeCast = Math.toIntExact(999_999_999_999L); // Throws if long exceeds int
```

### Instantiating `BigDecimal`
```java
// CRITICAL: Always use String constructor or BigDecimal.valueOf()
BigDecimal exact1 = new BigDecimal("0.1");     // EXACT: 0.1
BigDecimal exact2 = BigDecimal.valueOf(0.1);   // EXACT: 0.1 (calls Double.toString)

// ANTI-PATTERN: Passes inexact IEEE-754 double into constructor!
BigDecimal flawed = new BigDecimal(0.1);       // FLAWED: 0.1000000000000000055511...
```

### Rounding Modes in Division
When dividing with `BigDecimal`, you **must** specify a scale and a `RoundingMode` if the quotient is a non-terminating repeating decimal (like $1 / 3$):

```java
BigDecimal one = BigDecimal.ONE;
BigDecimal three = BigDecimal.valueOf(3);

// THROWS ArithmeticException: "Non-terminating decimal expansion"
// BigDecimal bad = one.divide(three);

// IDIOMATIC: Specify scale and RoundingMode
BigDecimal good = one.divide(three, 4, RoundingMode.HALF_EVEN); // 0.3333
```

---

## 5. Practical Examples

### Example 1: Floating-Point Precision Trap vs. `BigDecimal`
- **Goal:** Compare standard double arithmetic with `BigDecimal` on classic financial addition.
```java
import java.math.BigDecimal;

public class PrecisionComparison {
    public static void main(String[] args) {
        // Primitive double math:
        double d1 = 0.1;
        double d2 = 0.2;
        System.out.println("double (0.1 + 0.2): " + (d1 + d2));
        System.out.println("double == 0.3:      " + ((d1 + d2) == 0.3));

        // Exact BigDecimal math:
        BigDecimal b1 = BigDecimal.valueOf(0.1);
        BigDecimal b2 = BigDecimal.valueOf(0.2);
        BigDecimal bSum = b1.add(b2);
        System.out.println("BigDecimal sum:     " + bSum);
        System.out.println("BigDecimal == 0.3:  " + bSum.equals(BigDecimal.valueOf(0.3)));
    }
}
```
- **Console Output:**
```text
double (0.1 + 0.2): 0.30000000000000004
double == 0.3:      false
BigDecimal sum:     0.3
BigDecimal == 0.3:  true
```
- **Explanation:** Demonstrates that `0.1 + 0.2 == 0.3` evaluates to `false` with primitives, but evaluates accurately with `BigDecimal`.

---

### Example 2: Financial Rounding with `RoundingMode.HALF_EVEN` (Banker's Rounding)
- **Goal:** Apply Banker's Rounding to eliminate statistical accumulation bias in repeated calculations.
```java
import java.math.BigDecimal;
import java.math.RoundingMode;

public class BankersRoundingDemo {
    public static void main(String[] args) {
        // Banker's Rounding rounds toward the nearest even neighbor when equidistant
        BigDecimal val1 = new BigDecimal("2.5");
        BigDecimal val2 = new BigDecimal("3.5");

        System.out.println("2.5 rounded HALF_EVEN: " + val1.setScale(0, RoundingMode.HALF_EVEN)); // 2
        System.out.println("3.5 rounded HALF_EVEN: " + val2.setScale(0, RoundingMode.HALF_EVEN)); // 4
    }
}
```
- **Console Output:**
```text
2.5 rounded HALF_EVEN: 2
3.5 rounded HALF_EVEN: 4
```
- **Explanation:** In standard rounding (`HALF_UP`), $.5$ always rounds up, biasing financial balances upward over millions of transactions. Banker's rounding (`HALF_EVEN`) rounds to the nearest even integer, balancing upward and downward rounding statistically.

---

### Example 3: Edge Case — The `Math.abs(Integer.MIN_VALUE)` Trap
- **Goal:** Demonstrate that the absolute value of the minimum integer remains negative.
```java
public class MathAbsTrap {
    public static void main(String[] args) {
        int min = Integer.MIN_VALUE; // -2,147,483,648
        int absMin = Math.abs(min);  // Still -2,147,483,648!

        System.out.println("Integer.MIN_VALUE:          " + min);
        System.out.println("Math.abs(Integer.MIN_VALUE): " + absMin);
        System.out.println("Is still negative?          " + (absMin < 0));

        // SAFE DEFENSE: Use Math.absExact() (Java 15+)
        try {
            Math.absExact(min);
        } catch (ArithmeticException e) {
            System.out.println("CAUGHT: Math.absExact threw overflow exception!");
        }
    }
}
```
- **Console Output:**
```text
Integer.MIN_VALUE:          -2147483648
Math.abs(Integer.MIN_VALUE): -2147483648
Is still negative?          true
CAUGHT: Math.absExact threw overflow exception!
```
- **Explanation:** In two's complement, $-2^{31}$ has no positive 32-bit counterpart ($2^{31}-1$ is the maximum positive `int`). `Math.abs(min)` overflows and returns the exact same negative number.

---

### Example 4: Failure Case — `BigDecimal.equals()` Scale Sensitivity
- **Goal:** Demonstrate why `.equals()` is dangerous for comparing monetary `BigDecimal` values.
```java
import java.math.BigDecimal;

public class BigDecimalEqualsPitfall {
    public static void main(String[] args) {
        BigDecimal a = new BigDecimal("2.0");
        BigDecimal b = new BigDecimal("2.00");

        // PITFALL: .equals() compares both numeric value AND scale!
        System.out.println("a.equals(b):     " + a.equals(b)); // false! (Scale 1 vs Scale 2)

        // IDIOMATIC: compareTo() compares purely mathematical value
        System.out.println("a.compareTo(b):  " + (a.compareTo(b) == 0)); // true!
    }
}
```
- **Console Output:**
```text
a.equals(b):     false
a.compareTo(b):  true
```
- **Remedy:** Never use `.equals()` to check if two `BigDecimal` instances represent the same number. Always use `a.compareTo(b) == 0`.

---

### Example 5: High-Performance / Modern Idiom — Scaled Long Financial Math
- **Goal:** Implement high-speed currency math using a scaled `long` (storing cents) to avoid heap allocation overhead while maintaining 100% precision.
```java
public class ScaledLongCurrency {
    public static long calculateDiscountedPrice(long priceCents, int discountPercent) {
        // Multiply first, then divide by 100 with rounding
        long discountCents = Math.round((double)(priceCents * discountPercent) / 100.0);
        return priceCents - discountCents;
    }

    public static void main(String[] args) {
        long price = 4999; // $49.99
        long finalPrice = calculateDiscountedPrice(price, 15); // 15% off

        System.out.println("Original:   $" + (price / 100.0));
        System.out.println("Discounted: $" + (finalPrice / 100.0));
    }
}
```
- **Console Output:**
```text
Original:   $49.99
Discounted: $42.49
```
- **Explanation:** High-frequency trading engines and point-of-sale architectures represent money as fixed-point integers (`long` cents or micros) directly in CPU registers, achieving zero heap allocations and sub-nanosecond calculation latency.

---

## 6. Best Practices

1. **Always Use `new BigDecimal("...")` or `BigDecimal.valueOf(double)`:** Never use `new BigDecimal(double)` because it locks in the binary floating-point representation error.
2. **Always Use `compareTo()` to Compare `BigDecimal` Values:** `a.compareTo(b) == 0` ignores scale discrepancies (`2.0` vs `2.00`).
3. **Always Specify Scale and RoundingMode on Division:** Dividing without explicit rounding will throw runtime `ArithmeticException` whenever an unresolvable fraction (like $1/3$ or $1/7$) occurs.
4. **Use `Math.*Exact()` in Critical Business Calculations:** Protect against silent integer wrapping in ledger and billing computations.
5. **Favor Scaled `long` in High-Throughput Scenarios:** In performance-critical systems processing millions of transactions per second, store currency in minor units (cents, satoshis) using primitive `long`.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| **Using `double` for Money** | Accumulates binary floating-point rounding errors ($0.1 + 0.2 \ne 0.3$). | Use `BigDecimal` or scaled `long` (cents). |
| **Calling `new BigDecimal(0.1)`** | Passes inexact binary float to constructor ($0.10000000000000000555...$). | Use `BigDecimal.valueOf(0.1)` or `new BigDecimal("0.1")`. |
| **Using `a.equals(b)` on `BigDecimal`** | Fails if scales differ (`2.0` does not equal `2.00`). | Use `a.compareTo(b) == 0`. |
| **Dividing `BigDecimal` without RoundingMode** | Non-terminating decimal expansions throw runtime `ArithmeticException`. | Always supply scale and `RoundingMode.HALF_EVEN`. |
| **Assuming `Math.abs()` is Always Positive** | `Math.abs(Integer.MIN_VALUE)` overflows and remains negative. | Use `Math.absExact()` or handle `MIN_VALUE` explicitly. |
| **Comparing `NaN` with `==`** | IEEE 754 mandates `NaN == NaN` is always `false`. | Use `Double.isNaN(val)`. |

---

## 8. Performance & Security Notes

### Garbage Collection Pressure of `BigDecimal`
Every `BigDecimal` operation (`add`, `multiply`, `divide`) instantiates a new immutable `BigDecimal` object on the heap. In tight loops processing millions of records, this creates substantial young-generation garbage collection churn. Pre-allocate common constants (`BigDecimal.ZERO`, `BigDecimal.ONE`, `BigDecimal.TEN`) or use scaled `long` fixed-point representations.

### Deterministic Cryptographic Math
In cryptographic and security verification routines, variable-latency floating point or branchy math can expose side-channel timing attacks. Use constant-time integer operations and `BigInteger` methods designed for cryptographic security (such as `BigInteger.modPow()`).

---

## 9. Comparison / When to Use What

| Metric / Dimension | `double` (Primitive) | `BigDecimal` | Scaled `long` (Fixed-point) | `BigInteger` |
| :--- | :--- | :--- | :--- | :--- |
| **Precision** | 15–17 decimal digits | **Arbitrary / Exact** | Exact up to $9 \times 10^{18}$ | **Arbitrary / Exact** |
| **Allocation** | Stack / Registers ($0$ bytes) | Heap Object (~40 bytes) | Stack / Registers ($0$ bytes) | Heap Object (~32+ bytes) |
| **Performance** | Native CPU speed (sub-ns) | Slower (Software math) | Native CPU speed (sub-ns) | Slower (Software math) |
| **Rounding Control** | Hardware truncation | Complete (`RoundingMode`) | Manual rounding | N/A (Integers only) |
| **Ideal Scenario** | Physics, 3D games, metrics | Invoicing, taxation, banking | High-frequency trading, POS | Cryptography (RSA keys) |

---

## 10. Interview Questions

<details>
<summary><strong>1. Why is `0.1 + 0.2 != 0.3` in Java, and how does the IEEE 754 specification explain this?</strong></summary>

**Answer:**  
In binary base-2, numbers are represented as sums of powers of two ($1/2, 1/4, 1/8, \dots$). A fraction terminates only if its denominator is a power of 2. Decimal $0.1$ ($1/10$) and $0.2$ ($1/5$) are infinite repeating fractions in binary. Because a 64-bit `double` allocates only 52 bits to the mantissa, the values are truncated. When added, the cumulative rounding error results in `0.30000000000000004`, which fails an exact equality check against `0.3`.
</details>

<details>
<summary><strong>2. What is the fundamental difference between `new BigDecimal(0.1)` and `BigDecimal.valueOf(0.1)`?</strong></summary>

**Answer:**  
`new BigDecimal(0.1)` receives the already-inexact IEEE 754 binary floating-point representation of `0.1` and converts that exact bit pattern into an unscaled decimal value: `0.1000000000000000055511151231257827021181583404541015625`. `BigDecimal.valueOf(0.1)` internally calls `Double.toString(0.1)`, which uses canonical string formatting to produce `"0.1"`, constructing an exact decimal representation matching the human-intended number.
</details>

<details>
<summary><strong>3. Why does `new BigDecimal("2.0").equals(new BigDecimal("2.00"))` return `false`, and how should equality be evaluated?</strong></summary>

**Answer:**  
`BigDecimal.equals()` checks both numerical value **and scale**. `"2.0"` has a scale of 1, while `"2.00"` has a scale of 2. Because their scales differ, `.equals()` returns `false`. To compare `BigDecimal` values for mathematical equivalence regardless of scale or trailing zeros, you must use `a.compareTo(b) == 0`.
</details>

<details>
<summary><strong>4. Why does `Math.abs(Integer.MIN_VALUE)` return a negative number?</strong></summary>

**Answer:**  
In 32-bit signed two's complement integer representation:
- Minimum value: $-2^{31} = -2,147,483,648$
- Maximum value: $2^{31} - 1 = +2,147,483,647$
The positive counterpart $+2,147,483,648$ cannot fit in a 32-bit signed `int`. Negating `-2147483648` (inverting bits and adding 1) results in `0x80000000`, which is `-2147483648`. Therefore, `Math.abs(Integer.MIN_VALUE)` overflows and returns the exact same negative number.
</details>

<details>
<summary><strong>5. What is Banker's Rounding (`RoundingMode.HALF_EVEN`), and why is it mandated in financial standards?</strong></summary>

**Answer:**  
In standard rounding (`HALF_UP`), equidistant fractions (ending in $.5$) always round up. In high-volume financial accounting, this introduces a systematic upward cumulative bias. Banker's Rounding (`HALF_EVEN`) rounds equidistant numbers toward the nearest **even** neighbor (e.g., $2.5 \to 2$ and $3.5 \to 4$). Statistically, half of the equidistant numbers round down and half round up, canceling out rounding bias across millions of transactions.
</details>

<details>
<summary><strong>6. What happens if you divide two `BigDecimal` numbers without specifying a `RoundingMode`?</strong></summary>

**Answer:**  
If the division produces a terminating decimal expansion (such as $1 / 2 = 0.5$ or $1 / 4 = 0.25$), the division succeeds. However, if the division produces an infinite repeating non-terminating decimal (such as $1 / 3 = 0.3333\dots$), the JVM has no way to know where to stop and throws: `java.lang.ArithmeticException: Non-terminating decimal expansion; no exact representable decimal result`.
</details>

<details>
<summary><strong>7. What is the difference between `java.lang.Math` and `java.lang.StrictMath`?</strong></summary>

**Answer:**  
`Math` delegates transcendental functions (`sin`, `cos`, `pow`) to the host CPU's hardware floating-point instructions or vendor-optimized libraries for maximum execution speed, which may cause minor variations in the last 1–2 Ulps across different hardware architectures. `StrictMath` executes standardized software algorithms derived from `fdlibm`, guaranteeing 100% bit-for-bit identical floating-point results across every operating system and CPU architecture.
</details>

<details>
<summary><strong>8. How does `Double.NaN` behave in relational and equality operations?</strong></summary>

**Answer:**  
According to IEEE 754, `NaN` (Not-a-Number) is unordered. Any relational comparison (`<`, `>`, `<=`, `>=`) involving `NaN` evaluates to `false`. Crucially, `NaN == NaN` also evaluates to `false`. To check if a variable contains NaN, you must call the static method `Double.isNaN(val)`.
</details>

---

## 11. Practice Exercises

### Exercise 1: Safe Integer Multiplication Overflow Check (Easy)
- **Task:** Write a method `boolean isMultiplicationSafe(int a, int b)` that determines whether multiplying `a` and `b` will fit in a 32-bit `int` without throwing an uncaught exception.
<details>
<summary>Solution</summary>

```java
public class SafeMultiplication {
    public static boolean isMultiplicationSafe(int a, int b) {
        try {
            Math.multiplyExact(a, b);
            return true;
        } catch (ArithmeticException e) {
            return false;
        }
    }

    public static void main(String[] args) {
        System.out.println("100 * 200: " + isMultiplicationSafe(100, 200));       // true
        System.out.println("100000 * 50000: " + isMultiplicationSafe(100000, 50000)); // false
    }
}
```
</details>

### Exercise 2: Invoice Tax and Total Calculation with `BigDecimal` (Medium)
- **Task:** Implement a method `BigDecimal computeInvoiceTotal(BigDecimal subtotal, BigDecimal taxRate, BigDecimal discount)` that applies the discount, adds sales tax, and rounds to 2 decimal places using Banker's Rounding (`HALF_EVEN`).
<details>
<summary>Solution</summary>

```java
import java.math.BigDecimal;
import java.math.RoundingMode;

public class InvoiceCalculator {
    public static BigDecimal computeInvoiceTotal(BigDecimal subtotal, BigDecimal taxRate, BigDecimal discount) {
        // Net subtotal after discount
        BigDecimal net = subtotal.subtract(discount);

        // Tax calculation: net * taxRate
        BigDecimal tax = net.multiply(taxRate);

        // Final total rounded to 2 decimal places (cents)
        return net.add(tax).setScale(2, RoundingMode.HALF_EVEN);
    }

    public static void main(String[] args) {
        BigDecimal subtotal = new BigDecimal("100.00");
        BigDecimal taxRate = new BigDecimal("0.0825"); // 8.25%
        BigDecimal discount = new BigDecimal("15.50");

        BigDecimal total = computeInvoiceTotal(subtotal, taxRate, discount);
        System.out.println("Final Invoice Total: $" + total); // $91.47
    }
}
```
</details>

### Exercise 3: High-Performance Fixed-Point Currency Class (Hard)
- **Task:** Implement an immutable `Money` record that stores currency as a 64-bit scaled integer (micros: $1 \text{ dollar} = 1,000,000 \text{ micros}$) to deliver high-performance zero-allocation financial arithmetic.
<details>
<summary>Solution</summary>

```java
public record Money(long micros) {
    public static final long SCALE = 1_000_000L;

    public static Money ofDollars(double dollars) {
        return new Money(Math.round(dollars * SCALE));
    }

    public Money add(Money other) {
        return new Money(Math.addExact(this.micros, other.micros));
    }

    public Money multiply(double factor) {
        return new Money(Math.round(this.micros * factor));
    }

    public String toFormattedString() {
        long dollars = micros / SCALE;
        long cents = Math.abs((micros % SCALE) / 10_000);
        return String.format("$%d.%02d", dollars, cents);
    }

    public static void main(String[] args) {
        Money m1 = Money.ofDollars(19.99);
        Money m2 = Money.ofDollars(5.50);
        Money total = m1.add(m2);
        Money taxed = total.multiply(1.0825);

        System.out.println("Subtotal: " + total.toFormattedString()); // $25.49
        System.out.println("Total:    " + taxed.toFormattedString()); // $27.59
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **Two's Complement Overflow:** Standard integer math wraps silently; use `Math.addExact()` / `Math.multiplyExact()`.
- **Absolute Value Trap:** `Math.abs(Integer.MIN_VALUE)` returns `Integer.MIN_VALUE`.
- **Floating-Point Inexactness:** IEEE 754 cannot represent $0.1$ exactly in binary base-2; never use `double` for currency.
- **`BigDecimal` Instantiation:** Use `BigDecimal.valueOf(0.1)` or `new BigDecimal("0.1")`. Never use `new BigDecimal(0.1)`.
- **`BigDecimal` Equality:** Use `a.compareTo(b) == 0`. Never use `.equals()`.
- **`BigDecimal` Division:** Always specify scale and `RoundingMode.HALF_EVEN` to prevent `ArithmeticException`.

---

## 13. Further Reading

- [The Java Language Specification (Java SE 25 Edition) - Floating-Point Types and Operations](https://docs.oracle.com/javase/specs/jls/se25/html/jls-4.html#jls-4.2.3)
- [What Every Computer Scientist Should Know About Floating-Point Arithmetic by David Goldberg](https://docs.oracle.com/cd/E19957-01/806-3568/ncg_goldberg.html)
- [Official Java SE 25 BigDecimal API Documentation](https://docs.oracle.com/en/java/javase/25/docs/api/java.base/java/math/BigDecimal.html)

---

| [← Previous: 07_arrays.md](file:///E:/Learning/engineering-notes/01-languages/java/01-fundamentals/07_arrays.md) | [Section Index](file:///E:/Learning/engineering-notes/01-languages/java/01-fundamentals/README.md) | [Next: 09_console-input-output.md](file:///E:/Learning/engineering-notes/01-languages/java/01-fundamentals/09_console-input-output.md) → |
