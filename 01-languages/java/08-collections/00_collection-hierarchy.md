# Java Collection Framework Hierarchy

> The foundational architectural blueprint of Java collections: core interfaces, polymorphic contracts, and modern Sequenced Collections (JEP 431).

| Metadata | Value |
|---|---|
| **Level** | Intermediate |
| **Prerequisites** | [07-generics/00_generic-classes-and-methods.md](../07-generics/00_generic-classes-and-methods.md), [04-oop/09_interfaces.md](../04-oop/09_interfaces.md) |
| **Time to Read** | 25 minutes |
| **Status** | Active |
| **Java Version** | Java 1.2+ (Sequenced Collections Java 21 LTS JEP 431) |

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

Before Java 1.2, Java possessed limited, ad-hoc container classes: `Vector`, `Hashtable`, `Stack`, and primitive arrays. These legacy classes lacked a unified interface hierarchy, had synchronized methods that incurred heavy synchronization overhead, and had inconsistent method names (`addElement` vs `put`).

The **Java Collections Framework (JCF)** introduced a unified, highly optimized, and extensible architecture based on interfaces, implementations, and algorithms.

For over two decades, a notable architectural gap existed in JCF: collections with a defined encounter order (like `List`, `Deque`, `LinkedHashSet`, `SortedSet`) lacked a common superinterface to express "a collection with a first and last element". In **Java 21 LTS (JEP 431)**, this gap was definitively closed with **Sequenced Collections** (`SequencedCollection`, `SequencedSet`, `SequencedMap`), introducing uniform APIs for first/last element access and reversed views.

---

## 2. Core Concepts

### The Two Root Branches
The Collections Framework is split into two primary roots:
1. `java.lang.Iterable<T>` $\to$ `java.util.Collection<E>`: Represents a group of individual objects.
2. `java.util.Map<K, V>`: Represents key-value mappings (does **not** implement `Collection` or `Iterable`).

```
                    Iterable<E>
                        |
                  Collection<E>
                 /      |      \
                /       |       \
            List<E>   Set<E>   Queue<E>
                        |         |
                    SortedSet<E> Deque<E>

(Separate hierarchy):
                    Map<K, V>
                        |
                   SortedMap<K, V>
```

### Sequenced Collections (Java 21 LTS - JEP 431)
Java 21 retrofitted the hierarchy to standardize encounter-order operations across `List`, `Deque`, `LinkedHashSet`, and `SortedSet`:

```
               Collection<E>
                     |
            SequencedCollection<E>
            /         |          \
        List<E>    Deque<E>   SequencedSet<E>
                                  /         \
                       LinkedHashSet<E>   SortedSet<E>
```

For Maps:
```
                    Map<K, V>
                        |
                SequencedMap<K, V>
                /                \
        LinkedHashMap<K, V>    SortedMap<K, V>
```

New Uniform Methods on `SequencedCollection<E>`:
- `void addFirst(E e)`
- `void addLast(E e)`
- `E getFirst()`
- `E getLast()`
- `E removeFirst()`
- `E removeLast()`
- `SequencedCollection<E> reversed()`

---

## 3. How It Works Internally

### Retrofitting and Default Methods
The `SequencedCollection` interface was integrated without breaking existing code by utilizing **default methods**:
- In `List<E>`, `getFirst()` defaults to `get(0)` and `getLast()` defaults to `get(size() - 1)`.
- In `Deque<E>`, `getFirst()` delegates to `peekFirst()`.
- The `reversed()` method returns a lightweight reverse-ordered **view** without copying underlying data structures. Mutations to the reversed view mutate the original collection in reverse index position.

```
Original List: [ "A", "B", "C" ]  (indices 0, 1, 2)
                      ^
                      |  view wrapper (O(1) allocation)
                      v
Reversed View: [ "C", "B", "A" ]  (index 0 -> original index 2)
```

---

## 4. Syntax & Usage

### 1. Java 21 Sequenced Collection Operations
```java
import java.util.ArrayList;
import java.util.List;
import java.util.SequencedCollection;

public class SequencedDemo {
    public static void main(String[] args) {
        SequencedCollection<String> items = new ArrayList<>();
        items.add("middle");
        items.addFirst("first");
        items.addLast("last");

        System.out.println("First element: " + items.getFirst()); // "first"
        System.out.println("Last element: " + items.getLast());   // "last"

        // Reverse view traversal without copying
        for (String item : items.reversed()) {
            System.out.print(item + " -> "); // last -> middle -> first ->
        }
        System.out.println();
    }
}
```

