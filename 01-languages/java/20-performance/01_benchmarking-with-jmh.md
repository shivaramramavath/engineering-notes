# Microbenchmarking with JMH: Architecture and Pitfalls

> Measuring Java micro-performance scientifically: JMH Harness, JIT Dead Code Elimination, Constant Folding, Warmup Cycles, and Blackholes.

| Metadata | Details |
|---|---|
| **Level** | Core / Advanced |
| **Prerequisites** | JVM JIT Compilation, Tiered Compilation, CPU Caches |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **JDK Baseline** | Java 17 / 21 / 25 LTS (JMH 1.37+) |

---

## Table of Contents
1. [1. What & Why](#1-what--why)
2. [2. Core Concepts](#2-core-concepts)
3. [3. How It Works Internally: JIT Optimizations & Benchmark Hazards](#3-how-it-works-internally-jit-optimizations--benchmark-hazards)
4. [4. Syntax & Usage](#4-syntax--usage)
5. [5. Practical Examples](#5-practical-examples)
6. [6. Best Practices](#6-best-practices)
7. [7. Common Mistakes & Pitfalls](#7-common-mistakes--pitfalls)
8. [8. Performance & Security Notes](#8-performance--security-notes)
9. [9. Comparison / When to Use What](#9-comparison--when-to-use-what)
10. [10. Interview Questions](#10-interview-questions)
11. [11. Practice Exercises](#11-practice-exercises)
12. [12. Summary / Cheat Sheet](#12-summary--cheat-sheet)
13. [13. Further Reading](#13-further-reading)

---

## 1. What & Why
Writing microbenchmarks in Java using `System.currentTimeMillis()` or `System.nanoTime()` is fundamentally flawed. Modern JVMs (HotSpot) are dynamic optimizing compilers: they perform runtime profiling, speculative devirtualization, method inlining, loop unrolling, and dead code elimination.

A hand-crafted loop measuring a small calculation often ends up measuring nothing at all because the JIT compiler (C2) detects that the return value is unused and completely erases the loop from the compiled machine code! The **Java Microbenchmark Harness (JMH)** was built by the HotSpot compiler team (Aleksey Shipilëv et al.) to provide a rigorous, scientifically valid framework that prevents JIT optimization traps and produces reproducible microsecond and nanosecond measurements.

---

## 2. Core Concepts
- **Warmup Phase**: Initial benchmark iterations executed to allow the HotSpot Tiered Compilation system (C1 and C2 Server compiler) to profile execution, compile hot methods, and reach steady state before metrics are recorded.
- **Dead Code Elimination (DCE)**: A compiler optimization where HotSpot deletes code whose side-effects or return values are never consumed.
- **Constant Folding**: An optimization where the compiler pre-calculates the result of operations on constants at compile-time (e.g., replacing `Math.sin(3.14159)` with a precomputed literal).
- **Blackhole**: A JMH utility class designed to consume computed values in CPU registers, preventing Dead Code Elimination without introducing CPU branch or memory store penalties.
- **State Scope**:
  - `Scope.Benchmark`: A single state instance shared concurrently across all benchmark worker threads.
  - `Scope.Thread`: A separate state instance allocated per thread, eliminating thread contention.
  - `Scope.Group`: State instance shared across threads within a custom benchmark execution group.
- **Benchmark Modes**:
  - `Mode.Throughput`: Measures operations per unit time (e.g., ops/sec).
  - `Mode.AverageTime`: Measures average execution duration per operation (e.g., ns/op).
  - `Mode.SampleTime`: Samples execution duration, computing percentiles (P90, P99, P99.9).
  - `Mode.SingleShotTime`: Measures single cold invocation duration.

---

## 3. How It Works Internally: JIT Optimizations & Benchmark Hazards

Understanding how JMH defeats HotSpot JIT compiler traps:

```
Naive Hand-Written Loop:
long start = System.nanoTime();
for (int i = 0; i < 1_000_000; i++) {
    computeHash(data); // Return value discarded!
}
long elapsed = System.nanoTime() - start;
// C2 Compiler eliminates the loop entirely! Measures 0 ns!

JMH Architecture:
Forked Worker JVM
     |
     v  Executes Warmup Iterations (JIT compiles bytecode to C2 assembly)
Measurement Loop
     |
     +--> Executes @Benchmark method
     |    Passes result to Blackhole.consume()
     |
     v
Blackhole: Writes to volatile or compiler-opaque intrinsic
Result: JIT cannot eliminate computation, but incurs 0 memory barrier overhead!
```

### Why Forking is Mandatory:
JMH runs benchmarks in separate, newly forked JVM child processes by default (`@Fork(1)`). If multiple benchmarks ran in the same JVM, the JIT profiling profile and type feedback (monomorphic vs polymorphic call-site records) from Benchmark A would contaminate Benchmark B, corrupting measurement validity.

---

## 4. Syntax & Usage

### 4.1 Production JMH Benchmark Implementation
```java
package com.example.performance;

import org.openjdk.jmh.annotations.*;
import org.openjdk.jmh.infra.Blackhole;
import java.util.concurrent.TimeUnit;

@BenchmarkMode(Mode.Throughput) // Operations per second
@OutputTimeUnit(TimeUnit.MILLISECONDS)
@State(Scope.Thread) // Independent state per thread
@Warmup(iterations = 3, time = 1, timeUnit = TimeUnit.SECONDS)
@Measurement(iterations = 5, time = 1, timeUnit = TimeUnit.SECONDS)
@Fork(value = 1, jvmArgs = {"-Xms2g", "-Xmx2g", "-XX:+UseParallelGC"})
public class StringConcatenationBenchmark {

    @Param({"10", "100", "1000"})
    public int iterations;

    private String[] words;

    @Setup(Level.Trial)
    public void setup() {
        words = new String[iterations];
        for (int i = 0; i < iterations; i++) {
            words[i] = "Item_" + i;
        }
    }

    @Benchmark
    public void benchmarkStringBuilder(Blackhole bh) {
        StringBuilder sb = new StringBuilder();
        for (String word : words) {
            sb.append(word);
        }
        bh.consume(sb.toString()); // Consumed to defeat Dead Code Elimination!
    }

    @Benchmark
    public void benchmarkStringJoin(Blackhole bh) {
        bh.consume(String.join("", words));
    }
}
```

### 4.2 Executing JMH via Command Line
```bash
# Build JMH benchmark uber-jar via Maven
./mvnw clean package

# Run all benchmarks matching pattern with 4 worker threads
java -jar target/benchmarks.jar StringConcatenationBenchmark -t 4

# Run with profiling flags (perfasm disassembles emitted machine code!)
java -jar target/benchmarks.jar -prof perfasm
```

---

## 5. Practical Examples

### Example 1: Demonstrating and Defeating Dead Code Elimination
```java
package com.example.performance;

import org.openjdk.jmh.annotations.*;
import org.openjdk.jmh.infra.Blackhole;

@State(Scope.Benchmark)
public class DeadCodeDemo {
    private int x = 42;
    private int y = 84;

    // FLAWED BENCHMARK: Return value discarded, JIT erases math completely!
    @Benchmark
    public void flawedBenchmark() {
        int result = x * y + 100;
        // Result is never returned or consumed!
    }

    // ACCURATE BENCHMARK: Returns value directly (JMH implicitly consumes it)
    @Benchmark
    public int accurateReturnBenchmark() {
        return x * y + 100;
    }

    // ACCURATE BENCHMARK: Explicit Blackhole consumption
    @Benchmark
    public void accurateBlackholeBenchmark(Blackhole bh) {
        bh.consume(x * y + 100);
    }
}
```

### Example 2: Defeating Constant Folding with State Variables
```java
package com.example.performance;

import org.openjdk.jmh.annotations.*;

@State(Scope.Thread)
public class ConstantFoldingDemo {

    // FLAWED: Math.log on static constant literal is folded at compile time!
    @Benchmark
    public double flawedConstantMath() {
        return Math.log(1000.0); // Precomputed by C2 compiler to 6.907755...
    }

    // CORRECT: Reading from non-final state field forces runtime computation
    public double input = 1000.0;

    @Benchmark
    public double accurateMath() {
        return Math.log(input); // Must be calculated dynamically
    }
}
```

### Example 3: Edge Case - Measuring Allocation Rates with `-prof gc`
```bash
# Run benchmark while tracking JVM heap allocation rates per operation
java -jar target/benchmarks.jar StringConcatenationBenchmark -prof gc
# Output reveals: bytes allocated per operation and GC frequency!
```

---

## 6. Best Practices
1. **Always Return Values or Consume in Blackhole**: Never allow computation results to be discarded in `@Benchmark` methods.
2. **Never Pass Compile-Time Constants as Inputs**: Store inputs in `@State` fields to prevent constant folding.
3. **Always Run in Forked JVMs (`@Fork(1)`)**: Never disable forking; type feedback and deoptimization state from earlier benchmarks will poison subsequent measurements.
4. **Configure Sufficient Warmup Cycles**: Ensure at least 3 to 5 warmup iterations of 1 second each so Tiered Compilation reaches C2 server tier.
5. **Use `@Param` to Test Scalability Curves**: Test collections and algorithms across small (10), medium (1,000), and large (100,000) inputs.
6. **Benchmark Production Garbage Collectors**: Configure JVM flags in `@Fork(jvmArgs = {...})` matching production (e.g., G1 or Generational ZGC).

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Correction |
|---|---|---|
| Discarding return values in benchmark | JIT detects unused result and erases method code (Dead Code Elimination) | Return value or pass to `Blackhole.consume()` |
| Computing on literal constants | Compiler precomputes result at compile time (Constant Folding) | Read inputs from non-final `@State` fields |
| Disabling forking (`@Fork(0)`) | JVM type profiles and JIT feedback pollute subsequent tests | Always run with `@Fork(1)` or higher |
| Insufficient warmup time | Measures interpreted bytecode and C1 tiers rather than steady state | Configure at least 3-5 warmup iterations |
| Measuring microbenchmarks with `System.nanoTime()` | High timer measurement latency overhead and zero JIT trap prevention | Always use JMH framework |
| Memory allocation in setup measured as benchmark | Distorts allocation and CPU metrics | Perform resource setup in `@Setup(Level.Trial)` |
| Benchmarking with debug assertions enabled (`-ea`)| Incurs extra branch checks not present in production | Run benchmarks without assertion flags |
| Generalizing nanosecond results to macro architecture | Fast micro-ops can be dominated by network latency in real systems | Validate micro-results with macro integration tests |

---

## 8. Performance & Security Notes
- **Blackhole Internals**: Modern JMH Blackholes use compiler-opaque operations or volatile stores. In HotSpot, JMH takes advantage of intrinsic compiler fences so that no actual memory barrier instruction is emitted, yielding zero overhead while preventing dead-code elimination.
- **Hardware Counter Profiling (`perfasm`)**: Running JMH with `-prof perfasm` on Linux outputs the exact annotated x86-64 assembly emitted by the C2 JIT compiler, showing loop unrolling, vectorization (AVX-512), and hardware cache misses.
- **Cache Line Bouncing in Multi-Threaded Benchmarks**: When benchmarking multi-threaded code with `@Threads(N)`, avoid sharing mutable `@State(Scope.Benchmark)` fields without `@Contended` padding to prevent false sharing degradation.

---

## 9. Comparison / When to Use What

| Benchmarking Tool | Granularity | JIT Trap Prevention | Best Use Case |
|---|---|---|---|
| **JMH** | Nanosecond / Microsecond | Complete (Blackhole, Warmup, Forking) | Algorithms, data structures, parsing, serializers |
| **Gatling / k6** | Millisecond | N/A (External load) | Macro HTTP REST endpoints, network throughput |
| **JFR / async-profiler** | Microsecond | Passive observation | Production macro profiling under live traffic |

---

## 10. Interview Questions

<details>
<summary>1. What is Dead Code Elimination (DCE) and how does JMH prevent it?</summary>
Dead Code Elimination is a standard compiler optimization performed by the HotSpot C2 compiler. If the compiler determines that the output of a computation is never read or has no observable side-effects, it completely erases the instructions from the compiled assembly. In a naive benchmark, an algorithm that should take 10 milliseconds can execute in 0 nanoseconds because the JIT deleted the loop.<br>
JMH prevents this using <code>Blackhole.consume()</code>. A Blackhole provides a compiler-opaque sink that tricks the JIT into believing the value is used, forcing the computation to execute fully without adding significant measurement overhead.
</details>

<details>
<summary>2. What is Constant Folding and how do you prevent it in JMH?</summary>
Constant Folding occurs when the compiler evaluates mathematical expressions or pure method calls on compile-time constant literals during compilation (e.g., replacing <code>Math.sqrt(16.0)</code> with <code>4.0</code>). In a benchmark, this measures constant lookup time rather than computation speed. It is prevented by reading inputs from non-final instance fields inside an annotated <code>@State</code> class.
</details>

<details>
<summary>3. Why is JVM forking (@Fork) essential in JMH benchmarks?</summary>
HotSpot uses dynamic profiling and speculative optimization. The first benchmark executed in a JVM shapes the JIT's type feedback: it records whether interfaces have single implementations (monomorphic call sites) or multiple (megamorphic). If subsequent benchmarks run in the same JVM, they inherit this polluted profiling state. Forking creates a brand-new, unpolluted JVM process for each benchmark, guaranteeing isolated, reproducible conditions.
</details>

<details>
<summary>4. What are the four primary BenchmarkModes in JMH?</summary>
1. <b>Throughput</b>: Measures operations per unit time (ops/sec).<br>
2. <b>AverageTime</b>: Measures average time per operation (e.g., ns/op).<br>
3. <b>SampleTime</b>: Samples execution durations to compute distribution percentiles (P90, P99).<br>
4. <b>SingleShotTime</b>: Measures cold startup invocation time (useful for evaluating cold-start latency).
</details>

<details>
<summary>5. What is the difference between Scope.Thread and Scope.Benchmark in @State?</summary>
<code>Scope.Benchmark</code> creates a single shared instance of the state class accessed concurrently by all benchmark threads, requiring thread-safe fields or measuring lock contention.<br>
<code>Scope.Thread</code> instantiates a completely independent state object per benchmark worker thread, eliminating inter-thread memory contention and false sharing.
</details>

<details>
<summary>6. How do you inspect the assembly generated by the JIT compiler during a JMH run?</summary>
Run JMH with the <code>-prof perfasm</code> profiler on Linux (requires <code>hsdis</code> disassembly library in <code>$JAVA_HOME/lib/server/</code>). JMH will output the exact x86-64 machine instructions emitted by HotSpot C2 alongside execution clock cycles.
</details>

<details>
<summary>7. What is the purpose of @Setup(Level.Trial) vs Level.Iteration?</summary>
<code>Level.Trial</code> runs the setup method once before the entire benchmark trial begins.<br>
<code>Level.Iteration</code> runs before each individual measurement iteration.<br>
<code>Level.Invocation</code> runs before every single benchmark method invocation (adds high overhead and should be avoided unless strictly necessary).
</details>

<details>
<summary>8. Why should you avoid System.currentTimeMillis() for benchmarking?</summary>
<code>System.currentTimeMillis()</code> has poor granularity (typically 1–15 milliseconds on Windows/Linux) and is susceptible to operating system clock drift (NTP synchronization jumps). While <code>System.nanoTime()</code> provides higher precision, it still suffers from timer call overhead and cannot prevent JIT optimizations like Dead Code Elimination.
</details>

---

## 11. Practice Exercises

<details>
<summary>Exercise 1: Write a JMH benchmark comparing ArrayList vs LinkedList sequential traversal.</summary>

```java
@BenchmarkMode(Mode.AverageTime)
@OutputTimeUnit(TimeUnit.MICROSECONDS)
@State(Scope.Thread)
public class ListTraversalBenchmark {
    @Param({"1000", "10000"})
    public int size;

    private List<Integer> arrayList;
    private List<Integer> linkedList;

    @Setup(Level.Trial)
    public void setup() {
        arrayList = new ArrayList<>(size);
        linkedList = new LinkedList<>();
        for (int i = 0; i < size; i++) {
            arrayList.add(i);
            linkedList.add(i);
        }
    }

    @Benchmark
    public void iterateArrayList(Blackhole bh) {
        for (Integer val : arrayList) bh.consume(val);
    }

    @Benchmark
    public void iterateLinkedList(Blackhole bh) {
        for (Integer val : linkedList) bh.consume(val);
    }
}
```
</details>

<details>
<summary>Exercise 2: Create a benchmark measuring JSON serialization using Blackhole.</summary>

```java
@State(Scope.Benchmark)
public class JsonSerializationBenchmark {
    private ObjectMapper mapper;
    private UserRecord user;

    @Setup(Level.Trial)
    public void setup() {
        mapper = new ObjectMapper();
        user = new UserRecord(101L, "Alice", "alice@example.com");
    }

    @Benchmark
    public void serializeJson(Blackhole bh) throws Exception {
        bh.consume(mapper.writeValueAsString(user));
    }
}
```
</details>

<details>
<summary>Exercise 3: Write a JMH runner class to execute benchmarks programmatically from a main method.</summary>

```java
public class BenchmarkRunner {
    public static void main(String[] args) throws Exception {
        org.openjdk.jmh.runner.options.Options opt = new org.openjdk.jmh.runner.options.OptionsBuilder()
                .include(StringConcatenationBenchmark.class.getSimpleName())
                .forks(1)
                .warmupIterations(3)
                .measurementIterations(5)
                .build();

        new org.openjdk.jmh.runner.Runner(opt).run();
    }
}
```
</details>

<details>
<summary>Exercise 4: Measure the cost of Java 21 Record instantiation vs traditional Class allocation.</summary>

```java
@BenchmarkMode(Mode.Throughput)
@OutputTimeUnit(TimeUnit.MILLISECONDS)
@State(Scope.Thread)
public class AllocationCostBenchmark {
    public record PointRecord(int x, int y) {}
    public static class PointClass {
        final int x; final int y;
        public PointClass(int x, int y) { this.x = x; this.y = y; }
    }

    @Benchmark
    public PointRecord createRecord() { return new PointRecord(10, 20); }

    @Benchmark
    public PointClass createClass() { return new PointClass(10, 20); }
}
```
</details>

---

## 12. Summary / Cheat Sheet
- **Purpose**: Scientific nanosecond/microsecond measurement; eliminates JIT traps.
- **Dead Code Elimination**: Prevent by returning results or consuming in `Blackhole.consume()`.
- **Constant Folding**: Prevent by reading inputs from non-final `@State` fields.
- **Forking**: Always enable `@Fork(1)` to isolate JIT compilation and type profiles.
- **Warmup**: Configure 3–5 warmup iterations to allow Tiered Compilation to reach C2 server tier.
- **Analysis**: Use `-prof perfasm` to inspect JIT-emitted machine assembly.

---

## 13. Further Reading
- OpenJDK JMH Framework Source and Samples (Aleksey Shipilëv)
- Nanotrusting the Nanotime: Pitfalls of JVM Benchmarking
- Mechanical Sympathy: False Sharing and Cache Line Ping-Pong (Martin Thompson)
- *Java Performance: In-Depth Advice for Tuning and Acceleration* by Scott Oaks
