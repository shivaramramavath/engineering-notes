# Premature Optimisation & Speculative Abstraction

| Field | Value |
| :--- | :--- |
| **Category** | Anti-Pattern (Complexity Bloat) |
| **Origin** | Donald Knuth (*"Premature optimization is the root of all evil"*) |
| **Violates** | [KISS & YAGNI](file:///E:/Learning/engineering-notes/08-solid-design-patterns/03-design-principles/02-kiss-and-yagni.md) |

> **TL;DR**: Inventing `AbstractGenericEventDispatcherFactory` for 1 implementation "just in case", or hand-rolling unreadable bit-hacks before profiling, destroys readability and slows down every real change.

## Rules
1. **Make it work → Make it clean → Profile → Optimize the actual bottleneck**.
2. Wait for **3 concrete variations** (Rule of Three) before introducing a generic plugin framework.
