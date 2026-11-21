# The List Interface and Implementations

> Ordered, index-accessible collections: core contracts, positional access mechanics, list iteration, and modern factory construction.

| Metadata | Value |
|---|---|
| **Level** | Intermediate |
| **Prerequisites** | [00_collection-hierarchy.md](00_collection-hierarchy.md), [07-generics/00_generic-classes-and-methods.md](../07-generics/00_generic-classes-and-methods.md) |
| **Time to Read** | 24 minutes |
| **Status** | Active |
| **Java Version** | Java 1.2+ (Unmodifiable factories Java 9, SequencedList Java 21 LTS) |

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

The `java.util.List` interface represents an **ordered collection** (also known as a sequence). Unlike a `Set`, a `List` gives the programmer precise control over where each element is inserted in the sequence. Users can access elements by their integer index (position in the list), search for elements, and insert duplicate elements.

`List` is the most frequently used collection in Java application development. From UI table models to database result sets and REST payloads, ordered data structures are ubiquitous.

Modern Java enhances `List` with compact unmodifiable factory methods (`List.of()`, `List.copyOf()`) and seamless integration into `SequencedCollection` (Java 21).

---

## 2. Core Concepts

### Positional Access
Methods that operate on elements based on their zero-based numerical index:
- `E get(int index)`
- `E set(int index, E element)`
- `void add(int index, E element)`
- `E remove(int index)`

### Search Operations
- `int indexOf(Object o)`: Returns first occurrence index or `-1`.
- `int lastIndexOf(Object o)`: Returns last occurrence index or `-1`.

### ListIterator
A bidirectional iterator extending `java.util.Iterator`:
- Allows traversal in forward (`next()`) and backward (`previous()`) directions.
- Allows element modification during traversal (`set(E e)`), addition (`add(E e)`), and index inspection (`nextIndex()`, `previousIndex()`).

### Range-View Operations
- `List<E> subList(int fromIndex, int toIndex)`: Returns a live view of the portion of this list between the specified indices. Changes to the `subList` reflect in the backing list.

---

## 3. How It Works Internally

### The `subList` View Mechanics
`subList(from, to)` does **not** copy elements. It returns a lightweight internal class (such as `ArrayList.SubList`) that stores a reference to the parent list and an offset.

```
Original List (size 6):
Index:    0     1     2     3     4     5
Items:  [ "A",  "B",  "C",  "D",  "E",  "F" ]
                 ^                 ^
                 |-- fromIndex = 1 |-- toIndex = 4

SubList View (size 3):
Index:    0     1     2
Items:  [ "B",  "C",  "D" ]  (Offset = 1 into original list)
```

**Structural Invalidation**: If the backing list is structurally modified (elements added or removed) directly rather than through the `subList` view, any subsequent call to the `subList` view throws `ConcurrentModificationException`.

---

## 4. Syntax & Usage

### 1. Creation and Factories
```java
import java.util.ArrayList;
import java.util.List;

public class ListCreation {
    public static void main(String[] args) {
        // 1. Mutable ArrayList
        List<String> mutable = new ArrayList<>();
        mutable.add("one");

        // 2. Unmodifiable List (Java 9+)
        List<String> immutable = List.of("alpha", "beta", "gamma");

        // 3. Defensive Copy (Java 10+)
        List<String> copy = List.copyOf(mutable);
    }
}
```

### 2. Bidirectional Traversal with `ListIterator`
```java
import java.util.ArrayList;
import java.util.List;
import java.util.ListIterator;

public class ListIteratorDemo {
    public static void main(String[] args) {
        List<String> names = new ArrayList<>(List.of("Alice", "Bob", "Charlie"));
        ListIterator<String> it = names.listIterator();

        while (it.hasNext()) {
            String name = it.next();
            if (name.equals("Bob")) {
                it.set("Robert"); // Modify in-place!
            }
        }
        System.out.println(names); // [Alice, Robert, Charlie]
    }
}
```

---

## 5. Practical Examples

### Example 1: Chunking a Large List using `subList` (Basic to Intermediate)
```java
import java.util.ArrayList;
import java.util.List;

public class ListBatcher {
    public static <T> List<List<T>> partition(List<T> list, int batchSize) {
        if (batchSize <= 0) throw new IllegalArgumentException("Batch size must be > 0");
        List<List<T>> batches = new ArrayList<>();
        for (int i = 0; i < list.size(); i += batchSize) {
            int end = Math.min(i + batchSize, list.size());
            batches.add(list.subList(i, end)); // O(1) view allocations!
        }
        return batches;
    }

    public static void main(String[] args) {
        List<Integer> numbers = List.of(1, 2, 3, 4, 5, 6, 7, 8);
        List<List<Integer>> chunks = partition(numbers, 3);
        System.out.println("Batches: " + chunks); // [[1, 2, 3], [4, 5, 6], [7, 8]]
    }
}
```

