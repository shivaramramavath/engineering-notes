# Bounded Type Parameters

> Restricting type parameters to specific class hierarchies or interface contracts to unlock member access and guarantee compile-time polymorphism.

| Metadata | Value |
|---|---|
| **Level** | Intermediate |
| **Prerequisites** | [00_generic-classes-and-methods.md](00_generic-classes-and-methods.md), [04-oop/08_abstract-classes.md](../04-oop/08_abstract-classes.md), [04-oop/09_interfaces.md](../04-oop/09_interfaces.md) |
| **Time to Read** | 24 minutes |
| **Status** | Active |
| **Java Version** | Java 5+ |

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

Unbounded type parameters (like `<T>`) allow any object reference to be passed as a type argument. While this provides maximum generality, it limits the operations that can be performed on the generic instance. Because `T` could be any type, the compiler only permits calling methods declared on `java.lang.Object` (such as `toString()`, `equals()`, and `hashCode()`).

In many real-world scenarios, an algorithm requires specific capabilities from its arguments:
- Comparing elements (`compareTo`)
- Performing arithmetic operations on numbers (`doubleValue`, `intValue`)
- Closing resources (`close`)
- Serializing or cloning objects

**Bounded Type Parameters** solve this problem. By specifying an upper bound using the `extends` keyword (e.g., `<T extends Comparable<T>>`), you restrict the types that can be substituted for `T` and gain compile-time access to the methods declared by the bounding class or interface.

---

## 2. Core Concepts

### Single Upper Bound
An upper bound specifies that the type argument must either be the bounding class or a subclass of it (if a class), or implement the bounding interface (if an interface). In generic bounds, the keyword `extends` is used for **both classes and interfaces**:

```java
// T must be a subclass of Number (or Number itself)
public class NumberBox<T extends Number> {
    private T number;
    public double toDouble() {
        return number.doubleValue(); // Valid: doubleValue() is on Number!
    }
}
```

### Multiple Upper Bounds
A type parameter can have multiple bounds using the `&` operator:
```java
public class Processor<T extends Number & Comparable<T> & java.io.Serializable> {
    // T satisfies all three bounds
}
```
**Syntactic Rule**: If one of the bounds is a class, it **must** be specified first. Java does not support multiple class inheritance, so at most one class bound is permitted, followed by zero or more interface bounds.

### Recursive Type Bounds
A recursive type bound is a bound where the type parameter appears within its own bounding expression:
```java
public static <T extends Comparable<T>> T max(Collection<T> coll) { ... }
```
This expresses: "any type `T` that can be compared to other instances of `T`".

More flexibly, using wildcards:
```java
public static <T extends Comparable<? super T>> T max(Collection<T> coll) { ... }
```
This allows comparing types that inherit their comparison contract from a superclass (e.g., `java.sql.Timestamp` extends `java.util.Date`, which implements `Comparable<Date>`).

---

## 3. How It Works Internally

### Erasure to the First Bound
When compiling a bounded type parameter, `javac` erases the type parameter to its **first bound** instead of `java.lang.Object`.

```
Source:
public class Sorter<T extends Comparable<T>> {
    public int compare(T a, T b) {
        return a.compareTo(b);
    }
}

Bytecode (Decompiled):
public class Sorter {
    public int compare(Comparable a, Comparable b) {
        return a.compareTo(b); // invokes interface Comparable.compareTo
    }
}
```

If multiple bounds are declared:
```java
<T extends Number & Comparable<T>>
```
The type is erased to `Number` (the first bound). Wherever methods from `Comparable` are called on `T`, `javac` automatically inserts a synthetic cast to `Comparable`.

```
Bytecode:
aload_1           // load parameter 'a' (typed as Number)
checkcast         // cast to java/lang/Comparable
aload_2
invokeinterface   // Comparable.compareTo:(Ljava/lang/Object;)I
```

---

## 4. Syntax & Usage

