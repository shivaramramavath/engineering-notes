---
title: "Top 20 NestJS Anti-Patterns and Common Mistakes"
description: "Identifying and fixing the most pervasive mistakes in NestJS: request-scope bubble up, circular dependency abuse, and unhandled rejections"
tags: [nestjs, anti-patterns, common-mistakes, pitfalls, troubleshooting]
nestjs_version: "11.x"
node_version: ">=20.0.0"
express_version: "5.x"
---

# Top 20 NestJS Anti-Patterns and Common Mistakes

## 1. Accidental Request-Scope "Bubble Up"
- **Mistake**: Making an audit logging service `Scope.REQUEST` to read client IP, then injecting it into a Singleton database repository.
- **Impact**: The repository and its dependent controllers become Request-scoped, causing 180+ allocations per request and crippling throughput.
- **Fix**: Use Node.js `AsyncLocalStorage` to store request context without modifying provider scopes.

## 2. Using `forwardRef()` as an Architecture Crutch
- **Mistake**: Using `forwardRef()` across dozens of modules to silence circular dependency compiler errors.
- **Fix**: Extract shared logic into a common third module or communicate via event emitters.

## 3. Direct Manipulation of Express `@Res()`
- **Mistake**: Calling `@Res() res: Response; res.send(...)` in controllers.
- **Impact**: Disables NestJS interceptors, serializers, and HTTP status code annotations.
- **Fix**: Use `@Res({ passthrough: true })` or return plain values/observables.

## 4. Swallowing Exceptions in Services
- **Mistake**: Wrapping service code in `try { ... } catch (e) { console.log(e); }`.
- **Impact**: Returns HTTP 200 with `undefined` to clients; hides critical bugs from monitoring.
- **Fix**: Re-throw domain exceptions or allow them to bubble up to NestJS Exception Filters.

## 5. Synchronous Work on Main Event Loop
- **Mistake**: Running `bcrypt.hashSync`, `fs.readFileSync`, or heavy regex on the main V8 thread.
- **Impact**: Freezes the server for all concurrent users.
- **Fix**: Use asynchronous methods (`bcrypt.hash`) or offload to Worker Threads.

---

## Key Takeaways & Cheat Sheet

- Never use `Scope.REQUEST` without measuring GC overhead; prefer `AsyncLocalStorage`.
- Avoid `@Res()` without `{ passthrough: true }`.
- Never execute synchronous CPU-intensive tasks on the event loop.
