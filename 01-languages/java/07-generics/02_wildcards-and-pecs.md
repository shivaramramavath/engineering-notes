# Wildcards and PECS

> Mastering unbounded, upper-bounded, and lower-bounded wildcards to build flexible, type-safe API boundaries via the Producer Extends, Consumer Super principle.

| Metadata | Value |
|---|---|
| **Level** | Advanced |
| **Prerequisites** | [00_generic-classes-and-methods.md](00_generic-classes-and-methods.md), [01_bounded-types.md](01_bounded-types.md) |
| **Time to Read** | 25 minutes |
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

In Java, arrays are **covariant**: `Integer[]` is a subtype of `Number[]`. While this sounds intuitive, it introduces potential runtime failures because you can assign an `Integer[]` to a `Number[]` reference and attempt to insert a `Double`, resulting in an `ArrayStoreException` at runtime:

```java
Number[] arr = new Integer[5];
arr[0] = 3.14; // Throws ArrayStoreException at runtime!
```

To prevent this runtime failure, Java generics are **invariant**: `List<Integer>` is **NOT** a subtype of `List<Number>`, even though `Integer` is a subtype of `Number`.

While invariance provides compile-time safety, it can make APIs rigid and unusable:
```java
public void printNumbers(List<Number> list) { ... }

List<Integer> myInts = List.of(1, 2, 3);
printNumbers(myInts); // COMPILE ERROR! Invariance rejects List<Integer>
```

**Wildcards (`?`)** introduce controlled subtyping (use-site variance) to generics, restoring flexibility while maintaining complete compile-time type safety.

---

## 2. Core Concepts

### 1. Invariance vs Covariance vs Contravariance
- **Invariance**: For two distinct types `A` and `B`, `Container<A>` has no inheritance relationship with `Container<B>`. (Default Java generics behavior).
- **Covariance (`? extends T`)**: If `A` is a subtype of `B`, then `Container<A>` is a subtype of `Container<? extends B>`. Read-only access from the container.
- **Contravariance (`? super T`)**: If `A` is a subtype of `B`, then `Container<B>` is a subtype of `Container<? super A>`. Write-only access to the container.

```
Subtyping Hierarchy with Wildcards:

          List<?>  (Unbounded Wildcard: supertype of all Lists)
         /       \
List<? extends Number>   List<? super Integer>
       |                      |
  List<Number>           List<Integer>
       |                      |
  List<Integer>          List<Object>
```

### 2. The PECS Rule
Coined by Joshua Bloch in *Effective Java*:
> **PECS**: **P**roducer **E**xtends, **C**onsumer **S**uper.

- If a parameterized type represents a **producer** (you only **read** values from it), use `<? extends T>`.
- If a parameterized type represents a **consumer** (you only **write** values into it), use `<? super T>`.
- If you both read from and write to the container, do **not** use a wildcard; use exact invariance `T`.

### 3. Unbounded Wildcard (`List<?>`)
`List<?>` represents a list of an unknown type. You can read elements as `Object`, but you cannot add anything to it (except `null`). Use it when:
1. Writing a method that can be implemented using functionality provided in `Object`.
2. Operating on generic class methods that don't depend on the type parameter (e.g., `List.size()`, `List.clear()`).

### 4. Wildcard Capture
The compiler assigns a unique anonymous type (a capture) to each wildcard in an expression. You cannot write the name of this type, but you can capture it using a generic helper method.

---

## 3. How It Works Internally

### Compiler Capture and Type Checking
When the compiler encounters `List<? extends Number>`, it knows the list holds some subtype of `Number`, but it does not know which concrete subtype.
- When you call `list.get(index)`, the returned value is guaranteed to be at least a `Number`.
- When you call `list.add(item)`, the compiler must reject it because `item` could be an `Integer` while the underlying list might actually be `List<Double>`. The only value allowed is `null` (which belongs to every reference type).

```
+------------------------------------+
| List<? extends Number> list        |
+------------------------------------+
  |-- Read (get):    Returns Number  | (Valid: every element is a Number)
  |-- Write (add):   REJECTED!       | (Cannot guarantee concrete subtype)

+------------------------------------+
| List<? super Integer> list         |
+------------------------------------+
  |-- Read (get):    Returns Object  | (Could be Integer, Number, or Object)
  |-- Write (add):   Integer allowed | (Safe: target accepts Integer or super)
```