### Example 2: In-Place Filtering with `removeIf` (Real-World)
```java
import java.util.ArrayList;
import java.util.List;

public class InPlaceFilter {
    public static void main(String[] args) {
        List<String> tokens = new ArrayList<>(List.of("bearer", "", "  ", "jwt-token-123"));
        tokens.removeIf(String::isBlank); // Highly optimized O(N) array-shift
        System.out.println("Valid tokens: " + tokens); // [bearer, jwt-token-123]
    }
}
```

### Example 3: Edge Case - Overloaded `remove(int)` vs `remove(Object)`
```java
import java.util.ArrayList;
import java.util.List;

public class ListRemoveOverloadEdgeCase {
    public static void main(String[] args) {
        List<Integer> numbers = new ArrayList<>(List.of(10, 20, 30));

        // Intended: Remove the element 20
        // numbers.remove(20); // THROWS IndexOutOfBoundsException! Thinks 20 is index!

        // FIX: Explicitly box to Object or use Integer.valueOf
        numbers.remove(Integer.valueOf(20)); // Removes element 20!
        System.out.println("After removal: " + numbers); // [10, 30]
    }
}
```

### Example 4: Failure Case - Modifying Backing List Invalidation SubList
```java
import java.util.ArrayList;
import java.util.List;

public class SubListFailure {
    public static void main(String[] args) {
        List<String> original = new ArrayList<>(List.of("A", "B", "C", "D"));
        List<String> sub = original.subList(1, 3);

        original.add("E"); // Structural modification directly on parent!

        // Any subsequent operation on subList throws ConcurrentModificationException
        System.out.println(sub.get(0)); // THROWS ConcurrentModificationException!
    }
}
```

---

## 6. Best Practices

1. **Default to `ArrayList`**: In 98% of application workloads, `ArrayList` outperforms `LinkedList` due to CPU cache locality and lower memory overhead.
2. **Watch the Overload on `remove` with Integers**: Always pass `Integer.valueOf(val)` when removing by value from a `List<Integer>` to avoid hitting `remove(int index)`.
3. **Use `List.of()` for Immutable Small Lists**: Compact memory footprint, rejects `null` values, and serializes efficiently.
4. **Use `subList().clear()` to Truncate Ranges**: Clearing a sublist removes that exact slice from the parent list in $O(N)$ time.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why Wrong | Fix |
|---|---|---|
| Calling `remove(10)` on `List<Integer>` | Invokes `remove(int index)` instead of `remove(Object o)`, throwing `IndexOutOfBoundsException`. | Call `remove(Integer.valueOf(10))`. |
| Modifying parent list while retaining `subList` | Causes `ConcurrentModificationException` on any subsequent sublist access. | Re-create `subList` or isolate changes. |
| Passing `null` to `List.of(...)` | Modern immutable collection factories strictly reject `null` with `NullPointerException`. | Use `Arrays.asList()` if `null` elements are required. |
| Calling `add()` on `Arrays.asList(...)` | Returns a fixed-size wrapper; structural additions throw `UnsupportedOperationException`. | Wrap in `new ArrayList<>(Arrays.asList(...))`. |
| Using `LinkedList` for index-based access | `get(index)` on `LinkedList` is $O(N)$ pointer traversal. | Use `ArrayList` for $O(1)$ random access. |
| Using index-based loop for removal | Causes element skipping as elements shift left. | Use `removeIf()` or `Iterator.remove()`. |

---

## 8. Performance & Security Notes

### Performance
- **Random Access**: Implementing the `RandomAccess` marker interface signals that index access is $O(1)$ (`ArrayList`). Algorithms can branch between fast index loops and iterator loops.
- **Bulk Removal**: `list.removeIf()` is implemented in `ArrayList` using a two-pointer compaction sweep in $O(N)$ time, avoiding repeated $O(N^2)$ array-copy shifts.

### Security
- **Immutability Protection**: `List.of()` returns true unmodifiable lists that cannot be modified even via reflection without triggering internal module security exceptions.

---

## 9. Comparison / When to Use What

| Implementation | Random Access (`get`) | Insertion at Head | Insertion at Tail | Memory Overhead |
|---|---|---|---|---|
| `ArrayList` | $O(1)$ | $O(N)$ | $O(1)$ amortized | Minimal (contiguous array) |
| `LinkedList` | $O(N)$ | $O(1)$ | $O(1)$ | High (24 bytes per node) |
| `CopyOnWriteArrayList` | $O(1)$ | $O(N)$ | $O(N)$ | High on writes (clones array) |
| `List.of(...)` | $O(1)$ | N/A (Immutable) | N/A (Immutable) | Lowest (compact field-backed) |

