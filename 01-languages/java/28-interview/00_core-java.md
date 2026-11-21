# Interview Mastery: Core Java & Language Fundamentals

> Comprehensive interview guide for Core Java fundamentals: JVM execution life cycle, memory areas, object identity, value vs reference mechanics, primitive wrappers, operators, and initialization sequences.

| Metadata | Value |
|---|---|
| **Level** | Intermediate to Advanced |
| **Prerequisites** | [01-fundamentals](../01-fundamentals/README.md), [15-jvm-internals](../15-jvm-internals/README.md) |
| **Time to Read** | 35 minutes |
| **Status** | Complete |
| **Java Version** | Java 8 to Java 25 LTS |

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

Core Java interviews probe deep into language mechanics, memory safety invariants, and execution semantics that distinguish senior backend architects from entry-level developers.

Interviewers evaluate whether candidates understand:
- Why Java is strictly **pass-by-value**, even when passing object references.
- The fundamental distinction between **identity equality (`==`)** and **content equality (`equals()`)**.
- How class loading, static initializers, and instance initialization blocks execute in HotSpot JVM.
- Primitive autoboxing caching rules, two's complement arithmetic, and promotion traps.

---

## 2. Core Concepts

### 1. Strictly Pass-by-Value Semantics
In Java, **everything is passed by value**:
- For primitive types (`int`, `double`, etc.), a bitwise copy of the actual binary value is pushed onto the callee's stack frame.
- For object references (`String`, `List`, custom objects), a bitwise copy of the **memory pointer (OOP / Ordinary Object Pointer)** is passed by value. Reassigning the parameter reference inside a method has **zero effect** on the caller's reference.

### 2. The `equals()` and `hashCode()` Contract
If two objects are equal according to `equals(Object)`:
- Calling `hashCode()` on each must produce the **exact same 32-bit integer**.
- The converse is **not required**: two unequal objects may produce identical hash codes (hash collision).
- If `equals()` is overridden, `hashCode()` **must** be overridden to prevent catastrophic failures in `HashMap`, `HashSet`, and `ConcurrentHashMap`.

### 3. Object Initialization Lifecycle
When a class is instantiated (`new SubClass()`):
1. Class loading, linking, and static initialization of superclass, then subclass (`<clinit>`).
2. Allocation of heap memory zeroed out for all fields.
3. Superclass constructor invocation (`super()`).
4. Subclass instance field initializers and instance initializer blocks `{ ... }` in textual order.
5. Subclass constructor body execution (`<init>`).

---

## 3. How It Works Internally

### Stack Frame Parameter Passing Mechanics
```
Caller Stack Frame:
+-----------------------------------+
| local variable 'node' -> 0x7FA800 |
+-----------------------------------+
                  | (Copies reference bits by value)
                  v
Callee Stack Frame:
+-----------------------------------+
| param 'target'        -> 0x7FA800 |
+-----------------------------------+

Operation: target.val = 42;
-> Mutates object on heap at 0x7FA800 (VISIBLE to caller).

Operation: target = new Node(99);
-> Overwrites callee's local param to 0x8BC100 (INVISIBLE to caller).
```

### HotSpot JVM Integer Cache Mechanics
HotSpot JVM caches boxed integer instances in `java.lang.Integer$IntegerCache`:
```java
// HotSpot caches values from -128 to +127 (configurable via -XX:AutoBoxCacheMax)
Integer a = 127;
Integer b = 127;
System.out.println(a == b); // true (points to same cached heap instance)

Integer x = 128;
Integer y = 128;
System.out.println(x == y); // false (allocates two distinct heap objects!)
```

---

## 4. Syntax & Usage

### Canonical `equals()` and `hashCode()` Implementation (Java 7+ and Java 17+ Records)

```java
import java.util.Objects;

// Java 17+ Record: automatically generates correct equals, hashCode, toString
public record UserRecord(long id, String email, String department) {}

// Standard Class with explicit contract
public final class User {
    private final long id;
    private final String email;
    private final String department;

    public User(long id, String email, String department) {
        this.id = id;
        this.email = Objects.requireNonNull(email, "email must not be null");
        this.department = department;
    }

    @Override
    public boolean equals(Object obj) {
        if (this == obj) return true;
        if (!(obj instanceof User other)) return false; // Pattern matching for instanceof
        return this.id == other.id
            && Objects.equals(this.email, other.email)
            && Objects.equals(this.department, other.department);
    }

    @Override
    public int hashCode() {
        return Objects.hash(id, email, department);
    }
}
```

---