### 2. Polymorphic Collection Parameterization
```java
// Accept the most general interface required
public static <T> void printSummary(Collection<T> coll) {
    System.out.println("Size: " + coll.size());
    System.out.println("Is Empty: " + coll.isEmpty());
    for (T item : coll) {
        System.out.println(" - " + item);
    }
}
```

---

## 5. Practical Examples

### Example 1: Uniform Deque and List Traversal (Basic)
```java
import java.util.ArrayDeque;
import java.util.List;
import java.util.SequencedCollection;

public class UniformTraversal {
    public static <T> T extractEndpoints(SequencedCollection<T> seq) {
        if (seq.isEmpty()) return null;
        System.out.println("Head: " + seq.getFirst() + " | Tail: " + seq.getLast());
        return seq.getFirst();
    }

    public static void main(String[] args) {
        extractEndpoints(List.of("alpha", "beta", "omega"));
        extractEndpoints(new ArrayDeque<>(List.of(10, 20, 30)));
    }
}
```

### Example 2: LRU Cache Eviction via SequencedMap (Real-World)
```java
import java.util.LinkedHashMap;
import java.util.Map;
import java.util.SequencedMap;

public class SimpleLruCache<K, V> {
    private final int capacity;
    private final SequencedMap<K, V> map = new LinkedHashMap<>();

    public SimpleLruCache(int capacity) {
        this.capacity = capacity;
    }

    public V get(K key) {
        V value = map.remove(key);
        if (value != null) {
            map.putLast(key, value); // Mark as most recently used
        }
        return value;
    }

    public void put(K key, V value) {
        map.remove(key);
        if (map.size() >= capacity) {
            // Java 21: cleanly poll first entry without iterator boilerplate!
            Map.Entry<K, V> eldest = map.pollFirstEntry();
            System.out.println("Evicted eldest key: " + eldest.getKey());
        }
        map.putLast(key, value);
    }
}
```

### Example 3: Edge Case - Reversed View Mutation Reflection
```java
import java.util.ArrayList;
import java.util.List;

public class ReversedViewMutation {
    public static void main(String[] args) {
        List<String> list = new ArrayList<>(List.of("A", "B", "C"));
        List<String> reversed = list.reversed();

        reversed.set(0, "Z"); // Mutating reversed index 0 mutates list index 2!
        System.out.println("Original list: " + list); // [A, B, Z]
    }
}
```

### Example 4: Failure Case - UnsupportedOperationException on Unmodifiable Sequenced Collection
```java
import java.util.List;

public class UnmodifiableSequencedFailure {
    public static void main(String[] args) {
        List<String> immutable = List.of("x", "y");
        // immutable.addFirst("w");
        // Throws UnsupportedOperationException at runtime!
    }
}
```

---

## 6. Best Practices

1. **Program to Interfaces**: Always declare variables and parameters using `Collection`, `List`, `Set`, `Map`, or `SequencedCollection` rather than concrete types (`ArrayList`, `HashMap`).
2. **Leverage Sequenced Collections (Java 21+)**: Replace verbose iterator and index calculations (`list.get(list.size() - 1)`) with `coll.getLast()`.
3. **Prefer Unmodifiable Collections When Returning State**: Use `Collections.unmodifiableList` or `List.copyOf` to prevent internal state leaks.
4. **Choose the Right Interface**: Do not use `List` when uniqueness is required (use `Set`); do not use `List` when FIFO/LIFO is the core concern (use `Queue` or `Deque`).

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why Wrong | Fix |
|---|---|---|
| Using legacy `Vector` or `Hashtable` | Synchronizes every method, incurring severe performance overhead. | Use `ArrayList` / `HashMap` or `ConcurrentHashMap`. |
| Writing `list.get(list.size() - 1)` in Java 21+ | Verbose, error-prone with off-by-one errors. | Use `list.getLast()`. |
| Modifying collection during iteration without iterator | Throws `ConcurrentModificationException`. | Use `Iterator.remove()` or `removeIf()`. |
| Assuming `Set` has no order | While `HashSet` is unordered, `LinkedHashSet` and `TreeSet` are `SequencedSet`s with encounter order. | Use `SequencedSet` when order matters in a set. |
| Treating `Map` as a `Collection` | `Map` does not implement `Collection`. Calling `map.iterator()` will not compile. | Iterate over `map.entrySet()`, `keySet()`, or `values()`. |
| Assuming `reversed()` creates a copy | `reversed()` creates a live view; mutating it mutates the source collection. | Use `new ArrayList<>(coll.reversed())` if isolation is needed. |

