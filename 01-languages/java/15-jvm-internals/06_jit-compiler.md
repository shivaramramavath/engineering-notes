# JIT Compiler and HotSpot Optimization

> Deep dive into the HotSpot Just-In-Time (JIT) Compilers: Tiered Compilation (Tiers 0-4), C1 (Client) vs C2 (Server), On-Stack Replacement (OSR), method inlining, Escape Analysis (Scalar Replacement, Lock Coarsening), deoptimization, and Graal JIT.

| Metadata | Value |
|---|---|
| **Level** | Advanced |
| **Prerequisites** | [00_jvm-architecture.md](00_jvm-architecture.md), [03_bytecode.md](03_bytecode.md) |
| **Time to Read** | 28 minutes |
| **Status** | Active |
| **Java Version** | Java 1.0 to Java 25 LTS (HotSpot C1/C2 Engine) |

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

### The Dual Needs: Fast Startup and Peak Speed
Interpreting bytecode is portable and provides instant startup, but executing instruction-by-instruction is 10 to 20 times slower than native machine code.
Conversely, statically compiling code ahead-of-time (AOT) cannot utilize runtime profiling metrics (such as actual branch directions, concrete class implementations, and loop trip counts).

### The Solution: Adaptive Dynamic JIT Compilation
The **HotSpot JIT Compiler** marries both worlds:
1. It launches instantly using the Bytecode Interpreter.
2. It profiles running code to discover "hot spots" (frequently called methods and tight loops).
3. It dynamically compiles hot spots into **optimized native CPU assembly instructions** stored directly in the CodeCache.
4. Because compilation occurs while the application is running, the JIT compiler can perform aggressive **speculative optimizations** that static compilers cannot match.

---

## 2. Core Concepts

### Tiered Compilation Architecture (Tiers 0 to 4)
Standardized in Java 7 and default since Java 8:
- **Tier 0 (Interpreter)**: Executes bytecode immediately; zero warmup latency.
- **Tier 1 (Simple C1)**: Compiles without profiling; fast native code.
- **Tier 2 (Limited C1)**: Compiles with basic invocation and loop counters.
- **Tier 3 (Full C1 Profiling)**: Injects detailed profiling hooks (branch probabilities, type feedback).
- **Tier 4 (C2 Server Compiler)**: Uses Tier 3 profiling data to perform aggressive global optimizations, vectorization, and inlining.

### On-Stack Replacement (OSR)
If a method contains a massive loop executing 1,000,000 times, the method might never exit while cold.
**On-Stack Replacement (OSR)** allows the JVM to compile the hot loop into native machine code while it is still running, replacing the interpreted stack frame with a native compiled stack frame **on the fly**!

### Key JIT Optimizations
1. **Method Inlining**: The mother of all optimizations. Replaces method invocation call instructions with the actual body of the called method, eliminating call overhead and unlocking further dead-code elimination.
2. **Escape Analysis**: Determines if an object reference escapes the allocating method:
   - **Scalar Replacement**: If an object does not escape, the JVM dismantles the object and stores its fields directly in CPU registers or stack slots, bypassing heap allocation and GC entirely!
   - **Lock Elimination**: If an object is confined to a single thread, `synchronized` locks on that object are completely stripped away.
3. **Loop Unrolling & Vectorization**: Unrolls loops to reduce branch checks and converts scalar loops into SIMD (Single Instruction Multiple Data) vector instructions (AVX-512, NEON).
4. **Deoptimization**: If a speculative assumption is violated (e.g., a new class is loaded invalidating a monomorphic call site), the JVM safely drops native code and falls back to the Interpreter.

---

## 3. How It Works Internally

### The HotSpot Compilation Pipeline
```
Bytecode (Input)
  │
  ▼
[ Tier 0: Interpreter ] ──> Gathers Invocation & Backedge Counters
  │
  ├─(Exceeds C1 threshold)─> [ Tier 3: C1 Client Compiler ] ──> Generates Type Profiles
  │                                                                     │
  └─(Exceeds C2 threshold)─> [ Tier 4: C2 Server Compiler ] <───────────┘
                                       │
                              Escape Analysis
                              Method Inlining
                              Scalar Replacement
                              Dead-Code Elimination
                                       │
                                       ▼
                              Optimized Native Assembly
                              (Stored in CodeCache)
```

### Speculative Devirtualization and Uncommon Traps
When code invokes an interface method (`shape.draw()`), the compiler checks profiling data:
- If 99.9% of calls are to `Circle`, C2 speculatively inlines `Circle.draw()` guarded by a quick class check:
  ```java
  if (shape.getClass() == Circle.class) {
      // Inlined Circle assembly
  } else {
      uncommon_trap(); // Deoptimize and return to Interpreter!
  }
  ```
