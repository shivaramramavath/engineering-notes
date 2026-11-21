# Iterators and Fail-Fast Mechanics

> Traversal contracts in Java: Iterator, ListIterator, Spliterator, fail-fast modCount invariants, and fail-safe concurrent iterators.

| Metadata | Value |
|---|---|
| **Level** | Intermediate |
| **Prerequisites** | [00_collection-hierarchy.md](00_collection-hierarchy.md), [01_list.md](01_list.md) |
| **Time to Read** | 24 minutes |
| **Status** | Active |
| **Java Version** | Java 1.2+ (Spliterator Java 8+) |

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

In object-oriented programming, traversing a collection without exposing its underlying internal data structure (array, linked list, balanced tree, hash table) is achieved via the **Iterator Design Pattern**.

In Java, any class implementing `java.lang.Iterable<T>` can be traversed using the **enhanced for-loop** (`for (T item : collection)`). Under the hood, `javac` translates every enhanced for-loop into an explicit `Iterator` sequence.

A central design challenge during collection traversal is handling **concurrent modifications**: what happens if the collection is structurally modified (items added or deleted) while an iterator is actively stepping through it?

Java solves this via two distinct paradigms:
1. **Fail-Fast**: Immediately detects structural modifications and fails cleanly with a `ConcurrentModificationException` rather than returning corrupt data.
2. **Fail-Safe / Weakly Consistent**: Traverses a snapshot or clone of the data, never throwing `ConcurrentModificationException` during iteration.

---

## 2. Core Concepts

### 1. `java.util.Iterator<E>`
The root traversal interface:
- `boolean hasNext()`: Returns `true` if more elements exist.
- `E next()`: Returns the next element.
- `default void remove()`: Removes the last element returned by `next()`.
- `default void forEachRemaining(Consumer<? super E> action)`: Performs the action on remaining elements.

### 2. `java.util.ListIterator<E>`
Extends `Iterator` for bidirectional list traversal:
- `hasPrevious()`, `previous()`, `nextIndex()`, `previousIndex()`, `set(E e)`, `add(E e)`.

### 3. `java.util.Spliterator<E>`
Introduced in Java 8 to power the Streams API:
- Supports parallel decomposition (`trySplit()`).
- Traverses elements sequentially (`tryAdvance()`, `forEachRemaining()`).
- Exposes performance characteristics (`characteristics()` bitmask: `SIZED`, `SORTED`, `DISTINCT`, `IMMUTABLE`, `CONCURRENT`).

---

## 3. How It Works Internally

### The `modCount` Invariant
Standard collections (`ArrayList`, `HashMap`, `LinkedList`, `HashSet`) maintain an internal structural modification counter:
```java
protected transient int modCount = 0;
```
Every time a structural mutation occurs (`add()`, `remove()`, `clear()`, resizing), `modCount++` is executed.

When an iterator is created, it captures a snapshot of this counter:
```java
private class Itr implements Iterator<E> {
    int cursor;       // index of next element to return
    int lastRet = -1; // index of last element returned
    int expectedModCount = modCount; // SNAPSHOT OF MODCOUNT!

    public E next() {
        checkForComodification();
        // ... return element and update cursor
    }

    final void checkForComodification() {
        if (modCount != expectedModCount)
            throw new ConcurrentModificationException();
    }
}
```

```
+-------------------------------------------------------+
| Fail-Fast Verification Flow                           |
|                                                       |
| Collection: modCount = 5                              |
| Iterator created: expectedModCount = 5                |
|                                                       |
| Iteration 1: next() -> modCount(5) == expected(5) -> OK
| External mutation: coll.add("X") -> modCount becomes 6!
|                                                       |
| Iteration 2: next() -> modCount(6) != expected(5)     |
|              -> CRASH! ConcurrentModificationException|
+-------------------------------------------------------+
```

### Why `Iterator.remove()` Does Not Fail
When you remove an element through the iterator itself (`it.remove()`):
1. The iterator calls `collection.remove(lastRet)`.
2. This increments `modCount`.
3. The iterator immediately updates: `expectedModCount = modCount;`.
4. Because both counters stay in sync, no exception is thrown!

