# 02 — Fundamentals

This stage teaches the core building blocks of every NestJS application: **modules**, **controllers**, **providers** (services), **dependency injection**, **decorators**, how to **read request data**, how to **shape responses**, and how a **request travels** through the framework. These nine topics are the vocabulary of everything that follows. Guards, pipes, databases, authentication, and microservices are all built from them.

---

## Prerequisites

- [00-prerequisites](../00-prerequisites/README.md): TypeScript classes and decorators, the Node.js event loop, HTTP basics
- [01-getting-started](../01-getting-started/README.md): a working project, the CLI, and the ability to run and debug it

You should have a generated project running before you start, because every file in this stage has exercises that assume one.

---

## Learning Order

```text
01  Architecture Overview     ← the big picture: what the pieces are and why they exist
      │
      ▼
02  Modules                   ← how the application is organized and wired
      │
      ▼
03  Controllers               ← how routes are declared
      │
      ▼
04  Providers and Services    ← where logic lives
      │
      ▼
05  Dependency Injection      ← how pieces get each other
      │
      ▼
06  Decorators                ← the metadata layer behind all of it
      │
      ▼
07  Request Data              ← reading params, query, body, headers, cookies
      │
      ▼
08  Response Handling         ← status codes, headers, redirects, streaming
      │
      ▼
09  Request Lifecycle         ← the full journey, tying everything together
      │
      ▼
03-core-concepts
```

| # | File | What you gain | Approx. time |
|---|---|---|---|
| 01 | [Architecture Overview](./01-architecture-overview.md) | A mental model of the framework, platforms, and application types | 1 h |
| 02 | [Modules](./02-modules.md) | `@Module`, imports/exports, feature and shared modules | 1.5 h |
| 03 | [Controllers](./03-controllers.md) | Routing, method decorators, route conflicts, `@Res()` modes | 1.5 h |
| 04 | [Providers and Services](./04-providers-and-services.md) | `@Injectable`, service design, singleton behavior | 1 h |
| 05 | [Dependency Injection](./05-dependency-injection.md) | How resolution works, tokens, optional deps, the error anatomy | 1.5–2 h |
| 06 | [Decorators](./06-decorators.md) | Catalog of built-in decorators and what reads them | 1 h |
| 07 | [Request Data](./07-request-data.md) | `@Param`, `@Query`, `@Body`, `@Headers`, cookies, conversion | 1.5 h |
| 08 | [Response Handling](./08-response-handling.md) | Standard vs library mode, status codes, headers, streaming | 1.5 h |
| 09 | [Request Lifecycle](./09-request-lifecycle.md) | Order of middleware, guards, interceptors, pipes, filters | 1 h |

Read `01` first, then `02` to `05` in order. Files `06` to `08` can be read in any order after `05`. Read `09` last.

---

## What You Should Be Able to Do After This Stage

1. Draw the module graph of a small application and say which providers each module can see.
2. Build a feature module with a controller, a service, and a DTO, and register it correctly.
3. Explain, without notes, how `constructor(private readonly svc: Service) {}` becomes an injected instance.
4. Fix a `Nest can't resolve dependencies` error in under five minutes.
5. Choose the right decorator to read a route parameter, a query value, a body, a header, and a cookie, and convert values to the right type.
6. Control status codes, headers, redirects, and file responses without breaking Nest's response handling.
7. State the order in which middleware, guards, interceptors, pipes, handlers, and exception filters run, and where each can stop a request.

---

## Hands-On Checklist

Build this small API while you read. By the end of the stage it should exercise every file:

```text
□ Generate a feature:           nest g resource tasks
□ Replace placeholder service methods with an in-memory store         (04)
□ Make the service injectable into a second module via exports        (02, 05)
□ Add GET /tasks?status=done with a converted, typed query value      (07)
□ Add GET /tasks/:id that returns 404 for unknown ids                 (03, 08)
□ Add POST /tasks that returns 201 with a Location header             (08)
□ Add GET /tasks/export that streams a file                           (08)
□ Add a custom metadata decorator and read it in a guard              (06, 09)
□ Log from a middleware, guard, interceptor, and pipe to see the order (09)
```

---

## NestJS 12 Notes That Affect This Stage

| Change in NestJS 12 | Where it appears |
|---|---|
| Opt-in route conflict diagnostics: `routeConflictPolicy` and `routeResolutionStrategy` | [Controllers](./03-controllers.md) |
| `schema` option on `@Body()`, `@Query()`, `@Param()`, `@RawBody()` for Standard Schema libraries (Zod, Valibot, ArkType) | [Request Data](./07-request-data.md) |
| `@QueryMethod()` decorator for the HTTP `QUERY` method | [Controllers](./03-controllers.md), [Decorators](./06-decorators.md) |
| `@Optional()` markers are **no longer inherited** by subclasses | [Dependency Injection](./05-dependency-injection.md) |
| Lifecycle hooks are called by component hierarchy level, which can change hook order | [Modules](./02-modules.md) |
| `errorCode` option on `HttpException` options, serialized into the body | [Response Handling](./08-response-handling.md) |
| `decorator` schematic generates `Reflector.createDecorator()` form | [Decorators](./06-decorators.md) |
| ESM projects: `.js` import extensions, `await bootstrap()` | All code samples |
| Express graceful shutdown now drains in-flight requests | [Request Lifecycle](./09-request-lifecycle.md) |

Code samples in this stage use **ESM style** (relative imports end in `.js`). In a CommonJS project, drop the extensions. Type-only imports use `import type`, classes that Nest must see at runtime use a normal `import`. See [TypeScript Decorators](../00-prerequisites/02-typescript-decorators.md) for why.

---

## How This Stage Connects to the Rest of the Repository

| Concept learned here | Where it goes deeper |
|---|---|
| Modules, feature and shared modules | [Modules and DI](../03-core-concepts/04-modules-and-di/README.md) |
| Providers and custom providers | [Custom Providers](../03-core-concepts/04-modules-and-di/05-custom-providers.md) |
| Dependency injection, scopes | [Scopes and Request Context](../03-core-concepts/04-modules-and-di/07-scopes-and-request-context.md), [DI Internals](../06-internals/02-dependency-injection-internals.md) |
| Controllers | [Request Pipeline](../03-core-concepts/01-request-pipeline/README.md) |
| Decorators | [Custom Decorators](../03-core-concepts/01-request-pipeline/10-custom-decorators.md), [Metadata and Reflection](../06-internals/03-metadata-and-reflection.md) |
| Request data | [Validation and Serialization](../03-core-concepts/02-validation-and-serialization/README.md) |
| Response handling | [HTTP Exceptions](../03-core-concepts/01-request-pipeline/07-http-exceptions.md), [Response and Error Format](../04-intermediate/08-api-design/05-response-and-error-format.md) |
| Request lifecycle | [Pipeline Overview and Execution Order](../03-core-concepts/01-request-pipeline/01-pipeline-overview-and-execution-order.md), [Request Lifecycle Internals](../06-internals/04-request-lifecycle-internals.md) |
| Testing what you build | [Unit Testing](../04-intermediate/01-testing/02-unit-testing.md) |

---

## Next Stage

[03 — Core Concepts](../03-core-concepts/01-request-pipeline/README.md)