### 1. Numeric Aggregator
```java
public class NumericStats<T extends Number> {
    private final List<T> numbers;

    public NumericStats(List<T> numbers) {
        this.numbers = List.copyOf(numbers);
    }

    public double average() {
        double sum = 0.0;
        for (T num : numbers) {
            sum += num.doubleValue();
        }
        return numbers.isEmpty() ? 0.0 : sum / numbers.size();
    }
}
```

### 2. Generic Search Method with Recursive Bound
```java
public final class SearchUtils {
    private SearchUtils() {}

    public static <T extends Comparable<? super T>> T findMax(List<T> list) {
        if (list == null || list.isEmpty()) {
            throw new IllegalArgumentException("List must not be null or empty");
        }
        T max = list.get(0);
        for (T item : list) {
            if (item.compareTo(max) > 0) {
                max = item;
            }
        }
        return max;
    }
}
```

### 3. Multiple Bounds Enforcing Resource Lifecycle
```java
public static <T extends AutoCloseable & Runnable> void runAndClose(T task) {
    try (task) {
        task.run();
    } catch (Exception e) {
        throw new RuntimeException("Task execution failed", e);
    }
}
```

---

## 5. Practical Examples

### Example 1: Bounded Range Interval (Basic)
```java
import java.util.Objects;

public final class Range<T extends Comparable<? super T>> {
    private final T start;
    private final T end;

    public Range(T start, T end) {
        Objects.requireNonNull(start, "start must not be null");
        Objects.requireNonNull(end, "end must not be null");
        if (start.compareTo(end) > 0) {
            throw new IllegalArgumentException("start (" + start + ") cannot be after end (" + end + ")");
        }
        this.start = start;
        this.end = end;
    }

    public boolean contains(T value) {
        return value.compareTo(start) >= 0 && value.compareTo(end) <= 0;
    }

    public static void main(String[] args) {
        Range<Integer> scoreRange = new Range<>(10, 50);
        System.out.println("Contains 25: " + scoreRange.contains(25));
        System.out.println("Contains 75: " + scoreRange.contains(75));
    }
}
```

### Example 2: Polymorphic Builder with Self-Referential Bound (Real-World)
```java
// Curiously Recurring Template Pattern (CRTP) for fluent hierarchical builders
public abstract class EntityBuilder<T extends EntityBuilder<T>> {
    protected String id;
    protected String name;

    @SuppressWarnings("unchecked")
    protected T self() {
        return (T) this;
    }

    public T withId(String id) {
        this.id = id;
        return self();
    }

    public T withName(String name) {
        this.name = name;
        return self();
    }
}

public class UserAccountBuilder extends EntityBuilder<UserAccountBuilder> {
    private String email;

    public UserAccountBuilder withEmail(String email) {
        this.email = email;
        return self();
    }

    public String build() {
        return "User[id=" + id + ", name=" + name + ", email=" + email + "]";
    }

    public static void main(String[] args) {
        String user = new UserAccountBuilder()
            .withId("USR-101")
            .withName("Alice")
            .withEmail("alice@example.com")
            .build();
        System.out.println(user);
    }
}
```

### Example 3: Edge Case - Subclassing and Inheritance with Comparable
```java
import java.util.Date;
import java.sql.Timestamp;
import java.util.List;

public class BoundedInheritanceEdgeCase {
    // If declared as <T extends Comparable<T>>, Timestamp fails because it implements Comparable<Date>!
    // With <T extends Comparable<? super T>>, Timestamp works seamlessly.
    public static <T extends Comparable<? super T>> T findLatest(List<T> items) {
        T latest = items.get(0);
        for (T item : items) {
            if (item.compareTo(latest) > 0) {
                latest = item;
            }
        }
        return latest;
    }

    public static void main(String[] args) {
        List<Timestamp> timestamps = List.of(
            new Timestamp(1000L),
            new Timestamp(5000L),
            new Timestamp(2000L)
        );
        Timestamp latest = findLatest(timestamps);
        System.out.println("Latest: " + latest.getTime());
    }
}
```

