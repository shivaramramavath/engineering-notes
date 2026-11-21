# Parallel Streams and ForkJoinPool

> Multi-core stream processing: common ForkJoinPool work-stealing, Spliterator partitioning, performance heuristics, and stateful concurrency hazards.

| Metadata | Value |
|---|---|
| **Level** | Advanced |
| **Prerequisites** | [04_streams-fundamentals.md](04_streams-fundamentals.md), [05_stream-operations.md](05_stream-operations.md), [08-collections/12_iterators-and-fail-fast-behavior.md](../08-collections/12_iterators-and-fail-fast-behavior.md) |
| **Time to Read** | 24 minutes |
| **Status** | Active |
| **Java Version** | Java 8+ |

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

On modern multi-core processor architectures, executing computational loops across a single CPU core leaves the vast majority of hardware compute capacity idle. Writing concurrent code manually (managing `Thread`, `ExecutorService`, locks, and thread-safe data structures) is complex, error-prone, and full of race conditions.

**Parallel Streams** allow developers to transform a sequential stream pipeline into a parallel execution pipeline by simply calling `.parallel()`.

However, parallel streams are **not a magic performance switch**. Under many common conditions, parallel streams run **slower** than sequential streams, consume massive CPU resources, saturate shared thread pools, and introduce catastrophic race conditions if stateful or non-thread-safe operations are used.

Understanding the underlying `ForkJoinPool`, work-stealing mechanics, Spliterator decomposition characteristics, and Amdahl's Law is essential for using parallel streams safely.

---

## 2. Core Concepts

### How to Parallelize
Any stream can be parallelized via:
- `collection.parallelStream()`
- `stream.parallel()`
A parallel stream can be converted back to sequential via `.sequential()`. The **last call** in the pipeline dictates the execution mode for the entire pipeline.

### The $N \times Q$ Rule of Thumb
Coined by Brian Goetz in *Java Concurrency in Practice*:
Parallelism pays off only when:
$$N \times Q > 10,000$$
Where:
- $N$ = Number of data elements.
- $Q$ = Cost of computation per element.

If $Q$ is tiny (e.g. adding numbers or checking string length), $N$ must be in the hundreds of thousands or millions before parallelization overcomes thread coordination overhead.

### Spliterator Sizing Quality
How easily a collection splits into balanced chunks dictates parallel speedup:
- **Excellent Splitting ($O(1)$)**: `ArrayList`, primitive arrays (`int[]`), `IntStream.range`. (Size known, direct index splits).
- **Good Splitting ($O(\log N)$)**: `TreeSet`, `TreeMap`.
- **Poor Splitting ($O(N)$)**: `LinkedList`, `Stream.iterate()`, I/O streams (`Files.lines()`). (Size unknown, requires linear traversal to find split point).

---

## 3. How It Works Internally

### The Common ForkJoinPool
By default, all parallel streams in a JVM process share a single global thread pool:
```java
ForkJoinPool.commonPool();
```
- Default parallelism = $\text{Runtime.getRuntime().availableProcessors()} - 1$.
- Because the common pool is shared JVM-wide, blocking operations in a parallel stream (like I/O, database queries, or HTTP requests) will **starve all other parallel streams in the application**!

### Work-Stealing Algorithm
The `ForkJoinPool` operates via **work-stealing**:
1. When a stream is decomposed via `Spliterator.trySplit()`, subtasks are placed in the worker thread's local double-ended queue (deque).
2. A worker thread pushes and pops tasks from the **head** of its own deque (LIFO order).
3. When a worker thread runs out of local tasks, it "steals" tasks from the **tail** of another thread's deque (FIFO order).
4. This keeps all CPU cores uniformly utilized while minimizing thread contention.

```
+-------------------------------------------------------+
| ForkJoinPool Work-Stealing                            |
|                                                       |
| Worker Thread 1: [Task A][Task B][Task C] (Head)      |
|                                                       |
| Worker Thread 2 (Idle): Steals from Thread 1 Tail!    |
|                         [Task A] <--- Steals!         |
+-------------------------------------------------------+
```

---

## 4. Syntax & Usage

### 1. Basic Parallel Execution
```java
import java.util.List;
import java.util.stream.IntStream;

public class ParallelStreamDemo {
    public static void main(String[] args) {
        // Parallel computation of sum of squares
        long sum = IntStream.rangeClosed(1, 10_000_000)
            .parallel()
            .mapToLong(x -> (long) x * x)
            .sum();

        System.out.println("Sum of squares: " + sum);
    }
}
```

