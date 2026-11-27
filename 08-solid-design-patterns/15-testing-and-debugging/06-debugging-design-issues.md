# Debugging Design Issues

| Field | Value |
| :--- | :--- |
| **Category** | Debugging & Diagnostics |

> **TL;DR**: Many nasty production bugs aren't syntax errors—they're **design bugs**: circular DI initialization, Spring/ORM proxy bypasses, shared mutable state across requests, and swallowed async rejections.

## Top 5 Design-Induced Bugs
1. **Self-Invocation Proxy Bypass**: Calling `this.txMethod()` inside a Spring `@Service` bypasses `@Transactional` / `@Cacheable` proxies.
2. **Shared Mutable Singleton State**: Request A mutates a field on a singleton service/bean, corrupting Request B.
3. **Unobserved Rejected Promises / Fire-and-Forget Tasks**: Exceptions vanish silently without `await` or `StructuredTaskScope`.
4. **Circular Dependencies**: `A -> B -> A` fails DI startup or leaves half-initialized fields.
5. **N+1 Lazy-Load Storms**: Serialization triggers 500 hidden SQL queries inside a loop.