### Wildcard Capture Helper Pattern
If a method takes a wildcard `List<?>` and needs to swap elements:
```java
public static void swap(List<?> list, int i, int j) {
    // list.set(i, list.get(j)); // COMPILE ERROR: cannot capture wildcard
    swapHelper(list, i, j); // Delegate to generic helper!
}

private static <E> void swapHelper(List<E> list, int i, int j) {
    E temp = list.get(i);
    list.set(i, list.get(j));
    list.set(j, temp); // Fully valid! E is a concrete captured type variable.
}
```

---

## 4. Syntax & Usage

### 1. Producer Extends Example
```java
// Reads numbers and calculates sum -> Producer!
public static double sumOfList(List<? extends Number> list) {
    double sum = 0.0;
    for (Number n : list) {
        sum += n.doubleValue();
    }
    return sum;
}
```

### 2. Consumer Super Example
```java
// Writes integers into target collection -> Consumer!
public static void addNumbers(List<? super Integer> list, int count) {
    for (int i = 1; i <= count; i++) {
        list.add(i); // Valid: Integer is accepted by ? super Integer
    }
}
```

### 3. Canonical Copy Method (Combining Both)
```java
// Collections.copy signature:
public static <T> void copy(List<? super T> dest, List<? extends T> src) {
    for (int i = 0; i < src.size(); i++) {
        dest.set(i, src.get(i));
    }
}
```
Notice: `src` is the **Producer** (`<? extends T>`), `dest` is the **Consumer** (`<? super T>`).

---

## 5. Practical Examples

### Example 1: Type-Safe Pipeline Event Dispatcher (Basic)
```java
import java.util.ArrayList;
import java.util.List;

public class EventSystem {
    public interface Event {}
    public static class UserEvent implements Event {}
    public static class LoginEvent extends UserEvent {}

    // Consumer: List consumes events to store them
    public static void collectEvents(List<? super LoginEvent> sink) {
        sink.add(new LoginEvent());
    }

    // Producer: List produces events to process them
    public static void processEvents(List<? extends Event> source) {
        for (Event event : source) {
            System.out.println("Processing event: " + event.getClass().getSimpleName());
        }
    }

    public static void main(String[] args) {
        List<UserEvent> userEvents = new ArrayList<>();
        collectEvents(userEvents); // Valid: UserEvent is super of LoginEvent
        processEvents(userEvents); // Valid: UserEvent extends Event
    }
}
```

### Example 2: Stream-like Filter and Transform Pipeline (Real-World)
```java
import java.util.ArrayList;
import java.util.List;
import java.util.function.Function;
import java.util.function.Predicate;

public final class PipelineUtils {
    private PipelineUtils() {}

    public static <T, R> List<R> transformAndFilter(
            List<? extends T> input,                     // Producer of T
            Predicate<? super T> filterCondition,       // Consumer of T
            Function<? super T, ? extends R> mapper      // Consumes T, Produces R
    ) {
        List<R> output = new ArrayList<>();
        for (T item : input) {
            if (filterCondition.test(item)) {
                output.add(mapper.apply(item));
            }
        }
        return output;
    }

    public static void main(String[] args) {
        List<Integer> numbers = List.of(1, 2, 3, 4, 5, 6);
        Predicate<Number> isEven = n -> n.intValue() % 2 == 0;
        Function<Object, String> toStringMapper = Object::toString;

        List<String> results = transformAndFilter(numbers, isEven, toStringMapper);
        System.out.println("Filtered results: " + results);
    }
}
```

### Example 3: Edge Case - Return Types Should Almost Never Use Wildcards
```java
// BAD: Forces wildcard handling onto all client calling code!
// public static List<? extends Number> getNumbers() { ... }

// GOOD: Return precise concrete type so caller has maximum capability
public static List<Integer> getNumbers() {
    return List.of(1, 2, 3);
}
```

