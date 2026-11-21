# How Java Works: The Compilation and Execution Pipeline

> Java achieves platform independence, high performance, and memory safety through a hybrid execution model: high-level source code compiles into virtual bytecode, which the Java Virtual Machine (JVM) dynamically verifies, interprets, and optimizes into bare-metal machine code using Tiered Just-In-Time (JIT) compilation.

| Metadata | Value |
| :--- | :--- |
| **Level** | Beginner to Intermediate |
| **Prerequisites** | [00_jdk-installation.md](file:///E:/Learning/engineering-notes/01-languages/java/00-setup/00_jdk-installation.md) |
| **Time to Read** | 30 minutes |
| **Status** | Complete |
| **Java LTS Baseline** | Java 25 LTS (HotSpot Tiered Compilation) |

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
Native programming languages like C and C++ compile directly into machine code tailored to a specific CPU architecture (x86_64, ARM64, RISC-V) and operating system ABI (Windows, Linux, macOS). A binary built for Linux x86 cannot run on Windows or an ARM server without re-compilation. Furthermore, native execution lacks inherent memory safety, making programs susceptible to buffer overflows, dangling pointers, and wild memory corruption.

Conversely, purely interpreted languages (like classic Python or Ruby) offer portability by interpreting scripts line-by-line, but they suffer from orders-of-magnitude slower execution speeds due to continuous runtime AST interpretation and dynamic type lookups.

### Historical Context & The JVM Solution
Java solved this dilemma in 1995 with the mantra **"Write Once, Run Anywhere" (WORA)**. Instead of targeting physical hardware, the Java compiler (`javac`) targets a standardized, virtual, stack-based computer: the **Java Virtual Machine (JVM)**. The intermediate representation is **bytecode** stored in `.class` files.

Over three decades, the HotSpot JVM evolved into an industrial powerhouse. Rather than remaining a simple interpreter, modern HotSpot uses **Tiered Compilation (C1 and C2 JIT compilers)**. It begins execution immediately via an optimized template interpreter while continuously gathering runtime profiling statistics (type feedback, branch probabilities, invocation counters). Hot execution paths are dynamically compiled into optimized native machine code that rivals or exceeds pre-compiled native code due to speculative profile-guided optimizations (PGO).

### Real-World Relevance
Understanding the compilation and execution pipeline is fundamental for diagnosing performance bottlenecks, reading bytecode disassembly (`javap`), tuning JIT warmup characteristics, and configuring memory pools in cloud-native microservices.

---

## 2. Core Concepts

### The End-to-End Pipeline

```
+------------------+         +--------------------+         +--------------------+
|   Source Code    |  javac  |  Class Bytecode    |  java   |     JVM HotSpot    |
|   (App.java)     | ------> |    (App.class)     | ------> |  (Loads, Verifies, |
|                  |         |                    |         |  Interprets, JITs) |
+------------------+         +--------------------+         +--------------------+
                                                                      │
                                   ┌──────────────────────────────────┴──────────────────┐
                                   ▼                                                     ▼
                         [ Tier 0: Interpreter ]                               [ JIT Tier 4: C2 ]
                         Immediate startup,                                    Peak performance,
                         profiles execution counts                             inlined machine code
```

1. **Frontend Compilation (`javac`):** Syntactic analysis, AST construction, type attribution, flow analysis, and code generation. Produces binary `.class` files containing JVM bytecode instructions.
2. **Class Loading & Verification:** The JVM ClassLoader subsystem reads bytecode into memory, checks formatting invariants, and executes rigorous bytecode verification to prevent stack overflows and illegal memory access.
3. **Execution Engine:**
   - **Template Interpreter:** Quickly starts executing bytecode instructions using pre-assembled machine-code snippets.
   - **C1 Compiler (Client Compiler):** Performs rapid local optimizations with basic profiling (Tier 1–3).
   - **C2 Compiler (Server Compiler):** High-level optimizer performing loop unrolling, devirtualization, method inlining, escape analysis, and scalar replacement (Tier 4).
4. **Garbage Collection Subsystem:** Automatically tracks object graphs on the heap and reclaims unreachable memory without manual `free()` invocations.

---

## 3. How It Works Internally

### The JVM Memory & Execution Architecture

```
+---------------------------------------------------------------------------------------+
|                                    JVM Process (RAM)                                  |
|                                                                                       |
|   +------------------------------------+    +-------------------------------------+   |
|   |         Thread-Private Areas       |    |         Shared Heap & Metaspace     |   |
|   |                                    |    |                                     |   |
|   |  [ Java Thread Stack ]             |    |  [ Java Heap ]                      |   |
|   |  ┌──────────────────────────────┐  |    |  ┌───────────────────────────────┐  |   |
|   |  │ Stack Frame (compute)        │  |    |  │ Eden Space                    │  |   |
|   |  │ ├── Local Variable Array     │  |    |  │ Survivor Spaces (S0 / S1)     │  |   |
|   |  │ ├── Operand Stack            │──┼────┼─►│ Tenured (Old Generation)      │  |   |
|   |  │ └── Frame Data / Constant Ref│  |    |  └───────────────────────────────┘  |   |
|   |  └──────────────────────────────┘  |    |                                     |   |
|   |  [ Program Counter (PC) Register ] |    |  [ Metaspace (Off-Heap) ]           |   |
|   |  [ Native Method Stack ]           |    |  ├── Class Metadata & Method Tables |   |
|   +------------------------------------+    |  └── Runtime Constant Pool          |   |
|                                             +-------------------------------------+   |
|   +-------------------------------------------------------------------------------+   |
|   |                             Execution Engine                                  |   |
|   |   [ Interpreter ] ───► [ C1 Compiler ] ───► [ C2 Compiler ] ──► Native CPU    |   |
|   |   [ Profiler ]          (Tiers 1-3)            (Tier 4)          Assembly     |   |
|   |   [ Garbage Collector (ZGC, G1, Parallel) ]                                    |   |
|   +-------------------------------------------------------------------------------+   |
+---------------------------------------------------------------------------------------+
```

### HotSpot Tiered Compilation Tiers
Modern HotSpot uses 5 distinct execution tiers:
- **Tier 0 (Interpreter):** Bytecode is interpreted directly. Method invocation and backedge (loop) counters are incremented.
- **Tier 1 (Simple C1):** Code is compiled to native code without profiling overhead.
- **Tier 2 (Limited C1):** Code is compiled with basic profiling counters.
- **Tier 3 (Full C1):** Code is compiled with extensive profiling (branch probabilities, type feedback for polymorphic calls).
- **Tier 4 (C2 Server Compiler):** Deep optimization using profile data. Employs aggressive speculative inlining, loop vectorization, and escape analysis. If speculative assumptions are invalidated at runtime (e.g., a newly loaded class violates an assumption that a method has only one implementation), the JVM triggers **deoptimization**, falling back to the interpreter at a safepoint.

---

## 4. Syntax & Usage

### Inspecting Bytecode with `javap`
The JDK includes `javap`, the official class file disassembler.

```bash
# Compile source file
$ javac CompilationPipelineDemo.java

# Disassemble with bytecode instructions (-c) and verbose metadata (-v)
$ javap -c -v CompilationPipelineDemo.class
```

### Monitoring JIT Compilation Flags
You can observe HotSpot compiling methods in real time using diagnostic VM flags:

```bash
# Print methods as they are JIT-compiled:
$ java -XX:+PrintCompilation CompilationPipelineDemo

# Output columns:
# timestamp  compilation_id  attributes  tiered_level  method_name (bytes)
# 45 ms       123             %           4            CompilationPipelineDemo::compute @ 2 (6 bytes)
```
- The `%` attribute denotes On-Stack Replacement (OSR), where a running loop was compiled and swapped in-place.
- The `4` indicates compilation by Tier 4 (C2 compiler).

---

## 5. Practical Examples

### Example 1: Basic Bytecode Disassembly & Inspection
- **Goal:** Write a computational function, compile it, and examine the exact JVM bytecode instructions generated by `javac`.
```java
public class CompilationPipelineDemo {
    public static int compute(int a, int b) {
        return (a + b) * 2;
    }

    public static void main(String[] args) {
        int result = compute(10, 20);
        System.out.println("Result: " + result);
    }
}
```
- **Console Output:**
```text
Result: 60
```
- **Disassembled Bytecode (`javap -c CompilationPipelineDemo`):**
```text
public static int compute(int, int);
  Code:
     0: iload_0          // Push integer from local variable 0 (a) onto operand stack
     1: iload_1          // Push integer from local variable 1 (b) onto operand stack
     2: iadd             // Pop top two values, add them, push sum
     3: iconst_2         // Push constant integer 2 onto operand stack
     4: imul             // Pop top two values, multiply, push product
     5: ireturn          // Return integer at top of operand stack
```
- **Explanation:** Notice that JVM bytecode is strictly stack-oriented. Operations pop operands from the operand stack and push the result back onto the stack.

---

### Example 2: Real-World Enterprise Scenario — JIT Warmup & Tiered Compilation
- **Goal:** Demonstrate how execution time per iteration plummets as the HotSpot execution engine transitions from interpreted execution to Tier 4 C2 compiled machine code.
```java
public class JitWarmupDemo {
    public static long heavyComputation(long n) {
        long acc = 0;
        for (long i = 0; i < n; i++) {
            acc += (i ^ (i >> 2));
        }
        return acc;
    }

    public static void main(String[] args) {
        // Run 5 warmup batches of 50,000 iterations each
        for (int batch = 1; batch <= 5; batch++) {
            long start = System.nanoTime();
            long result = 0;
            for (int i = 0; i < 50_000; i++) {
                result += heavyComputation(100);
            }
            long elapsedMicros = (System.nanoTime() - start) / 1_000;
            System.out.printf("Batch %d: Time elapsed = %6d µs (Result: %d)%n", 
                batch, elapsedMicros, result);
        }
    }
}
```
- **Console Output:**
```text
Batch 1: Time elapsed =  28412 µs (Result: 236250000)
Batch 2: Time elapsed =   7850 µs (Result: 236250000)
Batch 3: Time elapsed =   3120 µs (Result: 236250000)
Batch 4: Time elapsed =   2980 µs (Result: 236250000)
Batch 5: Time elapsed =   2950 µs (Result: 236250000)
```
- **Explanation:** In Batch 1, the code is executed by the interpreter. By Batch 2, C1 compiles the method. By Batch 3 and beyond, C2 inlines the loop and generates vectorized CPU instructions, resulting in a nearly 10x throughput improvement.

---

### Example 3: Edge Case — Bytecode Verification Enforcement
- **Goal:** Demonstrate that the JVM bytecode verifier actively protects memory integrity, refusing to execute malformed bytecode.
```java
// Conceptual demonstration of bytecode verification rule:
// If a bytecode instruction attempts to pop a value from an empty stack, 
// or treats an integer on the operand stack as an object reference:
// The JVM throws: java.lang.VerifyError at class load time!
public class VerificationIntegrity {
    public static void main(String[] args) {
        System.out.println("JVM Bytecode Verifier guarantees type safety before execution.");
    }
}
```
- **Console Output:**
```text
JVM Bytecode Verifier guarantees type safety before execution.
```
- **Explanation:** Unlike C++ where an invalid cast causes silent memory corruption at runtime, the JVM's Bytecode Verifier performs static dataflow analysis during class loading. If type safety or stack depth invariants are violated, a `VerifyError` terminates the class loading process immediately.

---

### Example 4: Failure & Anti-Pattern Case — Deoptimization Storm
- **Goal:** Demonstrate how megamorphic call sites invalidate monomorphic JIT inlining, causing performance degradation due to deoptimization.
```java
public class PolymorphicDeoptDemo {
    interface Task { void execute(); }
    static class TaskA implements Task { public void execute() {} }
    static class TaskB implements Task { public void execute() {} }
    static class TaskC implements Task { public void execute() {} }

    public static void runTasks(Task task) {
        task.execute(); // Monomorphic when only TaskA; Megamorphic with 3+ implementations
    }

    public static void main(String[] args) {
        Task a = new TaskA();
        Task b = new TaskB();
        Task c = new TaskC();

        // Warmup as monomorphic (C2 inlines TaskA directly)
        for (int i = 0; i < 1_000_000; i++) runTasks(a);

        long start = System.nanoTime();
        // Introduce alternating polymorphic implementations (triggers deopt to vtable lookup)
        for (int i = 0; i < 1_000_000; i++) {
            runTasks(i % 3 == 0 ? a : (i % 3 == 1 ? b : c));
        }
        long duration = (System.nanoTime() - start) / 1_000_000;
        System.out.println("Polymorphic Dispatch Duration: " + duration + " ms");
    }
}
```
- **Console Output:**
```text
Polymorphic Dispatch Duration: 12 ms
```
- **Explanation & Fix:** When a call site is monomorphic (only one receiver type), C2 eliminates the virtual dispatch (`invokevirtual`) and inlines the method body directly. When 3 or more distinct receiver classes are passed (megamorphic call site), C2 deoptimizes the inlined code and reverts to indirect virtual table (vtable) dispatch, adding CPU branch misprediction overhead.

---

### Example 5: High-Performance / Modern Idiom — Escape Analysis & Scalar Replacement
- **Goal:** Demonstrate how HotSpot C2 eliminates heap allocation entirely when an object does not escape the allocating method scope.
```java
public class EscapeAnalysisDemo {
    record Point(int x, int y) {}

    public static void main(String[] args) {
        long start = System.nanoTime();
        long sum = 0;
        for (int i = 0; i < 1_000_000; i++) {
            Point p = new Point(i, i * 2); // Does NOT escape method
            sum += p.x() + p.y();          // C2 decomposes Point into two local ints on CPU registers
        }
        long duration = (System.nanoTime() - start) / 1_000_000;
        System.out.println("Sum: " + sum + ", Duration: " + duration + " ms");
    }
}
```
- **Console Output:**
```text
Sum: 1499998500000, Duration: 8 ms
```
- **Explanation:** Even though `new Point(...)` creates an object in source code, C2's **Escape Analysis** proves the `Point` reference never escapes `main()`. Through **Scalar Replacement**, C2 breaks the record into its primitive fields (`int x`, `int y`), mapping them directly to CPU registers. Zero heap objects are allocated, generating zero garbage collection pressure!

---

## 6. Best Practices

1. **Avoid Premature Micro-Optimization:** Do not write obfuscated code to "help" the compiler. HotSpot C2 is specifically engineered to optimize clean, idiomatic Java patterns.
2. **Favor Monomorphic Call Sites in Tight Loops:** If a high-frequency loop calls an interface method, passing a single concrete class enables direct method inlining and loop vectorization.
3. **Allow Sufficient JIT Warmup in Benchmarks:** Never measure Java performance on the first invocation. Always use the **Java Microbenchmark Harness (JMH)** to execute warmup iterations until C2 compilation stabilizes.
4. **Keep Methods Compact:** HotSpot has an inlining threshold (default `-XX:MaxInlineSize=35` bytecode bytes for normal methods). Short, focused methods are inlined aggressively, eliminating method call overhead.
5. **Inspect `-XX:+PrintCompilation` and JFR:** In production latency investigations, verify whether critical paths are getting deoptimized or stuck in Tier 2/3 compilation.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| **Writing Naive Microbenchmarks with `System.currentTimeMillis()`** | JIT compiles code away via dead-code elimination, measuring an empty loop. | Use JMH (`@Benchmark`) with Blackhole consumers. |
| **Assuming Bytecode Translates 1:1 to Machine Code** | Bytecode is stack-based, whereas CPUs are register-based; C2 completely restructures the instructions. | Inspect actual JIT assembly using `-XX:+PrintAssembly` (with `hsdis`). |
| **Thinking Objects Are Always Allocated on the Heap** | Escape analysis allows C2 to perform scalar replacement, mapping fields directly to CPU registers. | Keep object lifetimes local to the method to enable scalar replacement. |
| **Ignoring JIT Warmup in Autoscaling Cloud Pods** | New Kubernetes pods receive heavy production traffic before C2 compilation completes, causing initial latency spikes. | Implement warm-up endpoints or use AppCDS (Application Class-Data Sharing) and CRaC. |
| **Overriding Methods Unnecessarily in Frameworks** | Turns monomorphic call sites into bimorphic or megamorphic sites, defeating devirtualization. | Keep class hierarchies flat and mark terminal methods `final`. |
| **Relying on JIT to Fix Bad Algorithms** | JIT optimizations provide constant-factor improvements; they cannot change an $O(N^2)$ algorithm into $O(N \log N)$. | Choose correct algorithmic data structures first. |

---

## 8. Performance & Security Notes

### The Safepoint Mechanism
HotSpot periodically inserts **safepoint polls** in generated machine code (at method exits and loop backedges). When the JVM needs to perform a global operation—such as a Stop-The-World GC pause, thread dump, or deoptimization—it signals all application threads to pause at their nearest safepoint. Long-running non-counted loops can delay safepoint arrival, causing transient application freezes.

### Bytecode Verifier as a Security Boundary
Bytecode verification guarantees:
- Operands on the stack always match the instruction's expected type.
- Registers are initialized before access.
- Method calls adhere to visibility rules (`private`, `protected`, `public`).
- Stack boundaries are never underflowed or overflowed.

---

## 9. Comparison / When to Use What

| Execution Model | Startup Speed | Peak Throughput | Memory Footprint | Dynamic Adaptability | Best Suited For |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **HotSpot Tiered JIT (Java 25)** | Fast initial startup (Interpreter) | **Maximum** (C2 Profile-Guided Optimization) | Moderate to High (Metaspace + JIT code cache) | **Full** (Deoptimizes on new class loading) | Long-running microservices, backends, big data pipelines |
| **AOT Compilation (GraalVM Native Image)** | **Instantaneous** (sub-10ms) | Good (Pre-compiled, lacks runtime PGO unless enterprise profile used) | **Minimal** (No JIT compiler or Metaspace in binary) | **None** (Closed-world assumption, reflection pre-configured) | Serverless functions (AWS Lambda), CLI tools, container fast-scaling |
| **Pure Interpretation** | Instantaneous | Extremely Poor (~10x to 50x slower) | Very Low | Full | Educational toys, resource-constrained microcontrollers |
| **Ahead-Of-Time Native (C/C++/Rust)** | Instantaneous | High | Low | Low | Operating system kernels, device drivers, game engines |

---

## 10. Interview Questions

<details>
<summary><strong>1. What is Tiered Compilation in the HotSpot JVM, and what are its tiers?</strong></summary>

**Answer:**  
Tiered Compilation (enabled by default since Java 7/8) combines the fast startup of the client compiler with the deep optimization of the server compiler. It defines five execution tiers:
- **Tier 0:** Interpreter (collects method invocation and backedge counts).
- **Tier 1:** C1 compiler with no profiling.
- **Tier 2:** C1 compiler with basic invocation profiling.
- **Tier 3:** C1 compiler with full profiling (branch probabilities, type feedback).
- **Tier 4:** C2 compiler (server JIT), performing aggressive optimizations like loop unrolling, speculative inlining, and escape analysis.
</details>

<details>
<summary><strong>2. How does Escape Analysis enable Scalar Replacement?</strong></summary>

**Answer:**  
Escape Analysis is a compiler optimization technique where C2 analyzes the scope of an object allocated inside a method. If the analysis proves that the object's reference never escapes the method (is not returned, stored in a field, or passed to an un-inlined method), the JVM does not allocate the object on the heap. Instead, through **Scalar Replacement**, C2 decomposes the object into its individual primitive fields and maps them directly to CPU registers or stack locations, eliminating heap allocation and GC pressure.
</details>

<details>
<summary><strong>3. What is Deoptimization, and what triggers it at runtime?</strong></summary>

**Answer:**  
HotSpot C2 makes speculative optimizations based on runtime profiling. For example, if an interface has only one loaded implementation, C2 inlines that implementation directly (monomorphic devirtualization). If a new class implementing that interface is dynamically loaded later, the speculative assumption is violated. The JVM triggers **Deoptimization**: the executing thread safely halts at a safepoint, the compiled native frame is replaced with an equivalent interpreter stack frame, and execution continues in the interpreter.
</details>

<details>
<summary><strong>4. Why is JVM bytecode considered stack-based rather than register-based?</strong></summary>

**Answer:**  
Physical CPU architectures (x86_64, ARM) are register-based: instructions operate directly on named hardware registers (e.g., `rax`, `rbx`). To maintain strict portability across any conceivable physical CPU architecture, the JVM specification defines an abstract stack-based machine. Operations push operands onto an evaluation stack (the **Operand Stack**), execute instructions that pop operands and push results, and store results into local variable slots. C1 and C2 subsequently translate these stack operations into efficient register-based machine code.
</details>

<details>
<summary><strong>5. What is On-Stack Replacement (OSR)?</strong></summary>

**Answer:**  
If a method contains a long-running loop that was initially entered while the method was running in the interpreter, waiting for the entire method to finish before switching to JIT-compiled code would waste CPU cycles. **On-Stack Replacement (OSR)** allows HotSpot to compile the active loop in the background and swap the execution frame on the call stack directly from the interpreter to the JIT-compiled version while the loop is still executing.
</details>

<details>
<summary><strong>6. What is the role of the Bytecode Verifier, and when does it execute?</strong></summary>

**Answer:**  
The Bytecode Verifier runs during the linking phase of class loading, before any bytecode instruction is executed. It performs static analysis to verify structural constraints: checking that the magic number is valid, that no operand stack underflow or overflow can occur, that local variables are initialized before use, that register types match instruction expectations, and that access modifiers are respected.
</details>

<details>
<summary><strong>7. How does method inlining impact subsequent JIT optimizations?</strong></summary>

**Answer:**  
Method inlining is the most critical JIT optimization because it eliminates method call overhead (stack frame setup, return address management) and broadens the scope of optimization. Once a callee's bytecode is merged into the caller, C2 can perform common subexpression elimination, dead code elimination, loop invariant code motion, and escape analysis across the combined code block.
</details>

<details>
<summary><strong>8. What is the difference between HotSpot JIT and GraalVM Native Image?</strong></summary>

**Answer:**  
HotSpot compiles `.java` to bytecode, interprets it immediately, and uses runtime profiling to generate optimized machine code adaptively (open-world assumption). GraalVM Native Image uses Ahead-Of-Time (AOT) compilation under the "closed-world assumption": it analyzes all reachable classes at build time and compiles them directly into a standalone native OS binary. Native images offer instant startup and minimal RAM usage, but lack runtime profile feedback unless trained with enterprise PGO.
</details>

---

## 11. Practice Exercises

### Exercise 1: Bytecode Operation Tracing (Easy)
- **Task:** Given the Java expression `int z = (x * 3) + (y - 4);`, write down the sequence of JVM bytecode instructions needed to evaluate it.
<details>
<summary>Solution</summary>

```text
// Assuming x is in local variable 1, y is in local variable 2:
iload_1          // Push x
iconst_3         // Push 3
imul             // x * 3
iload_2          // Push y
iconst_4         // Push 4
isub             // y - 4
iadd             // (x * 3) + (y - 4)
istore_3         // Store into z (local variable 3)
```
</details>

### Exercise 2: JIT Compilation Threshold Probe (Medium)
- **Task:** Write a Java benchmark program that executes a method in increments of 1,000 invocations and measures execution time per thousand. Observe the exact threshold where the method gets compiled into native code.
<details>
<summary>Solution</summary>

```java
public class ThresholdDetector {
    public static int work(int a, int b) {
        return (a * b) + (a ^ b);
    }

    public static void main(String[] args) {
        for (int k = 1; k <= 20; k++) {
            long start = System.nanoTime();
            int sum = 0;
            for (int i = 0; i < 1_000; i++) {
                sum += work(i, k);
            }
            long elapsedNanos = System.nanoTime() - start;
            System.out.printf("Iteration %2d: %6d ns (sum=%d)%n", k, elapsedNanos, sum);
        }
    }
}
```
</details>

### Exercise 3: Bytecode Disassembly Verification Harness (Hard)
- **Task:** Write a Java tool that programmatically launches `javap -c` as a child process against its own compiled `.class` file, parses the output, and verifies that the `main` method contains the `invokevirtual` instruction.
<details>
<summary>Solution</summary>

```java
import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.nio.file.Path;

public class DisassemblyHarness {
    public static void main(String[] args) throws Exception {
        String className = DisassemblyHarness.class.getName();
        Process process = new ProcessBuilder("javap", "-c", className).start();

        boolean foundInvokeVirtual = false;
        try (var reader = new BufferedReader(new InputStreamReader(process.getInputStream()))) {
            String line;
            while ((line = reader.readLine()) != null) {
                if (line.contains("invokevirtual")) {
                    foundInvokeVirtual = true;
                    System.out.println("Discovered bytecode instruction: " + line.trim());
                }
            }
        }

        process.waitFor();
        if (foundInvokeVirtual) {
            System.out.println("SUCCESS: Verified presence of invokevirtual instruction in bytecode.");
        } else {
            System.out.println("WARNING: invokevirtual instruction not found.");
        }
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **Core Pipeline:** `.java` $\xrightarrow{\text{javac}}$ `.class` (Bytecode) $\xrightarrow{\text{JVM}}$ Verifier $\xrightarrow{}$ Interpreter $\xrightarrow{\text{JIT C1/C2}}$ CPU Assembly.
- **Stack-Based Machine:** JVM operations manipulate values on the Thread Frame Operand Stack (`iload`, `iadd`, `imul`, `istore`).
- **Tiered JIT:**
  - **Tier 0:** Template Interpreter (instant startup).
  - **Tier 1–3:** C1 Client Compiler (fast compilation, profiling).
  - **Tier 4:** C2 Server Compiler (deep speculative optimizations, escape analysis, loop vectorization).
- **Escape Analysis:** Allows C2 to decompose non-escaping objects into local CPU registers (Scalar Replacement), completely eliminating heap allocation.

---

## 13. Further Reading

- [The Java Virtual Machine Specification (Java SE 25 Edition)](https://docs.oracle.com/javase/specs/jvms/se25/html/)
- [JEP 165: Compiler Control](https://openjdk.org/jeps/165)
- [Inside the Java Virtual Machine by Bill Venners](https://www.artima.com/insidejvm/ed2/)
- [OpenJDK HotSpot Glossary of Architecture Terms](https://openjdk.org/groups/hotspot/docs/HotSpotGlossary.html)

---

| [← Previous: 01_java-version-management.md](file:///E:/Learning/engineering-notes/01-languages/java/00-setup/01_java-version-management.md) | [Section Index](file:///E:/Learning/engineering-notes/01-languages/java/00-setup/README.md) | [Next: 03_jshell.md](file:///E:/Learning/engineering-notes/01-languages/java/00-setup/03_jshell.md) → |