---

## 4. Syntax & Usage

### 1. Enhanced For-Loop Translation
```java
import java.util.List;

public class LoopTranslation {
    public static void main(String[] args) {
        List<String> list = List.of("A", "B", "C");

        // What you write:
        for (String s : list) {
            System.out.println(s);
        }

        // What javac compiles into bytecode:
        java.util.Iterator<String> it = list.iterator();
        while (it.hasNext()) {
            String s = it.next();
            System.out.println(s);
        }
    }
}
```

### 2. Safe Removal During Traversal
```java
import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;

public class SafeRemovalDemo {
    public static void main(String[] args) {
        List<Integer> numbers = new ArrayList<>(List.of(1, 2, 3, 4, 5));

        // Idiom 1: Explicit Iterator.remove()
        Iterator<Integer> it = numbers.iterator();
        while (it.hasNext()) {
            if (it.next() % 2 == 0) {
                it.remove(); // SAFE!
            }
        }

        // Idiom 2: Modern Java 8+ removeIf (Preferred)
        numbers.removeIf(n -> n > 3);

        System.out.println("Remaining: " + numbers); // [1, 3]
    }
}
```

---

## 5. Practical Examples

### Example 1: Bidirectional Editing with `ListIterator` (Basic)
```java
import java.util.ArrayList;
import java.util.List;
import java.util.ListIterator;

public class ListIteratorTransform {
    public static void main(String[] args) {
        List<String> words = new ArrayList<>(List.of("alpha", "beta", "gamma"));
        ListIterator<String> it = words.listIterator();

        while (it.hasNext()) {
            String word = it.next();
            if (word.equals("beta")) {
                it.set("BETA"); // Replace current
                it.add("beta-2"); // Insert immediately after
            }
        }
        System.out.println("Words: " + words); // [alpha, BETA, beta-2, gamma]
    }
}
```

### Example 2: Parallel Stream Spliterator Decomposition (Real-World)
```java
import java.util.ArrayList;
import java.util.List;
import java.util.Spliterator;

public class SpliteratorDemo {
    public static void main(String[] args) {
        List<String> items = new ArrayList<>(List.of("A1", "A2", "B1", "B2", "C1", "C2"));
        Spliterator<String> s1 = items.spliterator();

        // Split data in half for parallel processing
        Spliterator<String> s2 = s1.trySplit();

        System.out.print("Partition 1: ");
        s1.forEachRemaining(e -> System.out.print(e + " "));
        System.out.println();

        System.out.print("Partition 2: ");
        if (s2 != null) {
            s2.forEachRemaining(e -> System.out.print(e + " "));
        }
        System.out.println();
    }
}
```

### Example 3: Edge Case - Single-Threaded `ConcurrentModificationException`
```java
import java.util.ArrayList;
import java.util.List;

public class SingleThreadedCmeDemo {
    public static void main(String[] args) {
        List<String> list = new ArrayList<>(List.of("one", "two", "three"));

        // Misconception: CME only happens in multi-threaded code.
        // TRUTH: CME happens frequently in single-threaded code when modifying collection directly!
        try {
            for (String s : list) {
                if (s.equals("two")) {
                    list.remove(s); // Modifies collection directly!
                }
            }
        } catch (Exception e) {
            System.out.println("Caught: " + e.getClass().getSimpleName()); // ConcurrentModificationException
        }
    }
}
```

### Example 4: Failure Case - Calling `remove()` Before `next()`
```java
import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;

public class IllegalIteratorStateFailure {
    public static void main(String[] args) {
        List<String> list = new ArrayList<>(List.of("alpha"));
        Iterator<String> it = list.iterator();

        try {
            it.remove(); // IllegalStateException: next() has not been called!
        } catch (IllegalStateException e) {
            System.out.println("Caught expected IllegalStateException!");
        }
    }
}
```

---

## 6. Best Practices

