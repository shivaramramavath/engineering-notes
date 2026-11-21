# Operators, Precedence, and Evaluation Rules in Java

> Java operators manipulate primitives and object references through rigorous mathematical, logical, bitwise, and relational contracts, governed by strict precedence tables, binary numeric promotion, and short-circuit evaluation semantics.

| Metadata | Value |
| :--- | :--- |
| **Level** | Beginner to Intermediate |
| **Prerequisites** | [01_variables-and-data-types.md](file:///E:/Learning/engineering-notes/01-languages/java/01-fundamentals/01_variables-and-data-types.md) |
| **Time to Read** | 30 minutes |
| **Status** | Complete |
| **Java LTS Baseline** | Java 25 LTS (with Pattern Matching `instanceof`) |

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
In any programming language, computations require operators to combine values, compare states, branch logic, and manipulate individual bits. Without deterministic rules for operator precedence, associativity, short-circuit branching, and numeric promotions, complex mathematical expressions would produce erratic, compiler-dependent results.

### Historical Context & Design Principles
Java borrowed much of its operator syntax from C and C++, but deliberately removed undefined and platform-dependent behaviors:
- **No Undefined Evaluation Order:** In C/C++, expressions like `f() + g()` or `i = i++` have undefined evaluation order. In Java, operands are strictly evaluated from **left to right**.
- **No Pointer Arithmetic:** Operators like `++`, `--`, `+`, `-` cannot be applied to object references.
- **Strict Short-Circuiting:** Logical `&&` and `||` guarantee that the right-hand operand is never evaluated if the left-hand operand determines the outcome.
- **Pattern Matching `instanceof` (Java 16+ / 21+):** Evolved `instanceof` from a simple type query into a declarative pattern matching operator that automatically casts and binds variables.

### Real-World Relevance
Flaws in operator understanding lead to severe production incidents: null pointer exceptions due to eager `&` instead of short-circuit `&&`, infinite loops from silent integer overflow in `for` loop updates, and security authorization bypasses caused by bitwise operator precedence traps.

---

## 2. Core Concepts

### Operator Precedence Hierarchy (Top to Bottom)

```
Level 1 (Highest):  Postfix Operators        [expr]++  [expr]--
Level 2:            Unary Operators          ++[expr]  --[expr]  +[expr]  -[expr]  ~  !  (type)
Level 3:            Multiplicative           *   /   %
Level 4:            Additive                 +   -   (and String concatenation +)
Level 5:            Shift                    <<   >>   >>>
Level 6:            Relational               <   >   <=   >=   instanceof
Level 7:            Equality                 ==   !=
Level 8:            Bitwise AND              &
Level 9:            Bitwise XOR              ^
Level 10:           Bitwise OR               |
Level 11:           Logical AND              &&  (Short-circuit)
Level 12:           Logical OR               ||  (Short-circuit)
Level 13:           Ternary Conditional      ? :
Level 14 (Lowest):  Assignment Operators     =   +=   -=   *=   /=   %=   &=   ^=   |=   <<=   >>=   >>>=
```

### Key Operator Categories

#### 1. Arithmetic Operators (`+`, `-`, `*`, `/`, `%`)
- **Integer Division:** When both operands are integers, `/` truncates any fractional part toward zero ($7 / 2 = 3$, $-7 / 2 = -3$).
- **Remainder Formula:** The remainder operator `%` obeys the identity:  
  $$(a / b) * b + (a \% b) == a$$  
  Consequently, the sign of `a % b` strictly matches the sign of the dividend `a` (e.g., `-7 % 3 = -1`, while `7 % -3 = 1`).

#### 2. Relational and Pattern Matching (`<`, `>`, `<=`, `>=`, `==`, `!=`, `instanceof`)
- `==` checks primitive value equality or reference identity (same memory address).
- `instanceof` checks if an object is an instance of a specified class or interface.
- **Pattern Matching `instanceof`:** `if (obj instanceof String s)` tests type and binds `s` in the true branch scope.

#### 3. Logical Operators (`&&`, `||`, `!`)
- `&&` and `||` evaluate the left operand first. If `false && expr`, `expr` is skipped. If `true || expr`, `expr` is skipped.
- Non-short-circuit bitwise boolean operators `&` and `|` always evaluate both sides unconditionally.

#### 4. Bitwise and Shift Operators (`&`, `|`, `^`, `~`, `<<`, `>>`, `>>>`)
- `&` (AND), `|` (OR), `^` (XOR), `~` (Bitwise Complement).
- `<<` (Signed Left Shift): Shifts bits left, filling low-order bits with `0`. Equivalent to multiplying by $2^n$.
- `>>` (Signed Right Shift): Shifts bits right, preserving the sign bit (arithmetic shift).
- `>>>` (Unsigned Right Shift): Shifts bits right, filling high-order bits with `0` regardless of sign.

---

## 3. How It Works Internally

### The Shift Distance Masking Rule
A common pitfall is assuming that shifting an `int` by 32 bits clears it to `0`. In the JVM specification:
- For 32-bit `int`, only the lowest 5 bits of the shift distance are used (`shift % 32`). Therefore, `x << 32` is identical to `x << 0` (no shift!).
- For 64-bit `long`, only the lowest 6 bits are used (`shift % 64`).

```
Expression: 1 << 35
Binary of 35: 0010_0011 (Lowest 5 bits = 00011 = 3 in decimal)
Actual Operation: 1 << 3 = 8
```

### Binary Numeric Promotion
When evaluating binary arithmetic operators (`+`, `-`, `*`, `/`, `%`), the JVM enforces **Binary Numeric Promotion**:
1. If either operand is `double`, the other is converted to `double`.
2. Otherwise, if either operand is `float`, the other is converted to `float`.
3. Otherwise, if either operand is `long`, the other is converted to `long`.
4. **Otherwise, both operands are promoted to `int`!**

```java
short a = 5;
short b = 10;
// short c = a + b; // COMPILE ERROR! (a + b) promotes to int!
short c = (short)(a + b); // Requires explicit cast
```

### The Hidden Cast in Compound Assignments
Why does `s += 1` compile when `s = s + 1` fails?
According to the Java Language Specification (JLS §15.26.2), compound assignment operators (`E1 op= E2`) automatically inject an implicit narrowing cast:
$$E_1 = (T)(E_1 \text{ op } E_2)$$
Where $T$ is the type of $E_1$.

```java
short s = 32767;
s += 1; // Compiles as: s = (short)(s + 1); Wraps silently to -32768!
```

---

## 4. Syntax & Usage

### Increment and Decrement: Pre vs. Post
- **Prefix (`++i`, `--i`):** Increments the variable first, then evaluates to the new value.
- **Postfix (`i++`, `i--`):** Evaluates to the original value first, then increments the variable.

```java
int i = 5;
int a = ++i; // i becomes 6, a receives 6
int b = i++; // b receives 6, i becomes 7
```

### Short-Circuit Null Defense
Short-circuit evaluation is the standard idiom for preventing `NullPointerException`:

```java
if (user != null && user.isActive() && user.getBalance() > 0) {
    // user.isActive() is never called if user is null!
    processPayment(user);
}
```

---

## 5. Practical Examples

### Example 1: Arithmetic & Remainder Subtleties
- **Goal:** Demonstrate integer division truncation, negative remainder signs, and IEEE-754 floating-point remainder.
```java
public class ArithmeticDemo {
    public static void main(String[] args) {
        System.out.println("7 / 2:    " + (7 / 2));     // 3 (truncated)
        System.out.println("-7 / 2:   " + (-7 / 2));    // -3
        System.out.println("-7 % 3:   " + (-7 % 3));    // -1 (matches sign of -7)
        System.out.println("7 % -3:   " + (7 % -3));    // 1  (matches sign of 7)
        System.out.println("5.5 % 2:  " + (5.5 % 2));   // 1.5 (floating remainder)
    }
}
```
- **Console Output:**
```text
7 / 2:    3
-7 / 2:   -3
-7 % 3:   -1
7 % -3:   1
5.5 % 2:  1.5
```
- **Explanation:** Demonstrates that `%` in Java is a remainder operator, not a mathematical modulo operator (which is always non-negative). For true mathematical modulo, use `Math.floorMod(a, b)`.

---

### Example 2: Short-Circuit vs. Eager Evaluation Side Effects
- **Goal:** Demonstrate that eager operators (`&`, `|`) execute side-effecting methods, whereas short-circuit operators (`&&`, `||`) skip them.
```java
public class ShortCircuitDemo {
    private static boolean check(String tag, boolean value) {
        System.out.println("Evaluated: " + tag);
        return value;
    }

    public static void main(String[] args) {
        System.out.println("--- Short-Circuit (&&) ---");
        boolean sc = check("A", false) && check("B", true); // B is skipped

        System.out.println("--- Eager (&) ---");
        boolean eager = check("C", false) & check("D", true); // D is executed!
    }
}
```
- **Console Output:**
```text
--- Short-Circuit (&&) ---
Evaluated: A
--- Eager (&) ---
Evaluated: C
Evaluated: D
```
- **Explanation:** `check("B")` was never invoked because `check("A")` returned `false`. In contrast, the eager `&` operator forced execution of `check("D")`.

---

### Example 3: Edge Case — Bitwise Shift Masking & Silent Wrap
- **Goal:** Verify shift distance masking and compound assignment overflow.
```java
public class OperatorSubtleties {
    public static void main(String[] args) {
        // Shift masking (32 % 32 = 0; 35 % 32 = 3)
        System.out.println("1 << 32: " + (1 << 32));
        System.out.println("1 << 35: " + (1 << 35));

        // Negative remainder
        System.out.println("-7 % 3:  " + (-7 % 3));
        System.out.println("7 % -3:  " + (7 % -3));

        // Compound assignment implicit narrowing
        short s = 32767;
        s += 1; // Wraps silently to -32768
        System.out.println("short overflow via +=: " + s);

        // Pattern matching instanceof
        Object obj = "Hello Java 25";
        if (obj instanceof String str && str.length() > 5) {
            System.out.println("Pattern matched: " + str.toUpperCase());
        }
    }
}
```
- **Console Output:**
```text
1 << 32: 1
1 << 35: 8
-7 % 3:  -1
7 % -3:  1
short overflow via +=: -32768
Pattern matched: HELLO JAVA 25
```
- **Explanation:** Confirms that `1 << 32` equals `1` (not `0`), and that compound assignment `+=` silently narrows types without compiler warning.

---

### Example 4: Failure Case — The Ternary Unboxing NullPointerException
- **Goal:** Demonstrate how mixing primitive and boxed types in a ternary expression causes an insidious runtime crash.
```java
public class TernaryUnboxingCrash {
    public static void main(String[] args) {
        Integer boxedValue = null;
        boolean condition = false;

        // DANGER: Ternary unboxing rule (JLS §15.25):
        // If one operand is double and the other is Double, the result is double (primitive).
        // If condition is false, boxedValue is unboxed to double -> NullPointerException!
        try {
            double result = condition ? 1.0 : boxedValue;
        } catch (NullPointerException e) {
            System.out.println("CRASH: Caught unexpected NPE from ternary unboxing!");
        }
    }
}
```
- **Console Output:**
```text
CRASH: Caught unexpected NPE from ternary unboxing!
```
- **Remedy:** Never mix primitive and wrapper types across the second and third branches of a ternary expression when the wrapper can be `null`. Use explicit `if-else` blocks or `Optional`.

---

### Example 5: High-Performance / Modern Idiom — Bitmask Permissions System
- **Goal:** Implement a high-performance bitmask flags system using bitwise operators for compact security permission checking.
```java
public class BitmaskPermissions {
    public static final int READ    = 1 << 0; // 0001 (1)
    public static final int WRITE   = 1 << 1; // 0010 (2)
    public static final int EXECUTE = 1 << 2; // 0100 (4)
    public static final int DELETE  = 1 << 3; // 1000 (8)

    public static void main(String[] args) {
        int userPermissions = READ | WRITE; // Grant READ and WRITE (0011 = 3)

        System.out.println("Can Read:    " + ((userPermissions & READ) != 0));
        System.out.println("Can Execute: " + ((userPermissions & EXECUTE) != 0));

        // Grant EXECUTE dynamically
        userPermissions |= EXECUTE; // 0111 = 7
        System.out.println("After grant, Can Execute: " + ((userPermissions & EXECUTE) != 0));

        // Revoke WRITE dynamically
        userPermissions &= ~WRITE;  // 0101 = 5
        System.out.println("After revoke, Can Write:  " + ((userPermissions & WRITE) != 0));
    }
}
```
- **Console Output:**
```text
Can Read:    true
Can Execute: false
After grant, Can Execute: true
After revoke, Can Write:  false
```
- **Explanation:** Bitwise masks allow packing up to 32 independent boolean flags into a single 4-byte `int` register. Checking, adding, and revoking permissions executes in a single CPU clock cycle (`and`, `or`, `xor`).

---

## 6. Best Practices

1. **Use Parentheses to Clarify Precedence:** Even if you know operator precedence by heart, write `(a & b) != 0` instead of `a & b != 0` (which fails compilation because `!=` has higher precedence than `&`).
2. **Always Prefer `&&` and `||` for Boolean Logic:** Only use `&` and `|` when bitwise integer operations are explicitly intended.
3. **Use `Math.floorMod()` for Array Indexing:** When wrapping negative indices cyclically, `index % array.length` can produce a negative result. `Math.floorMod(index, array.length)` is guaranteed to return a positive index.
4. **Beware of Post-Increment in Self-Assignment:** Writing `i = i++;` does **not** increment `i`. The original value of `i` is evaluated and assigned back, overwriting the increment!
5. **Leverage Pattern Matching `instanceof`:** Replace ugly boilerplate casts:
   ```java
   // Modern Java 16+ / 21+ / 25:
   if (obj instanceof Order order) {
       order.process();
   }
   ```

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| **`a & b == 0` without parentheses** | `==` has higher precedence than `&`; evaluates as `a & (b == 0)`, causing compile error. | Wrap bitwise operations in parentheses: `(a & b) == 0`. |
| **Writing `i = i++`** | Postfix evaluates original value first, and the assignment overwrites the increment. | Write `i++;` or `++i;` as a standalone statement. |
| **Mixing wrappers in ternary expressions** | Triggers unexpected auto-unboxing, throwing `NullPointerException`. | Match wrapper types on both sides of `:`. |
| **Assuming `1 << 32` equals `0`** | JVM masks shift distance by 31 for `int`; `1 << 32` is equivalent to `1 << 0` ($1$). | Cast to `long` first: `1L << 32`. |
| **Using `==` to compare Strings** | Compares object reference addresses instead of character content. | Always use `.equals()` or `.equalsIgnoreCase()`. |
| **Accidental assignment in conditional `if (b = true)`** | Assigns `true` to `b` and evaluates to `true`, executing branch unconditionally. | Use `if (b)` or place constants on left: `if (true == b)`. |

---

## 8. Performance & Security Notes

### Bitwise Strength Reduction in JIT Compiler
HotSpot C2 automatically replaces expensive arithmetic operations with equivalent bitwise instructions:
- Multiplying by a constant power of two ($x \times 8$) is transformed into a single-cycle left shift (`x << 3`).
- Division by a power of two ($x / 4$) is transformed into a right shift with sign correction.
- Modulo by a power of two ($x \% 16$) is transformed into bitwise AND ($x \ \& \ 15$).

### Security Invariants: Timing Attacks and Short-Circuiting
When verifying sensitive cryptographic data (such as HMAC tokens or password hashes), **never** use standard short-circuit equality (`a.equals(b)` or `&&`). Standard comparison stops on the first mismatching byte, leaking execution timing information that allows attackers to recover secrets via side-channel timing analysis. Always use `MessageDigest.isEqual()`, which executes in constant time using unconditional bitwise XOR:
```java
// Constant-time equality check defense
boolean matches = MessageDigest.isEqual(suppliedSignature, expectedSignature);
```

---

## 9. Comparison / When to Use What

| Operator Pair | Behavior Difference | Recommended Scenario |
| :--- | :--- | :--- |
| **`&&` vs `&`** | `&&` short-circuits (skips right side); `&` evaluates both sides unconditionally. | Use `&&` for logic; use `&` strictly for bitwise masks. |
| **`||` vs `|`** | `||` short-circuits; `|` evaluates both sides unconditionally. | Use `||` for logic; use `|` strictly for bitwise masks. |
| **`>>` vs `>>>`** | `>>` preserves sign bit (arithmetic); `>>>` fills high bits with `0` (logical). | Use `>>` for signed division; use `>>>` for bit manipulation, hashing, and crypto. |
| **`%` vs `Math.floorMod()`** | `%` returns negative remainder for negative dividend; `floorMod` returns mathematical modulo. | Use `%` for standard remainder; use `floorMod` for circular buffer wrapping. |

---

## 10. Interview Questions

<details>
<summary><strong>1. Why does `short s = 1; s = s + 1;` fail compilation, while `s += 1;` compiles cleanly?</strong></summary>

**Answer:**  
In Java, binary arithmetic operations on values smaller than `int` undergo Binary Numeric Promotion: `s + 1` promotes `s` to `int` and produces an `int` result ($32$ bits). Assigning this `int` back to a `short` ($16$ bits) is a narrowing conversion that requires an explicit cast (`s = (short)(s + 1)`). However, the compound assignment operator `s += 1` is defined by JLS §15.26.2 to include an implicit narrowing cast: `s = (short)(s + 1)`.
</details>

<details>
<summary><strong>2. How does the JVM handle shift operators when the shift distance exceeds the bit width of the operand?</strong></summary>

**Answer:**  
According to JLS §15.19, the shift distance is masked:
- For 32-bit `int` operands, only the lowest 5 bits are used (`shiftDistance & 0x1F`, equivalent to `shiftDistance % 32`). Thus, `x << 32` shifts by 0, and `x << 33` shifts by 1.
- For 64-bit `long` operands, only the lowest 6 bits are used (`shiftDistance & 0x3F`, equivalent to `shiftDistance % 64`). Thus, `xL << 64` shifts by 0.
</details>

<details>
<summary><strong>3. What is the difference between signed right shift (`>>`) and unsigned right shift (`>>>`)?</strong></summary>

**Answer:**  
- **`>>` (Signed Right Shift):** Performs an arithmetic shift. It moves bits to the right and replicates the original sign bit (most significant bit) into the vacated leftmost positions. A negative number remains negative.
- **`>>>` (Unsigned Right Shift):** Performs a logical shift. It moves bits to the right and always inserts zeros into the vacated leftmost positions, regardless of whether the original number was positive or negative. For negative numbers, `>>>` turns them into large positive numbers.
</details>

<details>
<summary><strong>4. Why is `i = i++` considered a dangerous anti-pattern?</strong></summary>

**Answer:**  
The postfix increment operator `i++` evaluates to the current value of `i` *before* the increment, while queueing the increment side effect. In `i = i++`:
1. The right-hand side evaluates to the current value (e.g., 5).
2. The variable `i` is incremented to 6.
3. The assignment operator `=` executes, storing the evaluated result (5) back into `i`, completely overwriting the increment. The variable remains 5!
</details>

<details>
<summary><strong>5. What is the "Ternary Unboxing Trap", and how can it cause a `NullPointerException`?</strong></summary>

**Answer:**  
When the second and third operands of a ternary operator (`condition ? a : b`) have mixed types (e.g., one is a primitive `double` and the other is a boxed `Double`), JLS §15.25 mandates that the expression's overall type is the primitive type (`double`). If the branch evaluating to the boxed object is taken and that object reference is `null`, the JVM attempts to auto-unbox it to a primitive, immediately throwing a `NullPointerException` at runtime.
</details>

<details>
<summary><strong>6. How does Pattern Matching `instanceof` improve code safety and readability?</strong></summary>

**Answer:**  
Traditional type casting required two steps: an `instanceof` check followed by a redundant, explicit cast:
```java
if (obj instanceof String) {
    String s = (String) obj; // Redundant boilerplate
}
```
Pattern matching `instanceof` (final in Java 16, refined in 21/25) merges these:
`if (obj instanceof String s)` tests the type and binds the scoped pattern variable `s` automatically. Flow scoping ensures `s` is only in scope where the pattern is definitely matched.
</details>

<details>
<summary><strong>7. Why is `Math.floorMod(x, y)` preferred over `x % y` for circular buffer indices?</strong></summary>

**Answer:**  
In Java, `%` is a remainder operator, not a true mathematical modulo: if the dividend is negative, the remainder is negative (e.g., `-1 % 5 = -1`). Using this as an array index causes an `ArrayIndexOutOfBoundsException`. `Math.floorMod(-1, 5)` returns `4`, correctly wrapping around to the end of the circular buffer.
</details>

<details>
<summary><strong>8. Does Java guarantee a specific evaluation order for method arguments in expressions?</strong></summary>

**Answer:**  
Yes. Unlike C/C++ where evaluation order is unspecified or implementation-dependent, Java strictly guarantees that operands and method arguments are evaluated from **left to right**. In `f(a(), b(), c())`, `a()` is guaranteed to execute before `b()`, which executes before `c()`.
</details>

---

## 11. Practice Exercises

### Exercise 1: Expression Precedence Evaluation (Easy)
- **Task:** What is the exact value of `result` in each of the following expressions?
```java
int a = 10, b = 20, c = 30;
int r1 = a + b * c;
int r2 = (a + b) * c;
boolean r3 = a < b && b > c || a + 10 == b;
```
<details>
<summary>Solution</summary>

- `r1 = 10 + (20 * 30) = 10 + 600 = 610` (`*` has higher precedence than `+`).
- `r2 = (10 + 20) * 30 = 30 * 30 = 900` (parentheses override precedence).
- `r3 = true`:
  1. `a < b` is `true`.
  2. `b > c` is `false`.
  3. `true && false` evaluates to `false`.
  4. `a + 10 == b` evaluates to `20 == 20` (`true`).
  5. `false || true` evaluates to `true`.
</details>

### Exercise 2: Power-of-Two Detection via Bitwise Arithmetic (Medium)
- **Task:** Write an efficient method `boolean isPowerOfTwo(int n)` that returns `true` if `n` is a positive power of two ($1, 2, 4, 8, 16, \dots$), using a single bitwise expression without loops or math libraries.
<details>
<summary>Solution</summary>

```java
public class PowerOfTwoCheck {
    public static boolean isPowerOfTwo(int n) {
        // A power of two has exactly one '1' bit in binary.
        // Subtracting 1 flips all bits up to that '1' bit.
        // Therefore, n & (n - 1) clears the single bit to 0.
        return n > 0 && (n & (n - 1)) == 0;
    }

    public static void main(String[] args) {
        System.out.println("isPowerOfTwo(16): " + isPowerOfTwo(16)); // true
        System.out.println("isPowerOfTwo(18): " + isPowerOfTwo(18)); // false
        System.out.println("isPowerOfTwo(0):  " + isPowerOfTwo(0));  // false
    }
}
```
</details>

### Exercise 3: Circular Array Queue Index Generator (Hard)
- **Task:** Implement a circular buffer index calculation class that accepts positive or negative integer offsets and correctly computes the wrapped index within a buffer of size $N$ using both `Math.floorMod` and an optimized power-of-two bitwise mask.
<details>
<summary>Solution</summary>

```java
public class CircularIndex {
    public static int wrapStandard(int index, int capacity) {
        return Math.floorMod(index, capacity);
    }

    public static int wrapPowerOfTwo(int index, int capacity) {
        if ((capacity & (capacity - 1)) != 0) {
            throw new IllegalArgumentException("Capacity must be a power of two");
        }
        // Bitwise AND mask equivalent to modulo for power-of-two
        return index & (capacity - 1);
    }

    public static void main(String[] args) {
        int capacity = 8;
        int[] testOffsets = { 0, 7, 8, 9, -1, -8, -9 };

        System.out.println("Offset | Standard | Bitmask");
        System.out.println("---------------------------");
        for (int offset : testOffsets) {
            int std = wrapStandard(offset, capacity);
            int bit = wrapPowerOfTwo(offset, capacity);
            System.out.printf("%6d | %8d | %7d%n", offset, std, bit);
        }
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **Precedence Hierarchy:** Unary (`++`, `--`, `!`) $\to$ Multiplicative (`*`, `/`, `%`) $\to$ Additive (`+`, `-`) $\to$ Shift (`<<`, `>>`, `>>>`) $\to$ Relational (`<`, `>`, `instanceof`) $\to$ Equality (`==`, `!=`) $\to$ Bitwise (`&`, `^`, `|`) $\to$ Logical (`&&`, `||`) $\to$ Ternary (`? :`) $\to$ Assignment (`=`, `+=`).
- **Short-Circuit:** `&&` and `||` skip right operand when outcome is determined; use for null defense.
- **Evaluation Order:** Strict left-to-right evaluation across all operands and arguments.
- **Shift Masking:** Shift distances are masked: `shift % 32` for `int`, `shift % 64` for `long`.
- **Compound Assignment:** `op=` performs an implicit narrowing cast: `s += 1` $\iff$ `s = (short)(s + 1)`.

---

## 13. Further Reading

- [The Java Language Specification (Java SE 25 Edition) - Chapter 15: Expressions](https://docs.oracle.com/javase/specs/jls/se25/html/jls-15.html)
- [JEP 394: Pattern Matching for instanceof](https://openjdk.org/jeps/394)
- [Hacker's Delight by Henry S. Warren, Jr. (Bitwise Algorithms)](https://en.wikipedia.org/wiki/Hacker%27s_Delight)

---

| [← Previous: 01_variables-and-data-types.md](file:///E:/Learning/engineering-notes/01-languages/java/01-fundamentals/01_variables-and-data-types.md) | [Section Index](file:///E:/Learning/engineering-notes/01-languages/java/01-fundamentals/README.md) | [Next: 03_type-casting-and-conversion.md](file:///E:/Learning/engineering-notes/01-languages/java/01-fundamentals/03_type-casting-and-conversion.md) → |
