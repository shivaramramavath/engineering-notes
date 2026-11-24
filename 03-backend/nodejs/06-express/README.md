# Module 06: Express.js Framework Architecture

> Building production-grade REST APIs, middleware pipelines, authentication, and error handling with Express 5.0.

---

## Module Overview

Express remains the most battle-tested, widely adopted web framework in the Node.js ecosystem. This module covers core routing, middleware mechanics, controller patterns, validation with Zod, and the breaking changes and performance optimizations introduced in **Express 5.0**.

---

## Contents

| Index | Note | Core Focus |
| :---: | :--- | :--- |
| **01** | [01-setup-and-routing.md](./01-setup-and-routing.md) | Express application setup, router instance, route matching. |
| **02** | [02-middleware.md](./02-middleware.md) | Middleware mechanics, execution order, error-handling middleware. |
| **03** | [03-controllers.md](./03-controllers.md) | Request handling separation, DTO extraction, response formatting. |
| **04** | [04-error-handling.md](./04-error-handling.md) | Express 5 automatic promise rejection catching, centralized error middleware. |
| **05** | [05-validation.md](./05-validation.md) | Schema validation with Zod, request sanitization, typed middleware. |
| **06** | [06-auth-and-authorization.md](./06-auth-and-authorization.md) | Bearer token authentication, JWT verification, RBAC guard middleware. |
| **07** | [07-file-upload.md](./07-file-upload.md) | Multipart file handling with `multer`, memory storage vs streaming. |
| **08** | [08-http-errors-and-problem-details.md](./08-http-errors-and-problem-details.md) | RFC 7807 Problem Details for HTTP APIs, standardized error responses. |
| **09** | [09-express-5-migration-and-internals.md](./09-express-5-migration-and-internals.md) | Express 5 breaking changes, `path-to-regexp` v8, removed methods. |
| **10** | [10-express-security-middleware-and-clustering.md](./10-express-security-middleware-and-clustering.md) | Helmet security headers, rate limiting, and multi-core clustering. |

---

## Next Steps
Explore modern high-performance alternative frameworks:
- [06b-fastify](../06b-fastify/README.md) for schema-driven, ultra-low-overhead APIs.
- [06c-hono-and-edge-runtimes](../06c-hono-and-edge-runtimes/README.md) for Web Standard APIs across edge and Node.js runtimes.
- [06d-koa-and-framework-landscape](../06d-koa-and-framework-landscape/README.md) for the Koa onion model and ecosystem benchmarks.