1. **Always Use `removeIf()` for Conditional In-Place Removal**: `list.removeIf(predicate)` is cleaner and orders of magnitude faster in `ArrayList` than iterator removal.
2. **Never Call `collection.remove()` Inside Enhanced For-Loop**: This directly triggers `ConcurrentModificationException`.
3. **Use Fail-Safe Collections for Concurrent Iteration**: Use `CopyOnWriteArrayList` or `ConcurrentHashMap` when threads iterate over collections while others write.
4. **Leverage `Spliterator` for Custom Parallel Pipelines**: Custom data structures can achieve scalable parallel stream performance by implementing balanced `trySplit()`.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why Wrong | Fix |
|---|---|---|
| Calling `collection.remove()` inside enhanced for-loop | Mismatches `modCount` with `expectedModCount`, throwing `ConcurrentModificationException`. | Use `Iterator.remove()` or `removeIf()`. |
| Calling `it.remove()` twice in a row | `lastRet` is reset to -1 after removal, causing `IllegalStateException`. | Must call `next()` between `remove()` calls. |
| Assuming `ConcurrentModificationException` guarantees concurrency bug | CME is thrown on single-threaded structural mutations. | Fix the in-loop collection modification. |
| Assuming fail-fast is 100% guaranteed in multi-threaded code | Fail-fast is best-effort; data races on non-volatile `modCount` can miss violations. | Use thread-safe concurrent collections. |
| Forgetting `ListIterator.set()` modifies `lastRet` | Replaces the last element returned by `next()` or `previous()`. | Ensure `next()` was called first. |
| Ignoring `Spliterator` characteristics | Passing incorrect characteristics (e.g. `DISTINCT` when duplicates exist) corrupts stream operations. | Accurate bitmask reporting. |

---

## 8. Performance & Security Notes

### Performance
- **Zero Object Allocation for Enhanced For on Arrays**: When compiling an enhanced for-loop over a raw array (`int[]` or `String[]`), `javac` does not allocate an `Iterator` object; it generates an indexed `for (int i = 0; i < len; i++)` loop.
- **Fail-Fast Instruction Cost**: Inlining `checkForComodification()` adds a single comparison and jump instruction, which modern CPU branch predictors easily optimize to near-zero cost.

### Security
- **Fail-Fast Prevents Memory Corruption**: In C++, invalidating an iterator and continuing iteration causes undefined behavior or memory faults. Java's fail-fast design guarantees safe termination.

---

## 9. Comparison / When to Use What

| Iterator Type | Modification Safety | Exception on Concurrent Mutation? | Direction | Use Case |
|---|---|---|---|---|
| **Standard Iterator** | Fail-Fast | Yes (`ConcurrentModificationException`) | Forward only | Standard sequential traversal |
| **ListIterator** | Fail-Fast | Yes | Bidirectional | Indexed list manipulation |
| **Spliterator** | Fail-Fast / Fail-Safe | Dependent on collection | Partitioning | Parallel Streams |
| **Concurrent Iterator** | Weakly Consistent / Snapshot | No | Forward only | Multi-threaded reading (`ConcurrentHashMap`) |

---

## 10. Interview Questions

<details>
<summary>1. How does the fail-fast mechanism work in Java collections?</summary>
Collections maintain an internal `modCount` variable incremented on every structural modification. Iterators capture this value as `expectedModCount`. On every `next()` or `remove()` invocation, the iterator verifies `if (modCount != expectedModCount)`. If unequal, it immediately throws `ConcurrentModificationException`.
</details>

<details>
<summary>2. Why does `iterator.remove()` not trigger `ConcurrentModificationException`?</summary>
Because `iterator.remove()` updates both the backing collection's `modCount` and the iterator's own `expectedModCount` synchronously, preserving invariant equality.
</details>

<details>
<summary>3. Can `ConcurrentModificationException` occur in a single-threaded program?</summary>
Yes. Modifying a collection directly (`list.add()`, `list.remove()`) while iterating over it via an enhanced for-loop throws `ConcurrentModificationException` on a single thread.
</details>

