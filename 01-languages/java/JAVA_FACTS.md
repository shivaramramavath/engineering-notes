# Verified Java Facts & Standards Reference (`JAVA_FACTS.md`)

> **Verification Timestamp:** November 2026  
> **Source of Truth:** OpenJDK JEPs, Oracle Java SE Specifications, ISO/IEC Standards  
> **Host Compiler:** Oracle JDK 25.0.1 (LTS build 25.0.1+8-LTS-27)

---

## 1. Java Release Cadence & LTS Lifecycles

| Java Version | Release Date | Release Type | EOL / Support Status | Notes |
| :--- | :--- | :--- | :--- | :--- |
| **Java 8** | March 2014 | Long-Term Support (LTS) | Extended Support | Introduced lambdas, streams, default methods, `java.time` |
| **Java 11** | September 2018 | Long-Term Support (LTS) | Extended Support | Introduced `var` for lambda params, HTTP client, ZGC (experimental) |
| **Java 17** | September 2021 | Long-Term Support (LTS) | Extended Support | Introduced records, sealed classes, pattern matching instanceof, removed Applet API |
| **Java 21** | September 2023 | Long-Term Support (LTS) | Active Support | Virtual threads, sequenced collections, record patterns, pattern matching for switch |
| **Java 22** | March 2024 | Feature (Non-LTS) | End of Life | FFM API, unnamed variables & patterns, launch multi-file source |
| **Java 23** | September 2024 | Feature (Non-LTS) | End of Life | Markdown javadoc comments (`///`), String Templates WITHDRAWN |
| **Java 24** | March 2025 | Feature (Non-LTS) | End of Life | Stream Gatherers, Class-File API finalized; SecurityManager permanently removed |
| **Java 25** | September 16, 2025 | **Current Long-Term Support (LTS)** | **Active LTS Baseline** | Flexible constructor bodies, module imports, compact source files, scoped values |

---

## 2. JEP Status Matrix (Java 21 Through Java 25 LTS)

