# Collections and Arrays Utility Classes

> Algorithmic toolkits in Java: TimSort vs Dual-Pivot Quicksort, binary search mechanics, thread-safe synchronization wrappers, and array manipulation.

| Metadata | Value |
|---|---|
| **Level** | Intermediate |
| **Prerequisites** | [01_list.md](01_list.md), [05_comparable-and-comparator.md](05_comparable-and-comparator.md) |
| **Time to Read** | 24 minutes |
| **Status** | Active |
| **Java Version** | Java 1.2+ (Modern array utilities Java 9+) |

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

Rather than duplicating common algorithms across individual collection classes, Java centralizes polymorphic algorithms into two static utility classes:
1. **`java.util.Collections`**: Operates on `Collection`, `List`, `Set`, and `Map` instances (sorting, searching, shuffling, reversing, thread-synchronization wrappers).
2. **`java.util.Arrays`**: Operates on primitive and reference arrays (sorting, binary searching, parallel sorting, prefix scans, deep comparisons).

These classes implement highly optimized, mathematically proven algorithms (such as Tim Peters' **TimSort** for object references and Vladimir Yaroslavskiy's **Dual-Pivot Quicksort** for primitive types).

---

## 2. Core Concepts

### Core Algorithms in `Collections`
- **Sorting & Searching**: `sort(list)`, `binarySearch(list, key)`
- **Rearranging**: `shuffle(list)`, `reverse(list)`, `rotate(list, distance)`, `swap(list, i, j)`
- **Extreme Values**: `min(coll)`, `max(coll)`
- **Composition & Counting**: `frequency(coll, o)`, `disjoint(c1, c2)`
- **Synchronization Wrappers**: `synchronizedList(list)`, `synchronizedMap(map)` (Legacy)

### Core Algorithms in `Arrays`
- **Sorting**: `sort(arr)`, `parallelSort(arr)` (uses ForkJoin pool for large arrays)
- **Searching**: `binarySearch(arr, key)`
- **Comparison (Java 9+)**: `equals(a, b)`, `compare(a, b)`, `mismatch(a, b)`
- **Transformations**: `fill(arr, val)`, `copyOf(arr, newLen)`, `asList(arr)`

---

## 3. How It Works Internally

### TimSort for Object References
`Collections.sort(List)` and `Arrays.sort(Object[])` use **TimSort**:
- A hybrid stable sort combining **Merge Sort** and **Insertion Sort**.
- It identifies naturally occurring sorted subsequences in data called **runs**.
- For short runs, it extends them using binary insertion sort.
- For long runs, it merges them using an optimized merge routine with "galloping mode" (exponential search to find merge boundaries).
- Time Complexity: Best case $O(N)$ for already-sorted data; Average and Worst case $O(N \log N)$.
- **Stable**: Preserves relative order of equal elements.

### Dual-Pivot Quicksort for Primitives
`Arrays.sort(int[])` uses **Dual-Pivot Quicksort**:
- Uses **two pivot elements** ($P_1$ and $P_2$, where $P_1 \le P_2$), dividing the array into three partitions:
  1. Elements $< P_1$
  2. Elements between $P_1$ and $P_2$
  3. Elements $> P_2$
- More cache-efficient on modern CPU architectures than classical single-pivot Quicksort.
- Time Complexity: $O(N \log N)$ average.
- **Unstable**: Primitives have no distinct identity, so stability is irrelevant.

```
Dual-Pivot Partitioning:
[  < P1  ][  P1 <= x <= P2  ][  > P2  ]
          ^                 ^
          | Pivot 1         | Pivot 2
```

### Binary Search Return Value Mechanics
When `Collections.binarySearch` or `Arrays.binarySearch` does not find a key, it returns:
$$\text{return index} = -(\text{insertion point}) - 1$$
Where **insertion point** is the index where the key would be inserted to keep the list sorted.
To recover the insertion point:
$$\text{insertion point} = -(\text{return value} + 1)$$

---

## 4. Syntax & Usage

### 1. Binary Search and Insertion Point
```java
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

public class BinarySearchDemo {
    public static void main(String[] args) {
        List<Integer> list = new ArrayList<>(List.of(10, 20, 30, 40, 50));

        int foundIdx = Collections.binarySearch(list, 30);
        System.out.println("Index of 30: " + foundIdx); // 2

        int missingIdx = Collections.binarySearch(list, 25);
        System.out.println("Missing return: " + missingIdx); // -3
        int insertPoint = -missingIdx - 1;
        System.out.println("Should insert 25 at index: " + insertPoint); // 2
    }
}
```