## 5. Practical Examples

### Example 1: Demonstrating Strictly Pass-by-Value

```java
public class PassByValueProof {
    static class Box {
        int value;
        Box(int value) { this.value = value; }
    }

    public static void modify(Box b, int num) {
        b.value = 50;       // Mutates object payload (visible to caller)
        b = new Box(100);   // Rebinds local stack pointer (invisible to caller)
        num = 999;          // Mutates local stack primitive (invisible to caller)
    }

    public static void main(String[] args) {
        Box myBox = new Box(10);
        int myNum = 20;

        modify(myBox, myNum);

        System.out.println("myBox.value: " + myBox.value); // Prints 50
        System.out.println("myNum: " + myNum);             // Prints 20
    }
}
```

### Example 2: Class Initialization Order Trap

```java
public class InitializationOrder {
    static class Parent {
        static { System.out.print("1"); }
        { System.out.print("3"); }
        Parent() { System.out.print("4"); }
    }

    static class Child extends Parent {
        static { System.out.print("2"); }
        { System.out.print("5"); }
        Child() {
            super();
            System.out.print("6");
        }
    }

    public static void main(String[] args) {
        // Output order: 123456
        new Child();
    }
}
```

### Example 3: Autoboxing and Equality Comparison Pitfall

```java
public class AutoboxingTrap {
    public static void verifyCaching() {
        Integer val1 = 100;
        Integer val2 = 100;
        assert val1 == val2 : "Cached range must evaluate to true";

        Integer val3 = 200;
        Integer val4 = 200;
        assert val3 != val4 : "Exceeds cache; distinct object references";
        assert val3.equals(val4) : "Content equality must be true";

        // Unboxing in arithmetic expressions
        Long l1 = 100L;
        assert val1.equals(l1) == false : "Integer.equals(Long) returns false due to type check!";
        assert val1 == l1.intValue() : "Primitive equality unboxes and compares numeric value";
    }
}
```

### Example 4: Floating Point Precision & `BigDecimal`

```java
import java.math.BigDecimal;

public class FloatingPointInterview {
    public static void main(String[] args) {
        // Floating point IEEE 754 precision issue
        double diff = 1.0 - 0.9;
        System.out.println(diff); // 0.09999999999999998 != 0.1

        // Correct monetary handling: String constructor BigDecimal
        BigDecimal bd1 = new BigDecimal("1.0");
        BigDecimal bd2 = new BigDecimal("0.9");
        BigDecimal res = bd1.subtract(bd2);
        System.out.println(res); // 0.1

        // Pitfall: BigDecimal(double) preserves IEEE inaccuracy!
        BigDecimal bad = new BigDecimal(0.1);
        System.out.println(bad); // 0.10000000000000000555111512312578...
    }
}
```

### Example 5: Variable Shadowing vs Overriding

```java
public class FieldHidingInterview {
    static class Base {
        String name = "Base";
        void print() { System.out.println("Base:" + name); }
    }

    static class Derived extends Base {
        String name = "Derived"; // Hides Base.name (does NOT override)
        @Override
        void print() { System.out.println("Derived:" + name); }
    }

    public static void main(String[] args) {
        Base obj = new Derived();
        System.out.println(obj.name); // Prints "Base" (field access resolved at compile time!)
        obj.print();                  // Prints "Derived:Derived" (virtual dispatch at runtime)
    }
}
```

---

## 6. Best Practices

1. **Always Use `Objects.equals()`**: Prevents unexpected `NullPointerException` when comparing nullable references: `Objects.equals(a, b)`.
2. **Never Use `==` for Boxed Primitives**: Always compare wrapped primitives with `.equals()` or unbox explicitly to primitives.
3. **Instantiate `BigDecimal` via String or `valueOf()`**: Never write `new BigDecimal(0.1)`. Always use `new BigDecimal("0.1")` or `BigDecimal.valueOf(0.1)`.
4. **Prefer Immutable Records for DTOs**: Records automatically implement mathematically sound `equals()`, `hashCode()`, and `toString()` methods based on component states.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Fix |
|---|---|---|
| Comparing `Integer == Integer` above 127 | References point to distinct heap allocations outside cache | Use `.equals()` or unbox with `.intValue()`. |
| Overriding `equals` without `@Override` (`equals(User other)`) | Overloads method rather than overriding `equals(Object)`; collection lookups fail | Always add `@Override` and accept `Object obj`. |
| Overriding `equals` without `hashCode` | Objects evaluate equal but map to different hash buckets | Always override `hashCode` whenever `equals` is modified. |
| Using `new BigDecimal(double)` for currency | IEEE 754 precision noise is captured directly in decimal scale | Use `new BigDecimal(String)` or `BigDecimal.valueOf(d)`. |
| Expecting instance variables to override polymorphically | Field access is resolved statically by compiler based on reference type | Make fields `private` and expose polymorphic getter methods. |

