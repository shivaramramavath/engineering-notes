# 13 · Advanced Language Features

Frameworks like Spring, Hibernate, JUnit, Jackson, and Mockito seem to do magic: you add `@Test`, `@Autowired`, or `@Entity` and things happen. This module explains the three language features behind that magic:

- **Annotations**: metadata you attach to code.
- **Reflection**: code that inspects and manipulates other code at runtime.
- **Dynamic proxies**: objects whose behavior is generated at runtime from an interface.

Combined, they let a framework read your annotations, discover your classes, and wrap them with extra behavior (transactions, logging, security) without you writing that plumbing.

## Contents

| # | Note | What you get |
|---|------|--------------|
| 00 | [Annotations](00_annotations.md) | Built-in and custom annotations, retention and targets, reading them, annotation processors |
| 01 | [Reflection](01_reflection.md) | `Class`, `Method`, `Field`, `Constructor`, invoking and instantiating, access rules, cost and risks |
| 02 | [Dynamic Proxies](02_dynamic-proxies.md) | `Proxy` and `InvocationHandler`, AOP-style wrappers, limits, self-invocation trap |

## How they fit together

```text
@Key("server.port")                    ← 00  annotation: declarative metadata
int port();

method.getAnnotation(Key.class)        ← 01  reflection: read the metadata at runtime
method.invoke(target, args)

Proxy.newProxyInstance(...)            ← 02  proxy: intercept every call and apply the metadata
```

The closing example in [Dynamic Proxies](02_dynamic-proxies.md) ties all three together in about 30 lines.

## A word of caution

These are **framework-building tools**. In everyday application code, prefer plain method calls, interfaces, and generics: they are type-checked, fast, refactoring-safe, and easy to debug. Reach for reflection when you are writing a library, a plugin loader, or glue that really can't know types at compile time.

## Prerequisites

[Interfaces](../04-oop/09_interfaces.md), [Generics](../07-generics/README.md) (especially [type erasure](../07-generics/03_type-erasure.md)), [Class Loading](../15-jvm-internals/01_class-loading.md) (helpful), and [Java Modules](../05-packages-and-modules/02_java-modules.md) (for the access rules).

**Next module:** [Concurrency](../14-concurrency/README.md)