<details>
<summary>4. What is the difference between fail-fast and fail-safe iterators?</summary>
Fail-fast iterators operate directly on the collection's live data and throw `ConcurrentModificationException` immediately if structural changes occur. Fail-safe (or weakly consistent) iterators operate on a snapshot or cloned array, allowing modifications without throwing exceptions.
</details>

<details>
<summary>5. What does the `Spliterator` interface provide?</summary>
Introduced in Java 8, `Spliterator` ("splittable iterator") is designed for parallel traversal. It allows partitioning collections into chunks via `trySplit()`, estimating size via `estimateSize()`, and declaring optimization flags via `characteristics()`.
</details>

<details>
<summary>6. Why is fail-fast behavior documented as "best-effort"?</summary>
Because in multi-threaded programs without synchronization, changes to `modCount` across different CPU cores and caches are not guaranteed to be atomic or visible without memory barriers, meaning some race conditions may go undetected.
</details>

<details>
<summary>7. What happens if you call `it.remove()` before calling `it.next()`?</summary>
An `IllegalStateException` is thrown because there is no currently selected element to remove.
</details>

<details>
<summary>8. How does `javac` compile an enhanced for-loop over an array versus a `List`?</summary>
For an array, it compiles into a standard index-based loop (`for (int i = 0; i < len; i++)`) with zero object allocations. For a `List`, it compiles into an `Iterator` sequence (`iterator()`, `hasNext()`, `next()`).
</details>

---

## 11. Practice Exercises

### Easy
1. Iterate over an `ArrayList<String>` using a classic `while (it.hasNext())` loop and print elements.
2. Remove all words starting with the letter 'Z' from a list using `Iterator.remove()`.
3. Demonstrate that calling `list.remove()` during an enhanced for-loop throws `ConcurrentModificationException`.

### Medium
4. Write a custom `ReverseIterator<T>` that wraps a `List<T>` and iterates elements from end to start.
5. Implement a filtered iterator `FilterIterator<T>` that accepts an existing `Iterator<T>` and a `Predicate<T>`, skipping elements that do not match.
6. Split a `Spliterator<Integer>` across 4 threads to compute the sum of 1,000,000 integers.

### Hard
7. Implement a custom thread-safe ring buffer with a weakly-consistent snapshot iterator.
8. Design a custom `Spliterator<T>` for an unrolled linked list that dynamically balances split partitions based on node weights.
9. Implement a tree iterator that performs an in-order traversal of a binary search tree in $O(1)$ additional space without recursion.

<details>
<summary>Solutions</summary>

```java
// Easy 2: Iterator.remove
List<String> list = new ArrayList<>(List.of("Zebra", "Apple", "Zero", "Banana"));
Iterator<String> it = list.iterator();
while (it.hasNext()) {
    if (it.next().startsWith("Z")) it.remove();
}
System.out.println(list); // [Apple, Banana]

// Medium 4: ReverseIterator
public class ReverseIterator<T> implements Iterator<T> {
    private final List<T> list;
    private int cursor;

    public ReverseIterator(List<T> list) {
        this.list = list;
        this.cursor = list.size() - 1;
    }

    public boolean hasNext() { return cursor >= 0; }
    public T next() {
        if (!hasNext()) throw new NoSuchElementException();
        return list.get(cursor--);
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **Iterator**: `hasNext()`, `next()`, `remove()`.
- **Fail-Fast**: Throws `ConcurrentModificationException` when `modCount != expectedModCount`.
- **Safe Mutation**: Use `Iterator.remove()` or `Collection.removeIf()`.
- **ListIterator**: Bidirectional (`previous()`, `hasPrevious()`, `set()`, `add()`).
- **Spliterator**: Parallel partitioning via `trySplit()`.
- **Arrays**: Enhanced for on arrays compiles to index loops without iterator allocations.

---

## 13. Further Reading
- Gamma, Helm, Johnson, Vlissides. *Design Patterns: Elements of Reusable Object-Oriented Software* (Iterator Pattern).
- Bloch, Joshua. *Effective Java (3rd Edition)*: Item 58 (Prefer for-each loops to traditional for loops).
- Oracle Java Documentation: *The Spliterator Interface*.