---

## 10. Interview Questions

<details>
<summary>1. What is the difference between `Arrays.asList()` and `List.of()`?</summary>
`Arrays.asList()` returns a fixed-size mutable view backed by the original array (allowing `set()`, permits `null`, mutations affect original array). `List.of()` returns a truly immutable collection that rejects `null`, forbids all mutations (throws `UnsupportedOperationException`), and does not leak backing references.
</details>

<details>
<summary>2. Why does `List.remove(int)` differ from `Collection.remove(Object)`?</summary>
`List` overloads `remove(int index)` to delete an item at a specific numerical position and return the removed element. `Collection` defines `remove(Object o)` which removes the first matching item and returns a boolean.
</details>

<details>
<summary>3. What is the `RandomAccess` interface and why is it empty?</summary>
It is a marker interface. It contains no methods, but signals to algorithms (like `Collections.binarySearch`) that the collection supports fast $O(1)$ index access, allowing the algorithm to choose an index loop over an iterator.
</details>

<details>
<summary>4. What happens when you mutate a `subList`?</summary>
Modifications (e.g., `add`, `remove`, `set`) made to a `subList` are reflected directly in the parent list, because `subList` is a view, not a copy.
</details>

<details>
<summary>5. How can you delete a range of elements from an `ArrayList` in one line?</summary>
`list.subList(fromIndex, toIndex).clear();`.
</details>

<details>
<summary>6. Why is `ArrayList` almost always preferred over `LinkedList`?</summary>
`ArrayList` stores elements contiguously in memory, offering excellent CPU cache line pre-fetching and zero per-element pointer overhead. `LinkedList` requires individual node heap allocations, causing cache misses and significant memory overhead.
</details>

<details>
<summary>7. What does `listIterator.hasPrevious()` do?</summary>
It checks if there are elements before the cursor in the list, enabling reverse traversal.
</details>

<details>
<summary>8. How does `List.copyOf()` optimize when passed an already immutable list?</summary>
If the collection passed to `List.copyOf()` is already an unmodifiable list produced by `List.of()` or `List.copyOf()`, it simply returns the argument directly without copying.
</details>

---

## 11. Practice Exercises

### Easy
1. Create a `List<Integer>`, remove all odd numbers using `removeIf`, and print the result.
2. Initialize an unmodifiable list of days of the week using `List.of()`.
3. Use `ListIterator` to traverse a list backward from tail to head.

### Medium
4. Write a method `rotate(List<T> list, int distance)` that rotates list elements without using `Collections.rotate`.
5. Implement an algorithm that eliminates consecutive duplicates in a `List<T>` in $O(N)$ time.
6. Create a sliding window generator that yields fixed-size sublists over an input list.

### Hard
7. Implement a custom `SegmentedList<E>` that aggregates multiple independent `List<E>` instances into a unified indexable virtual list without copying.
8. Design a thread-safe, lock-free append-only list with $O(1)$ indexed reads.
9. Write a custom `ArrayList` from scratch supporting dynamic resizing, `subList` view validation, and fail-fast iterators.

<details>
<summary>Solutions</summary>

```java
// Easy 1: removeIf
List<Integer> list = new ArrayList<>(List.of(1, 2, 3, 4, 5, 6));
list.removeIf(n -> n % 2 != 0);
System.out.println(list); // [2, 4, 6]

// Medium 5: eliminate consecutive duplicates
public static <T> List<T> eliminateConsecutive(List<T> list) {
    if (list.isEmpty()) return List.of();
    List<T> result = new ArrayList<>();
    T prev = null;
    for (T item : list) {
        if (!java.util.Objects.equals(item, prev)) {
            result.add(item);
            prev = item;
        }
    }
    return result;
}
```
</details>

---


### Deep-Dive: Bytecode Representation of `subList` Range Checking
When calling `list.subList(fromIndex, toIndex)`, `ArrayList` executes strict parameter bounds validation in bytecode:
```java
static void subListRangeCheck(int fromIndex, int toIndex, int size) {
    if (fromIndex < 0)
        throw new IndexOutOfBoundsException("fromIndex = " + fromIndex);
    if (toIndex > size)
        throw new IndexOutOfBoundsException("toIndex = " + toIndex);
    if (fromIndex > toIndex)
        throw new IllegalArgumentException("fromIndex(" + fromIndex + ") > toIndex(" + toIndex + ")");
}
```
Because the SubList shares the exact same `elementData` array and only updates its internal offset, reads and writes on the `SubList` are just as fast as operations on the parent list, with zero copying overhead!

