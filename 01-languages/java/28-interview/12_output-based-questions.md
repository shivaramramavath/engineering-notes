# Output-Based and Tricky Core Java Interview Mastery

> Master tricky output-based Java interview questions covering method overloading precedence, autoboxing, type promotion, string interning, static initialization order, exception return semantics, and IEEE 754 floating-point traps.

| Metadata | Details |
| :--- | :--- |
| **Level** | Intermediate to Advanced |
| **Prerequisites** | [00_core-java.md](./00_core-java.md), [01_oop.md](./01_oop.md), [02_strings.md](./02_strings.md) |
| **Time to Read** | 45 minutes |
| **Status** | Complete |
| **Java Version** | Java 8 to 25 LTS |

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

### What It Is
Output-based and "code puzzle" interview questions present concise, syntactically valid Java code snippets containing subtle language quirks. The candidate must predict the exact compilation behavior, runtime output, or runtime exception without running the compiler.

### Why It Matters in Interviews
Top tech companies use code-prediction problems not as idle trivia, but as an acid test of a candidate's mastery of the Java Language Specification (JLS). They expose whether an engineer truly understands:
- Method resolution algorithms (JLS §15.12)
- Type widening vs. autoboxing vs. varargs precedence
- Class initialization phase ordering (JLS §12.4)
- Abrupt completion in `try-catch-finally` blocks (JLS §14.20.2)
- String pool deduplication and constant folding (JLS §3.10.5)

---

## 2. Core Concepts

### Core Puzzler Taxonomies

1. **Overloading Resolution Hierarchy (Most Specific Rule):**
   When resolving overloaded methods, the compiler follows a strict 3-phase priority:
   - Phase 1: Identity / Subtyping conversion without boxing or varargs (Widening primitive or reference types).
   - Phase 2: Boxing/unboxing conversion (Autoboxing).
   - Phase 3: Variable arity (Varargs `...`).
   - If two methods are equally specific at the same phase (e.g., `test(String s)` vs `test(Integer i)` called with `null`), compilation fails with `reference to test is ambiguous`.

2. **Static and Instance Initialization Order:**
   When a subclass instance is instantiated, the evaluation sequence is invariant:
   1. Superclass static variable initializers & static blocks (in textual order).
   2. Subclass static variable initializers & static blocks (in textual order).
   3. Superclass instance field initializers & instance initializer blocks.
   4. Superclass constructor body.
   5. Subclass instance field initializers & instance initializer blocks.
   6. Subclass constructor body.

3. **Try-Catch-Finally Abrupt Completion:**
   A `finally` block executes before control transfers out of a `try` or `catch` block. If the `finally` block executes a `return` or throws an exception, it **discards and suppresses** any return value or unhandled exception originating inside `try` or `catch`.

4. **Floating Point IEEE 754 Quirks:**
   - `Double.NaN == Double.NaN` evaluates to `false`.
   - `0.1 + 0.2 == 0.3` evaluates to `false` due to binary floating-point representation limits.
   - `Double.valueOf(Double.NaN).equals(Double.valueOf(Double.NaN))` evaluates to `true` (special JLS/JDK contract).

---

## 3. How It Works Internally

### Class Loading and Initialization Lifecycle

```
[ClassLoader Phase]
       |
       v
  1. Loading: Bytecode read into memory -> Instance of java.lang.Class created
       |
       v
  2. Linking:
     a. Verification: Bytecode verifier ensures structural & type safety
     b. Preparation: Static fields allocated & initialized to DEFAULT zero/null values
     c. Resolution: Symbolic references in constant pool resolved to direct memory references
       |
       v
  3. Initialization (<clinit>):
     - Executes static field explicit assignments and static blocks in textual order
     - Superclass <clinit> runs BEFORE subclass <clinit>
       |
       v
  4. Instantiation (<init>):
     - Superclass <init> -> Subclass fields/blocks -> Subclass <init>
```

#### Bytecode: Static Initialization Order (`<clinit>`)
In bytecode, all static variable assignments and `static { ... }` blocks are compiled into a synthetic method named `<clinit>`. The JVM guarantees `<clinit>` is thread-safe and executed at most once per classloader before the first active use of the class.