### Example 4: Failure Case - Adding Elements to `List<?>` or `List<? extends T>`
```java
import java.util.ArrayList;
import java.util.List;

public class WildcardFailure {
    public static void illegalAdd(List<? extends Number> list) {
        // list.add(Integer.valueOf(10));
        // COMPILE ERROR: The method add(capture#1-of ? extends Number) in the type
        // List<capture#1-of ? extends Number> is not applicable for the arguments (Integer)

        // Only null is permitted
        list.add(null); // Compiles, but dangerous!
    }
}
```

---

## 6. Best Practices

1. **Apply PECS Consistently on Public APIs**: Use `<? extends T>` for input parameters that only produce values, and `<? super T>` for parameters that only consume values.
2. **Never Use Wildcards in Return Types**: Returning `List<? extends T>` forces callers to clutter their code with wildcards and reduces flexibility.
3. **Use Unbounded Wildcards for Structure-Only Operations**: If you only read operations that do not depend on `T` (e.g., `list.size()`, `list.isEmpty()`, reversing), use `List<?>`.
4. **Use Helper Methods for Wildcard Capture**: When you need to read and write an element of an unknown type within a wildcarded method, delegate to a private generic helper `<E> void helper(List<E> list)`.
5. **Prefer `<T extends Comparable<? super T>>` Over `<T extends Comparable<T>>`**: Always use consumer super for `Comparable` and `Comparator`.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why Wrong | Fix |
|---|---|---|
| Returning wildcard types from methods | Pollutes client code; prevents callers from adding elements or chaining easily. | Return exact types (`List<T>`, `List<String>`). |
| Trying to insert non-null elements into `List<? extends T>` | Compiler cannot prove the underlying collection type; prevents heap corruption. | Use `<? super T>` if inserting elements. |
| Reading elements as `T` from `List<? super T>` | Only `Object` can be safely read because the container may hold any ancestor type. | Cast explicitly if necessary, or use `<? extends T>` if reading. |
| Using `List<Object>` instead of `List<?>` | `List<Object>` is invariant and will reject `List<String>`. | Use `List<?>` to accept any list. |
| Confusing `List<T>` with `List<?>` in method declarations | `List<T>` declares a type parameter reusable across parameters and return type; `List<?>` does not. | Use `T` when types across parameters must correlate. |
| Forgetting `? super T` on `Comparator` | Rejects comparators defined on superclasses. | Declare `Comparator<? super T>`. |

---

## 8. Performance & Security Notes

### Performance
- **Zero Runtime Difference**: Wildcards are entirely a compile-time mechanism. In bytecode, `List<? extends Number>` erases to `java.util.List` just like `List<Number>`. There is zero memory or instruction overhead.

### Security
- **API Boundary Protection**: Wildcards enforce immutability at the API boundary. By declaring a method parameter as `List<? extends Entity>`, you statically guarantee that the method cannot append malicious or malformed elements into the caller's collection.

---

## 9. Comparison / When to Use What

| Parameter Declaration | Can Read Elements As | Can Add Elements | Best Use Case |
|---|---|---|---|
| `List<T>` | `T` | `T` | Read and write; internal container operations |
| `List<? extends T>` | `T` | Only `null` | **Producer**: Reading from inputs |
| `List<? super T>` | `Object` | `T` and subclasses of `T` | **Consumer**: Writing into targets / sinks |
| `List<?>` | `Object` | Only `null` | Structure-only inspection (`size`, `clear`) |

---

## 10. Interview Questions

<details>
<summary>1. What does the PECS acronym stand for and what is its core rule?</summary>
PECS stands for **Producer Extends, Consumer Super**. If a generic container produces items for your method to read, declare it with `<? extends T>`. If the container consumes items written by your method, declare it with `<? super T>`.
</details>

<details>
<summary>2. Why does `List<? extends Number>` not permit calling `add(10)`?</summary>
Because `List<? extends Number>` could be referencing a `List<Double>` or `List<BigDecimal>` at runtime. Allowing an `Integer` to be added would corrupt the type safety of the underlying collection.
</details>

<details>
<summary>3. What is the difference between `List<Object>` and `List<?>`?</summary>
`List<Object>` is invariant; it can only accept a `List<Object>`, rejecting `List<String>`. `List<?>` is an unbounded wildcard that accepts a list of any type (e.g., `List<String>`, `List<Integer>`, `List<Double>`).
</details>