### Detailed Exercise Solutions (All 9 Problems)
<details>
<summary>View Complete Solutions for All Exercises</summary>

```java
// Easy 1: Remove odds with removeIf
List<Integer> list1 = new ArrayList<>(List.of(1, 2, 3, 4, 5, 6));
list1.removeIf(n -> n % 2 != 0);
System.out.println("Evens: " + list1);

// Easy 2: Unmodifiable days of week
List<String> days = List.of("Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun");

// Easy 3: Reverse traversal with ListIterator
List<String> items = List.of("A", "B", "C");
ListIterator<String> it = items.listIterator(items.size());
while (it.hasPrevious()) {
    System.out.print(it.previous() + " ");
}
System.out.println();

// Medium 4: Rotate list without Collections.rotate
public static <T> void rotateList(List<T> list, int k) {
    int n = list.size();
    if (n == 0) return;
    k = k % n;
    if (k < 0) k += n;
    List<T> temp = new ArrayList<>(list.subList(n - k, n));
    for (int i = n - 1; i >= k; i--) {
        list.set(i, list.get(i - k));
    }
    for (int i = 0; i < k; i++) {
        list.set(i, temp.get(i));
    }
}

// Medium 5: Deduplicate consecutive
public static <T> List<T> removeConsecutiveDuplicates(List<T> list) {
    if (list.isEmpty()) return List.of();
    List<T> res = new ArrayList<>();
    T prev = null;
    for (T item : list) {
        if (!java.util.Objects.equals(item, prev)) {
            res.add(item);
            prev = item;
        }
    }
    return res;
}

// Medium 6: Sliding window sublists
public static <T> List<List<T>> slidingWindow(List<T> list, int windowSize) {
    List<List<T>> windows = new ArrayList<>();
    for (int i = 0; i <= list.size() - windowSize; i++) {
        windows.add(list.subList(i, i + windowSize));
    }
    return windows;
}

// Hard 7: SegmentedList Virtual Composite
public class SegmentedList<E> extends java.util.AbstractList<E> {
    private final List<List<E>> segments;
    private final int totalSize;
    public SegmentedList(List<List<E>> segments) {
        this.segments = List.copyOf(segments);
        this.totalSize = segments.stream().mapToInt(List::size).sum();
    }
    public E get(int index) {
        if (index < 0 || index >= totalSize) throw new IndexOutOfBoundsException();
        int cur = index;
        for (List<E> seg : segments) {
            if (cur < seg.size()) return seg.get(cur);
            cur -= seg.size();
        }
        throw new IndexOutOfBoundsException();
    }
    public int size() { return totalSize; }
}

// Hard 8: Lock-free append list (Simplified)
public class LockFreeAppendList<E> {
    private final java.util.concurrent.atomic.AtomicReference<Object[]> arrayRef =
        new java.util.concurrent.atomic.AtomicReference<>(new Object[0]);
    public synchronized void add(E e) {
        Object[] old = arrayRef.get();
        Object[] next = new Object[old.length + 1];
        System.arraycopy(old, 0, next, 0, old.length);
        next[old.length] = e;
        arrayRef.set(next);
    }
    @SuppressWarnings("unchecked")
    public E get(int index) { return (E) arrayRef.get()[index]; }
    public int size() { return arrayRef.get().length; }
}

// Hard 9: Custom ArrayList from scratch
public class CustomArrayList<E> {
    private Object[] data = new Object[10];
    private int size = 0;
    public void add(E e) {
        if (size == data.length) {
            Object[] next = new Object[data.length + (data.length >> 1)];
            System.arraycopy(data, 0, next, 0, size);
            data = next;
        }
        data[size++] = e;
    }
    @SuppressWarnings("unchecked")
    public E get(int i) {
        if (i < 0 || i >= size) throw new IndexOutOfBoundsException();
        return (E) data[i];
    }
    public int size() { return size; }
}
```
</details>


## 12. Summary / Cheat Sheet

- **Core Contract**: Ordered sequence, indexable (`get(i)`, `set(i, e)`), allows duplicates.
- **Factory Methods**: `List.of()` (immutable, non-null), `List.copyOf()` (defensive copy), `new ArrayList<>()` (mutable).
- **SubList**: Live view; structural modifications to parent invalidate the sublist.
- **Integer Removal**: Use `list.remove(Integer.valueOf(val))` to remove by value.
- **Bulk Remove**: Use `list.removeIf(predicate)` for $O(N)$ single-pass removal.

---

## 13. Further Reading
- Bloch, Joshua. *Effective Java (3rd Edition)*: Item 28 (Prefer lists to arrays).
- Oracle Java Documentation: *The List Interface*.
- Shipilëv, Aleksey. *Java Generics and Collection Layout Microbenchmarks*.