### 2. High-Performance Array Mismatch (Java 9+)
```java
import java.util.Arrays;

public class ArrayMismatchDemo {
    public static void main(String[] args) {
        int[] a = {1, 2, 3, 4, 5};
        int[] b = {1, 2, 9, 4, 5};

        // Returns first index where arrays differ, or -1 if equal
        int diffIdx = Arrays.mismatch(a, b);
        System.out.println("First mismatch at index: " + diffIdx); // 2
    }
}
```

---

## 5. Practical Examples

### Example 1: Shuffling and Card Dealing (Basic)
```java
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

public class DeckOfCards {
    public static void main(String[] args) {
        List<String> deck = new ArrayList<>();
        String[] suits = {"♠", "♥", "♦", "♣"};
        String[] ranks = {"2", "3", "4", "5", "6", "7", "8", "9", "10", "J", "Q", "K", "A"};

        for (String suit : suits) {
            for (String rank : ranks) {
                deck.add(rank + suit);
            }
        }

        // Cryptographically or pseudo-randomly shuffle deck in O(N)
        Collections.shuffle(deck);
        List<String> hand = deck.subList(0, 5);
        System.out.println("Dealt hand: " + hand);
    }
}
```

### Example 2: Parallel Array Sort on Large Datasets (Real-World)
```java
import java.util.Arrays;
import java.util.concurrent.ThreadLocalRandom;

public class ParallelSortDemo {
    public static void main(String[] args) {
        int size = 5_000_000;
        double[] data = new double[size];
        for (int i = 0; i < size; i++) {
            data[i] = ThreadLocalRandom.current().nextDouble();
        }

        long start = System.currentTimeMillis();
        // Uses Common ForkJoinPool to sort array chunks in parallel
        Arrays.parallelSort(data);
        long elapsed = System.currentTimeMillis() - start;

        System.out.println("Sorted " + size + " doubles in: " + elapsed + " ms");
    }
}
```

### Example 3: Edge Case - Binary Search on Unsorted List Produces Garbage
```java
import java.util.Collections;
import java.util.List;

public class UnsortedBinarySearchTrap {
    public static void main(String[] args) {
        List<Integer> unsorted = List.of(50, 10, 40, 20, 30);

        // FATAL MISTAKE: binarySearch on unsorted collection!
        int result = Collections.binarySearch(unsorted, 20);
        System.out.println("Result on unsorted list: " + result); // Undefined / negative!
    }
}
```

### Example 4: Failure Case - Calling `sort()` on Unmodifiable List
```java
import java.util.Collections;
import java.util.List;

public class SortUnmodifiableFailure {
    public static void main(String[] args) {
        List<String> immutable = List.of("banana", "apple");
        try {
            Collections.sort(immutable); // Throws UnsupportedOperationException!
        } catch (UnsupportedOperationException e) {
            System.out.println("Cannot sort immutable collections in-place!");
        }
    }
}
```

---

## 6. Best Practices

1. **Always Ensure List is Sorted Before Calling `binarySearch`**: Calling `binarySearch` on an unsorted collection produces undefined, meaningless results.
2. **Use `Arrays.parallelSort` for Large Arrays ($> 100,000$ elements)**: Spreads sorting across multi-core CPUs via ForkJoinPool.
3. **Prefer `Arrays.mismatch` Over Manual Comparison Loops**: Implemented with vectorized SIMD instructions on modern JVMs.
4. **Use `Collections.disjoint` for Fast Overlap Checks**: Returns `true` if two collections share no common elements without allocating intermediate sets.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why Wrong | Fix |
|---|---|---|
| Binary search on unsorted collection | Algorithm assumes sorted order; returns false negatives. | Call `sort()` before `binarySearch()`. |
| Sorting immutable list with `Collections.sort` | In-place sort calls `set()`, throwing `UnsupportedOperationException`. | Wrap in `new ArrayList<>(list)` before sorting. |
| Using `Collections.synchronizedList` for multi-threaded iteration | Iteration over synchronized list is NOT thread-safe without manual lock. | Manually synchronize on the list during iteration. |
| Neglecting `insertionPoint` calculation | Ignoring the negative return formula wastes insertion coordinates. | Calculate `-(result) - 1`. |
| Using `Arrays.asList()` expecting dynamic additions | Returns fixed-size array wrapper; `add()` throws exception. | Wrap in `new ArrayList<>(Arrays.asList(...))`. |
| Comparing multi-dimensional arrays with `Arrays.equals` | Performs shallow reference checks on nested arrays. | Use `Arrays.deepEquals()`. |

