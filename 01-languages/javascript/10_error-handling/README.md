# Module 10: JavaScript Error Handling & Fault-Tolerant Architecture

> Authoritative curriculum and deep-dive technical notes on ECMAScript error architecture, the built-in error taxonomy, `try...catch...finally` mechanics, custom error subclassing, async error propagation, and production APM resilience.

## Module Overview

Reliable software engineering is defined by how applications behave when unexpected failures occur. This module covers error mechanics from low-level V8 stack frame capture to distributed production resilience:

1. **The Error Taxonomy & Chaining:** The 7 native error subclasses (`TypeError`, `RangeError`, `ReferenceError`, etc.), causal error chaining via `Error.cause` (ES2022), and multi-error bundling with `AggregateError` (ES2021).
2. **Structured Control Flow (`try...catch...finally`):** Call stack unwinding, optional catch binding (ES2019), and avoiding the infamous `finally` return override trap.
3. **Custom Error Architectures:** Subclassing `Error` with clean prototype inheritance, V8 stack trace trimming via `Error.captureStackTrace`, and RFC 7807 Problem Details serialization.
4. **Error Propagation & Crash Guards:** Event loop boundaries, Promise rejection cascades, unhandled rejection lifecycles (`unhandledRejection`, `uncaughtException`), and graceful shutdown pipelines.
5. **Production Error Engineering:** Operational vs programmer errors, centralized Express/Fastify error middleware, PII/secret redaction, circuit breakers, and 12-factor JSON telemetry logging.

---

## Curriculum & Reading Order

| Note | Title | Level | Reading Time | Topics Covered |
| :--- | :--- | :--- | :--- | :--- |
| **[01](01_errors.md)** | [Error Objects & Hierarchy](01_errors.md) | Core | 24 min | Error taxonomy, Error.cause chaining, AggregateError, V8 stack frame capture |
| **[02](02_try-catch-finally.md)** | [try...catch...finally Semantics](02_try-catch-finally.md) | Core | 25 min | Stack unwinding, optional catch binding, finally return override trap, V8 HandlerTable |
| **[03](03_custom-errors.md)** | [Custom Error Classes](03_custom-errors.md) | Core | 26 min | Subclassing Error, Error.captureStackTrace, RFC 7807 Problem Details |
| **[04](04_error-propagation.md)** | [Error Propagation & Process Guards](04_error-propagation.md) | Core | 26 min | Synchronous vs async bubbling, Promise rejections, unhandledRejection, graceful shutdown |
| **[05](05_production-error-handling.md)** | [Production Resilience & Observability](05_production-error-handling.md) | Advanced | 28 min | Operational vs programmer errors, PII redaction, circuit breakers, structured JSON logs |

---

## Cross-Module References

- **Execution Contexts & Call Stack:** Covered in [04_scope-and-execution/03_execution-context-and-call-stack.md](../04_scope-and-execution/03_execution-context-and-call-stack.md).
- **Class Syntax & Prototype Chains:** Covered in [05_this-and-oop/05_classes.md](../05_this-and-oop/05_classes.md).
- **Asynchronous Flow (Promises, `async`/`await`):** Covered in [11_asynchronous-javascript/](../11_asynchronous-javascript/).
- **Event Loop & Microtask Queues:** Covered in [12_event-loop/](../12_event-loop/).
- **Node.js Process Architecture:** Covered in [16_nodejs/](../16_nodejs/).