### 2. Inspecting Thread Execution
```java
import java.util.List;

public class ThreadInspectionDemo {
    public static void main(String[] args) {
        List.of(1, 2, 3, 4, 5).parallelStream().forEach(n -> {
            System.out.println("Processing " + n + " on thread: " + Thread.currentThread().getName());
        });
    }
}
```

---

## 5. Practical Examples

### Example 1: Parallel Primality Testing (CPU-Intensive) (Basic to Intermediate)
```java
import java.util.List;
import java.util.stream.IntStream;

public class ParallelPrimes {
    public static boolean isPrime(int n) {
        if (n < 2) return false;
        for (int i = 2; i <= Math.sqrt(n); i++) {
            if (n % i == 0) return false;
        }
        return true;
    }

    public static void main(String[] args) {
        long start = System.currentTimeMillis();
        // High Q (expensive primality check per element) -> Ideal for parallel!
        long count = IntStream.rangeClosed(1, 2_000_000)
            .parallel()
            .filter(ParallelPrimes::isPrime)
            .count();
        long elapsed = System.currentTimeMillis() - start;

        System.out.println("Found " + count + " primes in: " + elapsed + " ms");
    }
}
```

### Example 2: Dedicated ForkJoinPool for Isolated Workloads (Real-World)
```java
import java.util.List;
import java.util.concurrent.ForkJoinPool;

public class DedicatedPoolDemo {
    public static void main(String[] args) throws Exception {
        List<String> items = List.of("A", "B", "C", "D");

        // Create an isolated custom ForkJoinPool with 4 threads
        ForkJoinPool customPool = new ForkJoinPool(4);
        try {
            customPool.submit(() ->
                items.parallelStream().forEach(item -> {
                    System.out.println(item + " on " + Thread.currentThread().getName());
                })
            ).get(); // Await completion
        } finally {
            customPool.shutdown();
        }
    }
}
```

### Example 3: Edge Case - Race Conditions via Shared Mutable State
```java
import java.util.ArrayList;
import java.util.List;
import java.util.stream.IntStream;

public class ParallelRaceConditionHazard {
    public static void main(String[] args) {
        // FATAL ANTI-PATTERN: Mutating non-thread-safe collection in parallel stream!
        List<Integer> unsafeList = new ArrayList<>();

        IntStream.range(0, 10_000)
            .parallel()
            .forEach(unsafeList::add); // DATA RACE! Array corruption / index out of bounds!

        System.out.println("Expected: 10000, Actual: " + unsafeList.size());
        // Prints less than 10000, or crashes with ArrayIndexOutOfBoundsException!
    }
}
```

### Example 4: Failure Case - Parallelizing `Stream.iterate()`
```java
import java.util.stream.Stream;

public class IterateParallelDisaster {
    public static void main(String[] args) {
        // Stream.iterate has poor spliterator characteristics (unknown size, linear scan).
        // Parallelizing it causes massive thread coordination overhead, running SLOWER than sequential!
        long sum = Stream.iterate(1, n -> n + 1)
            .limit(10_000_000)
            .parallel() // SLOW!
            .mapToLong(Long::valueOf)
            .sum();
    }
}
```

---

## 6. Best Practices

1. **Only Parallelize CPU-Intensive Workloads with High $N \times Q$**: If computation per element is trivial and element count is small, sequential loops are faster.
2. **Never Perform Blocking I/O in the Common Pool**: Shared `ForkJoinPool.commonPool()` threads will be blocked, starving the entire JVM. Use Virtual Threads or dedicated executors for I/O.
3. **Ensure Operations are Truly Stateless, Associative, and Non-Interfering**: Reductions must obey mathematical associativity: `(a + b) + c == a + (b + c)`.
4. **Prefer Array-Backed Sources**: Arrays, `ArrayList`, and `IntStream.range` split cleanly in $O(1)$ time without traversal overhead.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why Wrong | Fix |
|---|---|---|
| Mutating external collection in `forEach()` | Non-thread-safe mutation causes data races, lost updates, and corrupted data. | Use `.collect(Collectors.toList())`. |
| Running blocking I/O on parallel streams | Starves the shared `ForkJoinPool.commonPool()`, freezing unrelated application tasks. | Use dedicated thread pools or Virtual Threads. |
| Parallelizing `LinkedList` | Cannot split evenly without $O(N)$ linear pointer traversal. | Use `ArrayList` for parallel streams. |
| Using non-associative accumulator in `reduce()` | E.g. `(a, b) -> a - b` yields completely wrong results in parallel. | Ensure accumulator is associative (`+`, `*`, `min`, `max`). |
| Parallelizing tiny collections ($< 10,000$ items) | Thread dispatch and context-switching overhead exceeds compute time. | Keep sequential. |
| Assuming `forEach` preserves order | In parallel streams, `forEach` executes out of order. | Use `forEachOrdered()` if order is critical. |