- If a `Square` is later passed, the uncommon trap triggers, discarding the compiled code and recompiling with bimorphic dispatch.

---

## 4. Syntax & Usage

### Inspecting JIT Compilation Flags
```bash
# Print compilation events in real-time
java -XX:+PrintCompilation MyApp

# Print detailed inlining decisions (Diagnostic flag required)
java -XX:+UnlockDiagnosticVMOptions -XX:+PrintInlining MyApp

# Trace deoptimization events
java -XX:+PrintDeoptimization MyApp
```

### Interpreting `-XX:+PrintCompilation` Output
```text
  73   28       3       java.lang.String::charAt (25 bytes)
  75   29       4       java.lang.String::hashCode (60 bytes)
  82   30   %   4       MyBenchmark::runLoop @ 4 (120 bytes)
```
- Column 1: Milliseconds since JVM startup (73 ms).
- Column 2: Compilation ID (28).
- Column 3: Flag (`%` means OSR compilation; `!` has exception handlers; `s` is synchronized).
- Column 4: Compilation Tier (3 = C1 Full Profiling; 4 = C2 Server Compiler).
- Column 5: Method name and bytecode size.

---

## 5. Practical Examples

### Example 1: Demonstrating Scalar Replacement via Escape Analysis
```java
// Goal: Prove that non-escaping objects are allocated without heap allocations
public class EscapeAnalysisDemo {
    record Coordinate(int x, int y) {}

    public static int computeSum() {
        // Coordinate NEVER escapes computeSum!
        // HotSpot C2 dismantles Coordinate into two register primitives 'int x' and 'int y'!
        // ZERO heap objects are allocated!
        Coordinate c = new Coordinate(10, 20);
        return c.x() + c.y();
    }

    public static void main(String[] args) {
        long total = 0;
        // Warmup loop to trigger Tier 4 C2 optimization
        for (int i = 0; i < 10_000_000; i++) {
            total += computeSum();
        }
        System.out.println("Finished 10M calculations. Total: " + total);
    }
}
```
*Run with `-XX:+DoEscapeAnalysis` (default) vs `-XX:-DoEscapeAnalysis` to observe massive GC allocation differences.*

### Example 2: Method Inlining Limits (`-XX:MaxInlineSize=35`)
```java
public class InliningDemo {
    // Bytecode size <= 35 bytes: Aggressively inlined by C2 into caller loops!
    public static int smallHelper(int a, int b) {
        return (a * 3) + (b * 7);
    }

    // Huge method > 325 bytes: Inlining rejected, forcing full call overhead!
    public static int hugeHelper(int a) {
        // Long sprawling switch statement...
        return a;
    }
}
```

---

## 6. Best Practices
1. **Keep Frequently Called Methods Small**: Keep hot utility methods under 35 bytecode bytes so the C2 compiler can inline them unconditionally.
2. **Write Monomorphic Code**: Strive for call sites that dispatch to a single concrete class implementation. Monomorphic calls are inlined directly into machine code.
3. **Avoid Microbenchmarking Without JMH**: Hand-rolled loops suffer from dead-code elimination, JIT warmup bias, and OSR artifacts. Always use **JMH (Java Microbenchmark Harness)**.
4. **Do Not Prematurely Optimize with Manual Inlining**: Write clean, modular methods; let the JIT compiler inline them automatically based on runtime profile data.
5. **Beware of Reflection and Megamorphic Call Sites**: Calling methods through 3+ interface implementations (megamorphic) disables inlining and degrades to slow vtable/itable dispatch.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why Wrong | Fix |
|---|---|---|
| Hand-rolled benchmarks with `System.currentTimeMillis` | JIT detects unused results and eliminates the entire loop as dead code. | Use JMH with `Blackhole.consume()`. |
| Monolithic 500-line methods | Exceeds `-XX:MaxInlineSize` and compilation limits, disabling inlining. | Break into small private helper methods. |
| Megamorphic call site degradation | Dispatching to 3+ subtypes prevents devirtualization inlining. | Maintain bimorphic or monomorphic interfaces. |
| Disabling Escape Analysis | Forces millions of ephemeral objects onto the heap, triggering massive GC churn. | Keep `-XX:+DoEscapeAnalysis` enabled. |

---

## 8. Performance & Security Notes

### Constant Folding and Loop Invariant Code Motion (LICM)
HotSpot C2 hoists loop-invariant calculations outside loops and pre-calculates compile-time constants. If a calculation depends only on immutable inputs, C2 replaces the entire calculation with its pre-computed constant literal in assembly.

---

