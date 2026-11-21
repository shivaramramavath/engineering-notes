# Arrays: Memory Layout, Covariance, and Performance in Java

> Arrays represent fixed-size, contiguous heap-allocated memory sequences providing $O(1)$ random element access, serving as the physical foundation for the Java Collections Framework while governed by strict runtime bounds checking and covariance rules.

| Metadata | Value |
| :--- | :--- |
| **Level** | Beginner to Intermediate |
| **Prerequisites** | [06_loops.md](file:///E:/Learning/engineering-notes/01-languages/java/01-fundamentals/06_loops.md) |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **Java LTS Baseline** | Java 25 LTS (HotSpot Array Memory Model) |

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

### Problem Statement
High-throughput computation requires storing homogeneous items consecutively in memory so that any element can be retrieved instantaneously by its numeric offset. While higher-level collections (like `ArrayList`) provide dynamic resizing, they internally wrap a raw array. Understanding raw arrays, their contiguous memory characteristics, and their interaction with the CPU cache hierarchy is fundamental to systems programming.

### Historical Context & Design Principles
Java designed arrays with distinct characteristics compared to C/C++:
- **Arrays are True Objects:** An array is not a loose pointer; it is a full object allocated on the Java heap with a Mark Word, a Klass pointer, and an immutable `.length` field.
- **Strict Bounds Checking:** Every array index read or write is guarded by the JVM to prevent out-of-bounds memory access.
- **Array Covariance:** Java arrays were made covariant (`String[]` is an `Object[]`) in Java 1.0 to enable generic utility methods like `Arrays.sort(Object[])` before Java had generics (Java 5). While convenient, covariance introduced the runtime `ArrayStoreException` trap.

### Real-World Relevance
Low-latency systems, database engines, serialization libraries (Jackson, Protobuf), and cryptographic suites rely directly on primitive arrays (`byte[]`, `int[]`, `long[]`) to eliminate object overhead, optimize garbage collection cycles, and exploit CPU L1/L2 cache prefetching.

---

## 2. Core Concepts

### Declaring and Instantiating Arrays
An array can be instantiated with an explicit size (filled with default values) or with an array literal:

```java
// Instantiation with explicit capacity (defaults to 0 or null)
int[] numbers = new int[5];          // [0, 0, 0, 0, 0]
String[] names = new String[3];      // [null, null, null]

// Array Literal Initializers
int[] primes = { 2, 3, 5, 7, 11 };   // Inferred length = 5
int[] explicit = new int[]{ 1, 2, 3 };// Explicit form (required for re-assignment)
```

### Multi-Dimensional and Jagged Arrays
Java does not support true contiguous multi-dimensional matrices in flat memory. Instead, a multi-dimensional array is an **array of arrays**. This enables **jagged (ragged) arrays**, where each row can have a completely different length:

```
int[][] jagged = new int[3][];
jagged[0] = new int[1]; // Row 0 has 1 element
jagged[1] = new int[2]; // Row 1 has 2 elements
jagged[2] = new int[3]; // Row 2 has 3 elements
```

### Array Covariance & `ArrayStoreException`
In Java, if `Sub` is a subtype of `Super`, then `Sub[]` is considered a subtype of `Super[]`:
```java
String[] strings = new String[2];
Object[] objects = strings; // Legal upcast due to covariance!
objects[0] = Integer.valueOf(42); // Compiles! But throws ArrayStoreException at runtime!
```
Because the underlying heap object is physically a `String[]`, attempting to store an `Integer` forces the JVM to throw `java.lang.ArrayStoreException` to preserve heap type safety.

---

## 3. How It Works Internally

### Memory Layout on the Java Heap
On a 64-bit JVM with Compressed OOPs enabled:

```
+---------------------------------------------------------------------------------------+
|                               Heap Layout of int[4] Array                             |
|                                                                                       |
|   +-------------------+-------------------+-------------------+-------------------+   |
|   | Mark Word (64b)   | Klass Pointer(32b)| Array Length (32b)| Element 0 (32b)   |   |
|   | [0x0000000000001] | [0x0001A8F0: [I ] | [Length: 4 ]      | [Value: 10]       |   |
|   +-------------------+-------------------+-------------------+-------------------+   |
|   | Element 1 (32b)   | Element 2 (32b)   | Element 3 (32b)   | Alignment Padding |   |
|   | [Value: 20]       | [Value: 30]       | [Value: 40]       | [0 bytes pad]     |   |
|   +-------------------+-------------------+-------------------+-------------------+   |
+---------------------------------------------------------------------------------------+
```

1. **Object Header:** Consists of an 8-byte Mark Word, a 4-byte Klass pointer (referencing the special array class `[I`), and a 4-byte array `length` field. Total header = **16 bytes**.
2. **Payload:** The primitive values (`int` = 4 bytes each) are laid out contiguously in memory immediately following the header.
3. **8-Byte Alignment:** Total memory is padded to a multiple of 8 bytes.
- Total memory for `new int[4]` = $16 \text{ (header)} + (4 \times 4) \text{ (payload)} = 32 \text{ bytes}$.

### Bytecode Operations
The JVM specification defines dedicated instructions for arrays:
- `newarray`: Allocates a primitive array.
- `anewarray`: Allocates an array of reference types.
- `multianewarray`: Allocates multi-dimensional arrays.
- `iaload` / `iastore`: Load / Store primitive `int` from array.
- `aaload` / `aastore`: Load / Store reference from array.
- `arraylength`: Reads the immutable 4-byte length field from the array header.

---

## 4. Syntax & Usage

### Standard vs. C-Style Declaration
- **Preferred:** `int[] numbers;` (clearly associates the array type with the type identifier).
- **Discouraged:** `int numbers[];` (retained only for C/C++ syntactic backward compatibility).

### The `java.util.Arrays` Toolkit
The standard library provides optimized static methods:
- `Arrays.toString(arr)`: Formats 1D array as `[1, 2, 3]`.
- `Arrays.deepToString(arr)`: Formats nested multi-dimensional arrays.
- `Arrays.sort(arr)`: Dual-Pivot Quicksort for primitives ($O(N \log N)$); TimSort for objects.
- `Arrays.binarySearch(arr, key)`: $O(\log N)$ search (requires array to be pre-sorted).
- `Arrays.equals(a, b)` / `Arrays.deepEquals(a, b)`: Content-based equality.
- `Arrays.mismatch(a, b)`: Returns the first index where two arrays differ, or `-1` if identical.

---

## 5. Practical Examples

### Example 1: Jagged (Ragged) Multi-Dimensional Arrays
- **Goal:** Dynamically allocate a lower-triangular jagged 2D matrix where each row length equals its 1-based row index.
```java
import java.util.Arrays;

public class JaggedArrayDemo {
    public static void main(String[] args) {
        int[][] triangle = new int[3][];
        for (int i = 0; i < triangle.length; i++) {
            triangle[i] = new int[i + 1];
            Arrays.fill(triangle[i], i + 1);
        }
        System.out.println("Jagged Triangle: " + Arrays.deepToString(triangle));
    }
}
```
- **Console Output:**
```text
Jagged Triangle: [[1], [2, 2], [3, 3, 3]]
```
- **Explanation:** Demonstrates that a 2D array in Java is simply an array of references to distinct 1D array objects on the heap. Each row is independently allocated and garbage-collected.

---

### Example 2: Array Covariance & `ArrayStoreException` Trap
- **Goal:** Demonstrate the runtime type check enforced by the `aastore` bytecode instruction when exploiting array covariance.
```java
public class ArrayCovarianceTrap {
    public static void main(String[] args) {
        String[] stringArray = new String[2];
        Object[] objectArray = stringArray; // Legal covariant reference assignment

        try {
            // DANGER: Compiles cleanly, but fails at runtime!
            objectArray[0] = Integer.valueOf(42);
        } catch (ArrayStoreException e) {
            System.out.println("Caught ArrayStoreException: " + e.getClass().getSimpleName());
        }
    }
}
```
- **Console Output:**
```text
Caught ArrayStoreException: ArrayStoreException
```
- **Explanation:** Even though `objectArray` is statically typed as `Object[]`, the JVM knows the actual object header points to `[Ljava.lang.String;`. The `aastore` instruction checks that the assigned instance is assignable to `String`; finding an `Integer`, it throws `ArrayStoreException`.

---

### Example 3: High-Speed Memory Block Copying via `System.arraycopy()`
- **Goal:** Transfer sub-sequences between arrays using native memory-block copying (`memmove`).
```java
import java.util.Arrays;

public class ArrayCopyDemo {
    public static void main(String[] args) {
        int[] src = { 1, 2, 3, 4, 5 };
        int[] dest = new int[5];

        // System.arraycopy(src, srcPos, dest, destPos, length)
        // Copy 3 elements from src[1..3] into dest[2..4]
        System.arraycopy(src, 1, dest, 2, 3);

        System.out.println("Array after System.arraycopy: " + Arrays.toString(dest));
    }
}
```
- **Console Output:**
```text
Array after System.arraycopy: [0, 0, 2, 3, 4]
```
- **Explanation:** `System.arraycopy()` is an intrinsic method recognized directly by HotSpot C2, which replaces the method call with high-speed vectorized native assembly instructions (`rep movsb` on x86), copying millions of elements in sub-millisecond time.

---

### Example 4: Failure Case — Misinterpreting Multi-Dimensional Cloning
- **Goal:** Demonstrate that `array.clone()` performs a shallow copy, leaving nested arrays shared.
```java
import java.util.Arrays;

public class ShallowCloneTrap {
    public static void main(String[] args) {
        int[][] original = { { 10, 20 }, { 30, 40 } };
        int[][] cloned = original.clone(); // Shallow clone!

        // Mutating cloned inner array:
        cloned[0][0] = 999;

        System.out.println("Original[0][0]: " + original[0][0]); // Also 999!
        System.out.println("Are inner arrays identical? " + (original[0] == cloned[0]));
    }
}
```
- **Console Output:**
```text
Original[0][0]: 999
Are inner arrays identical? true
```
- **Remedy:** To duplicate a multi-dimensional array safely, perform a deep copy by iterating through each row and cloning inner arrays individually.

---

### Example 5: High-Performance / Modern Idiom — Fast Array Mismatch (Vectorized)
- **Goal:** Compare two long arrays to detect corruption or divergence at vectorized speeds using `Arrays.mismatch()` (Java 9+ / 21+ / 25).
```java
import java.util.Arrays;

public class ArrayMismatchDemo {
    public static void main(String[] args) {
        byte[] bufferA = new byte[100_000];
        byte[] bufferB = new byte[100_000];
        Arrays.fill(bufferA, (byte) 0xAA);
        Arrays.fill(bufferB, (byte) 0xAA);

        // Inject difference at index 75,000
        bufferB[75_000] = (byte) 0xBB;

        long start = System.nanoTime();
        int mismatchIndex = Arrays.mismatch(bufferA, bufferB);
        long elapsedNanos = System.nanoTime() - start;

        System.out.println("First mismatch at index: " + mismatchIndex);
        System.out.println("Vectorized comparison elapsed: " + elapsedNanos + " ns");
    }
}
```
- **Console Output:**
```text
First mismatch at index: 75000
Vectorized comparison elapsed: 4800 ns
```
- **Explanation:** `Arrays.mismatch()` leverages the HotSpot vectorized mismatch intrinsic, comparing 64 bytes per CPU cycle using AVX instructions, finding divergence orders of magnitude faster than a manual loop.

---

## 6. Best Practices

1. **Always Use `int[] arr` Syntax:** Keep brackets with the type rather than the variable name to maintain readability and avoid declaration confusion (`int[] a, b;` creates two arrays).
2. **Prefer `System.arraycopy()` or `Arrays.copyOf()`:** Avoid manual `for` loops when copying array segments. Standard library methods are compiled into native vector assembly.
3. **Use `Arrays.toString()` for Debugging:** Never invoke `.toString()` directly on an array (`arr.toString()` prints memory hash like `[I@7a8114c7`). Always use `Arrays.toString(arr)` or `Arrays.deepToString(arr)`.
4. **Guard Against Out-of-Bounds:** Ensure loop upper bounds are strictly `< arr.length`.
5. **Beware of Defensive Copying:** When accepting an array in a constructor or returning an array from a getter in an immutable class, always return a defensive clone (`arr.clone()`). Otherwise, external callers can mutate internal state.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| **Directly Printing an Array (`arr.toString()`)** | Arrays do not override `Object.toString()`, printing type hash like `[I@1b6d3586`. | Use `Arrays.toString(arr)` or `Arrays.deepToString(arr)`. |
| **Assuming `.clone()` Deep Copies 2D Arrays** | Clones only the outer array of references; child arrays remain shared. | Clone each child row individually. |
| **Using `==` to Compare Array Contents** | Compares object heap addresses, not element values. | Use `Arrays.equals(a, b)` or `Arrays.deepEquals(a, b)`. |
| **Calling `.length()` with Parentheses** | `length` is an immutable `final` field on arrays, not a method. | Write `arr.length` without parentheses. |
| **Exploiting Array Covariance Unsafely** | Storing incompatible types into `Object[]` throws runtime `ArrayStoreException`. | Use generic collections (`List<T>`) which enforce compile-time invariance. |
| **Binary Search on Unsorted Array** | `Arrays.binarySearch()` returns unpredictable results if array is unsorted. | Always call `Arrays.sort(arr)` before searching. |

---

## 8. Performance & Security Notes

### Cache Locality & Zero Indirection
Primitive arrays are the most cache-friendly data structures in computer science. Because primitive array elements sit back-to-back in RAM, scanning an array triggers the CPU's hardware prefetcher, filling 64-byte cache lines before instructions even request them. In contrast, an `ArrayList<Integer>` requires dereferencing pointers to scattered heap locations, causing constant CPU cache misses.

### Security Invariants: Array Mutability
Arrays in Java are **always mutable**. Even if an array is declared `public static final int[] NUMBERS = {1, 2, 3};`, external callers can execute `NUMBERS[0] = 999;`! To expose an immutable list of elements, wrap the array using `List.of()` or `Collections.unmodifiableList()`.

---

## 9. Comparison / When to Use What

| Feature / Dimension | Primitive Array (`int[]`) | Reference Array (`String[]`) | `ArrayList<T>` |
| :--- | :--- | :--- | :--- |
| **Resizability** | Fixed at instantiation | Fixed at instantiation | Dynamic (Auto-resizes) |
| **Primitive Support** | Direct (Zero boxing) | No (Stores references) | Requires Boxing (`List<Integer>`) |
| **Memory Footprint** | Minimal (16B header + data) | Moderate (Pointers + objects) | Moderate to High |
| **Type Safety** | Homogeneous | Covariant (`ArrayStoreException`) | Invariant (Compile-time checked) |
| **API Richness** | Basic (`java.util.Arrays`) | Basic (`java.util.Arrays`) | Rich (`Collections`, Streams) |
| **Ideal Scenario** | Low-latency buffers, high-speed math | Low-level IO, CLI `main(args)` | Enterprise business logic, web APIs |

---

## 10. Interview Questions

<details>
<summary><strong>1. Why is array length a field (`arr.length`) while String length is a method (`str.length()`)?</strong></summary>

**Answer:**  
In Java, an array is a built-in virtual machine object whose memory size is fixed permanently at instantiation. The JVM stores the number of elements directly in a dedicated 4-byte header slot; accessing `arr.length` is a direct field read (`arraylength` opcode). Conversely, `String` is a class encapsulating a `byte[]` value array alongside coder encoding flags (LATIN1 vs UTF16). `String.length()` is an instance method that computes the character count based on the internal coder.
</details>

<details>
<summary><strong>2. What is Array Covariance, why was it introduced, and what is its fundamental drawback?</strong></summary>

**Answer:**  
Array covariance means that if `S` is a subtype of `T`, then `S[]` is a subtype of `T[]` (e.g., `String[]` can be assigned to `Object[]`). It was introduced in Java 1.0 to allow generic utility methods—such as `Arrays.sort(Object[])` or `System.arraycopy(Object, ...)`—to operate on any array before Java had Generics (Java 5). Its drawback is type safety: the compiler permits assigning an incompatible object into the array (`objectArray[0] = 123`), forcing the JVM to perform runtime type checks on every reference store (`aastore`), throwing `ArrayStoreException` on mismatch.
</details>

<details>
<summary><strong>3. How does `System.arraycopy()` achieve significantly higher performance than a manual `for` loop?</strong></summary>

**Answer:**  
A manual Java `for` loop executes index incrementation, conditional comparisons, and bounds checks on every iteration. `System.arraycopy()` is a native JNI method that HotSpot C2 treats as an intrinsic. The compiler replaces the call with a single block-memory transfer assembly instruction (such as `memmove` or SIMD vector moves), copying 16, 32, or 64 bytes per clock cycle while handling overlapping memory regions safely.
</details>

<details>
<summary><strong>4. What is the difference between shallow cloning and deep cloning of a multi-dimensional array?</strong></summary>

**Answer:**  
Calling `.clone()` on a 2D array (`int[][] original`) duplicates only the top-level array of references. The cloned outer array points to the exact same child 1D array instances as the original. Mutating `cloned[0][0]` modifies `original[0][0]`. A deep copy requires iterating through each row and cloning each child array individually (`cloned[i] = original[i].clone()`) to allocate independent memory.
</details>

<details>
<summary><strong>5. What is the maximum size of an array in Java, and why?</strong></summary>

**Answer:**  
The maximum theoretical size of an array in Java is `Integer.MAX_VALUE` ($2^{31} - 1 = 2,147,483,647$) because array indexing is defined using 32-bit signed integers. In practice, most JVM implementations reserve a few header words for array metadata; attempting to allocate an array larger than `Integer.MAX_VALUE - 8` typically throws `OutOfMemoryError: Requested array size exceeds VM limit`.
</details>

<details>
<summary><strong>6. How does `Arrays.sort()` choose its sorting algorithm for primitives versus objects?</strong></summary>

**Answer:**  
- For **primitives**, `Arrays.sort()` uses a **Dual-Pivot Quicksort** algorithm ($O(N \log N)$ average). Quicksort is not stable, but stability is irrelevant for primitive values because identical primitive numbers are indistinguishable.
- For **objects**, `Arrays.sort()` uses **TimSort** (an adaptive merge-sort). TimSort guarantees $O(N \log N)$ worst-case performance and is **stable** (preserving the relative order of equal elements), which is essential when sorting objects by multiple criteria.
</details>

<details>
<summary><strong>7. Why are array types named `[I` and `[Ljava.lang.String;` in JVM bytecode?</strong></summary>

**Answer:**  
The JVM uses internal type signatures (JVM descriptors) to identify types:
- `[` indicates an array dimension.
- `I` denotes primitive `int`. Hence, `int[]` is `[I`, and `int[][]` is `[[I`.
- `L<ClassName>;` denotes an object reference. Hence, `String[]` is `[Ljava/lang/String;`.
</details>

<details>
<summary><strong>8. What is the difference between `Arrays.equals()` and `Arrays.deepEquals()`?</strong></summary>

**Answer:**  
- `Arrays.equals(a, b)` performs a 1D comparison: for reference arrays, it invokes `Objects.equals(a[i], b[i])`. If the array contains nested child arrays, it compares the references of those child arrays, returning `false` unless the child arrays are the exact same instance.
- `Arrays.deepEquals(a, b)` recursively inspects nested arrays at all depths, comparing contents element-by-element.
</details>

---

## 11. Practice Exercises

### Exercise 1: In-Place Array Rotation (Easy)
- **Task:** Write a method `void rotateRight(int[] nums, int k)` that shifts array elements to the right by `k` steps in-place.
<details>
<summary>Solution</summary>

```java
import java.util.Arrays;

public class ArrayRotation {
    public static void rotateRight(int[] nums, int k) {
        if (nums == null || nums.length <= 1) return;
        k = k % nums.length;
        reverse(nums, 0, nums.length - 1);
        reverse(nums, 0, k - 1);
        reverse(nums, k, nums.length - 1);
    }

    private static void reverse(int[] arr, int start, int end) {
        while (start < end) {
            int temp = arr[start];
            arr[start++] = arr[end];
            arr[end--] = temp;
        }
    }

    public static void main(String[] args) {
        int[] arr = { 1, 2, 3, 4, 5, 6, 7 };
        rotateRight(arr, 3);
        System.out.println("Rotated: " + Arrays.toString(arr)); // [5, 6, 7, 1, 2, 3, 4]
    }
}
```
</details>

### Exercise 2: Matrix Transposition (Medium)
- **Task:** Implement a function `int[][] transpose(int[][] matrix)` that returns the transposed matrix (columns swapped with rows) for an arbitrary $R \times C$ rectangular matrix.
<details>
<summary>Solution</summary>

```java
import java.util.Arrays;

public class MatrixTranspose {
    public static int[][] transpose(int[][] matrix) {
        int rows = matrix.length;
        int cols = matrix[0].length;
        int[][] result = new int[cols][rows];

        for (int r = 0; r < rows; r++) {
            for (int c = 0; c < cols; c++) {
                result[c][r] = matrix[r][c];
            }
        }
        return result;
    }

    public static void main(String[] args) {
        int[][] original = { { 1, 2, 3 }, { 4, 5, 6 } }; // 2x3
        int[][] transposed = transpose(original);        // 3x2
        System.out.println("Transposed: " + Arrays.deepToString(transposed));
    }
}
```
</details>

### Exercise 3: Dynamic Circular Ring Buffer (Hard)
- **Task:** Implement a high-performance circular ring buffer using a raw primitive `long[]` array, supporting `push(long value)` and `long pop()` operations without allocating objects during execution.
<details>
<summary>Solution</summary>

```java
public class LongRingBuffer {
    private final long[] buffer;
    private final int capacity;
    private int head = 0;
    private int tail = 0;
    private int size = 0;

    public LongRingBuffer(int capacity) {
        this.capacity = capacity;
        this.buffer = new long[capacity];
    }

    public synchronized void push(long value) {
        if (size == capacity) throw new IllegalStateException("Buffer is full");
        buffer[tail] = value;
        tail = (tail + 1) % capacity;
        size++;
    }

    public synchronized long pop() {
        if (size == 0) throw new IllegalStateException("Buffer is empty");
        long value = buffer[head];
        head = (head + 1) % capacity;
        size--;
        return value;
    }

    public synchronized int size() { return size; }

    public static void main(String[] args) {
        LongRingBuffer rb = new LongRingBuffer(3);
        rb.push(100L);
        rb.push(200L);
        rb.push(300L);
        System.out.println("Popped: " + rb.pop()); // 100
        rb.push(400L);
        System.out.println("Popped: " + rb.pop()); // 200
        System.out.println("Popped: " + rb.pop()); // 300
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **True Objects:** Arrays are heap objects with a 16-byte header and contiguous payload.
- **Bounds Checking:** $0 \le \text{index} < \text{arr.length}$; violates throw `ArrayIndexOutOfBoundsException`.
- **Covariance:** `String[]` is an `Object[]`; assigning invalid types throws `ArrayStoreException`.
- **Fast Copying:** Use `System.arraycopy()` or `Arrays.copyOf()` for vectorized memory block transfers.
- **Multi-Dimensional:** Arrays of arrays; supports jagged/ragged dimensions.
- **Deep Formatting:** Use `Arrays.toString()` for 1D arrays; `Arrays.deepToString()` for nested arrays.

---

## 13. Further Reading

- [The Java Language Specification (Java SE 25 Edition) - Chapter 10: Arrays](https://docs.oracle.com/javase/specs/jls/se25/html/jls-10.html)
- [JVM Specification - Array Instructions](https://docs.oracle.com/javase/specs/jvms/se25/html/jvms-6.html)
- [Dual-Pivot Quicksort Algorithm by Vladimir Yaroslavskiy](https://iaroslavski.narod.ru/quicksort/DualPivotQuicksort.pdf)

---

| [← Previous: 06_loops.md](file:///E:/Learning/engineering-notes/01-languages/java/01-fundamentals/06_loops.md) | [Section Index](file:///E:/Learning/engineering-notes/01-languages/java/01-fundamentals/README.md) | [Next: 08_numeric-precision-and-math.md](file:///E:/Learning/engineering-notes/01-languages/java/01-fundamentals/08_numeric-precision-and-math.md) → |