| JEP # | Feature Name | Java Version | Formal Status | Command-Line Flags | Key Specifications / Notes | Source URL |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **431** | Sequenced Collections | Java 21 | **Final / Standard** | None | `SequencedCollection`, `SequencedSet`, `SequencedMap`, `addFirst()`, `reversed()` | [openjdk.org/jeps/431](https://openjdk.org/jeps/431) |
| **440** | Record Patterns | Java 21 | **Final / Standard** | None | Deconstruct record values in `instanceof` and `switch` | [openjdk.org/jeps/440](https://openjdk.org/jeps/440) |
| **441** | Pattern Matching for switch | Java 21 | **Final / Standard** | None | Type patterns, `when` guards, exhaustive enum/sealed switches, `case null` | [openjdk.org/jeps/441](https://openjdk.org/jeps/441) |
| **444** | Virtual Threads | Java 21 | **Final / Standard** | None | Lightweight user-mode threads scheduled by JVM on carrier OS threads | [openjdk.org/jeps/444](https://openjdk.org/jeps/444) |
| **454** | Foreign Function & Memory API | Java 22 | **Final / Standard** | None | `Arena`, `MemorySegment`, `Linker`, `SymbolLookup` in `java.lang.foreign` | [openjdk.org/jeps/454](https://openjdk.org/jeps/454) |
| **456** | Unnamed Variables & Patterns | Java 22 | **Final / Standard** | None | `_` for unused variables, caught exceptions, and record pattern components | [openjdk.org/jeps/456](https://openjdk.org/jeps/456) |
| **458** | Launch Multi-File Source Programs | Java 22 | **Final / Standard** | None | `java Prog.java` compiles auxiliary `.java` files in directory tree automatically | [openjdk.org/jeps/458](https://openjdk.org/jeps/458) |
| **467** | Markdown Documentation Comments | Java 23 | **Final / Standard** | None | `///` syntax with CommonMark Markdown support in javadoc | [openjdk.org/jeps/467](https://openjdk.org/jeps/467) |
| **430/459**| String Templates (`STR.""`) | Java 21/22 | **WITHDRAWN in 23** | N/A | Withdrawn after 2 previews; not in standard Java 23/24/25. Redesign ongoing. | [openjdk.org/jeps/459](https://openjdk.org/jeps/459) |
| **484** | Class-File API | Java 24 | **Final / Standard** | None | Native bytecode parsing, generation, transformation (`java.lang.classfile`) | [openjdk.org/jeps/484](https://openjdk.org/jeps/484) |
| **485** | Stream Gatherers | Java 24 | **Final / Standard** | None | Custom intermediate stream operations via `Stream::gather(Gatherer)` | [openjdk.org/jeps/485](https://openjdk.org/jeps/485) |
| **486** | Permanently Disable Security Manager | Java 24 | **Final / Standard** | None | SecurityManager calls throw `UnsupportedOperationException`; APIs removed | [openjdk.org/jeps/486](https://openjdk.org/jeps/486) |
| **506** | Scoped Values | Java 25 | **Final / Standard** | None | Immutable thread-shared data bound to execution scopes (`ScopedValue.where`) | [openjdk.org/jeps/506](https://openjdk.org/jeps/506) |
| **511** | Module Import Declarations | Java 25 | **Final / Standard** | None | `import module java.base;` imports all exported packages of a module | [openjdk.org/jeps/511](https://openjdk.org/jeps/511) |
| **512** | Compact Source Files & Main Methods | Java 25 | **Final / Standard** | None | `void main()` entry points without `class` or `static`; `java.lang.IO` | [openjdk.org/jeps/512](https://openjdk.org/jeps/512) |
| **513** | Flexible Constructor Bodies | Java 25 | **Final / Standard** | None | Statements permitted before `super(...)` and `this(...)` in constructors | [openjdk.org/jeps/513](https://openjdk.org/jeps/513) |
| **505** | Structured Concurrency | Java 25 | **Preview (5th)** | `--enable-preview` | `StructuredTaskScope` for coordinating concurrent sub-tasks | [openjdk.org/jeps/505](https://openjdk.org/jeps/505) |
| **508** | Vector API | Java 25 | **Incubator (10th)** | `--add-modules jdk.incubator.vector` | SIMD hardware vector computation across CPU registers | [openjdk.org/jeps/508](https://openjdk.org/jeps/508) |

---

## 3. Deprecations, Obsolete APIs & Security Invariants

1. **`java.lang.SecurityManager`:** Deprecated in Java 17 (JEP 411). Permanently removed in Java 24 (JEP 486). Any attempt to enable or configure a SecurityManager throws `UnsupportedOperationException`. Do not teach or recommend SecurityManager in any modern note.
2. **`Object.finalize()`:** Deprecated in Java 9, deprecated for removal in Java 18 (JEP 421). In Java 25, finalization is disabled by default. Replace with `java.lang.ref.Cleaner` or `AutoCloseable`.
3. **`Thread.stop()`, `Thread.suspend()`, `Thread.resume()`:** `stop()` unconditionally throws `UnsupportedOperationException` since Java 20. Must be replaced with cooperative interruption (`Thread.interrupt()`, `Thread.isInterrupted()`).
4. **Legacy Date/Calendar:** `java.util.Date`, `java.util.Calendar`, `java.text.SimpleDateFormat` are obsolete, mutable, and thread-unsafe. Modern code must exclusively use `java.time` (`LocalDate`, `Instant`, `ZonedDateTime`, `DateTimeFormatter`).
5. **Insecure Password Hashing:** MD5 and SHA-1/SHA-256 without salt are strictly prohibited for password storage. Use Argon2id, PBKDF2WithHmacSHA512, or BCrypt.
6. **SQL Injection:** String concatenation in SQL statements is banned. Use `PreparedStatement` with parameterized placeholders (`?`).
7. **Insecure Java Serialization:** `ObjectInputStream` without explicit `ObjectInputFilter` is a critical remote code execution (RCE) vulnerability vector. Use JEP 290/JEP 415 serialization filters or modern serialization formats (JSON, Protobuf, CBOR).
8. **Double-Checked Locking (DCL):** DCL without the `volatile` keyword is broken under the Java Memory Model because out-of-order writes can publish a partially initialized object reference. Always declare the singleton instance `private static volatile T instance;` or use the Initialization-on-Demand Holder idiom.