### Example 4: Failure Case - Illegal Class Order in Multiple Bounds
```java
// COMPILE ERROR: The class bound must be first
// public class BadOrder<T extends Runnable & Number> {}

// FIX: Class bound first, interface bound second
public class GoodOrder<T extends Number & Runnable> {}
```

---

## 6. Best Practices

1. **Always Favor `<T extends Comparable<? super T>>` Over `<T extends Comparable<T>>`**: This ensures compatibility with classes whose `Comparable` interface is implemented by an ancestor.
2. **Order Bounds Thoughtfully**: Always place the class bound first, followed by interfaces: `<T extends BaseClass & InterfaceA & InterfaceB>`.
3. **Keep Bounds as General as Possible**: Only require interfaces necessary for the operation (e.g., `<T extends AutoCloseable>` instead of `<T extends FileInputStream>`).
4. **Combine with Defensive Copies**: If accepting collections bounded by mutable types, perform defensive copying to preserve immutability.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why Wrong | Fix |
|---|---|---|
| Using `implements` in bounds: `<T implements AutoCloseable>` | Invalid syntax; Java uses `extends` for both classes and interfaces in bounds. | Use `extends`: `<T extends AutoCloseable>`. |
| Placing interface before class: `<T extends Cloneable & Number>` | The Java grammar requires class bound first. | Reorder: `<T extends Number & Cloneable>`. |
| Restrictive recursive bound: `<T extends Comparable<T>>` | Rejects subclasses where parent implemented `Comparable<Parent>`. | Use `<T extends Comparable<? super T>>`. |
| Multiple class bounds: `<T extends Number & String>` | Java does not permit multiple class inheritance. | Only one class bound is allowed. |
| Over-constraining bounds | Demanding unnecessary interfaces breaks flexibility for callers. | Require only the minimal required contract. |
| Cast assumption in multiple bounds erasure | Erased to first bound; accessing second bound relies on synthetic checkcast. | Ensure first bound aligns with the hottest call path. |

---

## 8. Performance & Security Notes

### Performance
- **Order of Bounds and Bytecode Invocations**: Because `javac` erases to the first bound, calling methods of the first bound uses `invokevirtual` directly. Calling methods on subsequent interface bounds requires synthetic `checkcast` and `invokeinterface`, which may exhibit minor differences in inline caching across legacy JVM hot spots.

### Security
- **Type Bounds Do Not Prevent Serialization Vulnerabilities**: If `<T extends Serializable>` is accepted, untrusted objects can still be serialized. Always validate classes against an `ObjectInputFilter` when deserializing.

---

## 9. Comparison / When to Use What

| Pattern | Syntax | Capability | Use Case |
|---|---|---|---|
| Unbounded | `<T>` | Only `Object` methods | Pure containers (`List<T>`, `Optional<T>`) |
| Single Bound | `<T extends Number>` | Methods of bound available | Numeric calculations, domain entities |
| Multiple Bounds | `<T extends Number & Comparable<T>>` | Union of all bounds' methods | Numeric sorting, multi-contract workflows |
| Recursive Bound | `<T extends Comparable<? super T>>` | Compares with self or ancestor | Sorting, searching, tree balancing |

---

## 10. Interview Questions

<details>
<summary>1. Why does Java use the keyword `extends` for interface bounds instead of `implements`?</summary>
To keep the language grammar simpler and avoid adding additional keywords to the generic type parameter syntax. In the context of generic bounds, `extends` means "is a subtype of", which applies equally to classes extending classes, classes implementing interfaces, and interfaces extending interfaces.
</details>

<details>
<summary>2. Why must the class bound come first in multiple bounds?</summary>
Because Java single-class inheritance guarantees that a type can extend at most one class. Enforcing the class as the first element makes the grammar unambiguous and dictates which type the parameter erases to in bytecode.
</details>

<details>
<summary>3. Explain the difference between `<T extends Comparable<T>>` and `<T extends Comparable<? super T>>`.</summary>
`<T extends Comparable<T>>` requires `T` to be directly comparable to `T`. If a subclass `Child` extends `Parent`, and `Parent` implements `Comparable<Parent>`, then `Child` implements `Comparable<Parent>`, NOT `Comparable<Child>`. Under `<T extends Comparable<T>>`, `Child` would be rejected. `<T extends Comparable<? super T>>` accommodates this cleanly.
</details>