---

## 8. Performance & Security Notes

### Performance
- **Zero-Copy Views**: `SequencedCollection.reversed()` executes in $O(1)$ time and allocates minimal wrapper memory. It does not perform an $O(N)$ reverse array copy.
- **Interface Call Overhead**: Calling interface methods in Java relies on `invokeinterface`. Modern HotSpot inlining optimizes monomorphic and bimorphic call sites to direct jumps with zero dynamic dispatch overhead.

### Security
- **Defensive Copying**: Passing internal collection references out of domain objects violates encapsulation. Always wrap or copy collections before exposing them outside the domain boundary.

---

## 9. Comparison / When to Use What

| Interface | Ordering | Duplicates | Key Operations | Typical Impl |
|---|---|---|---|---|
| `List` | Positional / Indexed | Yes | `get(i)`, `set(i, e)` | `ArrayList`, `LinkedList` |
| `Set` | No guarantee | No | `add(e)`, `contains(e)` | `HashSet` |
| `SequencedSet` | Deterministic order | No | `getFirst()`, `reversed()` | `LinkedHashSet`, `TreeSet` |
| `Queue` / `Deque` | FIFO / LIFO | Yes | `offer()`, `poll()` | `ArrayDeque`, `PriorityQueue` |
| `Map` | Key-Value pairs | Unique Keys | `get(k)`, `put(k, v)` | `HashMap`, `TreeMap` |

---

## 10. Interview Questions

<details>
<summary>1. What are Sequenced Collections introduced in Java 21 (JEP 431)?</summary>
Sequenced Collections introduce three core interfaces: `SequencedCollection`, `SequencedSet`, and `SequencedMap`. They provide a unified API to access, add, remove, and reverse elements in collections that have a well-defined encounter order (such as `List`, `Deque`, `LinkedHashSet`, and `SortedSet`).
</details>

<details>
<summary>2. Why does `Map` not extend `Collection` or `Iterable`?</summary>
`Collection` models a container of single elements (`E`), whereas `Map` models key-value pairs (`K, V`). Implementing `Collection` would force `Map` to choose whether it is a collection of keys, values, or entries, causing ambiguity and contract violations.
</details>

<details>
<summary>3. What is the difference between `Vector` and `ArrayList`?</summary>
`Vector` is a legacy synchronized class from Java 1.0 where every method acquires a lock, causing contention and performance degradation. `ArrayList` is an unsynchronized, highly optimized collection introduced in Java 1.2.
</details>

<details>
<summary>4. Does `coll.reversed()` create a copy of the collection?</summary>
No. `reversed()` returns a live, reverse-ordered view of the underlying collection. Modifications made to the view reflect in the original collection and vice versa.
</details>

<details>
<summary>5. What exception is thrown if you call `getFirst()` on an empty `SequencedCollection`?</summary>
`NoSuchElementException`.
</details>

<details>
<summary>6. How does `LinkedHashSet` fit into Sequenced Collections?</summary>
`LinkedHashSet` implements `SequencedSet`. It maintains insertion order, prevents duplicates, and now supports `addFirst()`, `addLast()`, `getFirst()`, and `reversed()`.
</details>

<details>
<summary>7. What happens if you call `reversed().reversed()` on a sequenced collection?</summary>
The implementation returns the original collection instance (or an equivalent direct forward view) without layering multiple view wrappers.
</details>

<details>
<summary>8. Why is programming to collection interfaces considered a fundamental best practice?</summary>
It decouples client business logic from specific internal data structures, allowing implementations to be swapped (e.g., replacing `ArrayList` with `CopyOnWriteArrayList` for concurrency) without modifying client code.
</details>

---

## 11. Practice Exercises

### Easy
1. Create a `List<String>`, populate it with 5 items, and print its first and last elements using Java 21 Sequenced Collection methods.
2. Iterate through a `SequencedSet<Integer>` in reverse order and print each element.
3. Write a utility method that checks whether any `Collection<?>` is null or empty.

### Medium
4. Implement a palindrome checker using `SequencedCollection.reversed()` comparison.
5. Create an in-memory audit log using `SequencedCollection` that automatically drops the oldest entry when reaching 1,000 entries.
6. Write a generic method that merges two `SequencedCollection<T>` instances into a single alternating list.

### Hard
7. Implement a custom `SequencedCollection<E>` wrapper that intercepts mutations and produces an immutable audit trail.
8. Design a bounded RingBuffer that implements `SequencedCollection<E>` with $O(1)$ operations on both ends.
9. Implement a dual-ended iterator for an arbitrary `SequencedCollection` that steps from both ends simultaneously until they meet in the middle.

