# Java Engineering Technical Glossary

> An authoritative reference dictionary defining key terminology, architectural patterns, HotSpot JVM internals, concurrency primitives, and modern language features (Java 8 to 25 LTS).

---

## Table of Contents
- [A](#a) • [B](#b) • [C](#c) • [D](#d) • [E](#e) • [F](#f) • [G](#g) • [H](#h) • [I](#i) • [J](#j) • [K](#k) • [L](#l) • [M](#m)
- [N](#n) • [O](#o) • [P](#p) • [R](#r) • [S](#s) • [T](#t) • [U](#u) • [V](#v) • [W](#w) • [Z](#z)

---

## A

### Ahead-of-Time Compilation (AOT)
Compilation of Java bytecode directly into native binary machine code prior to execution (e.g. GraalVM Native Image), eliminating JVM warmup time and JIT overhead at the expense of dynamic runtime profile-guided optimizations.

### AbstractQueuedSynchronizer (AQS)
The foundational synchronization framework (`java.util.concurrent.locks.AbstractQueuedSynchronizer`) in Java used to construct blocking locks, semaphores, and latches via an internal FIFO wait queue and atomic state manipulation.

### Application Class Data Sharing (AppCDS)
A HotSpot JVM feature that records class metadata into a shared archive at build or startup time, allowing multiple JVM processes to memory-map read-only class metadata, reducing startup latency and memory footprint.

### Async-Profiler
A low-overhead, sampling CPU and memory profiler for the JVM that avoids safepoint bias by attaching to Linux kernel `perf_events` and invoking HotSpot internal `AsyncGetCallTrace`.

### Autoboxing
The automatic conversion performed by the Java compiler between primitive types (e.g. `int`) and their corresponding object wrapper classes (e.g. `Integer`).

---

## B

### Biased Locking
A HotSpot locking optimization that dedicated monitor ownership to the first thread that acquired it, eliminating atomic CAS instructions on uncontended locks. Deprecated in Java 15 (JEP 374) and removed due to safepoint revocation overhead.

### Bytecode
The intermediate, platform-independent instruction set executed by the Java Virtual Machine. Stored in `.class` files and processed by the interpreter or JIT compiler.

### Bridge Method
A synthetic method generated automatically by the `javac` compiler to preserve polymorphism and binary compatibility when a class implements a parameterized interface with generic type erasure.

---

## C

### Card Table
A byte-array memory structure used by generational garbage collectors (such as G1 GC and Parallel GC) where each byte corresponds to a 512-byte card in the heap, tracking old-to-young generation cross-references.

### ClassLoader
The JVM subsystem responsible for dynamically loading class bytecode into runtime memory during the Loading phase, verifying format integrity, and creating `java.lang.Class` instances.

### Compressed OOPs (Ordinary Object Pointers)
A 64-bit JVM optimization enabled by default on heaps $\le 32	ext{GB}$ (`-XX:+UseCompressedOops`) that represents 64-bit memory addresses as 32-bit unsigned integers shifted right by 3 bits.

### Concurrent Mark Sweep (CMS)
A legacy, low-pause garbage collector that performed young generation collections with Stop-The-World and old generation collections concurrently. Deprecated in Java 9 and removed in Java 14.

### Constant Pool
A per-class table inside `.class` bytecode containing symbolic references to classes, field names, method signatures, string constants, and numerical literals.

---

## D

### Deadlock
A concurrency pathology where two or more threads are permanently blocked, each holding a lock that the other requires to make forward progress.

### Direct ByteBuffer
An off-heap memory buffer (`ByteBuffer.allocateDirect()`) allocated via the C-runtime `malloc()` outside the garbage-collected Java heap, enabling zero-copy OS socket I/O.

### Double-Checked Locking (DCL)
A lazy initialization idiom that reduces synchronization overhead by checking a condition before acquiring a lock, requiring the singleton field to be declared `volatile` to prevent CPU instruction reordering.

---

## E

### Escape Analysis
A JIT compiler analysis technique that determines whether an object allocation escapes the scope of the method in which it was created. If it does not escape, the JVM applies scalar replacement or eliminates synchronization locks.

### Epsilon Garbage Collector
A no-op garbage collector introduced in Java 11 (JEP 318) that allocates memory without ever collecting it, used for ultra-performance testing and ephemeral microbenchmarks.

---

## F

### False Sharing
A hardware cache coherency pathology occurring when two threads running on separate CPU cores concurrently modify independent variables residing on the exact same 64-byte L1/L2 cache line.

### Foreign Function & Memory API (FFM)
A modern Java API finalized in Java 22 (JEP 454) enabling Java code to interoperate with native C libraries and off-heap memory deterministically and safely without JNI.

### ForkJoinPool
An implementation of `ExecutorService` built upon the Work-Stealing algorithm, designed for divide-and-conquer parallel computing tasks (`ForkJoinTask`).

---

## G

### Garbage-First Collector (G1 GC)
The default HotSpot server garbage collector since Java 9, partitioning the heap into equal-sized virtual regions and prioritizing regions with the most garbage for collection.

### Generational ZGC
An advanced enhancement introduced in Java 21 (JEP 439) and made default in Java 25, separating young and old generation collections in ZGC to boost throughput while keeping STW pauses sub-millisecond.

---

## H

### Happens-Before Relationship
A formal ordering relationship defined by the Java Memory Model (JMM) that guarantees memory writes made by one thread are visibly observed by another thread.

### HotSpot JVM
The primary reference implementation of the Java Virtual Machine developed by Oracle and the OpenJDK community, featuring tiered JIT compilation and adaptive runtime profiling.

---

## I

### invokedynamic (Indy)
A JVM bytecode instruction introduced in Java 7 (JSR 292) that delays method linkage until runtime, powering lambda expressions, string concatenation, and dynamic language runtimes.

### Inlining
A core JIT compiler optimization where the body of a frequently called target method is substituted directly into the caller's call site, eliminating method dispatch overhead.

---

## J

### Java Community Process (JCP)
The formal organization that coordinates the evolution of the Java platform through Java Specification Requests (JSRs).

### Java Development Kit (JDK)
The complete software development environment for Java, including the runtime environment (JRE), compiler (`javac`), archiver (`jar`), and diagnostic tools.

### Java Enhancement Proposal (JEP)
The OpenJDK mechanism for proposing, drafting, reviewing, and tracking major new language features, runtime components, and library enhancements.

### Java Memory Model (JMM)
The section of the Java Language Specification (JLS §17.4) governing how threads interact through shared computer memory and specifying cache visibility semantics.

### JIT (Just-In-Time) Compiler
The HotSpot runtime engine (comprising C1 Client and C2 Server compilers) that compiles frequently executed bytecode into native machine instructions at runtime.

---

## K

### Klass Pointer (Klass Word)
A metadata header pointer stored within every Java object header referencing the HotSpot C++ `Klass` mirror object in native Metaspace.

---

## L

### Live Lock
A concurrency condition where two or more threads continuously change their states in response to each other without making computational forward progress.

### Local-Variable Type Inference (`var`)
A language feature introduced in Java 10 (JEP 286) allowing the compiler to infer local variable types from their initializers.

---

## M

### Mark Word
The first 8 bytes of a 64-bit Java object header, encoding the object's identity hash code, GC generation age, biased locking flags, and synchronization lock status.

### Metaspace
The native memory area introduced in Java 8 that replaced PermGen, storing class metadata, method bytecode, annotations, and constant pools outside the Java heap.

---

## N

### Native Memory Tracking (NMT)
An internal HotSpot diagnostic mechanism (`-XX:NativeMemoryTracking=summary`) that tracks off-heap native memory allocations across thread stacks, Metaspace, GC structures, and the C-heap.

---

## O

### Ordinary Object Pointer (OOP)
A HotSpot internal abstraction representing a managed object reference pointer in the Java heap.

### Outbox Pattern
A distributed systems architectural pattern where domain entity mutations and outbound event messages are committed within the same atomic ACID database transaction.

---

## P

### Pattern Matching
A language mechanism that combines a predicate test with conditional extraction and variable binding, applied to `instanceof` (Java 16), `switch` (Java 21), and records (Java 21).

### Project Loom
The OpenJDK project that introduced Virtual Threads (JEP 444) and Structured Concurrency to revolutionize high-throughput concurrent Java applications.

---

## R

### Record
An immutable data carrier class introduced in Java 16 (JEP 395) where the compiler automatically synthesizes constructor, accessors, `equals()`, `hashCode()`, and `toString()`.

---

## S

### Safepoint
A designated program location in compiled machine code where application mutator threads can safely pause to allow the JVM to perform global operations like GC sweeps or thread dumps.

### Sealed Types
A language feature introduced in Java 17 (JEP 409) that restricts which other classes or interfaces may extend or implement them via an explicit `permits` clause.

### Sequenced Collections
A set of interfaces introduced in Java 21 (JEP 431) providing uniform access to first and last elements (`getFirst()`, `getLast()`) and reverse views (`reversed()`) across lists, sets, and maps.

### Stop-The-World (STW)
A pause during which all application mutator threads are halted at safepoints to allow the JVM to execute operations that require an invariant memory state.

---

## T

### Thread-Local Allocation Buffer (TLAB)
A dedicated, pre-allocated region of the young generation Eden space assigned to a specific thread, allowing lock-free object allocations via simple pointer bumping.

### Two's Complement
The mathematical binary numbering representation used by Java for all signed integer types (`byte`, `short`, `int`, `long`), where negative numbers are formed by inverting bits and adding 1.

---

## U

### Unnamed Variables and Patterns (`_`)
A language feature introduced in Java 22 (JEP 456) that allows the underscore character `_` to denote an unused local variable, exception parameter, or pattern component.

---

## V

### Virtual Threads
Lightweight user-mode threads managed by the HotSpot runtime rather than the host operating system, introduced in Java 21 (JEP 444) to enable massive scalability for blocking I/O workloads.

### Volatile
A Java keyword guaranteeing that reads and writes to a variable establish a happens-before memory visibility order across threads and prevent CPU instruction reordering.

---

## W

### Work-Stealing Algorithm
A scheduling strategy where idle worker threads in a thread pool steal tasks from the back of deques belonging to busy worker threads, maximizing CPU core utilization.

---

## Z

### Z Garbage Collector (ZGC)
A scalable, low-latency concurrent garbage collector designed to handle heaps ranging from megabytes to multiple terabytes with pause times consistently below 1 millisecond.