## 9. Comparison / When to Use What

| Feature | Interpreter | C1 Compiler (Client) | C2 Compiler (Server) |
|---|---|---|---|
| **Compilation Speed** | Instant (Zero) | Very Fast | Slower (Complex graph optimization)|
| **Optimization Level**| None | Moderate | **Maximum (Peak Performance)** |
| **Warmup Latency** | None | Low | High (Requires profiling warmup) |
| **Tier Number** | Tier 0 | Tiers 1, 2, 3 | Tier 4 |

---

## 10. Interview Questions

<details>
<summary>1. What is the JIT compiler in the HotSpot JVM?</summary>
The Just-In-Time compiler is a runtime component that identifies frequently executed bytecode (hot spots) and compiles it into optimized native machine code stored in the CodeCache.
</details>

<details>
<summary>2. What is Tiered Compilation?</summary>
A multi-tier execution strategy introduced in Java 7 combining the Interpreter (Tier 0), the C1 Client compiler (Tiers 1–3 for fast startup and profiling), and the C2 Server compiler (Tier 4 for peak optimization).
</details>

<details>
<summary>3. What is Escape Analysis and what optimizations does it enable?</summary>
An analysis technique that determines if an object's reference can escape outside the allocating method or thread. It enables Scalar Replacement (allocating fields in registers instead of heap) and Lock Elimination.
</details>

<details>
<summary>4. What is Method Inlining and why is it considered the most important optimization?</summary>
The process of replacing a method call with the method's actual code body. It eliminates call stack overhead and enables downstream optimizations like constant folding and dead-code elimination.
</details>

<details>
<summary>5. What is On-Stack Replacement (OSR)?</summary>
A mechanism that compiles a long-running interpreted loop into native code and swaps the active stack frame to the compiled version while the loop is still executing.
</details>

<details>
<summary>6. What is Deoptimization?</summary>
The process where the JVM discards speculative native code and falls back to interpreted bytecode when a speculative optimization assumption is invalidated (via an uncommon trap).
</details>

<details>
<summary>7. What are Monomorphic, Bimorphic, and Megamorphic call sites?</summary>
Monomorphic call sites see only 1 concrete class (inlinable). Bimorphic call sites see 2 classes (inlinable via if-else guard). Megamorphic call sites see 3+ classes (cannot be inlined; requires dynamic table lookup).
</details>

<details>
<summary>8. Why are hand-written microbenchmarks misleading without JMH?</summary>
Because the JIT compiler aggressively eliminates dead code, folds constants, and optimizes loops in ways that do not reflect production realities. JMH ensures blackhole consumption and proper warmup cycles.
</details>

---

## 11. Practice Exercises

### Exercise 1 (Easy): Observing JIT Compilation in Real-Time
- **Task**: Run a computation loop with `-XX:+PrintCompilation` and identify when the method transitions to Tier 4 compilation.
<details>
<summary>Solution</summary>

```bash
# Execute with compilation logging
java -XX:+PrintCompilation -cp . EscapeAnalysisDemo
# Observe terminal output showing tier progression: Tier 3 -> Tier 4
```
</details>

### Exercise 2 (Medium): Dead-Code Elimination Simulation
- **Task**: Demonstrate how the JIT compiler eliminates code whose result is never read, contrasting execution time with and without variable usage.
<details>
<summary>Solution</summary>

```java
public class DeadCodeDemo {
    public static void main(String[] args) {
        long start = System.currentTimeMillis();
        // The JIT compiler detects that 'result' is never used and eliminates the entire loop!
        for (int i = 0; i < 50_000_000; i++) {
            int result = i * 2;
        }
        System.out.println("Loop finished in: " + (System.currentTimeMillis() - start) + " ms");
    }
}
```
</details>

### Exercise 3 (Hard): JMH Microbenchmark Structure
- **Task**: Write the canonical skeleton of a Java Microbenchmark Harness (JMH) benchmark using `@Benchmark`, `@BenchmarkMode(Mode.AverageTime)`, and `Blackhole`.
<details>
<summary>Solution</summary>

```java
/*
// Requires org.openjdk.jmh dependencies
import org.openjdk.jmh.annotations.*;
import org.openjdk.jmh.infra.Blackhole;
import java.util.concurrent.TimeUnit;

@BenchmarkMode(Mode.AverageTime)
@OutputTimeUnit(TimeUnit.NANOSECONDS)
@State(Scope.Thread)
@Warmup(iterations = 3, time = 1)
@Measurement(iterations = 5, time = 1)
public class BenchmarkSkeleton {
    private int a = 10;
    private int b = 20;

    @Benchmark
    public void testAddition(Blackhole bh) {
        int sum = a + b;
        bh.consume(sum); // Prevents dead-code elimination!
    }
}
*/
```
</details>