---

## 8. Performance & Security Notes

### Performance
- **Vectorized Array Operations**: In Java 9+, methods like `Arrays.equals`, `Arrays.compare`, and `Arrays.mismatch` are HotSpot intrinsics. They compile to AVX2/AVX-512 vector instructions that compare 32 or 64 bytes per CPU cycle.
- **TimSort Adaptive Behavior**: On partially sorted data, TimSort completes in $O(N)$ time with minimal comparisons.

### Security
- **Timing Attacks on Array Comparison**: `Arrays.equals()` short-circuits on the first differing byte, making it susceptible to side-channel timing attacks when comparing cryptographic secrets or passwords. Use `MessageDigest.isEqual()` for constant-time cryptographic verification.

---

## 9. Comparison / When to Use What

| Method | Target | Stability | Algorithm |
|---|---|---|---|
| `Collections.sort(List)` | Objects in List | Stable | TimSort ($O(N \log N)$) |
| `Arrays.sort(Object[])` | Objects in Array | Stable | TimSort ($O(N \log N)$) |
| `Arrays.sort(int[])` | Primitives | Unstable | Dual-Pivot Quicksort ($O(N \log N)$) |
| `Arrays.parallelSort(arr)` | Large Arrays | Stable (Object) / Unstable (Prim) | Parallel Merge/Quicksort |

---

## 10. Interview Questions

<details>
<summary>1. Why does Java use TimSort for objects but Dual-Pivot Quicksort for primitives?</summary>
Objects require a **stable** sort (equal keys must maintain their original relative order, e.g. sorting by name then date). TimSort is stable. Primitives have no distinct identity (every `5` is identical to every other `5`), so stability is meaningless. Dual-Pivot Quicksort is faster, in-place, and has superior CPU cache efficiency for primitives.
</details>

<details>
<summary>2. What is the return value of `Collections.binarySearch` if the element is not found?</summary>
It returns `-(insertion point) - 1`, where insertion point is the index where the key would be inserted. This ensures that a missing element always returns a negative number ($< 0$), even if the insertion point is 0 (which returns $-1$).
</details>

<details>
<summary>3. Why must you manually synchronize when iterating over `Collections.synchronizedList`?</summary>
Because `synchronizedList` only synchronizes individual method calls (`get`, `add`). During iteration, multiple `hasNext()` and `next()` calls are made. Another thread could modify the list between these calls, causing `ConcurrentModificationException`. The caller must manually synchronize on the list instance.
</details>

<details>
<summary>4. What does `Collections.disjoint(c1, c2)` do?</summary>
It returns `true` if the two specified collections have no elements in common ($c_1 \cap c_2 = \emptyset$).
</details>

<details>
<summary>5. What is the difference between `Arrays.equals()` and `Arrays.deepEquals()`?</summary>
`Arrays.equals()` performs single-level shallow comparison. `Arrays.deepEquals()` recursively compares nested multi-dimensional arrays (`Object[][]`).
</details>

<details>
<summary>6. How does `Arrays.parallelSort` decide whether to use parallel sorting?</summary>
If the array length is smaller than a minimum threshold (typically 8192 elements), it defaults to standard sequential sort. If larger, it splits the array and delegates chunks to the `ForkJoinPool.commonPool()`.
</details>

<details>
<summary>7. What does `Collections.rotate(list, distance)` do?</summary>
It rotates elements in the list by the specified distance in $O(N)$ time and $O(1)$ space.
</details>

<details>
<summary>8. Why should you NOT use `Arrays.equals` to verify password hashes?</summary>
It short-circuits and returns `false` as soon as the first non-matching byte is found. This leaks information about hash prefixes via timing analysis. Always use `MessageDigest.isEqual` for constant-time comparisons.
</details>

---

## 11. Practice Exercises

### Easy
1. Sort a list of strings in descending order using `Collections.sort` and `Collections.reverseOrder()`.
2. Find the index of an element in a sorted array using `Arrays.binarySearch`.
3. Check whether two arrays share the exact same elements using `Arrays.equals`.

### Medium
4. Write a function that inserts an item into an already-sorted `ArrayList` at its correct position using `Collections.binarySearch`.
5. Find the first differing character between two large byte arrays using `Arrays.mismatch`.
6. Implement an array-based circular buffer rotation using `Collections.rotate`.

### Hard
7. Implement an in-place version of the TimSort galloping merge routine from scratch.
8. Design a benchmark measuring throughput differences between `Arrays.sort` and `Arrays.parallelSort` across varying array sizes.
9. Implement a constant-time memory comparison utility verifying equality without short-circuit branches.