<details>
<summary>4. What type does `<T extends Number & Runnable>` erase to in bytecode?</summary>
It erases to `java.lang.Number` (the first bound). Calls to `run()` will emit an explicit `checkcast` to `java.lang.Runnable` followed by `invokeinterface`.
</details>

<details>
<summary>5. Can a bounded type parameter have multiple class bounds?</summary>
No. In Java, multiple class inheritance is forbidden. Attempting `<T extends Thread & Exception>` causes a compilation error.
</details>

<details>
<summary>6. How does Curiously Recurring Template Pattern (CRTP) work in Java builders?</summary>
By declaring `abstract class Builder<T extends Builder<T>>`, sub-builders pass themselves as the type argument: `class ConcreteBuilder extends Builder<ConcreteBuilder>`. The base builder can then return `(T) this`, maintaining fluent method chaining with the concrete subclass type.
</details>

<details>
<summary>7. What happens if you pass a type argument that does not satisfy the bound?</summary>
A compile-time error occurs: `type argument X is not within bounds of type-variable T`. No bytecode is generated.
</details>

<details>
<summary>8. Can you use primitive types as bounds, e.g. `<T extends int>`?</summary>
No. Generics only operate on reference types (`Object` subtypes).
</details>

---

## 11. Practice Exercises

### Easy
1. Implement a method `findMin` that returns the minimum value in an array `T[]` where `T extends Comparable<T>`.
2. Create a generic class `SummablePair<T extends Number>` that stores two numbers and has a method `double sum()`.
3. Write a generic method that accepts any `T extends CharSequence` and returns whether it is empty or null.

### Medium
4. Write a generic sorting algorithm (e.g., Insertion Sort) that operates on `List<T>` where `T extends Comparable<? super T>`.
5. Implement a generic bounded priority queue that stores elements implementing a custom `Prioritizable` interface.
6. Build a `TransactionManager` generic class with `<T extends AutoCloseable & Flushable>` that guarantees flushing before closing.

### Hard
7. Design a multi-level hierarchical AST visitor using recursive generic bounds for expression nodes.
8. Implement a complete fluent query builder using CRTP that supports deep inheritance across SQL `Select`, `Insert`, and `Update` builders.
9. Write a compile-time dimensional analysis unit system (Length, Time, Mass) using bounded phantom type parameters.

<details>
<summary>Solutions</summary>

```java
// Easy 2: SummablePair
public record SummablePair<T extends Number>(T first, T second) {
    public double sum() {
        return first.doubleValue() + second.doubleValue();
    }
}

// Medium 4: Insertion Sort
public static <T extends Comparable<? super T>> void insertionSort(List<T> list) {
    for (int i = 1; i < list.size(); i++) {
        T current = list.get(i);
        int j = i - 1;
        while (j >= 0 && list.get(j).compareTo(current) > 0) {
            list.set(j + 1, list.get(j));
            j--;
        }
        list.set(j + 1, current);
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **Single Bound**: `<T extends UpperBound>` (works for class or interface).
- **Multiple Bounds**: `<T extends ClassBound & InterfaceBound1 & InterfaceBound2>`.
- **Class Bound First**: If a class bound is present, it must be the first bound.
- **Erasure Target**: Erases to the first bound in bytecode.
- **PECS in Bounds**: Always use `<T extends Comparable<? super T>>` for robust comparison across class hierarchies.
- **Method Availability**: Inside the generic class or method, all public methods of the declared bounds are directly accessible.

---

## 13. Further Reading
- Bloch, Joshua. *Effective Java (3rd Edition)*: Item 30 (Favor generic methods) and Item 31 (Use bounded wildcards to increase API flexibility).
- Java Language Specification: *Section 4.4 - Type Variables and Bounds*.
- Naftalin, Maurice and Wadler, Philip. *Java Generics and Collections* (O'Reilly).
