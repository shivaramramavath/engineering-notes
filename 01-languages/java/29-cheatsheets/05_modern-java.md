# Modern Java (Java 8 to 25 LTS) Feature Reference Cheat Sheet

> Exhaustive version-by-version guide to modern Java: JEPs, records, pattern matching, sealed hierarchies, switch expressions, Virtual Threads, Foreign Function & Memory API, and Stream Gatherers.

| Metadata | Details |
| :--- | :--- |
| **Level** | Comprehensive Reference |
| **Prerequisites** | [00_core-language.md](./00_core-language.md) |
| **Time to Read** | 40 minutes |
| **Status** | Complete |
| **Java Version** | Java 8 to 25 LTS |

---

## Table of Contents
1. [Modern Java Version & Release Roadmap](#1-modern-java-version--release-roadmap)
2. [Java 8: Functional Foundation](#2-java-8-functional-foundation)
3. [Java 9–11: Modularity & Modern APIs](#3-java-911-modularity--modern-apis)
4. [Java 12–17: Expressive Language Features](#4-java-1217-expressive-language-features)
5. [Java 18–21: Concurrency & Runtime Revolution](#5-java-1821-concurrency--runtime-revolution)
6. [Java 22–25: Performance & Systems Integration](#6-java-2225-performance--systems-integration)
7. [Comprehensive JEP Number Matrix](#7-comprehensive-jep-number-matrix)
8. [Language Syntax Evolution Timeline](#8-language-syntax-evolution-timeline)
9. [Removed & Deprecated Features Matrix](#9-removed--deprecated-features-matrix)
10. [Migration Guide & Modernization Recipes](#10-migration-guide--modernization-recipes)

---

## 1. Modern Java Version & Release Roadmap

| Release | Release Date | Support Tier | Flagship Highlights |
| :---: | :---: | :---: | :--- |
| **Java 8** | March 2014 | LTS | Lambdas, Streams, `java.time`, Default Methods, Metaspace |
| **Java 11** | September 2018 | LTS | JPMS cleanup, HTTP Client, `var` in lambdas, String APIs |
| **Java 17** | September 2021 | LTS | Records, Sealed Classes, Pattern Matching instanceof, macOS/AArch64 |
| **Java 21** | September 2023 | LTS | Virtual Threads, Pattern Matching for switch, Record Patterns, Sequenced Collections |
| **Java 25** | October 2025 | LTS | FFM API, Stream Gatherers, Class-File API, Flexible Constructor Bodies |

---

## 2. Java 8: Functional Foundation

- **Lambda Expressions & SAM Interfaces:** Replaced verbose anonymous classes with concise functional expressions: `(a, b) -> a + b`.
- **Streams API (`java.util.stream`):** Declarative functional data pipelines supporting lazy evaluation and parallel execution.
- **Date and Time API (`java.time` - JSR 310):** Immutable, thread-safe calendar classes replacing flawed legacy `java.util.Date` and `Calendar`: `Instant`, `LocalDate`, `ZonedDateTime`, `Duration`.
- **Default and Static Interface Methods:** Allowed adding new methods to existing interfaces without breaking legacy implementations.
- **Metaspace:** Replaced fixed-size Permanent Generation (`PermGen`) with native memory Metaspace.

---

## 3. Java 9–11: Modularity & Modern APIs

### Java 9 (September 2017)
- **Java Platform Module System (JPMS - JEP 201):** Introduced `module-info.java` enforcing strong encapsulation.
- **Compact Strings (JEP 254):** Cut String heap footprints by up to 50% using Latin-1 byte arrays.
- **Collection Factory Methods:** `List.of()`, `Set.of()`, `Map.of()` providing unmodifiable, null-hostile collections.

### Java 10 (March 2018)
- **Local-Variable Type Inference (`var` - JEP 286):**
  ```java
  var userList = new ArrayList<UserAccount>(); // Inferred by compiler
  ```

### Java 11 (September 2018 - LTS)
- **Standard HTTP Client (JEP 321):** Reactive, non-blocking HTTP/1.1 and HTTP/2 client in `java.net.http`.
- **Single-File Source-Code Execution (JEP 330):** Execute scripts directly: `java Application.java`.
- **Epsilon No-Op Garbage Collector (JEP 318):** Zero-overhead GC for testing and micro-benchmarks.

---

## 4. Java 12–17: Expressive Language Features

### Java 14: Switch Expressions (JEP 361)
```java
int numLetters = switch (day) {
    case MONDAY, FRIDAY, SUNDAY -> 6;
    case TUESDAY -> 7;
    default -> {
        System.out.println("Processing midweek");
        yield 8;
    }
};
```

### Java 15: Text Blocks (JEP 378)
Multi-line string literals preserving indentation and suppressing trailing whitespace using `"""`.

### Java 16: Records (JEP 395)
Concise immutable data carrier classes:
```java
public record GeoPoint(double latitude, double longitude) {
    public GeoPoint {
        if (latitude < -90 || latitude > 90) throw new IllegalArgumentException();
    }
}
```

### Java 17 (September 2021 - LTS): Sealed Classes (JEP 409)
Enforces closed inheritance hierarchies:
```java
public sealed interface Transaction permits Deposit, Withdrawal, Transfer {}
public final class Deposit implements Transaction {}
public final class Withdrawal implements Transaction {}
public non-sealed class Transfer implements Transaction {}
```

---

## 5. Java 18–21: Concurrency & Runtime Revolution

### Java 21 (September 2023 - LTS)
1. **Virtual Threads (JEP 444):** High-throughput, lightweight user-mode threads decoupling application tasks from OS platform threads.
2. **Sequenced Collections (JEP 431):** Standardized interfaces for ordered collections (`getFirst()`, `getLast()`, `reversed()`).
3. **Pattern Matching for `switch` (JEP 441):** Type checking and extraction inside switch cases with guards.
4. **Record Patterns (JEP 440):** Deconstructing records directly in type patterns:
   ```java
   if (obj instanceof Order(String id, Amount(double val, String curr))) {
       System.out.printf("Order %s: %.2f %s%n", id, val, curr);
   }
   ```
5. **Generational ZGC (JEP 439):** Separates young and old generation collections in ZGC, boosting throughput while preserving sub-millisecond pause times.

---

## 6. Java 22–25: Performance & Systems Integration

### Java 22 & 23 (2024)
- **Foreign Function & Memory API (FFM - JEP 454):** Safe, high-performance alternative to JNI for invoking native C libraries and off-heap memory.
- **Unnamed Variables & Patterns (`_` - JEP 456):** Denotes unused variables or pattern components:
   ```java
   try (var _ = acquireLock()) { /* ... */ }
   ```

### Java 24 & 25 (2025 LTS)
- **Stream Gatherers (JEP 485):** Extensible intermediate stream transformations (windowing, fold, scan).
- **Class-File API (JEP 484):** Standard JDK library for parsing, generating, and transforming JVM bytecode without external ASM dependencies.
- **Flexible Constructor Bodies (JEP 482):** Allows statements to execute *before* `super(...)` in constructors, provided they do not access instance state (`this`).

---

## 7. Comprehensive JEP Number Matrix

| JEP # | Feature Title | Introduced / Finalized | Category |
| :---: | :--- | :---: | :--- |
| **JEP 254** | Compact Strings | Java 9 | Runtime Optimization |
| **JEP 286** | Local-Variable Type Inference (`var`) | Java 10 | Language Syntax |
| **JEP 321** | HTTP Client (Standard) | Java 11 LTS | Library API |
| **JEP 361** | Switch Expressions | Java 14 | Language Syntax |
| **JEP 378** | Text Blocks | Java 15 | Language Syntax |
| **JEP 394** | Pattern Matching for `instanceof` | Java 16 | Language Syntax |
| **JEP 395** | Records | Java 16 | Language Syntax |
| **JEP 409** | Sealed Classes | Java 17 LTS | Language Syntax |
| **JEP 431** | Sequenced Collections | Java 21 LTS | Library API |
| **JEP 439** | Generational ZGC | Java 21 LTS | Garbage Collection |
| **JEP 440** | Record Patterns | Java 21 LTS | Language Syntax |
| **JEP 441** | Pattern Matching for switch | Java 21 LTS | Language Syntax |
| **JEP 444** | Virtual Threads | Java 21 LTS | Concurrency |
| **JEP 454** | Foreign Function & Memory API | Java 22 | Systems Programming |
| **JEP 456** | Unnamed Variables & Patterns | Java 22 | Language Syntax |
| **JEP 485** | Stream Gatherers | Java 25 LTS | Library API |

---

## 8. Language Syntax Evolution Timeline

```
[Java 8]:  (x) -> x + 1;
[Java 10]: var list = new ArrayList<String>();
[Java 14]: return switch (x) { case 1 -> "One"; default -> "Other"; };
[Java 15]: String s = """
           hello
           """;
[Java 16]: record Point(int x, int y) {}
           if (obj instanceof String s) { ... }
[Java 17]: sealed interface Node permits Leaf, Branch {}
[Java 21]: Thread.startVirtualThread(() -> {});
           switch (obj) { case Point(int x, int y) -> x + y; }
[Java 22]: catch (Exception _) { /* unused */ }
[Java 25]: stream.gather(Gatherers.windowFixed(3)).toList();
```

---

## 9. Removed & Deprecated Features Matrix

| Feature | Deprecated In | Removed In | Recommended Modern Replacement |
| :--- | :---: | :---: | :--- |
| **Applet API** | Java 9 | Java 20 | Modern Web (WebAssembly / React / HTML5) |
| **Nashorn JavaScript Engine** | Java 11 | Java 15 | GraalVM JavaScript |
| **RMI Activation** | Java 15 | Java 17 | REST / gRPC Microservices |
| **Security Manager** | Java 17 | Java 24 (Disabled/Removed) | OS-level Containers (Docker / Linux namespaces) |
| **Biased Locking** | Java 15 | Java 21 | Lightweight & Contended monitor locking |
| **Finalization (`Object.finalize()`)**| Java 9 | Deprecated for removal | `java.lang.ref.Cleaner` or try-with-resources |

---

## 10. Migration Guide & Modernization Recipes

### Recipe 1: Replacing Anonymous Classes with Lambdas
```java
// Legacy Java 7
button.addActionListener(new ActionListener() {
    @Override
    public void actionPerformed(ActionEvent e) {
        System.out.println("Clicked!");
    }
});

// Modern Java
button.addActionListener(e -> System.out.println("Clicked!"));
```

### Recipe 2: Replacing Verbose DTOs with Records
```java
// Modern Java 16+ (replaces 80 lines of boilerplate getters, equals, hashCode, and toString)
public record CustomerDto(UUID id, String firstName, String lastName, String email) {}
```

---

## 11. Foreign Function & Memory (FFM) API Deep Dive (Java 22+ JEP 454)

The Foreign Function & Memory API enables Java programs to interoperate with code and data outside the Java runtime without the security vulnerabilities and complexity of JNI.

```java
package com.engineering.modern;

import java.lang.foreign.*;
import java.lang.invoke.MethodHandle;

public class NativeMemoryDemo {
    public static void main(String[] args) throws Throwable {
        // 1. Off-heap memory allocation via Arena
        try (Arena arena = Arena.ofConfined()) {
            // Allocate off-heap native memory segment for a C string
            MemorySegment nativeString = arena.allocateFrom("Hello Native World!");
            System.out.println("Allocated at off-heap address: " + nativeString.address());

            // 2. Lookup standard C library 'strlen' function
            Linker linker = Linker.nativeLinker();
            SymbolLookup stdlib = linker.defaultLookup();
            MemorySegment strlenAddress = stdlib.find("strlen").orElseThrow();

            // Function descriptor: size_t strlen(const char *s)
            FunctionDescriptor descriptor = FunctionDescriptor.of(ValueLayout.JAVA_LONG, ValueLayout.ADDRESS);
            MethodHandle strlenHandle = linker.downcallHandle(strlenAddress, descriptor);

            // 3. Invoke native C function directly from Java
            long length = (long) strlenHandle.invokeExact(nativeString);
            System.out.println("Calculated length via C strlen(): " + length);
        } // Arena closes: off-heap memory is deterministically and immediately freed!
    }
}
```

---

## 12. Scoped Values (JEP 446 / 481 Preview)

Scoped Values provide a lightweight, immutable alternative to `ThreadLocal`, optimized for Virtual Threads where creating millions of mutable thread-locals wastes memory:

```java
import java.lang.ScopedValue;

public class ScopedValueExample {
    // Immutable scoped value
    public static final ScopedValue<String> CURRENT_TENANT = ScopedValue.newInstance();

    public static void handleRequest(String tenantId) {
        // Bind value to the execution scope
        ScopedValue.where(CURRENT_TENANT, tenantId).run(() -> {
            processService(); // Inherits CURRENT_TENANT automatically
        });
        // Out of scope: CURRENT_TENANT is unbound
    }

    private static void processService() {
        System.out.println("Processing for tenant: " + CURRENT_TENANT.get());
    }
}
```

---

## 13. Class-File API (JEP 484 - Java 25 LTS)

Standard JDK API for reading, transforming, and generating JVM bytecode:

```java
import java.lang.classfile.*;
import java.lang.constant.ClassDesc;
import java.lang.constant.MethodTypeDesc;

public class BytecodeGeneratorDemo {
    public static byte[] generateHelloWorldClass() {
        ClassDesc cd = ClassDesc.of("com.engineering.generated.HelloWorld");
        return ClassFile.of().build(cd, clb -> {
            clb.withMethodBody("main",
                    MethodTypeDesc.of(ClassDesc.ofDescriptor("V"), ClassDesc.ofDescriptor("[Ljava/lang/String;")),
                    ClassFile.ACC_PUBLIC | ClassFile.ACC_STATIC,
                    code -> code
                            .getstatic(ClassDesc.of("java.lang.System"), "out", ClassDesc.of("java.io.PrintStream"))
                            .ldc("Hello from Generated Bytecode!")
                            .invokevirtual(ClassDesc.of("java.io.PrintStream"), "println",
                                    MethodTypeDesc.of(ClassDesc.ofDescriptor("V"), ClassDesc.of("java.lang.String")))
                            .return_()
            );
        });
    }
}
```

---

## 14. Modern Java Best Practice Checklist

1. **Adopt Records for Data Carriers:** Eliminates Lombok dependencies and makes intent transparent.
2. **Use Pattern Matching Switch with Sealed Hierarchies:** Guarantees compile-time exhaustiveness without brittle `default` branches.
3. **Use Virtual Threads for Blocking I/O:** Drastically improves server concurrency with simple sequential code.
4. **Use Text Blocks for SQL, JSON, and HTML:** Keeps formatted multi-line strings legible and eliminates escape clutter.
5. **Leverage FFM API over JNI:** Guarantees memory safety and avoids C header generation.

---

## 15. Garbage Collector Evolution Across Java LTS Releases

| LTS Version | Default Garbage Collector | Supported Production GCs | Flagship GC Innovation |
| :---: | :---: | :--- | :--- |
| **Java 8** | Parallel GC (Throughput) | Serial, Parallel, CMS, G1 (experimental) | PermGen removed; Metaspace introduced |
| **Java 11** | G1 GC | Serial, Parallel, G1, Epsilon, ZGC (Linux x64 preview) | G1 becomes default; Epsilon no-op GC |
| **Java 17** | G1 GC | Serial, Parallel, G1, ZGC, Shenandoah | Concurrent thread-stack processing; macOS/AArch64 ZGC |
| **Java 21** | G1 GC | Serial, Parallel, G1, ZGC, Shenandoah | Generational ZGC (JEP 439) with sub-ms pauses |
| **Java 25** | G1 GC | Serial, Parallel, G1, ZGC (Generational default), Shenandoah | Compact Object Headers (JEP 450 preview) |

---

## 16. Java 25 LTS Language & Runtime Innovations

### 1. Markdown Documentation Comments (JEP 467)
Replaces HTML comments in Javadoc with intuitive Markdown syntax using `///`:
```java
/// Represents an authenticated customer account.
///
/// ### Usage Example:
/// ```java
/// var customer = new Customer("Alice", 101);
/// customer.processOrder();
/// ```
///
/// - **id**: Unique positive customer identifier
/// - **name**: Verified account name
public record Customer(String name, long id) {}
```

### 2. Flexible Constructor Bodies (JEP 482)
Allows statements before `super(...)` to prepare arguments or validate inputs:
```java
public class SubClass extends SuperClass {
    public SubClass(int value) {
        if (value < 0) throw new IllegalArgumentException("Negative value!");
        int normalized = Math.max(value, 100);
        super(normalized); // Call to super() after pre-initialization statements!
    }
}
```

---

## 17. JDK Strong Encapsulation & Migration Flags

Starting in Java 16+, HotSpot strictly encapsulates JDK internal packages (JEP 396 / JEP 403). Code using deep reflection on internal HotSpot fields (`sun.misc.Unsafe`, `java.lang.reflect.AccessibleObject`) throws `InaccessibleObjectException`.

```bash
# Explicitly open an internal package to deep reflection for legacy libraries
--add-opens java.base/java.lang=ALL-UNNAMED
--add-opens java.base/java.util=ALL-UNNAMED

# Export a package for public access
--add-exports java.base/sun.security.x509=ALL-UNNAMED
```

---

## 18. Vector API (SIMD Hardware Vectorization)

```java
import jdk.incubator.vector.FloatVector;
import jdk.incubator.vector.VectorSpecies;

public class VectorApiDemo {
    // Hardware SIMD vector species (e.g., 256-bit AVX or 512-bit AVX-512)
    private static final VectorSpecies<Float> SPECIES = FloatVector.SPECIES_PREFERRED;

    public static void vectorAdd(float[] a, float[] b, float[] c) {
        int i = 0;
        int upperBound = SPECIES.loopBound(a.length);
        for (; i < upperBound; i += SPECIES.length()) {
            // Load SIMD vector slices directly into CPU vector registers
            var va = FloatVector.fromArray(SPECIES, a, i);
            var vb = FloatVector.fromArray(SPECIES, b, i);
            var vc = va.add(vb); // Executes single SIMD instruction across 8 or 16 floats simultaneously!
            vc.intoArray(c, i);
        }
        // Tail loop for residual elements
        for (; i < a.length; i++) {
            c[i] = a[i] + b[i];
        }
    }
}
```

---

## 19. Quick Summary / Modern Java Feature Card

- **Java 8:** Lambdas, Streams, `java.time`, default methods.
- **Java 11:** HTTP Client, `var`, String methods (`strip`, `isBlank`, `repeat`).
- **Java 17:** Sealed classes, records, pattern matching for `instanceof`, text blocks.
- **Java 21:** Virtual Threads, Sequenced Collections, pattern matching for `switch`, record patterns, Generational ZGC.
- **Java 25:** Foreign Function & Memory API, Stream Gatherers, Class-File API, Flexible constructor bodies.