---

## 8. Performance & Security Notes

- **Autoboxing Allocation Pressure**: Autoboxing inside tight loops creates millions of short-lived heap wrapper objects, triggering frequent Young Generation GC pauses. Use primitive collections (e.g. Eclipse Collections, Trove) or primitive streams (`IntStream`).
- **Hash Flood Denial-of-Service**: Deliberately crafting strings with identical `hashCode()` used to degrade `HashMap` to $O(N)$ linked lists. Java 8+ mitigates this by treeifying collided buckets into Red-Black trees ($O(\log N)$) when `Comparable` is implemented.
- **Defensive Copying**: When exposing mutable fields from immutable classes (e.g. `java.util.Date` or arrays), return a defensive clone to prevent external modification.

---

## 9. Comparison / When to Use What

| Feature | `==` | `equals()` | `Objects.equals()` |
|---|---|---|---|
| **Primitives** | Compares raw binary value | Compile error | Compile error (unless autoboxed) |
| **Object References** | Compares heap memory address | Logical equality (if overridden) | Logical equality + null-safe |
| **Null Safety** | Null-safe (`a == null`) | Throws NPE if `a == null` | Null-safe (`Objects.equals(null, null) == true`) |
| **Boxed Numbers** | True only if within cache | True if values match and types match | True if values match and types match |

---

## 10. Interview Questions

<details>
<summary>1. Is Java pass-by-reference or pass-by-value? Prove your answer.</summary>

Java is strictly **pass-by-value**. When an object is passed as an argument, what is passed is a bitwise copy of the reference pointer (its address), not the object itself or a reference alias. If you reassign the parameter reference inside the method (`param = new Object()`), the caller's reference remains completely unchanged.
</details>

<details>
<summary>2. What happens if you override `equals()` but do not override `hashCode()`?</summary>

It breaks the core contract of the Java Collections Framework. If two objects are equal according to `equals()`, they must return the same `hashCode()`. If `hashCode()` is not overridden, two logically identical objects will hash to different buckets in a `HashMap` or `HashSet`, resulting in duplicate entries or `get()` returning `null` for a key that exists.
</details>

<details>
<summary>3. Why does `Integer.valueOf("127") == Integer.valueOf("127")` return `true`, while `Integer.valueOf("128") == Integer.valueOf("128")` returns `false`?</summary>

HotSpot JVM maintains an `IntegerCache` for values between `-128` and `+127` (mandated by JLS §5.1.7). For values in this range, `Integer.valueOf()` returns the canonical cached object reference. For numbers $\ge 128$, a new `Integer` object is instantiated on the heap each time, resulting in distinct memory addresses.
</details>

<details>
<summary>4. Explain the execution order of static blocks, instance initializers, constructors, and superclasses.</summary>

1. Superclass static initializer blocks.
2. Subclass static initializer blocks.
3. Superclass instance initializers and field default initializers in textual order.
4. Superclass constructor.
5. Subclass instance initializers and field default initializers in textual order.
6. Subclass constructor.
</details>

<details>
<summary>5. Can a constructor be `final`, `static`, or `abstract`?</summary>

No. Constructors cannot be `final` because constructors are never inherited and cannot be overridden. They cannot be `static` because constructors belong to an instance during its creation lifecycle. They cannot be `abstract` because a constructor must provide concrete initialization code for allocating object memory.
</details>

<details>
<summary>6. What is the difference between `checked` and `unchecked` exceptions at compile-time and runtime?</summary>

- **Checked Exceptions** (subclasses of `Exception` excluding `RuntimeException`) are verified by the compiler. Methods must either handle them (`try-catch`) or declare them in their signature (`throws`).
- **Unchecked Exceptions** (subclasses of `RuntimeException` and `Error`) represent programming bugs or unrecoverable system failures. They do not require explicit declarations or catching.
</details>

<details>
<summary>7. What is the difference between field hiding and method overriding?</summary>

- **Method Overriding** utilizes runtime dynamic dispatch based on the actual runtime object type on the heap.
- **Field Hiding** occurs when a subclass declares a field with the same name as a superclass. Field access is **resolved at compile-time** based strictly on the reference type, never through dynamic dispatch.
</details>