---

## 4. Syntax & Usage

### Common Ambiguity & Resolution Patterns

#### Overloading Priority Matrix
```
Call: foo(10)
Method Candidates:
1. void foo(long x)     <-- MATCH (Widening beats boxing)
2. void foo(Integer x)  <-- Skipped (boxing is Phase 2)
3. void foo(int... x)   <-- Skipped (varargs is Phase 3)
```

#### Autoboxing Cache Range
The JVM caches boxed wrappers for values between `-128` and `127` inclusive (for `Byte`, `Short`, `Integer`, `Long`, and characters `\u0000` to `\u007f`).
```java
Integer a = 127;
Integer b = 127;
System.out.println(a == b); // TRUE (Cached reference identity)

Integer c = 128;
Integer d = 128;
System.out.println(c == d); // FALSE (Distinct heap allocations)
```

---

## 5. Practical Examples

### Example 1: Method Overloading with `null` Argument
```java
package com.engineering.puzzles;

public class OverloadingNullPuzzle {
    public static void test(Object obj) {
        System.out.println("Object version");
    }

    public static void test(String str) {
        System.out.println("String version");
    }

    public static void main(String[] args) {
        test(null); 
        // Output: "String version"
        // Reason: JLS §15.12.2.5 - String is a subtype of Object, so it is strictly MORE SPECIFIC.
    }
}
```

### Example 2: Ambiguous Overloading Compilation Failure
```java
package com.engineering.puzzles;

public class AmbiguousNullPuzzle {
    public static void test(Integer num) {
        System.out.println("Integer version");
    }

    public static void test(String str) {
        System.out.println("String version");
    }

    public static void main(String[] args) {
        // Uncommenting the next line fails compilation:
        // test(null);
        // Error: reference to test is ambiguous; both test(Integer) and test(String) match.
        // Reason: Neither String nor Integer is a subtype of the other!
    }
}
```

### Example 3: Polymorphic Field Access Trap
```java
package com.engineering.puzzles;

class Parent {
    int value = 10;
    void printValue() { System.out.println("Parent method: " + value); }
}

class Child extends Parent {
    int value = 20; // Field shadowing, NOT overriding!
    @Override
    void printValue() { System.out.println("Child method: " + value); }
}

public class FieldShadowingPuzzle {
    public static void main(String[] args) {
        Parent ref = new Child();
        System.out.println("Direct field access: " + ref.value); // 10!
        ref.printValue(); // "Child method: 20"
        // Reason: Fields are NOT polymorphic in Java. Field resolution is bound statically
        // at compile-time by the reference type (Parent). Methods are dispatched dynamically.
    }
}
```

### Example 4: Try-Finally Return Value Masking
```java
package com.engineering.puzzles;

public class TryFinallyPuzzle {
    public static int compute() {
        try {
            return 1;
        } finally {
            return 2; // Swallows return 1!
        }
    }

    public static int computeWithPrimitiveModification() {
        int x = 10;
        try {
            return x; // Evaluates x (10) and places it on the operand stack
        } finally {
            x = 20; // Modifies local variable, but operand stack already holds 10!
        }
    }

    public static void main(String[] args) {
        System.out.println("compute(): " + compute()); // 2
        System.out.println("computeWithPrimitive(): " + computeWithPrimitiveModification()); // 10
    }
}
```

### Example 5: Static Initialization Ordering Trap
```java
package com.engineering.puzzles;

class Base {
    static { System.out.print("A "); }
    { System.out.print("B "); }
    Base() { System.out.print("C "); }
}

class Derived extends Base {
    static { System.out.print("D "); }
    { System.out.print("E "); }
    Derived() { System.out.print("F "); }
}

public class InitOrderPuzzle {
    public static void main(String[] args) {
        System.out.print("Start -> ");
        new Derived();
        System.out.print("| ");
        new Derived();
    }
    // Output: Start -> A D B C E F | B C E F
    // Reason: Static blocks run only once on first active class load (A D).
    // Instance blocks and constructors run on every object instantiation (B C E F).
}
```