<details>
<summary>4. Why should you never use wildcard types as return types?</summary>
Returning wildcards forces callers to use wildcards in their own variable declarations, making the API clumsy and restricting the caller from invoking methods that take the type parameter as an argument.
</details>

<details>
<summary>5. What is "wildcard capture" and how do you resolve a capture error?</summary>
Wildcard capture is the compiler's internal assignment of a specific type variable to a wildcard. If you need to access and manipulate that captured type (e.g., swapping two elements), create a private generic helper method parameterized with `<T>` and pass the wildcarded collection into it.
</details>

<details>
<summary>6. Why is `Comparator<? super T>` preferred over `Comparator<T>`?</summary>
Because a comparator capable of comparing a superclass of `T` (e.g., `Person`) can validly compare instances of `T` (e.g., `Employee`). Without `? super T`, such a comparator would be rejected.
</details>

<details>
<summary>7. Can you instantiate a generic object with a wildcard, e.g., `new ArrayList<?>()`?</summary>
No. You cannot instantiate a collection with a wildcard directly. You must specify a concrete type or diamond: `new ArrayList<String>()` or `new ArrayList<>()`.
</details>

<details>
<summary>8. How does `Collections.max` use wildcards in its signature?</summary>
`public static <T extends Object & Comparable<? super T>> T max(Collection<? extends T> coll)`. It uses `<? extends T>` because the collection produces values of `T`, and `<? super T>` because `T` may inherit its `Comparable` implementation from a superclass.
</details>

---

## 11. Practice Exercises

### Easy
1. Write a method `printAll(Collection<?> coll)` that prints all elements in any collection.
2. Implement a method `countGreaterThan(List<? extends Comparable<T>> list, T elem)` that counts occurrences strictly greater than `elem`.
3. Create a utility that checks whether a `List<?>` contains any `null` elements.

### Medium
4. Implement a generic `addAll` method that copies all elements from a source `Collection<? extends E>` into a target `Collection<? super E>`.
5. Write a method `reverse(List<?> list)` that reverses the elements in any list in-place using a private wildcard capture helper.
6. Build a `ComparatorChain<T>` that chains multiple `Comparator<? super T>` instances.

### Hard
7. Implement a generic topological sort algorithm on a DAG where node keys satisfy `<K extends Comparable<? super K>>`.
8. Design a typesafe event bus where listeners register for `<? super E>` and publishers dispatch `<? extends E>`.
9. Construct a generic tree fold/reduce operation using wildcards that supports aggregating arbitrary numeric AST node values.

<details>
<summary>Solutions</summary>

```java
// Easy 1: printAll
public static void printAll(Collection<?> coll) {
    for (Object elem : coll) {
        System.out.println(elem);
    }
}

// Medium 4: addAll
public static <E> void addAll(Collection<? super E> target, Collection<? extends E> source) {
    for (E item : source) {
        target.add(item);
    }
}

// Medium 5: reverse with capture helper
public static void reverse(List<?> list) {
    reverseHelper(list);
}

private static <T> void reverseHelper(List<T> list) {
    int left = 0;
    int right = list.size() - 1;
    while (left < right) {
        T temp = list.get(left);
        list.set(left, list.get(right));
        list.set(right, temp);
        left++;
        right--;
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **Invariance**: `List<String>` is NOT a `List<Object>`.
- **PECS**: Producer Extends (`<? extends T>`), Consumer Super (`<? super T>`).
- **Producer**: Read-only (`get()` returns `T`, `add()` forbidden).
- **Consumer**: Write-oriented (`add(T)` allowed, `get()` returns `Object`).
- **Unbounded**: `List<?>` accepts any list; read as `Object`, cannot add elements.
- **Return Types**: Avoid wildcards in return types; always return precise types.
- **Helper Pattern**: Use `<T>` generic private helper to capture unknown wildcards for mutations.

---

## 13. Further Reading
- Bloch, Joshua. *Effective Java (3rd Edition)*: Item 31 - Use bounded wildcards to increase API flexibility.
- Oracle Java Documentation: *Wildcards and Subtyping*.
- Goetz, Brian. *Java Theory and Practice: Generics gotchas* (IBM DeveloperWorks).
