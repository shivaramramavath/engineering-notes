# Module 13: Advanced Language Features

> Deep dive into Java metadata, runtime introspection, dynamic proxies, Project Panama Foreign Function & Memory API, and bytecode engineering.

---

## Module Overview

This module covers advanced Java features that power enterprise frameworks, low-latency infrastructure, and systems programming. Topics span annotation mechanics, runtime reflection and MethodHandles, dynamic proxies, off-heap memory management via the Foreign Function & Memory (FFM) API (JEP 454), and the standardized Class-File API (JEP 484).

---

## Notes Index

| # | Note | Description | Status |
|---|---|---|---|
| 00 | [00_annotations.md](00_annotations.md) | Standard annotations, meta-annotations, APT processing, dynamic reflection proxies | Complete (400+ lines) |
| 01 | [01_reflection.md](01_reflection.md) | Core reflection, MethodHandles, VarHandle, JPMS encapsulation (--add-opens) | Complete (400+ lines) |
| 02 | [02_dynamic-proxies.md](02_dynamic-proxies.md) | Proxy, InvocationHandler, AOP interception, default method handling, limitations | Complete (400+ lines) |
| 03 | [03_foreign-function-and-memory-api.md](03_foreign-function-and-memory-api.md) | FFM API (JEP 454), Arena, MemorySegment, Linker, Class-File API (JEP 484) | Complete (400+ lines) |

---

## Recommended Learning Path
1. Master metadata design in `00_annotations.md`.
2. Understand runtime introspection and MethodHandles in `01_reflection.md`.
3. Build runtime interception and AOP decorators in `02_dynamic-proxies.md`.
4. Harness off-heap performance and native library interoperability in `03_foreign-function-and-memory-api.md`.