---


### 11.4 JIT Compilation Diagnostics and Compilation Tracing

To understand JIT compiler decisions at machine-level granularity in a development or staging environment:

1. **Tracing Compilation Events**:
   \\ash
   java -XX:+PrintCompilation -XX:+UnlockDiagnosticVMOptions -XX:+PrintInlining -jar target/app.jar
   \   Output columns explained:
   - Column 1: Milliseconds since VM start.
   - Column 2: Compilation ID (sequential integer).
   - Column 3: Flags (\%\ = OSR, \s\ = synchronized, \!\ = has exception handler, \\ = blocking compilation, \ = native wrapper).
   - Column 4: Tier level (\, \, \ for C1, \ for C2 Server compiler).
   - Column 5: Method name and signature.
   - Column 6: Deoptimization or inlining notes.

2. **Inspecting Disassembled Assembly with hsdis**:
   To inspect the exact AVX-512, NEON, or x86-64 machine instructions emitted by HotSpot:
   - Place \hsdis-amd64.dll\ or \hsdis-amd64.so\ into \$JAVA_HOME/lib/server/\.
   - Run:
     \\ash
     java -XX:+UnlockDiagnosticVMOptions -XX:+PrintAssembly -XX:CompileCommand=print,*OrderCalculator.computeTotal target/app.jar
     \   Examine how HotSpot unrolls tight loops, replaces virtual method invocations with direct jumps after speculative devirtualization, and eliminates null-pointer checks via hardware signal handlers.

3. **Compiler Directives File (CompileCommand File)**:
   You can instruct HotSpot to exclude, inline, or force-compile specific methods dynamically via \.hotspot_compiler\ or JVM option:
   \\	ext
   # hotspot_compiler file syntax
   quiet
   exclude com/example/LegacyModule unoptimizedMethod
   inline com/example/FastMath *
   \


### 11.5 GraalVM Native Image vs HotSpot JIT Compilation Dynamics

Modern cloud-native Java architecture presents two distinct compilation paradigms:

1. **Ahead-of-Time (AOT) via GraalVM Native Image**:
   - Compiles bytecode to native machine binary (`ELF` on Linux, `PE` on Windows) during CI/CD build time.
   - Advantages: Instantaneous startup (< 10 ms), zero JVM warmup curve, ultra-compact memory footprint (~30 MB RSS).
   - Limitations: Closed-World Assumption requires explicit reflection configuration files; no runtime profile-guided optimization unless using Enterprise PGO.

2. **Just-in-Time (JIT) Dynamic Optimization on HotSpot**:
   - Executes bytecode via interpreter initially, compiling hot paths at runtime.
   - Advantages: Peak peak throughput surpasses AOT because runtime profiling captures actual execution branches, devirtualizing 99% of hot calls and re-optimizing dynamically if branch behavior changes.
   - Dynamic Deoptimization: If speculative assumptions are invalidated, HotSpot deoptimizes to interpreter cleanly via uncommon traps.

3. **Coordinated Restore at Checkpoint (CRaC)**:
   - Warms up the JVM in staging, creates an OS-level snapshot (CRIU), and restores the warmed memory image in milliseconds in production containers.
   - Eliminates both cold-start JIT compilation latency and the rigid restrictions of closed-world Ahead-of-Time compilation.


## 12. Summary / Cheat Sheet

```
+-----------------------------------------------------------------------------------------+
|                                    JIT COMPILER CHEAT SHEET                             |
+-----------------------------------------------------------------------------------------+
| Tiered Compilation       | Tier 0 (Interpreter) -> Tiers 1-3 (C1) -> Tier 4 (C2)        |
| Inlining Rule            | Methods <= 35 bytes inlined unconditionally (-XX:MaxInline)  |
| Escape Analysis          | Enables Scalar Replacement (No heap alloc) & Lock Elimination|
| OSR (On-Stack Replace)   | Compiles long-running loops while they are actively executing|
| Diagnostics Flags        | -XX:+PrintCompilation / -XX:+PrintInlining                   |
| Devirtualization         | Monomorphic/Bimorphic calls inlined with type guards         |
| Benchmarking Rule        | Always use JMH (Java Microbenchmark Harness) for micro-tests |
+-----------------------------------------------------------------------------------------+
```

---

## 13. Further Reading
- [OpenJDK HotSpot Compiler Documentation](https://openjdk.org/groups/hotspot/compiler/)
- "Java Performance: In-Depth Advice for Tuning and Acceleration" by Scott Oaks
- "JMH - Java Microbenchmark Harness" Documentation
