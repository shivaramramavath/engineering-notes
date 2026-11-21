# Variables and Data Types in Java

> Java enforces strict static typing through a dual type system: eight high-performance hardware-aligned primitive types storing raw binary values directly on the stack or inline within objects, and reference types pointing to dynamic heap-allocated objects.

| Metadata | Value |
| :--- | :--- |
| **Level** | Beginner |
| **Prerequisites** | [00_syntax-and-program-structure.md](file:///E:/Learning/engineering-notes/01-languages/java/01-fundamentals/00_syntax-and-program-structure.md) |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **Java LTS Baseline** | Java 25 LTS (with `var` Local Variable Type Inference) |

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
Programs process data: numbers, text, timestamps, sensor readings, and binary streams. To allocate memory safely and execute CPU operations correctly, a language must know the exact binary layout, bit width, and permissible operations for each piece of data. 

### Historical Context & Static Type Safety
Java was designed in reaction to C/C++, where untyped memory pointers and arbitrary type punning led to frequent security breaches, buffer overflows, and segmentation faults. Java chose **strong static typing**:
- Every variable must have an explicit or inferred type known at compile time.
- Variables cannot hold data outside their type contract without explicit casting.
- Types are divided into **primitive types** (for maximum CPU execution speed) and **reference types** (for rich object-oriented abstractions).
- In Java 10 (JEP 286), Java introduced `var` (Local Variable Type Inference) to reduce boilerplate while maintaining 100% static compile-time type safety.

### Real-World Relevance
Understanding data types and their memory footprint is critical for high-throughput engineering. Choosing `long` over `int` for entity IDs prevents integer overflow in growing databases. Choosing primitive arrays (`int[]`) over boxed collections (`List<Integer>`) can reduce memory consumption by over 400% in high-frequency trading and data engineering systems.

---

## 2. Core Concepts

### The Dual Type System: Primitive vs. Reference

```
+---------------------------------------------------------------------------------+
|                                 Java Type System                                |
|                                                                                 |
|   +------------------------------------+    +-------------------------------+   |
|   |         Primitive Types            |    |       Reference Types         |   |
|   |                                    |    |                               |   |
|   |  Integral: byte, short, int, long  |    |  Classes (String, Customer)   |   |
|   |  Floating: float, double           |    |  Interfaces (List, Runnable)  |   |
|   |  Textual:  char                    |    |  Records (UserDto)            |   |
|   |  Logical:  boolean                 |    |  Arrays (int[], String[])     |   |
|   |                                    |    |  Enums (Status)               |   |
|   |  • Holds VALUE directly            |    |  • Holds MEMORY ADDRESS       |   |
|   |  • Never null                      |    |  • Can be null                |   |
|   |  • Allocated on Stack / Inlined    |    |  • Allocated on Java Heap     |   |
|   +------------------------------------+    +-------------------------------+   |
+---------------------------------------------------------------------------------+
```

### The 8 Primitive Types
| Type | Bits | Bytes | Minimum Value | Maximum Value | Default Value | Wrapper Class |
| :--- | :---: | :---: | :--- | :--- | :---: | :--- |
| `byte` | 8 | 1 | $-128$ ($-2^7$) | $127$ ($2^7 - 1$) | `0` | `Byte` |
| `short` | 16 | 2 | $-32,768$ ($-2^{15}$) | $32,767$ ($2^{15} - 1$) | `0` | `Short` |
| `int` | 32 | 4 | $-2,147,483,648$ ($-2^{31}$) | $2,147,483,647$ ($2^{31} - 1$) | `0` | `Integer` |
| `long` | 64 | 8 | $-2^{63}$ | $2^{63} - 1$ | `0L` | `Long` |
| `float` | 32 | 4 | $\approx 1.4 \times 10^{-45}$ (IEEE 754) | $\approx 3.4 \times 10^{38}$ | `0.0f` | `Float` |
| `double` | 64 | 8 | $\approx 4.9 \times 10^{-324}$ (IEEE 754) | $\approx 1.8 \times 10^{308}$ | `0.0d` | `Double` |
| `char` | 16 | 2 | `\u0000` ($0$) | `\uffff` ($65,535$) | `\u0000` | `Character` |
| `boolean`| JVM | JVM | `false` | `true` | `false` | `Boolean` |

### Default Values vs. Definite Assignment
- **Fields (Instance and Static):** Automatically initialized to their default values (`0`, `0.0`, `false`, or `null` for references) when the containing class or object is created.
- **Local Variables:** Reside in stack frames and are **never** automatically initialized. The compiler enforces **Definite Assignment**: reading an uninitialized local variable causes a compile-time error.

### Local Variable Type Inference (`var`)
Introduced in Java 10 (JEP 286):
- `var` is **not** dynamic typing. It is compile-time type inference. The compiler inspects the right-hand initializer and writes the exact static type into the `.class` file.
- **Where allowed:** Local variables with initializers, resource variables in `try-with-resources`, and `for` loop indices.
- **Where forbidden:** Class fields, method parameters, method return types, and uninitialized variables.

---

## 3. How It Works Internally

### Memory Layout: Stack vs. Heap Allocation

```
   [ Thread Stack Frame ]                                [ Java Heap ]
+---------------------------+                +-----------------------------------+
| int count = 42;           |                |                                   |
| [ Value: 42 (32 bits) ]   |                |                                   |
|                           |                |  Customer Object                  |
| Customer c = new Customer |───────────────►|  ┌─────────────────────────────┐  |
| [ Pointer: 0x7FFF0040 ]   |                |  │ Mark Word (64 bits)         │  |
+---------------------------+                |  │ Klass Word (32 bits comp)   │  |
                                             |  │ int id = 101;               │  |
                                             |  │ double balance = 500.50;    │  |
                                             |  └─────────────────────────────┘  |
                                             +-----------------------------------+
```

1. **Primitives on the Stack:** When declared as local variables, primitives are placed directly in the Thread Stack Frame's **Local Variable Array**. Accessing an `int` requires a single register read instruction (`iload_1`), incurring zero pointer indirection or GC overhead.
2. **Primitives in Objects:** When declared as fields of a class or record, primitives are inlined directly into the object's contiguous heap memory block following the 12-byte or 16-byte object header.
3. **Reference Storage:** A reference variable holds an address pointing to a heap object. On 64-bit JVMs with heaps under 32 GB, **Compressed OOPs** (`-XX:+UseCompressedOops`) compress 64-bit object pointers down to 32 bits, halving pointer memory overhead.

### JVM Bytecode Mapping
The JVM instruction set does not have distinct computational opcodes for `byte`, `short`, and `char`. On the operand stack:
- `byte`, `short`, `char`, and `boolean` are widened and processed as 32-bit `int` instructions (`iload`, `istore`, `iadd`).
- They are narrowed only when stored back into heap fields or arrays (`bastore`, `sastore`, `castore`).

---

## 4. Syntax & Usage

### Literals and Numeric Formatting
Java provides rich literal notations for clarity:

```java
// Integer Literals
int decimal = 100;
int hex = 0xFF;              // 255 (prefix 0x)
int binary = 0b1010_1010;    // 170 (prefix 0b)
int octal = 077;             // 63 (prefix 0)

// Underscores in Numeric Literals (Java 7+) for readability
long creditCardNumber = 1234_5678_9012_3456L;  // Suffix 'L' for long
double microSeconds = 0.000_001;

// Floating Point Literals
float f = 3.14159f;          // Suffix 'F' or 'f' mandatory for float
double d = 2.71828d;         // Suffix 'D' or 'd' optional for double
double scientific = 1.5e6;   // 1.5 * 10^6 = 1,500,000.0

// Character Literals
char letter = 'A';
char unicodeChar = '\u0041'; // Unicode escape for 'A'
char newline = '\n';         // Escape sequence
```

### Type Inference (`var`) Usage
```java
// Idiomatic use: avoids repeating verbose generic types
var customerMap = new HashMap<String, List<Transaction>>();

// Equivalent to:
// HashMap<String, List<Transaction>> customerMap = new HashMap<>();

// In try-with-resources:
try (var reader = Files.newBufferedReader(Path.of("data.csv"))) {
    // reader inferred as BufferedReader
}
```

---

## 5. Practical Examples

### Example 1: Comprehensive Primitive Matrix Probe
- **Goal:** Print the exact bit ranges and wrapper boundaries of all primitive types.
```java
public class DataTypesDemo {
    public static void main(String[] args) {
        System.out.println("Byte:    " + Byte.MIN_VALUE + " to " + Byte.MAX_VALUE);
        System.out.println("Short:   " + Short.MIN_VALUE + " to " + Short.MAX_VALUE);
        System.out.println("Int:     " + Integer.MIN_VALUE + " to " + Integer.MAX_VALUE);
        System.out.println("Long:    " + Long.MIN_VALUE + " to " + Long.MAX_VALUE);
        System.out.println("Float:   " + Float.MIN_VALUE + " to " + Float.MAX_VALUE);
        System.out.println("Double:  " + Double.MIN_VALUE + " to " + Double.MAX_VALUE);
        System.out.println("Char:    " + (int)Character.MIN_VALUE + " to " + (int)Character.MAX_VALUE);
    }
}
```
- **Console Output:**
```text
Byte:    -128 to 127
Short:   -32768 to 32767
Int:     -2147483648 to 2147483647
Long:    -9223372036854775808 to 9223372036854775807
Float:   1.4E-45 to 3.4028235E38
Double:  4.9E-324 to 1.7976931348623157E308
Char:    0 to 65535
```
- **Explanation:** Shows the hardware-enforced boundaries. Notice `char` is unsigned ($0$ to $65,535$), while all other numeric primitives are signed two's complement.

---

### Example 2: Definite Assignment & Local Variable Scoping
- **Goal:** Demonstrate compiler enforcement of definite assignment across conditional branching.
```java
public class DefiniteAssignmentDemo {
    public static void main(String[] args) {
        int x;
        boolean condition = true;

        if (condition) {
            x = 100;
        } else {
            x = 200;
        }

        // Legal: Compiler proves x is assigned in both if and else branches
        System.out.println("Definite assignment satisfied: x = " + x);
    }
}
```
- **Console Output:**
```text
Definite assignment satisfied: x = 100
```
- **Explanation:** If the `else` branch were omitted, `javac` would throw `variable x might not have been initialized`. The compiler performs flow analysis to ensure no memory read occurs on uninitialized stack slots.

---

### Example 3: Edge Case — Integer Overflow vs. `Math.addExact`
- **Goal:** Observe silent two's complement integer wrap-around and contrast with safe exact arithmetic.
```java
public class OverflowDemo {
    public static void main(String[] args) {
        int max = Integer.MAX_VALUE; // 2,147,483,647
        int wrapped = max + 1;       // Silent overflow wraps to negative!
        System.out.println("Silent Overflow: " + max + " + 1 = " + wrapped);

        try {
            Math.addExact(max, 1);
        } catch (ArithmeticException e) {
            System.out.println("Safe Guard: Math.addExact threw " + e.getClass().getSimpleName());
        }
    }
}
```
- **Console Output:**
```text
Silent Overflow: 2147483647 + 1 = -2147483648
Safe Guard: Math.addExact threw ArithmeticException
```
- **Explanation:** In standard Java, integer overflow is silent and undefined check-free for maximum hardware performance. For critical financial or cryptographic logic, `Math.addExact()` checks the CPU overflow flag and throws `ArithmeticException`.

---

### Example 4: Failure Case — Misuse of `var`
- **Goal:** Demonstrate illegal and anti-pattern uses of local variable type inference.
```java
public class VarPitfalls {
    // ILLEGAL: var cannot be used for fields
    // private var count = 10; 

    public static void main(String[] args) {
        // ILLEGAL: var cannot be uninitialized
        // var x;

        // ILLEGAL: var cannot be assigned null (type cannot be inferred)
        // var obj = null;

        // ANTI-PATTERN: Loss of readability
        var result = getInternalData(); // Reader has no idea what 'result' is!
    }

    private static String getInternalData() {
        return "data";
    }
}
```
- **Remedy:** Only use `var` when the variable type is immediately obvious from the right-hand side (e.g., `var list = new ArrayList<String>();` or `var user = findUserById(id);`).

---

### Example 5: High-Performance / Modern Idiom — Memory Footprint: Primitives vs. Boxed Objects
- **Goal:** Compare memory consumption of 1,000,000 primitive `int` values vs. 1,000,000 boxed `Integer` references.
```java
public class MemoryFootprintComparison {
    public static void main(String[] args) {
        int count = 1_000_000;

        // Primitive int array: 1,000,000 * 4 bytes ≈ 4 MB total memory
        long primitiveBytes = (long) count * Integer.BYTES;

        // Boxed Integer array:
        // 1,000,000 object references (4 bytes each with Compressed OOPs)
        // + 1,000,000 Integer objects (16 bytes each on heap: 12-byte header + 4-byte int)
        // Total ≈ 20 MB (5x larger!)
        long boxedEstimatedBytes = (long) count * (4 + 16);

        System.out.printf("Primitive int[] array:  ~%d MB%n", primitiveBytes / (1024 * 1024));
        System.out.printf("Boxed Integer[] array:    ~%d MB%n", boxedEstimatedBytes / (1024 * 1024));
        System.out.printf("Memory Amplification:     %.1fx overhead%n", 
            (double) boxedEstimatedBytes / primitiveBytes);
    }
}
```
- **Console Output:**
```text
Primitive int[] array:  ~3 MB
Boxed Integer[] array:    ~19 MB
Memory Amplification:     5.0x overhead
```
- **Explanation:** Boxed objects incur object header overhead (12–16 bytes per object), pointer indirection, and garbage collection tracking. High-performance collections (like Eclipse Collections or Agrona) use primitive arrays to eliminate this overhead.

---

## 6. Best Practices

1. **Use `int` by Default for Whole Numbers:** Unless memory constraints require `byte`/`short` arrays, or magnitude requires `long`, always use `int`. 32-bit integers align naturally with CPU word boundaries and operand stack instructions.
2. **Use `long` for Database Primary Keys and Timestamps:** Avoid running out of IDs after reaching $2.14$ billion rows. Timestamps in milliseconds since epoch (`System.currentTimeMillis()`) already exceed `int` boundaries.
3. **Never Use `float` or `double` for Monetary Calculations:** IEEE 754 floating-point numbers cannot represent decimal fractions like $0.1$ exactly. Always use `BigDecimal` or store integer cents in `long`.
4. **Prefer `var` When the Initializer States the Type:**
   ```java
   var users = new ArrayList<User>(); // GOOD
   var parser = new JsonParser();     // GOOD
   var x = calculate();               // BAD: obscures type
   ```
5. **Mark Unmodifiable Variables `final`:** Explicitly declaring `final int maxRetries = 3;` communicates intent, prevents accidental reassignment, and helps the JIT compiler reason about constant folding.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| **Using `==` on Reference Types instead of `.equals()`** | `==` compares memory addresses, not object values. | Use `.equals()` for strings and objects; use `==` only for primitives. |
| **Accidental Octal Literals** | Writing `int code = 077;` interprets `0` as an octal prefix ($63$), not decimal $77$. | Remove leading zeros from decimal integers. |
| **Silent Integer Overflow** | Adding to `Integer.MAX_VALUE` rolls over to a negative number without warning. | Use `Math.addExact()` or `long` for large accumulations. |
| **Comparing Floating-Point Numbers with `==`** | Precision rounding ($0.1 + 0.2 = 0.30000000000000004$) causes `==` to return `false`. | Compare with an epsilon: `Math.abs(a - b) < 1e-9` or use `BigDecimal`. |
| **Uninitialized Local Variable Access** | Reading local variables before assignment causes compile error. | Initialize local variables at declaration point. |
| **Confusing `var` with JavaScript `var`** | Java's `var` is statically typed; reassigning a different type causes a compile error. | Remember `var` is static type inference, not dynamic typing. |

---

## 8. Performance & Security Notes

### Cache Locality and Spatial Prefetching
Primitive arrays (`int[]`, `double[]`) are allocated as contiguous memory blocks on the heap. When CPU cores access an element, hardware prefetchers load adjacent array elements into the L1/L2 CPU cache lines (64 bytes per cache line) simultaneously. Conversely, an array of objects (`Integer[]` or `Customer[]`) is an array of pointers: accessing elements causes random pointer chases across memory, triggering CPU cache misses.

### Security Invariants: Defensive Copying of Primitives
Because primitive variables are passed by value and stored directly on the stack, passing a primitive to an untrusted method guarantees that the caller's value cannot be mutated. However, for reference types, passing an object reference allows the callee to mutate internal fields unless the class is strictly immutable (such as a Java `record` or `String`).

---

## 9. Comparison / When to Use What

| Data Type | Memory | Precision / Semantics | When to Choose | When to Avoid |
| :--- | :---: | :--- | :--- | :--- |
| **`int`** | 4 bytes | 32-bit signed integer | Default for counters, loops, array indices | Entity IDs that may exceed 2 billion |
| **`long`** | 8 bytes | 64-bit signed integer | Database IDs, timestamps, file byte offsets | Small in-memory structures with memory limits |
| **`double`** | 8 bytes | 64-bit IEEE 754 float | Scientific calculations, graphics, metrics | Currency, accounting, exact decimals |
| **`BigDecimal`** | ~40 bytes | Arbitrary-precision decimal | Financial transactions, invoicing, tax math | Tight game loops, graphics, low-latency math |
| **`boolean`** | 1 byte | True / False logical state | Flags, conditions, binary state | Multi-state values (use an `enum` instead) |
| **`char`** | 2 bytes | 16-bit UTF-16 code unit | Low-level character parsing | General text processing (use `String`) |

---

## 10. Interview Questions

<details>
<summary><strong>1. Why doesn't Java support unsigned integers as primitive types?</strong></summary>

**Answer:**  
James Gosling originally omitted unsigned integers to simplify the language and prevent subtle bugs arising from signed/unsigned comparison conversions (a notorious source of security vulnerabilities in C/C++). However, Java 8 added static utility methods in the wrapper classes—such as `Integer.divideUnsigned()`, `Integer.compareUnsigned()`, and `Integer.toUnsignedString()`—allowing standard signed 32-bit and 64-bit integers to be treated as unsigned values when necessary.
</details>

<details>
<summary><strong>2. How are `boolean` variables represented in memory by the JVM?</strong></summary>

**Answer:**  
The Java Virtual Machine Specification does not mandate an exact binary representation for standalone `boolean` variables. In Oracle HotSpot:
- Standalone `boolean` local variables and fields are compiled into 32-bit `int` values (where `1` is `true` and `0` is `false`).
- Inside arrays (`boolean[]`), the JVM packs elements as 8-bit bytes (1 byte per boolean) to conserve heap memory.
</details>

<details>
<summary><strong>3. What is the difference between `float` and `double` according to IEEE 754?</strong></summary>

**Answer:**  
- `float` is a 32-bit single-precision floating point number consisting of 1 sign bit, 8 exponent bits, and 23 significand (mantissa) bits, providing approximately 6 to 7 decimal digits of precision.
- `double` is a 64-bit double-precision floating point number consisting of 1 sign bit, 11 exponent bits, and 52 significand bits, providing approximately 15 to 17 decimal digits of precision.
</details>

<details>
<summary><strong>4. Why does `0.1 + 0.2` not equal `0.3` in Java?</strong></summary>

**Answer:**  
In binary (base 2), fractional numbers like $0.1$ ($1/10$) and $0.2$ ($1/5$) are repeating infinite fractions, just as $1/3$ is $0.333...$ in decimal (base 10). Because IEEE 754 floating-point representations have finite mantissa precision (52 bits for `double`), the repeating fraction is rounded. Adding the rounded binary representations of $0.1$ and $0.2$ results in `0.30000000000000004`, which fails an exact `==` equality check.
</details>

<details>
<summary><strong>5. Can `var` be used to declare method parameters or return types? Why or why not?</strong></summary>

**Answer:**  
No. `var` is strictly limited to local variables. Allowing `var` on method parameters or return types would turn Java's public API contracts into implicit, fragile signatures. A change in method implementation could inadvertently change the return type, silently breaking downstream consumers. Java enforces explicit types on all public API boundaries.
</details>

<details>
<summary><strong>6. What is the impact of Compressed OOPs on 64-bit JVMs?</strong></summary>

**Answer:**  
On a 64-bit architecture, native memory pointers require 8 bytes. This increases memory consumption and reduces CPU cache density. Because JVM heap allocations are 8-byte aligned (the lower 3 bits of every object address are always zero), HotSpot shifts 32-bit integers by 3 bits to address up to $2^{32} \times 8 \text{ bytes} = 32 \text{ GB}$ of heap memory. **Compressed OOPs** (`-XX:+UseCompressedOops`) enables 64-bit JVMs to store 32-bit object references, saving 4 bytes per reference.
</details>

<details>
<summary><strong>7. What happens when a local variable is shadowed by another variable?</strong></summary>

**Answer:**  
In Java, you cannot declare two local variables with the same name in the same scope or in nested inner blocks (unlike C++). For example:
```java
int x = 10;
{ int x = 20; } // COMPILE ERROR: Variable 'x' is already defined
```
However, a local variable or method parameter *can* shadow an **instance field** of the enclosing class. The field can then only be accessed using the `this.` qualifier (e.g., `this.x = x;`).
</details>

<details>
<summary><strong>8. How does `char` handle Unicode code points above `\uffff` (surrogate pairs)?</strong></summary>

**Answer:**  
A Java `char` is a 16-bit unsigned value that represents a single UTF-16 code unit ($0$ to $65,535$). Characters outside the Basic Multilingual Plane (BMP)—such as emojis, mathematical symbols, or historical scripts (code points $U+10000$ to $U+10FFFF$)—cannot fit in a single `char`. They are encoded as **surrogate pairs**: two consecutive `char` values (a high surrogate and a low surrogate). Text processing code should use code point methods (e.g., `String.codePoints()`, `Character.isSupplementaryCodePoint()`) rather than counting raw `char` units.
</details>

---

## 11. Practice Exercises

### Exercise 1: Literal Range Bounds Evaluation (Easy)
- **Task:** Identify which of the following literal assignments will fail compilation and explain why:
```java
byte b1 = 127;
byte b2 = 128;
long l1 = 2147483648;
long l2 = 2147483648L;
float f1 = 3.14;
float f2 = 3.14f;
```
<details>
<summary>Solution</summary>

- `b2 = 128;`: **Fails compilation**. Max value for signed `byte` is 127.
- `l1 = 2147483648;`: **Fails compilation**. The integer literal without suffix `L` is parsed as an `int`, and $2,147,483,648$ exceeds `Integer.MAX_VALUE`.
- `f1 = 3.14;`: **Fails compilation**. Floating-point literals are `double` by default; assigning a `double` to a `float` loses precision without an explicit cast.
- `b1`, `l2`, and `f2` compile successfully.
</details>

### Exercise 2: Safe Financial Arithmetic Utility (Medium)
- **Task:** Write a utility method `long calculateTotalCents(long priceCents, int quantity, double taxRate)` that calculates the total cost in cents, preventing silent integer overflow and rounding accurately without floating-point errors.
<details>
<summary>Solution</summary>

```java
public class FinancialUtil {
    public static long calculateTotalCents(long priceCents, int quantity, double taxRate) {
        // Multiply quantity and price safely using exact math
        long subtotal = Math.multiplyExact(priceCents, (long) quantity);

        // Apply tax and round to nearest cent
        double taxAmount = subtotal * taxRate;
        long roundedTax = Math.round(taxAmount);

        return Math.addExact(subtotal, roundedTax);
    }

    public static void main(String[] args) {
        long total = calculateTotalCents(1999, 3, 0.0825); // $19.99 * 3 + 8.25% tax
        System.out.println("Total Cents: " + total + " ($" + (total / 100.0) + ")");
    }
}
```
</details>

### Exercise 3: Bitwise Primitive Serialization Harness (Hard)
- **Task:** Implement two methods: `byte[] serializeInt(int value)` that serializes a 32-bit integer into a 4-byte big-endian array using bitwise shifts, and `int deserializeInt(byte[] bytes)` that reconstructs the original integer.
<details>
<summary>Solution</summary>

```java
public class PrimitiveSerializer {
    public static byte[] serializeInt(int value) {
        return new byte[] {
            (byte) ((value >>> 24) & 0xFF),
            (byte) ((value >>> 16) & 0xFF),
            (byte) ((value >>> 8) & 0xFF),
            (byte) (value & 0xFF)
        };
    }

    public static int deserializeInt(byte[] bytes) {
        if (bytes.length != 4) throw new IllegalArgumentException("Requires exactly 4 bytes");
        return ((bytes[0] & 0xFF) << 24) |
               ((bytes[1] & 0xFF) << 16) |
               ((bytes[2] & 0xFF) << 8)  |
               (bytes[3] & 0xFF);
    }

    public static void main(String[] args) {
        int original = -987654321;
        byte[] bytes = serializeInt(original);
        int reconstructed = deserializeInt(bytes);

        System.out.println("Original:      " + original);
        System.out.println("Reconstructed: " + reconstructed);
        System.out.println("Matches:       " + (original == reconstructed));
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **8 Primitives:** `byte` (1B), `short` (2B), `int` (4B), `long` (8B), `float` (4B), `double` (8B), `char` (2B unsigned), `boolean` (logical).
- **Default Values:** Applied only to fields (`0`, `0.0`, `false`, `null`). Local variables require definite assignment before read.
- **Literals:** `0x` = Hex, `0b` = Binary, `_` = numeric separator, `L` = long, `f` = float.
- **Type Inference (`var`):** Local variables with initializers only; compile-time static type inference.
- **Arithmetic Safety:** Standard integer math wraps silently; use `Math.addExact()` / `Math.multiplyExact()` for overflow protection.
- **Decimals:** Never use `float` or `double` for currency; use `BigDecimal` or store integer cents.

---

## 13. Further Reading

- [The Java Language Specification (Java SE 25 Edition) - Chapter 4: Types, Values, and Variables](https://docs.oracle.com/javase/specs/jls/se25/html/jls-4.html)
- [JEP 286: Local-Variable Type Inference (`var`)](https://openjdk.org/jeps/286)
- [IEEE 754 Standard for Floating-Point Arithmetic](https://standards.ieee.org/ieee/754/6260/)
- [OpenJDK Compressed OOPs Documentation](https://wiki.openjdk.org/display/HotSpot/CompressedOops)

---

| [← Previous: 00_syntax-and-program-structure.md](file:///E:/Learning/engineering-notes/01-languages/java/01-fundamentals/00_syntax-and-program-structure.md) | [Section Index](file:///E:/Learning/engineering-notes/01-languages/java/01-fundamentals/README.md) | [Next: 02_operators.md](file:///E:/Learning/engineering-notes/01-languages/java/01-fundamentals/02_operators.md) → |