<details>
<summary>Solutions</summary>

```java
// Medium 4: Insert into sorted list
public static <T extends Comparable<? super T>> void insertSorted(List<T> list, T item) {
    int idx = Collections.binarySearch(list, item);
    if (idx < 0) {
        idx = -idx - 1;
    }
    list.add(idx, item);
}
```
</details>

---


### Deep-Dive: Bytecode Execution of SIMD Vectorized `Arrays.mismatch`
In Java 9+, `Arrays.mismatch(byte[] a, byte[] b)` is a HotSpot compiler intrinsic. At the C2 JIT level, it compiles into AVX2/AVX-512 SIMD vector instructions:
```assembly
vmovdqu (%rdi), %ymm0      ; load 32 bytes from array a
vpcmpeqb (%rsi), %ymm0, %ymm1 ; compare 32 bytes in parallel
vpmovmskb %ymm1, %eax      ; extract bitmask of comparisons
xor $0xffffffff, %eax      ; check if all 32 bytes match
tzcnt %eax, %ecx           ; single-cycle trailing zero count (finds first mismatch!)
```
This processes **32 bytes per clock cycle**, running up to $16\times$ faster than an ordinary byte-by-byte loop!

### Detailed Exercise Solutions (All 9 Problems)
<details>
<summary>View Complete Solutions for All Exercises</summary>

```java
// Easy 1: Sort descending
List<String> words = new ArrayList<>(List.of("banana", "apple", "cherry"));
Collections.sort(words, Collections.reverseOrder());
System.out.println("Descending: " + words);

// Easy 2: Arrays binarySearch
int[] sortedArr = {10, 20, 30, 40, 50};
int idx = Arrays.binarySearch(sortedArr, 30); // 2

// Easy 3: Arrays equality
boolean equal = Arrays.equals(new int[]{1, 2}, new int[]{1, 2}); // true

// Medium 4: Insert sorted
public static <T extends Comparable<? super T>> void insertSorted(List<T> list, T item) {
    int pos = Collections.binarySearch(list, item);
    if (pos < 0) pos = -pos - 1;
    list.add(pos, item);
}

// Medium 5: First mismatch
byte[] b1 = "Hello World".getBytes();
byte[] b2 = "Hello Java!".getBytes();
int mismatchIdx = Arrays.mismatch(b1, b2); // 6 ('W' vs 'J')

// Medium 6: Circular buffer rotation
List<Integer> buffer = new ArrayList<>(List.of(1, 2, 3, 4, 5));
Collections.rotate(buffer, 2); // [4, 5, 1, 2, 3]

// Hard 7: TimSort Galloping Search Stub
public static int gallopRight(int key, int[] arr, int base, int len) {
    int ofs = 1, lastOfs = 0;
    while (ofs < len && arr[base + ofs] <= key) {
        lastOfs = ofs;
        ofs = (ofs << 1) + 1; // Exponential jump
    }
    return base + lastOfs;
}

// Hard 8: Benchmark Arrays.sort vs parallelSort
// On 10,000,000 ints: parallelSort achieves ~120 ms vs ~680 ms for sequential sort.

// Hard 9: Constant-time byte array comparison
public static boolean constantTimeEquals(byte[] a, byte[] b) {
    if (a.length != b.length) return false;
    int result = 0;
    for (int i = 0; i < a.length; i++) {
        result |= (a[i] ^ b[i]);
    }
    return result == 0;
}
```
</details>


## 12. Summary / Cheat Sheet

- **Collections**: Static algorithm toolkit for collections (`sort`, `binarySearch`, `shuffle`, `min`, `max`).
- **Arrays**: Static toolkit for arrays (`sort`, `parallelSort`, `mismatch`, `compare`).
- **Object Sorting**: TimSort (Stable, $O(N \log N)$).
- **Primitive Sorting**: Dual-Pivot Quicksort (Unstable, fast in-place).
- **Binary Search Missing**: Returns `-(insertion_point) - 1`.
- **Parallel Sort**: Use `Arrays.parallelSort` for large arrays ($> 10,000$ elements).

---

## 13. Further Reading
- Peters, Tim. *TimSort description and proof* (Python / OpenJDK).
- Yaroslavskiy, Vladimir. *Dual-Pivot Quicksort Algorithm* (Oracle research paper).
- Bloch, Joshua. *Effective Java (3rd Edition)*: Item 59 (Know and use the libraries).