<details>
<summary>8. Why is `String` immutable in Java?</summary>

1. **Security**: Strings carry database credentials, network URLs, and file paths; immutability prevents malicious alteration after permission checks.
2. **String Pool Caching**: Caching literals in the JVM String Pool requires that strings never change; otherwise mutating one literal would corrupt all other references.
3. **Thread Safety**: Immutable strings are inherently thread-safe without synchronization.
4. **Hash Code Caching**: `String` pre-computes and caches its `hashCode` at first use, making it ideal for `HashMap` keys.
</details>

---

## 11. Practice Exercises

### Exercise 1: Implement an Immutable Deep-Copy Class
Create a thread-safe immutable `Student` class holding a mutable `java.util.Date` and `List<String>` courses.

<details>
<summary>Solution</summary>

```java
import java.util.*;

public final class Student {
    private final String name;
    private final Date graduationDate;
    private final List<String> courses;

    public Student(String name, Date graduationDate, List<String> courses) {
        this.name = name;
        // Defensive copy of mutable Date
        this.graduationDate = graduationDate == null ? null : new Date(graduationDate.getTime());
        // Defensive copy and unmodifiable wrapping of List
        this.courses = courses == null ? List.of() : List.copyOf(courses);
    }

    public String getName() { return name; }

    public Date getGraduationDate() {
        // Return defensive clone
        return graduationDate == null ? null : new Date(graduationDate.getTime());
    }

    public List<String> getCourses() {
        return courses; // Already unmodifiable via List.copyOf
    }
}
```
</details>

### Exercise 2: Fix the Broken `HashCode` Contract
Identify and fix the bug in this employee cache key.

```java
// BUGGY CODE:
class EmployeeKey {
    int id;
    String name;
    public boolean equals(Object o) {
        if (!(o instanceof EmployeeKey e)) return false;
        return this.id == e.id;
    }
    // hashCode() is omitted!
}
```

<details>
<summary>Solution</summary>

```java
import java.util.Objects;

public final class EmployeeKey {
    private final int id;
    private final String name;

    public EmployeeKey(int id, String name) {
        this.id = id;
        this.name = name;
    }

    @Override
    public boolean equals(Object o) {
        if (this == o) return true;
        if (!(o instanceof EmployeeKey e)) return false;
        return this.id == e.id && Objects.equals(this.name, e.name);
    }

    @Override
    public int hashCode() {
        return Objects.hash(id, name);
    }
}
```
</details>

### Exercise 3: Predict the Output of Static and Instance Polymorphic Calls

```java
class A {
    static void display() { System.out.print("A_STAT "); }
    void show() { System.out.print("A_INST "); }
}
class B extends A {
    static void display() { System.out.print("B_STAT "); }
    void show() { System.out.print("B_INST "); }
}
public class Test {
    public static void main(String[] args) {
        A obj = new B();
        obj.display();
        obj.show();
    }
}
```

<details>
<summary>Solution</summary>

**Output**: `A_STAT B_INST `

**Explanation**:
1. `obj.display()` calls a **static method**. Static methods do not participate in runtime virtual method dispatch; the compiler resolves the call based on the declared type `A`, invoking `A.display()`.
2. `obj.show()` calls an **instance method**. Virtual method dispatch resolves the method at runtime based on the actual object on the heap (`new B()`), invoking `B.show()`.
</details>

---

## 12. Summary / Cheat Sheet

- **Parameter Passing**: Strictly pass-by-value. Pointers are copied; rebinding parameters does not affect caller.
- **Integer Cache**: Defaults from `-128` to `+127`. Always use `.equals()` for object wrappers.
- **Equality Contract**: If `a.equals(b)` is true, `a.hashCode() == b.hashCode()` must be true.
- **Static vs Instance**: Static members resolve at compile-time by reference; instance methods resolve at runtime by object instance.
- **BigDecimal**: Never initialize with `double`; use `new BigDecimal("0.1")`.

---

## 13. Further Reading

- [James Gosling et al.: The Java Language Specification (Java SE 25 Edition)](https://docs.oracle.com/javase/specs/)
- [Joshua Bloch: Effective Java (3rd Edition), Items 10-14: Methods Common to All Objects](https://www.oreilly.com/library/view/effective-java-3rd/9780134686097/)
- [Baeldung: Guide to Pass-by-Value in Java](https://www.baeldung.com/java-pass-by-value-or-pass-by-reference)
- [OpenJDK Source Code: `java.lang.Object`](https://github.com/openjdk/jdk/blob/master/src/java.base/share/classes/java/lang/Object.java)
