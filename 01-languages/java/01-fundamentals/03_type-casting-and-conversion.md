# Type Casting and Conversion in Java

> Type conversion bridges the Java type system by translating values between compatible types: widening conversions execute implicitly with guaranteed numeric safety, while narrowing and reference conversions require explicit casting and runtime bytecode verification.

| Metadata | Value |
| :--- | :--- |
| **Level** | Beginner to Intermediate |
| **Prerequisites** | [01_variables-and-data-types.md](file:///E:/Learning/engineering-notes/01-languages/java/01-fundamentals/01_variables-and-data-types.md) |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **Java LTS Baseline** | Java 25 LTS (with Pattern Matching and Bytecode Verifier) |

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
In strongly-typed programming environments, calculations frequently involve different data representations. An application may read an 8-bit byte from a network socket, perform 32-bit integer arithmetic, calculate a 64-bit floating-point statistical average, and persist the result into an arbitrary-precision database column. A language must define precise, mathematically sound rules governing when types can be converted automatically, when explicit programmer consent is required, and what happens when data exceeds the capacity of the target type.

### Historical Context & Design Principles
Java eliminates the silent memory corruption found in C/C++ (where arbitrary pointers can be cast to any type via `reinterpret_cast`):
- **Widening is Implicit:** Converting to a wider or more expressive type happens automatically without special syntax.
- **Narrowing is Explicit:** Any conversion that could potentially discard high-order bits, lose fractional precision, or fail at runtime requires an explicit prefix cast `(TargetType)`.
- **Reference Safety:** Object references can only be cast across valid inheritance hierarchies. The JVM actively enforces type safety at runtime via the `checkcast` bytecode instruction, throwing `ClassCastException` instead of corrupting memory.

### Real-World Relevance
Understanding type conversions prevents subtle financial calculation bugs (such as precision loss when converting 64-bit `long` to 64-bit `double`), avoids data corruption caused by integer truncation in high-throughput network streams, and eliminates runtime `ClassCastException` crashes in enterprise services.

---

## 2. Core Concepts

### The Primitive Conversion Hierarchy

```
+---------------------------------------------------------------------------------+
|                         Widening Primitive Conversions                          |
|                               (Implicit / Automatic)                            |
|                                                                                 |
|   byte ──► short ──► int ──► long ──► float ──► double                          |
|                       ▲                                                         |
|                     char                                                        |
+---------------------------------------------------------------------------------+
|                         Narrowing Primitive Conversions                         |
|                         (Explicit Cast Required: (type))                        |
|                                                                                 |
|   double ──► float ──► long ──► int ──► short ──► byte                          |
|                                  │                                              |
|                                  └──► char                                      |
+---------------------------------------------------------------------------------+
```

### 1. Widening Primitive Conversion
- Converts from a smaller primitive type to a larger primitive type.
- **Safety Guarantee:** The magnitude of the number is preserved.
- **The Precision Caveat:** Converting `int` or `long` to `float`, or `long` to `double`, is an automatic widening conversion, but it can lose least-significant bits of precision because floating-point mantissas have fewer significant bits than 64-bit integers.

### 2. Narrowing Primitive Conversion
- Converts from a larger primitive type to a smaller primitive type (e.g., `double` to `int` or `int` to `byte`).
- Requires an explicit prefix cast: `int x = (int) 3.99;`.
- **Mechanics:** Floating-point numbers are rounded toward zero (fraction truncated). Integers are narrowed by simply discarding the high-order bits (modulo arithmetic).

### 3. Reference Type Casting (Upcasting vs. Downcasting)
- **Upcasting (Widening Reference):** Casting a subclass reference to a superclass or interface type (`Object o = "hello";` or `List<String> list = new ArrayList<>();`). Always safe, implicit.
- **Downcasting (Narrowing Reference):** Casting a general reference to a specific subtype (`String s = (String) o;`). Requires an explicit cast. Verified by the JVM at runtime; throws `ClassCastException` if the underlying object is not an instance of the target type.

---

## 3. How It Works Internally

### Bytecode Conversion Instructions
The JVM provides dedicated bytecode opcodes for primitive conversions on the operand stack:

| Instruction | Source Type | Target Type | Operation Details |
| :--- | :--- | :--- | :--- |
| `i2l` | `int` (32b) | `long` (64b) | Sign-extends 32-bit int to 64-bit long. |
| `i2f` | `int` (32b) | `float` (32b) | Converts integer to IEEE 754 single precision. |
| `i2d` | `int` (32b) | `double` (64b) | Converts integer to IEEE 754 double precision. |
| `l2i` | `long` (64b) | `int` (32b) | Discards high-order 32 bits. |
| `d2i` | `double` (64b) | `int` (32b) | Truncates fractional part toward zero; handles NaN / Infinity. |
| `i2b` | `int` (32b) | `byte` (8b) | Discards high-order 24 bits, sign-extends lowest 8 bits. |
| `i2c` | `int` (32b) | `char` (16b) | Discards high-order 16 bits, zero-extends (unsigned). |

### Two's Complement Bit Chopping
When you cast `(byte) 300`, what physically happens in the CPU?

```
Decimal 300 in 32-bit binary:
00000000 00000000 00000001 00101100

Cast to byte (discards high 24 bits):
[Discarded 24 bits]        00101100

Remaining 8 bits: 00101100
Evaluates in two's complement to: 32 + 8 + 4 = 44!
```
Similarly, `(byte) 130`:
`130` in binary is `... 10000010`. In an 8-bit signed byte, the most significant bit is `1` (negative sign bit). In two's complement, `10000010` represents $-128 + 2 = -126$.

### Reference Casting: The `checkcast` Bytecode
When executing `Dog d = (Dog) animal;`, `javac` emits:
```text
aload_1                  // Push animal reference onto stack
checkcast  #7            // Class com/example/Dog
astore_2                 // Store into d
```
The JVM looks up the object header of `animal` on the heap, extracts its 32-bit Compressed Klass Pointer, inspects the class vtable in Metaspace, and verifies whether `Dog` exists in its superclass or interface hierarchy. If not, the JVM raises `java.lang.ClassCastException`.

---

## 4. Syntax & Usage

### Primitive Casting Syntax
```java
// Widening (Implicit)
int count = 100;
long bigCount = count;      // Automatic int -> long
double rate = bigCount;     // Automatic long -> double

// Narrowing (Explicit)
double pi = 3.1415926535;
int truncatedPi = (int) pi; // Explicit: truncatedPi becomes 3

int large = 250;
byte b = (byte) large;      // Explicit: wraps to -6 (250 - 256)
```

### Safe Reference Downcasting
Always guard reference downcasts using modern pattern matching `instanceof`:

```java
// ANTI-PATTERN: Dangerous blind downcast
Object payload = getPayload();
String text = (String) payload; // Throws ClassCastException if payload is Integer!

// IDIOMATIC: Safe pattern matching downcast (Java 16+ / 21+ / 25)
if (payload instanceof String s) {
    System.out.println("Payload length: " + s.length());
} else {
    System.out.println("Payload is not a String: " + payload);
}
```

---

## 5. Practical Examples

### Example 1: Precision Loss in Widening Conversions
- **Goal:** Demonstrate that widening conversion from `long` to `double` can silently lose precision due to IEEE 754 mantissa limitations.
```java
public class PrecisionLossDemo {
    public static void main(String[] args) {
        long bigLong = 123456789012345678L; // 18-digit integer
        double asDouble = bigLong;            // Automatic widening!
        long recovered = (long) asDouble;    // Cast back

        System.out.println("Original Long:  " + bigLong);
        System.out.println("As Double:      " + asDouble);
        System.out.println("Recovered Long: " + recovered);
        System.out.println("Precision Lost: " + (bigLong != recovered));
    }
}
```
- **Console Output:**
```text
Original Long:  123456789012345678
As Double:      1.2345678901234568E17
Recovered Long: 123456789012345680
Precision Lost: true
```
- **Explanation:** A 64-bit `long` has 63 bits of integer magnitude. A 64-bit `double` allocates only 52 bits to the significand (mantissa). Large integers exceeding $2^{53} - 1$ ($9,007,199,254,740,991$) lose least-significant bits during widening conversion.

---

### Example 2: Bit Chopping in Narrowing Conversions
- **Goal:** Observe the mathematical predictability of integer truncation when narrowing to `byte`.
```java
public class NarrowingBitChopping {
    public static void main(String[] args) {
        int val1 = 300;
        byte b1 = (byte) val1; // 300 % 256 = 44
        System.out.println("(byte) 300 = " + b1);

        int val2 = 130;
        byte b2 = (byte) val2; // 130 - 256 = -126
        System.out.println("(byte) 130 = " + b2);
    }
}
```
- **Console Output:**
```text
(byte) 300 = 44
(byte) 130 = -126
```
- **Explanation:** Narrowing from 32-bit `int` to 8-bit `byte` discards the top 24 bits, leaving only the lowest 8 bits, which are interpreted as a signed two's complement value.

---

### Example 3: Edge Case — Floating-Point to Integer NaN and Infinity Truncation
- **Goal:** Observe how Java converts IEEE-754 special values (`NaN`, `POSITIVE_INFINITY`) to `int`.
```java
public class FloatSpecialCastDemo {
    public static void main(String[] args) {
        double nan = Double.NaN;
        double posInf = Double.POSITIVE_INFINITY;
        double negInf = Double.NEGATIVE_INFINITY;

        int fromNan = (int) nan;
        int fromPosInf = (int) posInf;
        int fromNegInf = (int) negInf;

        System.out.println("(int) NaN:               " + fromNan);
        System.out.println("(int) POSITIVE_INFINITY: " + fromPosInf);
        System.out.println("(int) NEGATIVE_INFINITY: " + fromNegInf);
    }
}
```
- **Console Output:**
```text
(int) NaN:               0
(int) POSITIVE_INFINITY: 2147483647
(int) NEGATIVE_INFINITY: -2147483648
```
- **Explanation:** By JLS §5.1.3 specification: `NaN` converts to `0`, `+Infinity` clamps to `Integer.MAX_VALUE`, and `-Infinity` clamps to `Integer.MIN_VALUE`.

---

### Example 4: Failure Case — `ClassCastException` via Unchecked Downcasting
- **Goal:** Demonstrate traditional runtime downcast failure and contrast with pattern matching safety.
```java
public class ClassCastFailureDemo {
    public static void main(String[] args) {
        Object val = Integer.valueOf(42);

        try {
            // UNCHECKED CAST: Causes ClassCastException
            String str = (String) val;
        } catch (ClassCastException e) {
            System.out.println("CAUGHT: " + e.getMessage());
        }

        // SAFE ALTERNATIVE: Pattern Matching
        if (val instanceof String s) {
            System.out.println("String value: " + s);
        } else {
            System.out.println("Safe rejection: Object is an " + val.getClass().getSimpleName());
        }
    }
}
```
- **Console Output:**
```text
CAUGHT: class java.lang.Integer cannot be cast to class java.lang.String (java.lang.Integer and java.lang.String are in module java.base of loader 'bootstrap')
Safe rejection: Object is an Integer
```
- **Explanation:** The JVM's `checkcast` instruction detected that `Integer` is not in `String`'s type hierarchy and safely threw `ClassCastException`, protecting heap memory integrity.

---

### Example 5: High-Performance / Modern Idiom — High-Throughput Integer Parsing
- **Goal:** Parse strings to integers without heap allocations or regex overhead in performance-critical code.
```java
public class FastIntegerParser {
    public static int parsePositiveInt(CharSequence cs) {
        int result = 0;
        for (int i = 0; i < cs.length(); i++) {
            char c = cs.charAt(i);
            if (c < '0' || c > '9') {
                throw new NumberFormatException("Invalid digit: " + c);
            }
            result = (result * 10) + (c - '0');
        }
        return result;
    }

    public static void main(String[] args) {
        String input = "987654";
        int parsed = parsePositiveInt(input);
        System.out.println("Fast parsed: " + parsed + " (Type: int)");
    }
}
```
- **Console Output:**
```text
Fast parsed: 987654 (Type: int)
```
- **Explanation:** Subtracting `'0'` (`48`) from a character code converts ASCII digits to their numeric value directly on the CPU stack with zero heap allocation.

---

## 6. Best Practices

1. **Guard All Reference Casts with Pattern Matching:** Never perform a raw `(TargetType)` cast without verifying the type using `if (obj instanceof TargetType target)`.
2. **Be Mindful of Large `long` to `double` Conversions:** If numeric accuracy must be preserved for 64-bit integer values exceeding $9 \times 10^{15}$, avoid `double`. Use `BigDecimal` or maintain arithmetic in `long`.
3. **Use Explicit Suffixes for Literal Conversions:** Write `100L` instead of `(long) 100`, and `3.14f` instead of `(float) 3.14`.
4. **Avoid Casts in Method Signatures:** Design polymorphic interfaces or use generics (`<T>`) rather than accepting `Object` and forcing consumers to cast return values.
5. **Check Bounds Before Narrowing:** When converting a dynamic `int` or `long` to `short` or `byte`, explicitly validate bounds or use `Math.toIntExact()`:
   ```java
   long id = getLongId();
   int compactId = Math.toIntExact(id); // Throws ArithmeticException on overflow!
   ```

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| **Assuming Widening Never Loses Data** | `long` to `float` or `double` loses least-significant bits. | Keep exact integer IDs in `long` or use `BigDecimal`. |
| **Silent Truncation via `(byte)`** | Discards high bits without warning; positive integers become negative. | Validate range `val >= Byte.MIN_VALUE && val <= Byte.MAX_VALUE`. |
| **Unchecked Downcasting** | Throws runtime `ClassCastException` when object type differs. | Use pattern matching: `if (obj instanceof Dog d)`. |
| **Integer Division Truncation before Cast** | `(double) (5 / 2)` evaluates `5 / 2` first to `2`, producing `2.0`. | Cast an operand first: `(double) 5 / 2` produces `2.5`. |
| **Casting Between Incompatible Class Hierarchies** | `(String) new Integer(5)` fails compilation; compiler knows they cannot be related. | Use `String.valueOf()` or `.toString()`. |
| **Downcasting Generic Collections Blindly** | `(List<String>) list` causes unchecked cast warning; leads to `ClassCastException` later. | Use type tokens, schema validation, or stream filtering. |

---

## 8. Performance & Security Notes

### Zero-Cost Primitive Casting
Primitive conversions between integer types (`i2l`, `l2i`, `i2b`) are native CPU instructions that execute in a single clock cycle (typically a `movsxd` sign-extension or bit truncate in x86_64 assembly).

### Reference Cast Overhead
Upcasting incurs **zero CPU cost**; the reference address is unchanged. Downcasting requires a `checkcast` test, which HotSpot optimizes by caching recent successful types in the inline cache. Monomorphic downcasts are inlined into a single pointer comparison.

### Security Invariants: Type Confusion Immunity
In languages like C, type punning allows casting memory pointers arbitrarily to read private memory. Java's Bytecode Verifier and runtime `checkcast` guarantee **Type Safety**: memory allocated for an `int[]` can never be interpreted as an object reference or executable code, preventing type-confusion exploits.

---

## 9. Comparison / When to Use What

| Conversion Type | Syntax Example | When It Occurs | Performance Cost | Risk / Failure Mode |
| :--- | :--- | :--- | :--- | :--- |
| **Widening Primitive** | `double d = intVal;` | Implicitly by compiler | Single CPU instruction | Silent precision loss for large `long` |
| **Narrowing Primitive** | `byte b = (byte) intVal;` | Explicit `(type)` cast | Single CPU instruction | Silent bit truncation / sign change |
| **Widening Reference** | `Object o = stringVal;` | Implicitly by compiler | **Zero** | None (always safe) |
| **Narrowing Reference** | `String s = (String) o;` | Explicit `(type)` cast | `checkcast` check | Runtime `ClassCastException` |
| **Boxing / Unboxing** | `Integer b = intVal;` | Implicit auto-boxing | Heap allocation / read | `NullPointerException` on unboxing |
| **String Parsing** | `Integer.parseInt(s)` | Explicit method call | String traversal & math | `NumberFormatException` |

---

## 10. Interview Questions

<details>
<summary><strong>1. Why is casting from `long` to `double` allowed implicitly if precision can be lost?</strong></summary>

**Answer:**  
According to JLS §5.1.2, widening primitive conversions are permitted implicitly when the target type has a larger representable *magnitude* (range), even if precision is sacrificed. A 64-bit `long` has a maximum magnitude of $\approx 9.22 \times 10^{18}$, while a 64-bit `double` can represent magnitudes up to $\approx 1.8 \times 10^{308}$. Because the magnitude of the number is preserved without catastrophic arithmetic overflow, Java treats it as a widening conversion, despite discarding least-significant bits.
</details>

<details>
<summary><strong>2. How does the JVM execute the `checkcast` instruction at runtime?</strong></summary>

**Answer:**  
When the JVM encounters `checkcast T`, it pops the object reference from the operand stack. If the reference is `null`, `checkcast` succeeds immediately (null can be cast to any reference type). If non-null, the JVM reads the object's header to find its runtime class pointer (Klass). It checks if the runtime class is identical to `T`, or is a subclass of `T`, or implements interface `T`. If the check fails, the JVM throws `java.lang.ClassCastException`; if it passes, the reference remains on the stack.
</details>

<details>
<summary><strong>3. What is the difference between `(short) (s + 1)` and `s += 1`?</strong></summary>

**Answer:**  
Functionally, both achieve the same result: they increment `s` and narrow the resulting `int` back to `short`. However, `s = s + 1` fails compilation because the binary `+` operator promotes operands to `int`, producing an `int` that cannot be assigned to `short` without an explicit cast. The compound assignment operator `s += 1` includes an implicit cast defined by JLS §15.26.2 as `s = (short)(s + 1)`.
</details>

<details>
<summary><strong>4. What is the result of casting `(char) -1` in Java?</strong></summary>

**Answer:**  
`char` is an unsigned 16-bit type ($0$ to $65,535$). The integer `-1` in 32-bit two's complement binary is all ones (`0xFFFFFFFF`). Casting `(char) -1` discards the top 16 bits, leaving 16 ones (`0xFFFF`). In decimal, this evaluates to **$65,535$**!
</details>

<details>
<summary><strong>5. Can an array of a primitive type be cast to `Object[]`? Why or why not?</strong></summary>

**Answer:**  
No. An array of primitives (such as `int[]`) is an `Object`, but it is **not** an `Object[]`. A primitive array directly contains raw binary primitives, whereas an `Object[]` contains references (pointers) to heap objects. Casting `(Object[]) new int[5]` causes a compile-time error: `int[] cannot be converted to Object[]`.
</details>

<details>
<summary><strong>6. Why does `(int) (double) 5 / 2` evaluate to `2.5` while `(double) (5 / 2)` evaluates to `2.0`?</strong></summary>

**Answer:**  
Operator precedence and associativity determine the order of evaluation:
- In `(double) 5 / 2`, the cast `(double)` binds tightly to the literal `5`, converting it to `5.0`. Then `5.0 / 2` undergoes binary numeric promotion, dividing `5.0 / 2.0 = 2.5`.
- In `(double) (5 / 2)`, the parentheses force integer division `5 / 2` to execute first, producing integer `2`. The cast `(double) 2` then converts `2` to `2.0`.
</details>

<details>
<summary><strong>7. How does `Math.toIntExact(long value)` work, and why is it preferred over raw `(int) value`?</strong></summary>

**Answer:**  
A raw cast `(int) value` silently truncates the top 32 bits, causing massive numbers to wrap unpredictably to negative or small numbers without warning. `Math.toIntExact(value)` inspects whether `(int) value == value`. If the top 32 bits contained significant data, it immediately throws `ArithmeticException("integer overflow")`, preventing silent data corruption.
</details>

<details>
<summary><strong>8. What is Pattern Matching for `instanceof`, and how does it prevent casting errors?</strong></summary>

**Answer:**  
Pattern Matching for `instanceof` (introduced in Java 16, refined in Java 21/25) combines the type test and extraction into a single atomic construct:
`if (obj instanceof String s)`. If the test returns `true`, the variable `s` is automatically cast to `String` and bound in the scope of the conditional block. This eliminates manual downcasting and guarantees that `ClassCastException` can never occur.
</details>

---

## 11. Practice Exercises

### Exercise 1: Predicting Truncation Outputs (Easy)
- **Task:** Determine the exact printed values of variables `a`, `b`, and `c`:
```java
int i = 130;
byte a = (byte) i;
double d = 9.99;
int b = (int) d;
char c = (char) 65;
```
<details>
<summary>Solution</summary>

- `a = -126`: $130 - 256 = -126$ (8-bit two's complement overflow).
- `b = 9`: Double-to-int truncates the fractional part toward zero.
- `c = 'A'`: ASCII/Unicode character code 65 corresponds to capital `'A'`.
</details>

### Exercise 2: Safe Downcasting Validator (Medium)
- **Task:** Write a method `double extractNumericValue(Object obj)` that accepts an `Object` and returns its `double` value if the object is an instance of `Byte`, `Short`, `Integer`, `Long`, `Float`, or `Double`. If not numeric, throw `IllegalArgumentException`. Use modern pattern matching.
<details>
<summary>Solution</summary>

```java
public class NumericExtractor {
    public static double extractNumericValue(Object obj) {
        // Number is the abstract superclass of all numeric wrappers
        if (obj instanceof Number num) {
            return num.doubleValue();
        }
        throw new IllegalArgumentException("Object is not a numeric type: " + obj);
    }

    public static void main(String[] args) {
        System.out.println("Extracted: " + extractNumericValue(42));       // 42.0
        System.out.println("Extracted: " + extractNumericValue(3.14159f)); // 3.14159...
        System.out.println("Extracted: " + extractNumericValue((short)10));// 10.0
    }
}
```
</details>

### Exercise 3: Safe Narrowing Converter with Exception Diagnostics (Hard)
- **Task:** Implement a utility class `SafeCaster` that provides `byte toByteExact(int value)` and `short toShortExact(int value)` that return the narrowed primitive if within bounds, or throw a descriptive `ArithmeticException` containing the overflowing value.
<details>
<summary>Solution</summary>

```java
public class SafeCaster {
    public static byte toByteExact(int value) {
        if (value < Byte.MIN_VALUE || value > Byte.MAX_VALUE) {
            throw new ArithmeticException("Value out of byte range: " + value);
        }
        return (byte) value;
    }

    public static short toShortExact(int value) {
        if (value < Short.MIN_VALUE || value > Short.MAX_VALUE) {
            throw new ArithmeticException("Value out of short range: " + value);
        }
        return (short) value;
    }

    public static void main(String[] args) {
        System.out.println("Safe byte: " + toByteExact(127)); // OK
        try {
            toByteExact(128); // Throws
        } catch (ArithmeticException e) {
            System.out.println("Caught expected overflow: " + e.getMessage());
        }
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **Widening (Automatic):** `byte` $\to$ `short` $\to$ `int` $\to$ `long` $\to$ `float` $\to$ `double`.
- **Narrowing (Explicit Cast):** Requires `(TargetType)`. Discards high-order bits or truncates float fractions toward zero.
- **Reference Casting:**
  - Upcasting: Subclass to superclass; always safe, implicit.
  - Downcasting: Superclass to subclass; requires `(Type)` cast, verified at runtime by `checkcast`.
- **Idiomatic Defense:** Use Pattern Matching `instanceof`: `if (obj instanceof String s)`.
- **Safe Integer Narrowing:** Use `Math.toIntExact(longVal)` to guard against silent overflow.

---

## 13. Further Reading

- [The Java Language Specification (Java SE 25 Edition) - Chapter 5: Conversions and Contexts](https://docs.oracle.com/javase/specs/jls/se25/html/jls-5.html)
- [JVM Specification - checkcast instruction](https://docs.oracle.com/javase/specs/jvms/se25/html/jvms-6.html#jvms-6.5.checkcast)
- [JEP 394: Pattern Matching for instanceof](https://openjdk.org/jeps/394)

---

| [← Previous: 02_operators.md](file:///E:/Learning/engineering-notes/01-languages/java/01-fundamentals/02_operators.md) | [Section Index](file:///E:/Learning/engineering-notes/01-languages/java/01-fundamentals/README.md) | [Next: 04_wrapper-classes-and-autoboxing.md](file:///E:/Learning/engineering-notes/01-languages/java/01-fundamentals/04_wrapper-classes-and-autoboxing.md) → |
