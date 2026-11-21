# Module 10: Design and Architectural Patterns in C

> Master modular decomposition, object-oriented paradigms in procedural C, robust error reporting architectures, professional style standards, resource lifetime management, and stable ABI library engineering.

| Metadata | Details |
| :--- | :--- |
| **Module Code** | `10-design-and-patterns` |
| **Prerequisites** | [02-core-concepts/03-pointers.md](../02-core-concepts/03-pointers.md), [02-core-concepts/06-structs-unions-enums.md](../02-core-concepts/06-structs-unions-enums.md), [03-intermediate/13-opaque-pointers-and-adts.md](../03-intermediate/13-opaque-pointers-and-adts.md) |
| **Duration** | ~25 hours |
| **Status** | Complete |
| **Standard Baseline**| C99 / C11 / C17 / C23 / POSIX.1-2008 / GCC / Clang / MSVC |

---

## Overview & Learning Outcomes

While C is fundamentally an imperative, procedural programming language without native language constructs for classes, exceptions, or garbage collection, enterprise-grade C systems (such as the Linux kernel, Git, SQLite, PostgreSQL, and Redis) exhibit exceptional modularity, extensibility, and maintainability.

Achieving architectural excellence in C requires deliberate software engineering patterns:
1. **Modular Encapsulation**: Decoupling public API declarations from private implementation structures using incomplete types and opaque pointer patterns (`typedef struct Foo Foo;`).
2. **Object-Oriented Design in C**: Implementing runtime polymorphism via virtual function pointer tables (vtables), single inheritance through struct embedding, and Gang of Four (GoF) patterns (Strategy, Factory, Observer, Iterator).
3. **Enterprise Error Handling**: Building comprehensive error propagation hierarchies using status codes, out-parameters, structured error objects, thread-local error contexts, and the canonical `goto cleanup;` resource unrolling idiom.
4. **Professional Coding Standards**: Adhering to strict naming conventions, namespace prefixes (`lib_module_action`), const-correctness, header inclusion hygiene, and MISRA/Linux kernel formatting disciplines.
5. **Deterministic Resource Management**: Implementing RAII approximations in C using GCC/Clang `__attribute__((cleanup))`, arenas, reference counting (`incref`/`decref`), and paired initialization/destruction contracts.
6. **Robust Public Library API Design**: Architecting shared libraries with long-term Application Binary Interface (ABI) stability, versioned symbol export visibility, forward-compatible structs, and reentrant interfaces.
7. **Explicit Ownership & Lifetimes**: Formalizing memory ownership transfer contracts (caller-allocates vs callee-allocates) and documentation annotations to prevent leaks and use-after-free defects.

---

## Module Table of Contents

| Note | Title | Level | Status | Focus Areas |
| :--- | :--- | :--- | :--- | :--- |
| **01** | [Modular Design in C](./01-modular-design-in-c.md) | Intermediate/Advanced | Complete | Single-responsibility modules, interface headers (`.h`), opaque pointers, loose coupling |
| **02** | [Design Patterns in C](./02-design-patterns-in-c.md) | Advanced | Complete | Strategy, Observer, Factory, Iterator, State Machine, Vtables, Struct Embedding |
| **03** | [Error Handling Patterns](./03-error-handling-patterns.md) | Intermediate/Advanced | Complete | Status codes, out-parameters, `goto cleanup` idiom, structured error objects, `errno` |
| **04** | [Coding Style Guide](./04-coding-style-guide.md) | All Levels | Complete | Naming conventions, namespace prefixing, const-correctness, formatting discipline |
| **05** | [Resource Management Patterns](./05-resource-management-patterns.md) | Advanced | Complete | RAII in C (`__attribute__((cleanup))`), arena allocators, reference counting, dual init/destroy |
| **06** | [API Design for C Libraries](./06-api-design-for-c-libraries.md) | Advanced | Complete | ABI stability, opaque types, forward-compatible structs, symbol visibility, reentrancy |
| **07** | [Ownership and Lifetime Conventions](./07-ownership-and-lifetime-conventions.md) | Advanced | Complete | Ownership transfer semantics, borrow vs own, caller-allocates vs callee-allocates |

---

## Suggested Study Order
1. **Structural Foundation**: Start with [Modular Design in C](./01-modular-design-in-c.md) and [Coding Style Guide](./04-coding-style-guide.md).
2. **Behavioral Patterns**: Study [Design Patterns in C](./02-design-patterns-in-c.md) and [Error Handling Patterns](./03-error-handling-patterns.md).
3. **Systems & API Engineering**: Conclude with [Resource Management Patterns](./05-resource-management-patterns.md), [API Design for C Libraries](./06-api-design-for-c-libraries.md), and [Ownership & Lifetimes](./07-ownership-and-lifetime-conventions.md).

---

[← Previous Module: Performance & Optimization](../09-performance/README.md) | [C Language Track Index](../README.md) | [Next Module: Production Projects](../11-projects/README.md)