<details>
<summary>Solutions</summary>

```java
// Easy 1: Sequenced operations
List<String> list = new ArrayList<>(List.of("one", "two", "three"));
System.out.println("First: " + list.getFirst());
System.out.println("Last: " + list.getLast());

// Medium 4: Palindrome checker
public static <T> boolean isPalindrome(List<T> list) {
    return list.equals(list.reversed());
}
```
</details>

---


### Deep-Dive: Bytecode Analysis of Sequenced Collections
In Java 21 LTS, the `SequencedCollection` interface is implemented using default methods on existing interfaces. Let us examine how `List.reversed()` is executed in bytecode:
```
public java.util.List reversed();
  Code:
     0: aload_0
     1: invokestatic  #14 // InterfaceMethod java/util/List.super:reversed()Ljava/util/List;
     4: areturn
```
Under the hood, `java.util.Collections$ReverseOrderListView` is instantiated:
```java
// Simplified OpenJDK internal representation
static class ReverseOrderListView<E> extends AbstractList<E> {
    private final List<E> base;
    ReverseOrderListView(List<E> base) { this.base = base; }
    public E get(int index) { return base.get(base.size() - 1 - index); }
    public E set(int index, E element) { return base.set(base.size() - 1 - index, element); }
    public int size() { return base.size(); }
}
```
Because this view only translates index arithmetic (`size - 1 - index`), it incurs:
- Exactly **one reference allocation** (the view instance).
- **Zero data array duplication**.
- $O(1)$ constant time complexity for creation and mutations.

### Detailed Exercise Solutions (All 9 Problems)
<details>
<summary>View Complete Solutions for All Exercises</summary>

