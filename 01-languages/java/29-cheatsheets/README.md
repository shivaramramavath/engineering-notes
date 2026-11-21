# Java Quick Reference Cheatsheets Track (`29-cheatsheets`)

> Exhaustive, high-density reference guides for Core Java syntax, String processing, Collections, Streams, Multithreading, Modern Java (8–25 LTS), GoF Design Patterns, JVM Tuning flags, Build Tools, and Data Structures & Algorithms.

---

## Track Index & Quick Navigation

| # | Cheatsheet | Core Focus Area | Verified Scope |
| :---: | :--- | :--- | :---: |
| **00** | [Core Java Language Reference](./00_core-language.md) | Primitives, operators, switch expressions, records, sealed types, bitwise hacks | Java 8 to 25 |
| **01** | [Strings & Text Processing](./01_strings.md) | Compact Strings, String Constant Pool, regex, formatting, Unicode code points | Java 8 to 25 |
| **02** | [Collections Framework Master Index](./02_collections.md) | Sequenced Collections, Map/Set/List methods, HashMap treeification, thread safety | Java 8 to 25 |
| **03** | [Streams & Functional Pipelines](./03_streams.md) | Intermediate/terminal operations, Collectors, primitive streams, gatherers | Java 8 to 25 |
| **04** | [Concurrency & Multithreading](./04_concurrency.md) | JMM happens-before, Virtual Threads, AQS, locks, atomics, CompletableFuture | Java 8 to 25 |
| **05** | [Modern Java (8 to 25 LTS)](./05_modern-java.md) | JEP master matrix, syntax evolution timeline, FFM API, Generational ZGC | Java 8 to 25 |
| **06** | [GoF Design Patterns Reimagined](./06_design-patterns.md) | 23 GoF patterns implemented in Modern Java using lambdas, records & pattern switch | Java 8 to 25 |
| **07** | [JVM Flags, Tuning & Diagnostics](./07_jvm-flags-and-diagnostics.md) | Heap sizing, container percentage, GC options (G1/ZGC), -Xlog, jcmd, NMT, async-profiler | Java 8 to 25 |
| **08** | [Maven & Gradle Build Automation](./08_maven-and-gradle-commands.md) | Lifecycles, dependency scopes, Gradle Kotlin DSL, multi-module POMs, CI/CD recipes | Maven 3.9+ / Gradle 8+ |
| **09** | [DSA Patterns & Big-O Complexity](./09_dsa-patterns-and-complexity.md) | Master Big-O tables, 16 algorithmic patterns, sorting matrices, Java templates | Complete DSA |

---

## How to Use These Cheatsheets

1. **Production Incident Triage:** Refer to `07_jvm-flags-and-diagnostics.md` for rapid runbooks on high CPU, thread dumps, GC logs, and memory leak analysis.
2. **Architecture & Modernization:** Refer to `05_modern-java.md` and `06_design-patterns.md` when upgrading legacy Java codebases to Java 21 or Java 25 LTS.
3. **Coding Interviews & Daily Reference:** Refer to `09_dsa-patterns-and-complexity.md` for Big-O limits and clean algorithmic boilerplate.