---

## 6. Best Practices

1. **Never Shadow Fields in Subclasses:**
   - Subclasses should never declare fields with the exact same name as superclass fields. Use getter methods to preserve polymorphic dispatch.

2. **Never Return from a `finally` Block:**
   - Using `return`, `throw`, `break`, or `continue` inside `finally` discards exceptions thrown in the `try` block and causes severe diagnostic blind spots.

3. **Explicitly Cast Ambiguous `null` Calls:**
   - When calling overloaded methods accepting reference types with `null`, always specify the target type: `test((String) null)`.

4. **Use `equals()` for Wrapper Comparisons:**
   - Never compare boxed wrappers (`Integer`, `Long`) using `==`. Always use `Objects.equals(a, b)` or `.intValue()`.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Fix |
| :--- | :--- | :--- |
| **Assuming Fields Override Polymorphically** | Variables are resolved statically at compile time based on the declared reference type. | Make fields `private` and access through virtual getter methods. |
| **Comparing Double.NaN with `==`** | IEEE 754 mandates that `NaN` is not equal to any value, including itself (`NaN == NaN` is `false`). | Use `Double.isNaN(value)` or `Double.compare(a, b) == 0`. |
| **Assuming `finally` Always Runs** | If the JVM terminates via `System.exit(0)`, host OS SIGKILL, or fatal hardware crash, `finally` does not execute. | Do not rely solely on `finally` for transactional commits; use two-phase commit or write-ahead logs. |
| **Using `+` with String and Integers** | `"Value: " + 1 + 2` evaluates left-to-right as `"Value: 12"`, whereas `"Value: " + (1 + 2)` evaluates to `"Value: 3"`. | Use parentheses to enforce arithmetic precedence before string conversion. |

---

## 8. Performance & Security Notes

### Performance Notes
- **String Constant Folding:** The Java compiler automatically folds string literal concatenation at compile time. `"a" + "b" + "c"` compiles directly to `ldc "abc"` in bytecode, costing zero runtime heap allocations.
- **Autoboxing in Loops:** Writing `Long sum = 0L; for (long i = 0; i < N; i++) sum += i;` creates $N$ temporary heap-allocated `Long` objects, causing severe GC pressure. Always use primitive types for loop accumulators.

### Security Notes
- **Subclassing Overridable Methods in Constructors:** Calling an overridable method inside a constructor allows a subclass to execute code *before* the subclass instance fields have been initialized, exposing uninitialized state (a common vulnerability in security frameworks).

---

## 9. Comparison / When to Use What

### Compilation Phase Resolution Matrix

| Scenario | Candidate 1 | Candidate 2 | Winning Choice | Reason |
| :--- | :--- | :--- | :--- | :--- |
| `foo(10)` | `foo(long x)` | `foo(Integer x)` | `foo(long x)` | Widening beats Autoboxing (Phase 1 beats Phase 2) |
| `foo(10)` | `foo(Integer x)` | `foo(int... x)` | `foo(Integer x)` | Autoboxing beats Varargs (Phase 2 beats Phase 3) |
| `foo(null)` | `foo(Object o)` | `foo(String s)` | `foo(String s)` | String is more specific subtype of Object |
| `foo(null)` | `foo(Integer i)` | `foo(String s)` | **Compile Error** | Unrelated types; ambiguous invocation |

---

## 10. Interview Questions

### Q1: What is the output of the following code and why?
```java
public class StringTrap {
    public static void main(String[] args) {
        String s1 = "Java";
        String s2 = "Ja" + "va";
        String s3 = "Ja";
        String s4 = s3 + "va";
        System.out.println(s1 == s2);
        System.out.println(s1 == s4);
    }
}
```
<details>
<summary>Detailed Answer</summary>

