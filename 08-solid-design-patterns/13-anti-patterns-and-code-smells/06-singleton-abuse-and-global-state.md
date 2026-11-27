# Singleton Abuse & Global State

| Field | Value |
| :--- | :--- |
| **Category** | Anti-Pattern (Hidden Coupling) |
| **Related** | [Singleton](file:///E:/Learning/engineering-notes/08-solid-design-patterns/05-creational-patterns/01-singleton.md), [Dependency Injection](file:///E:/Learning/engineering-notes/08-solid-design-patterns/04-patterns-intro/03-dependency-injection.md) |
| **Fix** | Constructor / Functional Dependency Injection |

> **TL;DR**: Using `Singleton.getInstance()` everywhere is just **object-oriented global mutable state**—hiding dependencies from constructors, breaking parallel unit tests, and locking you into one global implementation.

## Fix
Let your **Composition Root / DI Container** manage a single shared instance (`@Singleton` scope) and inject it via constructor/function parameters.
