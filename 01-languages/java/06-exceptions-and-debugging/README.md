# 06. Exceptions and Debugging in Java

> Comprehensive reference and architecture guide to structured exception handling, exception hierarchy mechanics, bytecode exception tables, fault tolerance patterns, and the Java 9 StackWalker API.

---

## Module Overview

Reliable software architectures distinguish themselves by how they anticipate, isolate, and recover from failures. Java provides structured exception handling built on the `java.lang.Throwable` hierarchy.

This module covers the mechanics of checked vs unchecked exceptions, zero-cost bytecode exception tables, automatic resource cleanup, resilience patterns (Retry, Circuit Breaker), and modern diagnostic tools like `StackWalker` and Helpful NullPointerExceptions.

---

## Document Index

| # | Note | Level | Description | Key Topics |
| :- | :--- | :--- | :--- | :--- |
| 00 | [Exception Hierarchy & Types](./00_exception-hierarchy-and-types.md) | Intermediate | Throwable, Error, Exception, and RuntimeException | Checked vs Unchecked, `fillInStackTrace()` cost, suppressed exceptions |
| 01 | [Try-Catch-Finally Mechanism](./01_try-catch-finally.md) | Intermediate | Execution blocks & bytecode exception tables | Exception tables, multi-catch, `finally` guarantees, return in finally trap |
| 02 | [Throw & Throws](./02_throw-and-throws.md) | Intermediate | Propagation, precise rethrow, translation | `athrow` opcode, stack unwinding, Java 7 precise rethrow, cause chaining |
| 03 | [Try-With-Resources](./03_try-with-resources.md) | Intermediate | Automated resource management (JEP 110) | `AutoCloseable`, reverse-order closing, suppressed exceptions, Java 9 syntax |
| 04 | [Custom Exceptions](./04_custom-exceptions.md) | Intermediate | Domain-driven error modeling | 4 canonical constructors, rich typed properties, checked vs unchecked matrix |
| 05 | [Exception Handling Patterns](./05_exception-handling-patterns.md) | Int-Adv | Fault tolerance patterns and anti-patterns | Retry with backoff/jitter, Circuit Breaker, Fallback, black hole trap |
| 06 | [Stack Traces & Debugging](./06_stack-traces-and-debugging.md) | Int-Adv | Diagnostics, StackWalker, and Helpful NPEs | `StackWalker` (JEP 259), Helpful NPEs (JEP 358), `getCallerClass()`, JIT fast throw |

---

## Conceptual Architecture Map

```
                             ┌──────────────────────────┐
                             │   java.lang.Throwable    │
                             └────────────┬─────────────┘
                                          │
            ┌─────────────────────────────┴─────────────────────────────┐
            ▼                                                           ▼
┌──────────────────────────┐                               ┌──────────────────────────┐
│     java.lang.Error      │                               │   java.lang.Exception    │
├──────────────────────────┤                               ├──────────────────────────┤
│ - Fatal JVM conditions   │                               │ - Recoverable conditions │
│ - OutOfMemoryError       │                               │ - Checked Exceptions     │
│ - StackOverflowError     │                               │   (IOException, etc.)    │
│ - NEVER CATCH!           │                               └────────────┬─────────────┘
└──────────────────────────┘                                            │
                                                                        ▼
                                                           ┌──────────────────────────┐
                                                           │  java.lang.RuntimeException
                                                           ├──────────────────────────┤
                                                           │ - Unchecked              │
                                                           │ - Contract/logic bugs    │
                                                           │ - NullPointerException   │
                                                           │ - Custom Domain Errors   │
                                                           └──────────────────────────┘
```

---

## Learning Objectives Checklist

- [x] Classify failures across `Error`, Checked `Exception`, and Unchecked `RuntimeException`.
- [x] Trace bytecode Exception Table execution and explain zero-cost try blocks.
- [x] Apply multi-catch syntax (`catch (A | B e)`) without inheritance redundancy.
- [x] Eliminate the lethal "return in finally" anti-pattern.
- [x] Implement deterministic cleanup and inspect suppressed exceptions with try-with-resources.
- [x] Author robust custom domain exceptions with all 4 canonical constructors.
- [x] Design fault-tolerant retry loops using exponential backoff and randomized jitter.
- [x] Implement a thread-safe Circuit Breaker state machine (CLOSED $\to$ OPEN $\to$ HALF-OPEN).
- [x] Inspect call stacks lazily using the Java 9 `StackWalker` API in $O(1)$ time.
- [x] Troubleshoot failures using Java 14+ Helpful NullPointerExceptions (JEP 358).