---

## 8. Performance & Security Notes

### Performance
- **Amdahl's Law Limitation**: Speedup is strictly bounded by the sequential portion of the pipeline:
  $$\text{Speedup} = \frac{1}{(1 - P) + \frac{P}{S}}$$
  If 10% of your code is sequential, maximum theoretical speedup on an infinite number of CPU cores is $10\times$.
- **Thread Context Switching Overhead**: Dividing work across threads introduces cache bouncing, memory bus traffic, and scheduler context switches.

### Security
- **Thread Local Leakage**: Parallel stream worker threads in `ForkJoinPool.commonPool()` are long-lived worker threads. Storing security contexts or credentials in `ThreadLocal` will cause cross-tenant credential leaks across stream tasks.

---

## 9. Comparison / When to Use What

| Criteria | Sequential Stream | Parallel Stream | Virtual Threads (Java 21+) |
|---|---|---|---|
| **Best Workload** | Small data, low compute, I/O | CPU-intensive, high $N \times Q$ | High-throughput blocking I/O |
| **Thread Overhead** | Zero (Current thread) | Work-stealing ForkJoinPool | Million lightweight threads |
| **Spliterator Importance** | Low | Critical ($O(1)$ splits needed) | Low |
| **Order Preservation** | Natural | High overhead (`forEachOrdered`) | N/A |

---

## 10. Interview Questions

<details>
<summary>1. Which thread pool executes parallel streams by default?</summary>
The global `ForkJoinPool.commonPool()`. Its parallelism level is typically equal to the number of available logical CPU cores minus one.
</details>

<details>
<summary>2. Why does `LinkedList` perform poorly with parallel streams?</summary>
Because `LinkedList` does not support $O(1)$ random access. To split a linked list into two balanced halves (`trySplit()`), it must traverse $N/2$ node pointers in $O(N)$ time, destroying parallel speedup.
</details>

<details>
<summary>3. What is the $N \times Q$ rule?</summary>
A heuristic proposed by Brian Goetz: parallel streams provide performance benefits only when the product of the number of elements ($N$) and the computational cost per element ($Q$) is greater than 10,000.
</details>

<details>
<summary>4. What happens if you call `parallel()` and `sequential()` in the same stream pipeline?</summary>
The entire pipeline executes using the mode specified by the **last call** in the pipeline. Streams do not support executing half the pipeline sequentially and half in parallel.
</details>

<details>
<summary>5. Why should you never execute database queries or HTTP calls inside a parallel stream?</summary>
Because parallel streams use the common `ForkJoinPool`. Blocking I/O calls will occupy the limited worker threads, starving all other concurrent parallel operations across the entire application.
</details>

<details>
<summary>6. What mathematical property must the accumulator function in `reduce()` satisfy for parallel correctness?</summary>
It must be **associative**: `(a op b) op c == a op (b op c)`. Non-associative operations (like subtraction or division) yield incorrect, non-deterministic results when evaluated across parallel subtrees.
</details>

<details>
<summary>7. What is the difference between `forEach()` and `forEachOrdered()` on a parallel stream?</summary>
`forEach()` executes elements out of order as soon as worker threads finish them, providing maximum parallelism. `forEachOrdered()` forces downstream output to obey the stream's original encounter order, adding synchronization overhead.
</details>

<details>
<summary>8. How can you run a parallel stream on a dedicated custom thread pool?</summary>
By submitting the parallel stream task inside a custom `ForkJoinPool`:
`customPool.submit(() -> stream.parallel().forEach(...)).get();`.
</details>

---

## 11. Practice Exercises

### Easy
1. Convert a sequential stream summing 1,000,000 numbers to a parallel stream and compare execution time.
2. Demonstrate that `forEach` executes out of order in a parallel stream.
3. Check the default parallelism level of `ForkJoinPool.commonPool()`.

### Medium
4. Benchmark parallel vs sequential stream performance on calculating MD5 hashes of 500,000 strings.
5. Safely collect elements from a parallel stream into a list without using `forEach` or external mutation.
6. Verify whether `reduce()` with subtraction produces non-deterministic results in parallel.

