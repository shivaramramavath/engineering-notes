# 00 — Prerequisites

NestJS is not a standalone framework you can learn in isolation. It is built on TypeScript classes and decorators, runs on the Node.js event loop, and exists to build HTTP APIs. This stage covers the four foundations that every later stage assumes. If any of them is shaky, NestJS will feel like magic. If they are solid, NestJS will feel obvious.

---

## Who This Stage Is For

| If you... | Then... |
|---|---|
| Know JavaScript but not TypeScript | Read everything, in order |
| Know TypeScript but have never used decorators | Start at `02-typescript-decorators.md` |
| Know Node.js and REST from another framework (Express, Spring, Django) | Take the skip-check below, then read only the gaps |
| Are an experienced TypeScript/Node developer | Skim `02-typescript-decorators.md` (the DI and metadata sections), then go to [01-getting-started](../01-getting-started/README.md) |

**Assumed knowledge before this stage:** basic JavaScript (variables, functions, arrays/objects, `let`/`const`, arrow functions, destructuring, `import`/`export`, callbacks). If you cannot read a `.map()` / `.filter()` chain comfortably, learn modern JavaScript first.

---

## Learning Order

```text
JavaScript basics
      │
      ▼
01  TypeScript Essentials          ← the language NestJS is written in
      │
      ▼
02  TypeScript Decorators          ← the mechanism NestJS is built on
      │
      ▼
03  Node.js Async & Event Loop     ← the runtime your code executes in
      │
      ▼
04  HTTP & REST Basics             ← the protocol your code speaks
      │
      ▼
01-getting-started
```

Files `03` and `04` do not depend on `01` and `02`. You can read them in any order, but `02` requires `01`.

| # | File | What you gain | Approx. time |
|---|---|---|---|
| 01 | [TypeScript Essentials](./01-typescript-essentials.md) | Types, interfaces, generics, classes, modules, `tsconfig.json` | 3–4 h |
| 02 | [TypeScript Decorators](./02-typescript-decorators.md) | How decorators work, `reflect-metadata`, how a DI container uses them | 2–3 h |
| 03 | [Node.js Async & Event Loop](./03-nodejs-async-and-event-loop.md) | Promises, `async`/`await`, event loop phases, streams, blocking pitfalls | 3 h |
| 04 | [HTTP & REST Basics](./04-http-and-rest-basics.md) | Methods, status codes, headers, cookies, CORS, REST vocabulary | 2–3 h |

Time estimates assume you type the examples yourself. Reading alone is faster, but retention is much lower.

---

## Skip-Check

Answer these without looking anything up. If you can answer a group confidently, skip that file. If not, read it.

### TypeScript (`01`)

- What is the difference between `interface` and `type`, and when can you not use an `interface`?
- Why does `unknown` exist when `any` exists?
- What does `constructor(private readonly svc: Service) {}` do?
- Why can a `class` be used as a value but an `interface` cannot?
- What does `strict: true` enable?

### Decorators (`02`)

- What arguments does a method decorator receive?
- In what order are stacked decorators evaluated and then applied?
- What is `emitDecoratorMetadata`, and what does it emit?
- Why does `constructor(private repo: UserRepository)` work for injection when `UserRepository` is a class but fail when it is an interface?

### Node.js (`03`)

- What is printed first: `setTimeout(..., 0)`, `Promise.resolve().then(...)`, or `process.nextTick(...)`?
- What happens to other requests while one request runs a 2-second synchronous loop?
- Why does `await` inside `Array.prototype.forEach` not do what people expect?
- What is the difference between `Promise.all` and `Promise.allSettled`?

### HTTP & REST (`04`)

- Which HTTP methods are safe? Which are idempotent? Is `PATCH` idempotent?
- When do you return `401` versus `403`? `400` versus `422`? `404` versus `204`?
- What do `HttpOnly`, `Secure`, and `SameSite` do on a cookie?
- Why does the browser send an `OPTIONS` request before some `fetch` calls?

---

## What You Should Be Able to Do After This Stage

By the end of this stage you should be able to:

1. Write a small TypeScript program with classes, interfaces, generics, and a `tsconfig.json`, and explain what the compiler emits.
2. Write your own decorator, read and write metadata with `reflect-metadata`, and build a tiny dependency-injection container in about 40 lines.
3. Predict the output order of mixed `setTimeout`, `setImmediate`, `Promise`, and `process.nextTick` code, and explain why.
4. Read an HTTP request and response by hand, choose correct methods and status codes, and explain how cookies, CORS, and caching headers behave.

If you can do all four, continue to [01-getting-started](../01-getting-started/README.md).

---

## How This Stage Connects to the Rest of the Repository

| Concept learned here | Where NestJS uses it |
|---|---|
| Classes and parameter properties | Every controller and service constructor |
| Interfaces vs classes (type erasure) | DTOs, injection tokens, [validation](../03-core-concepts/02-validation-and-serialization/README.md) |
| Decorators and metadata | `@Module`, `@Controller`, `@Injectable`, [custom decorators](../03-core-concepts/01-request-pipeline/10-custom-decorators.md) |
| `reflect-metadata` | [DI internals](../06-internals/02-dependency-injection-internals.md), [metadata internals](../06-internals/03-metadata-and-reflection.md) |
| Promises and `async`/`await` | Every route handler, every database call |
| Event loop and blocking | [Performance](../07-production/02-performance/README.md), [background processing](../05-advanced/02-background-processing/README.md) |
| Streams | File upload, `StreamableFile`, SSE |
| HTTP semantics | [Request data](../02-fundamentals/07-request-data.md), [response handling](../02-fundamentals/08-response-handling.md), [API design](../04-intermediate/08-api-design/README.md) |

---

## Conventions Used in This Repository

- Code samples are TypeScript unless marked otherwise.
- Version-specific statements are marked in a **Version / Compatibility Notes** section. Items marked *verify* depend on release details you should confirm against official documentation for your installed version.
- Each file ends with **Interview Questions**, a **Quick Reference**, and **Related Topics**.
- Files contain no NestJS-specific teaching until stage `02`. Examples in this stage use NestJS-flavoured code only to show why the concept matters.

---

## Next Stage

[01 — Getting Started](../01-getting-started/README.md)
