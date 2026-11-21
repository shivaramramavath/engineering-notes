# Java Interview Preparation Track (`28-interview`)

> Comprehensive, battle-tested technical interview curriculum covering core Java internals, OOP design, concurrency, JVM diagnostics, tricky code puzzles, and large-scale backend distributed system design.

---

## Overview

This track provides senior, lead, and staff software engineers with an exhaustive technical preparation curriculum. Every concept note is structured with in-depth theoretical explanations, HotSpot memory layouts, verified production code, failure analyses, and curated interview questions with collapsible answers.

---

## Complete Track Index

| # | Note | Focus Area | Recommended Time |
| :---: | :--- | :--- | :---: |
| **00** | [Core Java Interview Mastery](./00_core-java.md) | Primitives, operators, control flow, pass-by-value, wrapper caches | 40 mins |
| **01** | [Object-Oriented Programming Mastery](./01_oop.md) | Encapsulation, inheritance, polymorphism, abstraction, composition | 45 mins |
| **02** | [Strings and Text Internals](./02_strings.md) | String constant pool, compact strings, StringBuilder, regex | 40 mins |
| **03** | [Collections Framework Mastery](./03_collections.md) | HashMap treeification, ConcurrentHashMap, Sequenced Collections | 45 mins |
| **04** | [Generics and Type Systems](./04_generics.md) | Type erasure, PECS wildcards, bridge methods, heap pollution | 45 mins |
| **05** | [Exception Handling & Debugging](./05_exceptions.md) | Hierarchy, checked vs unchecked, try-with-resources, suppressed errors | 40 mins |
| **06** | [Functional & Modern Java (Java 8–25)](./06_functional-and-modern-java.md) | Lambdas, streams, records, pattern matching, sealed types, FFM API | 50 mins |
| **07** | [Concurrency & Multi-Threading](./07_concurrency.md) | Java Memory Model, locks, atomics, CompletableFuture, Virtual Threads | 50 mins |
| **08** | [JVM Architecture & Internals](./08_jvm.md) | Classloading, bytecode execution, JIT compilation, G1/ZGC, HotSpot memory | 50 mins |
| **09** | [JDBC & Persistence Mastery](./09_jdbc-and-persistence.md) | Connection pooling (HikariCP), transactions, batching, ACID isolation | 45 mins |
| **10** | [Design Patterns & Principles](./10_design-patterns-and-principles.md) | GoF design patterns, SOLID principles, clean architecture | 50 mins |
| **11** | [Performance & Troubleshooting](./11_performance-and-troubleshooting.md) | Thread dump triage, memory leak forensics, TTSP safepoints, MAT | 45 mins |
| **12** | [Output-Based & Tricky Questions](./12_output-based-questions.md) | Overloading precedence, static init order, try-finally returns, IEEE 754 | 45 mins |
| **13** | [Backend System Design in Java](./13_backend-system-design.md) | Distributed rate limiting, Snowflake IDs, two-tier caching, outbox pattern | 50 mins |

---

## Recommended Preparation Strategy

```
                          [Week 1: Foundations]
               00_core-java -> 01_oop -> 02_strings -> 05_exceptions
                                    |
                                    v
                          [Week 2: Data & Types]
                     03_collections -> 04_generics -> 09_jdbc
                                    |
                                    v
                     [Week 3: Concurrency & Modern Java]
                06_functional-and-modern-java -> 07_concurrency
                                    |
                                    v
                     [Week 4: Runtime & Architecture]
               08_jvm -> 10_design-patterns -> 11_performance
                                    |
                                    v
                   [Week 5: Puzzles & System Architecture]
              12_output-based-questions -> 13_backend-system-design
```

---

## Key Verification Commands
All code snippets in this track have been compiled and verified against Oracle JDK 25 LTS:
```bash
# Verify sample code compilation
javac --release 25 -Xlint:all Sample.java

# Run with assertions enabled
java -ea Sample
```