```java
// Easy 1: First and last access
List<String> list = new ArrayList<>(List.of("Alpha", "Beta", "Gamma", "Delta", "Omega"));
System.out.println("First element: " + list.getFirst());
System.out.println("Last element: " + list.getLast());

// Easy 2: Reverse Set traversal
SequencedSet<Integer> seqSet = new LinkedHashSet<>(List.of(10, 20, 30, 40));
for (int num : seqSet.reversed()) {
    System.out.print(num + " "); // 40 30 20 10
}
System.out.println();

// Easy 3: Null/empty check
public static boolean isNullOrEmpty(Collection<?> coll) {
    return coll == null || coll.isEmpty();
}

// Medium 4: Palindrome check using reversed view
public static <T> boolean isPalindrome(List<T> list) {
    if (list == null || list.size() <= 1) return true;
    return list.equals(list.reversed());
}

// Medium 5: Bounded in-memory audit log
public class AuditLog<T> {
    private final int maxSize;
    private final SequencedCollection<T> log = new ArrayDeque<>();
    public AuditLog(int maxSize) { this.maxSize = maxSize; }
    public synchronized void append(T entry) {
        if (log.size() >= maxSize) {
            log.removeFirst(); // Drop oldest
        }
        log.addLast(entry);
    }
    public synchronized List<T> getEntriesRecentFirst() {
        return List.copyOf(log.reversed());
    }
}

// Medium 6: Alternating merge
public static <T> List<T> mergeAlternating(SequencedCollection<T> a, SequencedCollection<T> b) {
    List<T> result = new ArrayList<>(a.size() + b.size());
    var itA = a.iterator();
    var itB = b.iterator();
    while (itA.hasNext() || itB.hasNext()) {
        if (itA.hasNext()) result.add(itA.next());
        if (itB.hasNext()) result.add(itB.next());
    }
    return result;
}

// Hard 7: Intercepting Sequenced Collection Wrapper
public class AuditedSequencedCollection<E> implements SequencedCollection<E> {
    private final SequencedCollection<E> delegate;
    private final List<String> mutations = new ArrayList<>();
    public AuditedSequencedCollection(SequencedCollection<E> delegate) { this.delegate = delegate; }
    public void addFirst(E e) { mutations.add("addFirst: " + e); delegate.addFirst(e); }
    public void addLast(E e) { mutations.add("addLast: " + e); delegate.addLast(e); }
    public E getFirst() { return delegate.getFirst(); }
    public E getLast() { return delegate.getLast(); }
    public E removeFirst() { mutations.add("removeFirst"); return delegate.removeFirst(); }
    public E removeLast() { mutations.add("removeLast"); return delegate.removeLast(); }
    public SequencedCollection<E> reversed() { return delegate.reversed(); }
    public int size() { return delegate.size(); }
    public boolean isEmpty() { return delegate.isEmpty(); }
    public boolean contains(Object o) { return delegate.contains(o); }
    public java.util.Iterator<E> iterator() { return delegate.iterator(); }
    public Object[] toArray() { return delegate.toArray(); }
    public <T> T[] toArray(T[] a) { return delegate.toArray(a); }
    public boolean add(E e) { mutations.add("add: " + e); return delegate.add(e); }
    public boolean remove(Object o) { mutations.add("remove: " + o); return delegate.remove(o); }
    public boolean containsAll(Collection<?> c) { return delegate.containsAll(c); }
    public boolean addAll(Collection<? extends E> c) { return delegate.addAll(c); }
    public boolean removeAll(Collection<?> c) { return delegate.removeAll(c); }
    public boolean retainAll(Collection<?> c) { return delegate.retainAll(c); }
    public void clear() { mutations.add("clear"); delegate.clear(); }
    public List<String> getAuditLog() { return List.copyOf(mutations); }
}

// Hard 8: Bounded Ring Buffer
public class RingBuffer<E> implements SequencedCollection<E> {
    private final Object[] buffer;
    private int head = 0, tail = 0, count = 0;
    public RingBuffer(int capacity) { buffer = new Object[capacity]; }
    public void addFirst(E e) {
        if (count == buffer.length) throw new IllegalStateException("Buffer full");
        head = (head - 1 + buffer.length) % buffer.length;
        buffer[head] = e;
        count++;
    }
    public void addLast(E e) {
        if (count == buffer.length) throw new IllegalStateException("Buffer full");
        buffer[tail] = e;
        tail = (tail + 1) % buffer.length;
        count++;
    }
    @SuppressWarnings("unchecked") public E getFirst() {
        if (count == 0) throw new NoSuchElementException();
        return (E) buffer[head];
    }
    @SuppressWarnings("unchecked") public E getLast() {
        if (count == 0) throw new NoSuchElementException();
        return (E) buffer[(tail - 1 + buffer.length) % buffer.length];
    }
    public E removeFirst() {
        E item = getFirst();
        buffer[head] = null;
        head = (head + 1) % buffer.length;
        count--;
        return item;
    }
    public E removeLast() {
        E item = getLast();
        int idx = (tail - 1 + buffer.length) % buffer.length;
        buffer[idx] = null;
        tail = idx;
        count--;
        return item;
    }
    public SequencedCollection<E> reversed() { throw new UnsupportedOperationException(); }
    public int size() { return count; }
    public boolean isEmpty() { return count == 0; }
    public boolean contains(Object o) { return false; }
    public java.util.Iterator<E> iterator() { throw new UnsupportedOperationException(); }
    public Object[] toArray() { throw new UnsupportedOperationException(); }
    public <T> T[] toArray(T[] a) { throw new UnsupportedOperationException(); }
    public boolean add(E e) { addLast(e); return true; }
    public boolean remove(Object o) { return false; }
    public boolean containsAll(Collection<?> c) { return false; }
    public boolean addAll(Collection<? extends E> c) { return false; }
    public boolean removeAll(Collection<?> c) { return false; }
    public boolean retainAll(Collection<?> c) { return false; }
    public void clear() { head = tail = count = 0; }
}

// Hard 9: Dual-ended iterator
public static <T> void dualEndedTraverse(SequencedCollection<T> seq, java.util.function.BiConsumer<T, T> consumer) {
    var forward = seq.iterator();
    var backward = seq.reversed().iterator();
    int count = seq.size();
    for (int i = 0; i < count / 2; i++) {
        consumer.accept(forward.next(), backward.next());
    }
}
```
</details>


## 12. Summary / Cheat Sheet

- **Roots**: `Collection<E>` (single elements) and `Map<K, V>` (key-value associations).
- **Sequenced Collections (Java 21)**: `SequencedCollection`, `SequencedSet`, `SequencedMap`.
- **First/Last Access**: `getFirst()`, `getLast()`, `addFirst()`, `addLast()`, `removeFirst()`, `removeLast()`.
- **Reverse View**: `reversed()` returns a live $O(1)$ reverse-ordered view.
- **Rule of Thumb**: Program to the highest abstract interface required (`Collection`, `List`, `Set`, `SequencedCollection`).

---

## 13. Further Reading
- JEP 431: *Sequenced Collections* (OpenJDK).
- Bloch, Joshua. *Effective Java (3rd Edition)*: Item 64 (Refer to objects by their interfaces).
- Oracle Java Documentation: *The Collections Framework Guide*.