**Output:**
```
true
false
```
**Explanation:**
1. `s1` and `s2`: Both `"Java"` and `"Ja" + "va"` are compile-time constants. The compiler folds `"Ja" + "va"` into `"Java"` and interns it in the String Constant Pool. Therefore, `s1 == s2` evaluates to `true`.
2. `s4`: Because `s3` is a variable reference (non-constant expression), `s3 + "va"` cannot be folded at compile-time. At runtime, the JVM uses `invokedynamic` (or `StringBuilder`) to allocate a new `String` instance on the heap. Therefore, `s1 == s4` evaluates to `false`.
</details>

### Q2: What is the output of the following polymorphic constructor call?
```java
class A {
    A() { print(); }
    void print() { System.out.println("A"); }
}
class B extends A {
    int x = 42;
    void print() { System.out.println("B, x=" + x); }
}
public class Test {
    public static void main(String[] args) {
        new B();
    }
}
```
<details>
<summary>Detailed Answer</summary>

**Output:**
```
B, x=0
```
**Explanation:**
When `new B()` is instantiated:
1. `B`'s constructor first invokes `super()` (class `A`'s constructor).
2. `A`'s constructor calls `print()`. Because method dispatch in Java is dynamic (virtual), the overridden method `B.print()` is executed.
3. However, `B`'s instance field initializers have **not yet run**! The memory for `B.x` has only been initialized to its default zero value (`0`).
4. Therefore, `B.print()` prints `"B, x=0"`. After `A`'s constructor completes, `B.x` is assigned `42`.
</details>

### Q3: What happens when an exception is thrown in both `try` and `finally`?
```java
public class ExceptionSuppressionPuzzle {
    public static int test() {
        try {
            throw new RuntimeException("Try Error");
        } finally {
            throw new IllegalStateException("Finally Error");
        }
    }
    public static void main(String[] args) {
        test();
    }
}
```
<details>
<summary>Detailed Answer</summary>

**Output:**
Throws `java.lang.IllegalStateException: Finally Error`.
**Explanation:**
An abrupt completion in a `finally` block completely overrides and suppresses any pending unhandled exception or return value originating in the `try` block. The original `RuntimeException("Try Error")` is lost unless captured via Java 7 try-with-resources suppression mechanisms.
</details>

### Q4: What does `System.out.println(1.0 / 0.0)` and `System.out.println(0.0 / 0.0)` output?
<details>
<summary>Detailed Answer</summary>

**Output:**
```
Infinity
NaN
```
**Explanation:**
Unlike integer division (`1 / 0`) which throws `ArithmeticException: / by zero`, floating-point arithmetic adheres to the IEEE 754 standard:
- Dividing a positive non-zero floating-point number by zero yields `Double.POSITIVE_INFINITY`.
- Dividing zero by zero (`0.0 / 0.0`) yields `Double.NaN` (Not a Number).
</details>

### Q5: What is the output of `Integer a = 1000; Integer b = 1000; System.out.println(a <= b && a >= b && a != b);`?
<details>
<summary>Detailed Answer</summary>

**Output:**
`true`
**Explanation:**
1. Comparison operators `<=`, `>=`, `<`, and `>` trigger **unboxing** conversion. Thus, `a <= b` and `a >= b` compare the primitive `int` values (`1000 <= 1000` and `1000 >= 1000`), both evaluating to `true`.
2. Equality operators `==` and `!=` when applied to wrapper objects compare **reference identity**. Because `1000` is outside the default Integer Cache range (`-128` to `127`), `a` and `b` reference distinct heap objects. Thus `a != b` is `true`.
</details>

### Q6: Can a constructor be `final`, `static`, or `synchronized` in Java?
<details>
<summary>Detailed Answer</summary>

**Answer:**
**None of them.**
- A constructor cannot be `final` because constructors are never inherited or overridden by subclasses.
- A constructor cannot be `static` because a constructor is specifically invoked to initialize a newly allocated instance of a class.
- A constructor cannot be `synchronized` because the newly created object is confined to the thread that instantiated it until construction completes. Synchronizing on `this` inside the constructor body is permitted if thread leakage occurs.
</details>

### Q7: What is the difference between `i++` and `++i` in bytecode?
<details>
<summary>Detailed Answer</summary>

