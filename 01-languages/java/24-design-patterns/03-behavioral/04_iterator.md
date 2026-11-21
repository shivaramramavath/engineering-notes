# Iterator Pattern in Modern Java

> Sequential traversal without exposing underlying representation: Fail-Fast vs. Fail-Safe iterators, `java.util.Iterator`, custom collection iterators, and `java.util.Spliterator`.

| Metadata | Details |
| :--- | :--- |
| **Level** | Core Design Pattern / Behavioral |
| **Prerequisites** | [08_collections](../../08-collections/README.md), [04_oop](../../04-oop/README.md) |
| **Time to Read** | 35 minutes |
| **Status** | Verified Production Guide |
| **Java Baseline** | Java 17 LTS / Java 21 LTS / Java 25 LTS |

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
10. [Interview Questions & Answers](#10-interview-questions--answers)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary / Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

### What is the Iterator Pattern?
The Iterator pattern is a behavioral design pattern that provides a way to **access the elements of an aggregate object sequentially without exposing its underlying representation** (whether an array, linked list, hash table, or balanced binary tree).

In Java:
- It is the foundation of the Java Collections Framework (JCF) and the enhanced for-loop (`for (T item : collection)`).
- It decouples traversal algorithms from collection data structures.
- It enables multiple concurrent traversals over the same collection independently.

---

## 2. Core Concepts

### `Iterable` vs. `Iterator`
- **`java.lang.Iterable<T>`**: The collection interface that declares `Iterator<T> iterator()`. Any class implementing `Iterable` can be used in an enhanced for-loop.
- **`java.util.Iterator<T>`**: The cursor interface that declares:
  - `boolean hasNext()`: Returns true if more elements exist.
  - `T next()`: Advances the cursor and returns the next element.
  - `default void remove()`: Safely removes the last element returned by `next()`.

### Fail-Fast vs. Fail-Safe Iterators
- **Fail-Fast (`ArrayList`, `HashMap`)**: Modifying the collection directly during iteration increments `modCount`, throwing `ConcurrentModificationException` immediately on the next step.
- **Fail-Safe / Weakly Consistent (`CopyOnWriteArrayList`, `ConcurrentHashMap`)**: Iterates over a snapshot or internal bucket structure without throwing `ConcurrentModificationException`.

---

## 3. How It Works Internally

### Enhanced For-Loop Translation by `javac`
When you write:
```java
for (String s : list) {
    System.out.println(s);
}
```
The Java compiler desugars this into bytecode equivalent to:
```java
Iterator<String> it = list.iterator();
while (it.hasNext()) {
    String s = it.next();
    System.out.println(s);
}
```

---

## 4. Syntax & Usage

### 1. Implementing a Custom Iterable / Iterator
```java
import java.util.Iterator;
import java.util.NoSuchElementException;

public class Range implements Iterable<Integer> {
    private final int start;
    private final int end;

    public Range(int start, int end) {
        this.start = start;
        this.end = end;
    }

    @Override
    public Iterator<Integer> iterator() {
        return new Iterator<>() {
            private int current = start;

            @Override
            public boolean hasNext() {
                return current < end;
            }

            @Override
            public Integer next() {
                if (!hasNext()) throw new NoSuchElementException();
                return current++;
            }
        };
    }
}
```

---

## 5. Practical Examples

### Example 1: Tree Pre-Order Depth-First Iterator
**Goal:** Implement an iterator that traverses a binary tree without exposing tree nodes or recursion.

```java
import java.util.ArrayDeque;
import java.util.Deque;
import java.util.Iterator;
import java.util.NoSuchElementException;

public class BinaryTree<T> implements Iterable<T> {
    public static class Node<T> {
        T val;
        Node<T> left;
        Node<T> right;
        public Node(T v) { this.val = v; }
    }

    private final Node<T> root;

    public BinaryTree(Node<T> root) { this.root = root; }

    @Override
    public Iterator<T> iterator() {
        return new PreOrderIterator<>(root);
    }

    private static class PreOrderIterator<T> implements Iterator<T> {
        private final Deque<Node<T>> stack = new ArrayDeque<>();

        public PreOrderIterator(Node<T> root) {
            if (root != null) stack.push(root);
        }

        @Override
        public boolean hasNext() {
            return !stack.isEmpty();
        }

        @Override
        public T next() {
            if (!hasNext()) throw new NoSuchElementException();
            Node<T> current = stack.pop();
            // Push right child first so left is processed first
            if (current.right != null) stack.push(current.right);
            if (current.left != null) stack.push(current.left);
            return current.val;
        }
    }
}
```

### Example 2: Parallel Iteration with `Spliterator`
**Goal:** Split collection chunks across multiple threads for parallel processing.

```java
import java.util.List;
import java.util.Spliterator;

public class SpliteratorDemo {
    public static void main(String[] args) {
        List<String> items = List.of("A", "B", "C", "D", "E", "F");
        Spliterator<String> s1 = items.spliterator();
        
        // Split s1 in half: s2 gets first half, s1 keeps remainder
        Spliterator<String> s2 = s1.trySplit();

        System.out.println("--- Chunk 1 ---");
        s2.forEachRemaining(System.out::println);

        System.out.println("--- Chunk 2 ---");
        s1.forEachRemaining(System.out::println);
    }
}
```

---

### Example 3: Infinite Generators via Stream Iterators
**Goal:** Generate mathematical streams using `Stream.iterate` and custom `Spliterator`.

```java
import java.util.stream.Stream;

public class InfiniteGenerator {
    public static Stream<Long> naturalNumbers() {
        return Stream.iterate(1L, n -> n + 1);
    }
}

### Example 4: Safe Reversible ListIterator Traversal
**Goal:** Demonstrate bidirectional cursor movement and element replacement with `ListIterator`.

```java
import java.util.ArrayList;
import java.util.List;
import java.util.ListIterator;

public class ListIteratorDemo {
    public static void replaceItems(List<String> words) {
        ListIterator<String> it = words.listIterator();
        while (it.hasNext()) {
            String word = it.next();
            if ("target".equals(word)) {
                it.set("replacement"); // Replaces element without breaking iterator!
            }
        }
    }
}

## 6. Best Practices

1. **Always Implement `Iterable<T>` on Custom Aggregates**: Enables seamless use with the enhanced for-loop.
2. **Throw `NoSuchElementException` in `next()`**: If `hasNext()` is false, `next()` must throw `NoSuchElementException`.
3. **Use `removeIf()` for Filtering**: Prefer `collection.removeIf()` over manual `iterator.remove()` loops.
4. **Prefer Streams and Spliterators for Parallel Traversal**: Use Java Streams to traverse data in parallel without manual index slicing.
5. **Keep Iterators Independent**: Each call to `iterator()` must return a fresh, independent cursor.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Secure Fix |
| :--- | :--- | :--- |
| **`collection.remove()` during enhanced for-loop** | Fails with `ConcurrentModificationException`. | Use `iterator.remove()` or `collection.removeIf()`. |
| **Returning null in `next()` when finished** | Violates Iterator contract; callers expect `NoSuchElementException`. | Check `hasNext()` and throw `NoSuchElementException`. |
| **Calling `next()` twice in one loop condition** | Skips elements and causes index out of bounds. | Call `next()` once per loop iteration. |

---

## 8. Performance & Security Notes

### Inlining Iterators
In HotSpot, small iterator objects (`ArrayList$Itr`) allocated inside methods are eliminated via **Escape Analysis and Scalar Replacement**, compiling the iteration directly into a fast pointer loop with zero garbage generation.

---

## 9. Comparison / When to Use What

| Technique | State | Thread-Safety | Best For |
| :--- | :--- | :--- | :--- |
| **Enhanced For-loop** | Hidden Iterator | Fail-fast | 95% of standard traversals |
| **Explicit Iterator** | Visible cursor | Supports safe `remove()` | In-flight filtering / removals |
| **Spliterator** | Partitionable | Parallel Streams | Multi-threaded parallel processing |

---

## 10. Interview Questions & Answers

### Q1: What is the difference between Fail-Fast and Fail-Safe iterators?
<details>
<summary>View Answer</summary>
- **Fail-Fast** (`ArrayList`, `HashSet`): Uses an internal modification count (`modCount`). If the collection is structurally modified during iteration by any means other than `iterator.remove()`, it throws `ConcurrentModificationException` immediately.
- **Fail-Safe / Weakly Consistent** (`ConcurrentHashMap`, `CopyOnWriteArrayList`): Iterates over a snapshot or internal bucket nodes. It does not throw `ConcurrentModificationException` and guarantees that elements existing at creation time will be traversed.
</details>

### Q2: What happens if `next()` is called when `hasNext()` is false?
<details>
<summary>View Answer</summary>
The Java Iterator specification strictly mandates throwing a `java.util.NoSuchElementException`. Returning null or hanging violates the contract and leads to bugs in client loops.
</details>

### Q3: What is a `Spliterator` in Java 8?
<details>
<summary>View Answer</summary>
`Spliterator` (Splittable Iterator) is designed for parallel traversal. In addition to sequential element access (`tryAdvance`), it provides `trySplit()`, which partitions a portion of the aggregate into a new `Spliterator`, allowing multiple threads to process partitions concurrently in the ForkJoinPool.
</details>

### Q4: How does the enhanced for-loop work behind the scenes?
<details>
<summary>View Answer</summary>
For objects implementing `Iterable`, `javac` generates bytecode that calls `iterable.iterator()` and loops using `while (iterator.hasNext()) { Object o = iterator.next(); }`. For primitive or object arrays, `javac` generates an index-based `for (int i = 0; i < arr.length; i++)` loop.
</details>

### Q5: Can an Iterator traverse in reverse?
<details>
<summary>View Answer</summary>
Standard `Iterator<E>` is one-way forward only. `java.util.ListIterator<E>` extends `Iterator` to support bidirectional traversal (`hasPrevious()`, `previous()`), index queries (`nextIndex()`), and element insertion/replacement (`add()`, `set()`).
</details>

---

## 11. Practice Exercises

### Exercise 1: Build a Filtered Iterator
**Task:** Create an iterator wrapper `FilteringIterator<T>` that skips elements not matching a `Predicate<T>`.
<details>
<summary>View Solution</summary>

```java
import java.util.Iterator;
import java.util.NoSuchElementException;
import java.util.function.Predicate;

public class FilteringIterator<T> implements Iterator<T> {
    private final Iterator<T> delegate;
    private final Predicate<T> predicate;
    private T nextElement;
    private boolean hasNext = false;

    public FilteringIterator(Iterator<T> delegate, Predicate<T> predicate) {
        this.delegate = delegate;
        this.predicate = predicate;
    }

    @Override
    public boolean hasNext() {
        while (!hasNext && delegate.hasNext()) {
            T candidate = delegate.next();
            if (predicate.test(candidate)) {
                nextElement = candidate;
                hasNext = true;
            }
        }
        return hasNext;
    }

    @Override
    public T next() {
        if (!hasNext()) throw new NoSuchElementException();
        hasNext = false;
        return nextElement;
    }
}
```
</details>

### Exercise 2: Infinite Number Sequence Iterator
**Task:** Create an iterator yielding Fibonacci numbers up to $N$.
<details>
<summary>View Solution</summary>

```java
import java.util.Iterator;

public class FibonacciRange implements Iterable<Long> {
    private final int count;
    public FibonacciRange(int count) { this.count = count; }

    @Override
    public Iterator<Long> iterator() {
        return new Iterator<>() {
            private int seen = 0;
            private long a = 0, b = 1;

            @Override public boolean hasNext() { return seen < count; }
            @Override public Long next() {
                seen++;
                long cur = a;
                long nextVal = a + b;
                a = b; b = nextVal;
                return cur;
            }
        };
    }
}
```
</details>

### Exercise 3: Array Iterator Implementation
**Task:** Build an iterator over a raw array without using standard collections.
<details>
<summary>View Solution</summary>

```java
import java.util.Iterator;
import java.util.NoSuchElementException;

public class ArrayIterator<T> implements Iterator<T> {
    private final T[] array;
    private int cursor = 0;
    public ArrayIterator(T[] arr) { this.array = arr; }

    @Override public boolean hasNext() { return cursor < array.length; }
    @Override public T next() {
        if (!hasNext()) throw new NoSuchElementException();
        return array[cursor++];
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **Core Goal**: Access elements sequentially without exposing internal collection structure.
- **Fail-Fast**: Standard collections throw `ConcurrentModificationException` on illegal mutations.
- **Fail-Safe**: Concurrent collections iterate over snapshots without exceptions.
- **Enhanced For**: Sugar for `while(it.hasNext()) { it.next(); }`.
- **Spliterator**: Parallel iteration engine for Java Streams.
- **Exception**: Must throw `NoSuchElementException` when finished.

---

## 13. Further Reading
- Effective Java: [Item 58: Prefer for-each loops to traditional for loops](https://www.oreilly.com/library/view/effective-java-3rd/9780134686097/)
- GoF Design Patterns: [Iterator Pattern](https://www.oreilly.com/library/view/design-patterns-elements/0201633612/)
- Oracle Java Documentation: [Spliterator Interface](https://docs.oracle.com/en/java/javase/21/docs/api/java.base/java/util/Spliterator.html)