### Hard
7. Implement a custom `Spliterator<T>` for an in-memory binary matrix that partitions cleanly into balanced quadrants.
8. Design a rate-limited parallel processor that limits concurrent processing to 10 active tasks using a custom pool.
9. Measure CPU cache coherence misses (false sharing) during parallel stream array updates using JMH and Linux `perf`.

<details>
<summary>Solutions</summary>

```java
// Easy 3: Parallelism level
int parallelism = ForkJoinPool.commonPool().getParallelism();
System.out.println("Common pool parallelism: " + parallelism);

// Medium 5: Safe parallel collection
List<Integer> safeList = IntStream.range(0, 100_000).parallel().boxed().toList();
```
</details>

---


### Deep-Dive: Bytecode Execution of Parallel Streams
When `.parallel()` is called on a stream, it simply sets an internal bitmask flag on the pipeline head:
```java
public final S parallel() {
    sourceStage.parallel = true;
    return (S) this;
}
```
When the terminal operation executes, `AbstractPipeline.evaluate()` branches:
```java
final <R> R evaluate(TerminalOp<E_OUT, R> terminalOp) {
    if (linkedOrConsumed) throw new IllegalStateException(MSG_STREAM_LINKED);
    linkedOrConsumed = true;
    return isParallel()
        ? terminalOp.evaluateParallel(this, sourceSpliterator(terminalOp.getOpFlags()))
        : terminalOp.evaluateSequential(this, sourceSpliterator(terminalOp.getOpFlags()));
}
```
If parallel, it instantiates a `ForkJoinTask` subtask tree using `java.util.stream.ForEachOps$ForEachTask` or `ReduceTask`.

### Detailed Exercise Solutions (All 9 Problems)
<details>
<summary>View Complete Solutions for All Exercises</summary>

```java
// Easy 1: Sum comparison
long sumSeq = LongStream.rangeClosed(1, 1_000_000).sum();
long sumPar = LongStream.rangeClosed(1, 1_000_000).parallel().sum();

// Easy 2: Unordered forEach
List.of(1, 2, 3, 4, 5).parallelStream().forEach(System.out::print);
System.out.println();

// Easy 3: Inspect pool parallelism
int p = ForkJoinPool.commonPool().getParallelism();

// Medium 4: MD5 Parallel hashing
public static byte[] md5(String s) {
    try { return MessageDigest.getInstance("MD5").digest(s.getBytes()); }
    catch (Exception e) { throw new RuntimeException(e); }
}
List<String> list = List.of("data1", "data2", "data3");
List<byte[]> hashes = list.parallelStream().map(ParallelPrimes::md5).toList();

// Medium 5: Safe parallel collection
List<Integer> safe = IntStream.range(0, 100_000).parallel().boxed().toList();

// Medium 6: Non-associative reduction trap
int nonAssociative = IntStream.range(1, 10).parallel().reduce(0, (a, b) -> a - b);
// Returns non-deterministic, incorrect result!

// Hard 7: Matrix Quadrant Spliterator (Stub)
// Partitions 2D array by rows and columns into 4 sub-spliterators.

// Hard 8: Rate-limited parallel processor
public class RateLimitedProcessor {
    private final Semaphore sem = new Semaphore(10);
    public <T> void process(List<T> items, Consumer<T> consumer) {
        items.parallelStream().forEach(item -> {
            try {
                sem.acquire();
                consumer.accept(item);
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
            } finally {
                sem.release();
            }
        });
    }
}

// Hard 9: False sharing measurement
// Cache line padding (Contended) prevents cache coherence bouncing across parallel threads.
```
</details>

## 12. Summary / Cheat Sheet

- **Parallelize**: `stream.parallel()` or `coll.parallelStream()`.
- **Thread Pool**: Uses `ForkJoinPool.commonPool()` by default.
- **Heuristic**: $N \times Q > 10,000$.
- **Source Quality**: `ArrayList`, `int[]` = Excellent; `LinkedList`, `iterate()` = Terrible.
- **Cardinal Rule**: Never mutate shared state; never execute blocking I/O in parallel streams.
- **Reduction**: Accumulator MUST be associative.

---

## 13. Further Reading
- Goetz, Brian. *Java Concurrency in Practice*: Chapter 7 (Fork/Join Framework).
- Lea, Doug. *A Java Fork/Join Framework* (Original design specification).
- Bloch, Joshua. *Effective Java (3rd Edition)*: Item 48 (Use caution when making streams parallel).