- `i++` (postfix): Loads the current value of `i` onto the operand stack (`iload`), then increments the local variable directly in the local variable array (`iinc`). The old value on the stack is consumed.
- `++i` (prefix): Increments the local variable directly in the local variable array (`iinc`), then loads the updated value onto the operand stack (`iload`).
- Example trap: `int i = 0; i = i++;` results in `i = 0` because `i++` places `0` on the stack, increments `i` to `1` in local storage, and then assigns the stack value `0` back into `i`!
</details>

### Q8: What is the output of `System.out.println(Math.min(Double.NaN, 0.0))`?
<details>
<summary>Detailed Answer</summary>

**Output:**
`NaN`
**Explanation:**
According to the specification of `Math.min(double a, double b)`, if either argument is `NaN`, the result is `NaN`.
</details>

---

## 11. Practice Exercises

### Exercise 1: Predict the Output of Static and Instance Blocks
**Task:** Without running the code, predict the exact console output of the program below.

<details>
<summary>Solution</summary>

```java
package com.engineering.puzzles;

public class Quiz1 {
    static int count = 0;
    static { count += 10; }
    { count += 5; }
    public Quiz1() { count += 1; }

    public static void main(String[] args) {
        System.out.println(Quiz1.count); // 10 (class loaded, <clinit> ran)
        Quiz1 q1 = new Quiz1();          // instance block (+5) -> ctor (+1) = 16
        Quiz1 q2 = new Quiz1();          // instance block (+5) -> ctor (+1) = 22
        System.out.println(Quiz1.count); // 22
    }
}
```
</details>

### Exercise 2: Tricky Autoboxing with Collection Removal
**Task:** Predict what `list` contains after this code executes.

<details>
<summary>Solution</summary>

```java
package com.engineering.puzzles;

import java.util.ArrayList;
import java.util.List;

public class Quiz2 {
    public static void main(String[] args) {
        List<Integer> list = new ArrayList<>();
        list.add(1);
        list.add(2);
        list.add(3);

        // Note: List has two remove methods:
        // 1. remove(int index)
        // 2. remove(Object o)
        list.remove(1); // Calls remove(int index)! Removes element at index 1 (value 2).
        list.remove(Integer.valueOf(1)); // Calls remove(Object)! Removes value 1.

        System.out.println(list); // [3]
    }
}
```
</details>

### Exercise 3: Array Covariance Runtime Exception
**Task:** Does the following code compile? What happens at runtime?

<details>
<summary>Solution</summary>

```java
package com.engineering.puzzles;

public class Quiz3 {
    public static void main(String[] args) {
        // Arrays in Java are covariant (String[] is a subtype of Object[])
        Object[] objArray = new String[3]; // Compiles cleanly!

        // At runtime, the JVM retains array component type metadata
        try {
            objArray[0] = Integer.valueOf(42); 
        } catch (ArrayStoreException e) {
            System.out.println("Caught expected: " + e.getClass().getSimpleName());
            // Output: Caught expected: ArrayStoreException
        }
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **Widening > Boxing > Varargs:** Method overloading prioritizes primitive widening first, boxing/unboxing second, and varargs last.
- **Specific Subtype Overloading:** `foo(null)` selects the most specific subtype method (`String` over `Object`). If types are unrelated (`String` vs `Integer`), it fails compilation.
- **Field Shadowing:** Fields are resolved statically by the compile-time reference type, not polymorphically.
- **Finally Return:** A `return` inside `finally` discards any previous return value or exception.
- **Integer Cache:** `Integer.valueOf()` caches `-128` to `127`. `==` compares references, `.equals()` compares values.

---

## 13. Further Reading

- [Java Language Specification (JLS) - Chapter 15: Expressions](https://docs.oracle.com/javase/specs/jls/se21/html/jls-15.html)
- [Java Puzzlers: Traps, Pitfalls, and Corner Cases by Joshua Bloch and Neal Gafter](https://www.oreilly.com/library/view/java-puzzlers-traps/032133678X/)
- [JLS §12.4 - Initialization of Classes and Interfaces](https://docs.oracle.com/javase/specs/jls/se21/html/jls-12.html#jls-12.4)
