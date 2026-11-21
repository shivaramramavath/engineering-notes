# Core Java Language Reference & Syntax Cheat Sheet

> An exhaustive technical quick-reference for Java core syntax, primitive types, bitwise operators, control structures, records, sealed types, pattern matching, and memory boundaries.

| Metadata | Details |
| :--- | :--- |
| **Level** | Comprehensive Reference |
| **Prerequisites** | None |
| **Time to Read** | 35 minutes |
| **Status** | Complete |
| **Java Version** | Java 8 to 25 LTS |

---

## Table of Contents
1. [Primitive Data Types & Memory Bounds](#1-primitive-data-types--memory-bounds)
2. [Operators & Evaluation Precedence](#2-operators--evaluation-precedence)
3. [Bitwise Manipulation & Two's Complement](#3-bitwise-manipulation--twos-complement)
4. [Control Flow & Enhanced Switch Expressions](#4-control-flow--enhanced-switch-expressions)
5. [Arrays & Multidimensional Storage](#5-arrays--multidimensional-storage)
6. [Classes, Records, Interfaces & Sealed Types](#6-classes-records-interfaces--sealed-types)
7. [Access Modifiers & Visibility Inheritance](#7-access-modifiers--visibility-inheritance)
8. [Variable Storage Specifiers & Memory Scopes](#8-variable-storage-specifiers--memory-scopes)
9. [Type Casting & Numeric Promotion Rules](#9-type-casting--numeric-promotion-rules)
10. [Pattern Matching & Type Deconstruction](#10-pattern-matching--type-deconstruction)
11. [Standard Annotations Reference](#11-standard-annotations-reference)
12. [HotSpot Memory Footprints & Object Headers](#12-hotspot-memory-footprints--object-headers)
13. [Practical Code Recipes & Idioms](#13-practical-code-recipes--idioms)
14. [Compiler Pitfalls & Gotchas](#14-compiler-pitfalls--gotchas)

---

## 1. Primitive Data Types & Memory Bounds

| Type | Bits | Bytes | Default | Minimum Value | Maximum Value | Wrapper Class |
| :--- | :---: | :---: | :---: | :--- | :--- | :--- |
| `byte` | 8 | 1 | `0` | $-128$ ($-2^7$) | $127$ ($2^7-1$) | `java.lang.Byte` |
| `short` | 16 | 2 | `0` | $-32,768$ ($-2^{15}$) | $32,767$ ($2^{15}-1$) | `java.lang.Short` |
| `int` | 32 | 4 | `0` | $-2,147,483,648$ ($-2^{31}$) | $2,147,483,647$ ($2^{31}-1$) | `java.lang.Integer` |
| `long` | 64 | 8 | `0L` | $-9,223,372,036,854,775,808$ ($-2^{63}$) | $9,223,372,036,854,775,807$ ($2^{63}-1$) | `java.lang.Long` |
| `float` | 32 | 4 | `0.0f` | $\approx 1.4 \times 10^{-45}$ (IEEE 754) | $\approx 3.4028235 \times 10^{38}$ | `java.lang.Float` |
| `double` | 64 | 8 | `0.0d` | $\approx 4.9 \times 10^{-324}$ (IEEE 754) | $\approx 1.7976931348623157 \times 10^{308}$ | `java.lang.Double` |
| `char` | 16 | 2 | `'\u0000'` | `'\u0000'` ($0$) | `'\uffff'` ($65,535$) | `java.lang.Character` |
| `boolean`| JVM | JVM | `false` | `false` | `true` | `java.lang.Boolean` |

```java
// Literal Syntax Examples
int dec = 42;
int hex = 0x2A;
int bin = 0b0010_1010;
int oct = 052;
long large = 10_000_000_000L;
float fVal = 3.14159f;
double dVal = 3.141592653589793d;
char cUni = '\u03A9'; // Greek capital letter Omega
```

---

## 2. Operators & Evaluation Precedence

| Level | Category | Operators | Associativity | Description |
| :---: | :--- | :--- | :---: | :--- |
| 15 | Postfix | `expr++`, `expr--` | Left-to-right | Post-increment and post-decrement |
| 14 | Unary Prefix | `++expr`, `--expr`, `+expr`, `-expr`, `~`, `!` | Right-to-left | Pre-increment, negation, bitwise NOT, logical NOT |
| 13 | Creation & Cast | `new`, `(type) expr` | Right-to-left | Object creation, type casting |
| 12 | Multiplicative | `*`, `/`, `%` | Left-to-right | Multiplication, division, modulo remainder |
| 11 | Additive | `+`, `-` | Left-to-right | Addition, subtraction, string concatenation |
| 10 | Shift | `<<`, `>>`, `>>>` | Left-to-right | Signed left shift, signed right shift, unsigned right shift |
| 9 | Relational | `<`, `>`, `<=`, `>=`, `instanceof` | Left-to-right | Numeric comparison, type comparison |
| 8 | Equality | `==`, `!=` | Left-to-right | Value equality (primitives) or reference identity |
| 7 | Bitwise AND | `&` | Left-to-right | Bitwise AND, non-short-circuit logical AND |
| 6 | Bitwise XOR | `^` | Left-to-right | Bitwise exclusive OR |
| 5 | Bitwise OR | `|` | Left-to-right | Bitwise inclusive OR, non-short-circuit logical OR |
| 4 | Logical AND | `&&` | Left-to-right | Short-circuit logical AND |
| 3 | Logical OR | `||` | Left-to-right | Short-circuit logical OR |
| 2 | Ternary | `? :` | Right-to-left | Conditional ternary operator |
| 1 | Assignment | `=`, `+=`, `-=`, `*=`, `/=`, `%=`, `&=`, `^=`, `|=`, `<<=`, `>>=`, `>>>=` | Right-to-left | Variable assignment operations |

---

## 3. Bitwise Manipulation & Two's Complement

Java represents all signed integers using **Two's Complement** notation.
The most significant bit (MSB) denotes sign: `0` for positive, `1` for negative.
To compute negative representation: invert all bits (`~x`) and add 1 (`~x + 1 == -x`).

```
  Positive 5:   0000 0000 0000 0000 0000 0000 0000 0101
  Invert (~5):  1111 1111 1111 1111 1111 1111 1111 1010
  Add 1 (-5):   1111 1111 1111 1111 1111 1111 1111 1011
```

### Essential Bitwise Idioms
```java
// 1. Check if integer is power of two
boolean isPowerOfTwo = (x > 0) && ((x & (x - 1)) == 0);

// 2. Clear the lowest set bit
x = x & (x - 1);

// 3. Extract the lowest set bit
int lowestBit = x & -x;

// 4. Set, clear, and toggle bit at index k
int setBit = x | (1 << k);
int clearBit = x & ~(1 << k);
int toggleBit = x ^ (1 << k);
boolean isSet = (x & (1 << k)) != 0;

// 5. Unsigned shift vs Signed shift
int neg = -8;
int signedShift = neg >> 2;    // -2 (Sign bit 1 replicated on left)
int unsignedShift = neg >>> 2;  // 1073741822 (0 inserted on left)
```

---

## 4. Control Flow & Enhanced Switch Expressions

### Enhanced Switch Expression (Java 14+)
```java
// Yielding values with arrow syntax
String dayType = switch (day) {
    case MONDAY, TUESDAY, WEDNESDAY, THURSDAY, FRIDAY -> "Workday";
    case SATURDAY, SUNDAY -> "Weekend";
    default -> {
        if (day == null) yield "Unknown Day";
        yield "Invalid";
    }
};
```

### Pattern Matching Switch with Guards (Java 21+)
```java
static String formatValue(Object obj) {
    return switch (obj) {
        case null -> "Null reference";
        case Integer i -> String.format("Integer: %d", i);
        case Long l -> String.format("Long: %d", l);
        case Double d when d > 100.0 -> String.format("High Double: %.2f", d); // Guard condition
        case Double d -> String.format("Normal Double: %.2f", d);
        case String s -> "String of length " + s.length();
        default -> "Unknown Object: " + obj.toString();
    };
}
```

### Labeled Loops & Break/Continue
```java
searchLoop:
for (int i = 0; i < matrix.length; i++) {
    for (int j = 0; j < matrix[i].length; j++) {
        if (matrix[i][j] == target) {
            foundRow = i;
            foundCol = j;
            break searchLoop; // Breaks directly out of outer loop
        }
    }
}
```

---

## 5. Arrays & Multidimensional Storage

```java
// Allocation and initialization
int[] numbers = new int[5]; // Default zero-filled
int[] primes = {2, 3, 5, 7, 11};
int[] cloned = primes.clone();

// Multidimensional arrays (Array of array references)
int[][] grid = new int[3][3];
int[][] jagged = new int[3][];
jagged[0] = new int[2];
jagged[1] = new int[5];
jagged[2] = new int[1];

// Standard java.util.Arrays utilities
Arrays.sort(primes);
int index = Arrays.binarySearch(primes, 7);
Arrays.fill(numbers, -1);
boolean isEqual = Arrays.equals(primes, cloned);
boolean isDeepEqual = Arrays.deepEquals(grid, grid);
String arrayRepr = Arrays.toString(primes);
```

---

## 6. Classes, Records, Interfaces & Sealed Types

### Records (Java 16+)
```java
// Immutable data carrier with auto-generated constructor, getters, equals, hashCode, toString
public record User(long id, String name, String email) {
    // Compact constructor for validation and normalization
    public User {
        Objects.requireNonNull(name, "Name cannot be null");
        email = email.toLowerCase().trim();
    }
}
```

### Sealed Hierarchies (Java 17+)
```java
// Exhaustive subtyping controlled by the permits clause
public sealed interface Shape permits Circle, Rectangle, Square {}

public final class Circle implements Shape { private final double radius; public Circle(double r) { this.radius = r; } }
public final class Rectangle implements Shape { private final double w, h; public Rectangle(double w, double h) { this.w = w; this.h = h; } }
public final class Square implements Shape { private final double side; public Square(double s) { this.side = s; } }
```

---

## 7. Access Modifiers & Visibility Inheritance

| Modifier | Current Class | Package Members | Subclasses (Different Package) | Universal World |
| :--- | :---: | :---: | :---: | :---: |
| `public` | Yes | Yes | Yes | Yes |
| `protected` | Yes | Yes | Yes (via inheritance) | No |
| *default* (package-private) | Yes | Yes | No | No |
| `private` | Yes | No | No | No |

### Method Overriding Visibility Rules:
A subclass method overriding a superclass method **cannot reduce visibility**.
- Overriding `protected` $	o$ may become `protected` or `public`.
- Overriding `public` $	o$ must remain `public`.

---

## 8. Variable Storage Specifiers & Memory Scopes

- **Local Variables:** Stored on the thread call stack frame. Allocated upon method entry; deallocated upon frame pop. Must be explicitly initialized.
- **Instance Fields:** Allocated on the Java heap as part of object memory layout. Initialized to defaults (`0`, `0.0`, `false`, `null`).
- **Static Fields:** Allocated on the Java heap as part of the `Class` object in HotSpot.
- **`final`:**
  - Primitive variable: Value is immutable once assigned.
  - Reference variable: Memory reference is immutable; referenced object internal state may still be mutated.
  - Method: Disallows overriding in subclasses.
  - Class: Disallows inheritance.
- **`volatile`:** Enforces memory visibility across threads (flushes CPU write buffers) and forbids instruction reordering.
- **`transient`:** Instructs Java Serialization to ignore this field during persistence.

---

## 9. Type Casting & Numeric Promotion Rules

### Primitive Promotion Hierarchy
$$	ext{byte} 	o 	ext{short} 	o 	ext{int} 	o 	ext{long} 	o 	ext{float} 	o 	ext{double}$$
$$	ext{char} 	o 	ext{int}$$

### Binary Numeric Promotion Algorithm
1. If either operand is `double`, the other operand is promoted to `double`.
2. Otherwise, if either operand is `float`, the other is promoted to `float`.
3. Otherwise, if either operand is `long`, the other is promoted to `long`.
4. Otherwise, **both operands are promoted to `int`**.

```java
short s1 = 10;
short s2 = 20;
// short s3 = s1 + s2; // COMPILE ERROR: s1 + s2 evaluates to int!
short s3 = (short) (s1 + s2); // Correct explicit cast
```

---

## 10. Pattern Matching & Type Deconstruction

### Pattern Matching for `instanceof` (Java 16+)
```java
if (obj instanceof String s && s.length() > 5) {
    System.out.println("Long string: " + s.toUpperCase());
}
```

### Record Patterns (Java 21+)
```java
public record Point(int x, int y) {}
public record BoundingBox(Point topLeft, Point bottomRight) {}

static void inspect(Object obj) {
    if (obj instanceof BoundingBox(Point(int x1, int y1), Point(int x2, int y2))) {
        int width = Math.abs(x2 - x1);
        int height = Math.abs(y2 - y1);
        System.out.printf("Box dimensions: %d x %d%n", width, height);
    }
}
```

---

## 11. Standard Annotations Reference

| Annotation | Target | Retention | Purpose |
| :--- | :--- | :--- | :--- |
| `@Override` | Method | `SOURCE` | Asserts method overrides superclass or interface declaration |
| `@Deprecated` | Any declaration | `RUNTIME` | Marks element as obsolete; supports `since` and `forRemoval` |
| `@SuppressWarnings`| Any declaration | `SOURCE` | Suppresses compiler warnings (e.g. `"unchecked"`, `"rawtypes"`) |
| `@FunctionalInterface` | Interface | `RUNTIME` | Enforces single abstract method (SAM) contract |
| `@SafeVarargs` | Static/final method | `RUNTIME` | Suppresses unchecked warnings on parameterized varargs |

---

## 12. HotSpot Memory Footprints & Object Headers

### 64-bit JVM Memory Layout (with Compressed OOPs $\le 32	ext{GB}$)
- **Mark Word:** 8 bytes (Hashcode, GC age, lock bits, biased lock flags).
- **Klass Word:** 4 bytes (Pointer to class metadata in Metaspace; 8 bytes if `-XX:-UseCompressedClassPointers`).
- **Object Alignment:** All objects are padded to multiples of 8 bytes.

```
+-------------------------------------------------------------+
|                     Standard Java Object                    |
|  [Mark Word: 8 Bytes] [Klass Pointer: 4 Bytes]              |
|  [Instance Fields: 0 to N Bytes]                            |
|  [Padding to align to 8-byte boundary: 0 to 7 Bytes]        |
+-------------------------------------------------------------+
```

| Type | Shallow Size | Calculation |
| :--- | :---: | :--- |
| `new Object()` | 16 bytes | 8 (Mark) + 4 (Klass) + 4 (Padding) |
| `new Integer(42)` | 16 bytes | 8 (Mark) + 4 (Klass) + 4 (int field) |
| `new Long(42L)` | 24 bytes | 8 (Mark) + 4 (Klass) + 8 (long field) + 4 (Pad) |
| `int[0]` | 16 bytes | 8 (Mark) + 4 (Klass) + 4 (array length) |
| `int[10]` | 56 bytes | 16 (Header) + 40 (10 ints) = 56 bytes |

---

## 13. Practical Code Recipes & Idioms

### Recipe 1: Safe Double Comparison
```java
public static boolean approximatelyEqual(double a, double b, double epsilon) {
    return Math.abs(a - b) < epsilon;
}
```

### Recipe 2: Defensive Cloning Pattern
```java
public final class Period {
    private final Date start;
    private final Date end;

    public Period(Date start, Date end) {
        this.start = new Date(start.getTime()); // Defensive copy in constructor
        this.end = new Date(end.getTime());
        if (this.start.after(this.end)) throw new IllegalArgumentException();
    }

    public Date getStart() {
        return new Date(start.getTime()); // Defensive copy in accessor
    }
}
```

### Recipe 3: Safe Enum Singleton
```java
public enum GlobalConfiguration {
    INSTANCE;
    private final Map<String, String> properties = new ConcurrentHashMap<>();
    public String getProperty(String key) { return properties.get(key); }
    public void setProperty(String key, String value) { properties.put(key, value); }
}
```

---

## 14. Compiler Pitfalls & Gotchas

1. **Autoboxing Identity Trap:** `Integer a = 1000; Integer b = 1000; (a == b)` evaluates to `false`. Always use `.equals()`.
2. **Numeric Overflow Silence:** Java arithmetic wraps around without throwing exceptions on overflow. Use `Math.addExact()`, `Math.multiplyExact()` to detect integer overflow.
3. **Compound Assignment Hidden Cast:** `short x = 3; x += 4.6;` compiles cleanly because compound assignments secretly insert narrowing casts: `x = (short) (x + 4.6)`.
4. **NaN Comparisons:** `Double.NaN == Double.NaN` is always `false`. Use `Double.isNaN(d)`.


---

## 15. Exhaustive Operators and Bitwise Reference Recipes

### Bitwise Flag Masking Pattern
```java
public class SecurityPermissionMask {
    public static final int READ = 1 << 0;    // 0001 (1)
    public static final int WRITE = 1 << 1;   // 0010 (2)
    public static final int EXECUTE = 1 << 2; // 0100 (4)
    public static final int DELETE = 1 << 3;  // 1000 (8)

    public static int addPermission(int mask, int permission) {
        return mask | permission;
    }

    public static int removePermission(int mask, int permission) {
        return mask & ~permission;
    }

    public static boolean hasPermission(int mask, int permission) {
        return (mask & permission) == permission;
    }

    public static int togglePermission(int mask, int permission) {
        return mask ^ permission;
    }
}
```

### Fast Base-2 Math Hacks
```java
// Fast multiplication by powers of 2
int multBy16 = x << 4; // x * 16

// Fast division by powers of 2
int divBy8 = x >> 3; // x / 8 (preserves sign)

// Fast modulo by power of 2: x % 2^k == x & (2^k - 1)
int mod32 = x & 31; // x % 32 (only for positive x)

// Absolute value without branching
int absVal = (x ^ (x >> 31)) - (x >> 31);
```

---

## 16. Switch Expressions Comprehensive Grammar & Edge Cases

```java
public class AdvancedSwitchExhaustiveness {
    // Sealed hierarchy exhaustiveness check
    public sealed interface Operation permits Add, Subtract, Multiply, Divide {}
    public record Add(double val) implements Operation {}
    public record Subtract(double val) implements Operation {}
    public record Multiply(double val) implements Operation {}
    public record Divide(double val) implements Operation {}

    public static double apply(double current, Operation op) {
        // Compiler guarantees compile-time exhaustiveness: NO default branch required!
        return switch (op) {
            case Add(double v) -> current + v;
            case Subtract(double v) -> current - v;
            case Multiply(double v) -> current * v;
            case Divide(double v) when v != 0 -> current / v;
            case Divide(double v) -> throw new ArithmeticException("Divide by zero!");
        };
    }
}
```
