# Functional Programming in Java (`09-functional-java`)

> Comprehensive guide to functional programming in modern Java: lambdas, functional interfaces, method references, Optional monads, streams, collectors, parallel streams, and functional design patterns.

---

## Module Overview

Java 8 introduced a monumental paradigm shift by integrating functional programming into Java's object-oriented foundation. This module provides a rigorous, deep-dive reference for writing declarative, robust, and high-performance functional code in Java (Java 8 through Java 25 LTS).

---

## Notes Directory

| # | Note | Description | Level | Estimated Reading Time |
|---|---|---|---|---|
| 00 | [Lambda Expressions](00_lambda-expressions.md) | Anonymous functional expressions, invokedynamic desugaring, LambdaMetafactory, capturing vs non-capturing closures. | Intermediate | 24 min |
| 01 | [Functional Interfaces](01_functional-interfaces.md) | SAM contract, @FunctionalInterface, java.util.function taxonomy, primitive specializations. | Intermediate | 25 min |
| 02 | [Method References](02_method-references.md) | First-class method pointers: static, bound instance, unbound instance, and constructor references. | Intermediate | 22 min |
| 03 | [Optional Deep Dive](03_optional.md) | Value-based container semantics, avoiding anti-patterns, monadic pipelines, and primitive Optionals. | Intermediate | 24 min |
| 04 | [Streams Fundamentals](04_streams-fundamentals.md) | Stream pipeline architecture, lazy evaluation, loop fusion, consumption lifecycle, and creation sources. | Intermediate | 24 min |
| 05 | [Intermediate & Terminal Stream Operations](05_stream-operations.md) | Stateless vs stateful operations, short-circuiting, and modern Stream Gatherers (Java 22/24 JEP 485). | Advanced | 26 min |
| 06 | [The Collector Architecture](06_collectors.md) | Collector contract (5 components), multi-level groupingBy, partitioningBy, teeing, and custom collectors. | Advanced | 25 min |
| 07 | [Parallel Streams & ForkJoinPool](07_parallel-streams.md) | Work-stealing ForkJoinPool, Spliterator partitioning, Amdahl's Law, and concurrency hazards. | Advanced | 24 min |
| 08 | [Functional Design Patterns](08_functional-patterns.md) | Monads, currying, partial application, Railway Oriented Programming (`Result<T, E>`), and combinators. | Advanced | 25 min |

---

## Core Principles Quick Reference

### The 4 Core Functional Interfaces
1. **`Predicate<T>`**: `boolean test(T t)` - Evaluates condition.
2. **`Function<T, R>`**: `R apply(T t)` - Transforms input to output.
3. **`Consumer<T>`**: `void accept(T t)` - Consumes input with side effects.
4. **`Supplier<T>`**: `T get()` - Generates output with no input.

### The Streams Golden Rules
1. **Never Reuse Streams**: Streams are single-use consumable pipelines.
2. **Beware Stateful Operations**: Avoid `sorted()` or `distinct()` on unbounded streams.
3. **Parallelize Judiciously**: Obey $N \times Q > 10,000$; never run blocking I/O on parallel streams.
4. **Prefer `Stream.toList()`**: Use Java 16+ `.toList()` for concise immutable collection.

---

## Prerequisites
- [04-oop](../04-oop/README.md) - Object-Oriented Programming (Classes, Interfaces, Polymorphism)
- [08-collections](../08-collections/README.md) - Java Collections Framework
