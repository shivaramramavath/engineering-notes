# Module 20: Node.js Backend Typing

> Comprehensive curriculum for TypeScript integration in Node.js backend systems, covering Node 22 native type stripping, environment validation, web frameworks, middleware, databases, caching, authentication, and NestJS.

## Overview
This module explores the typing angle of backend engineering in Node.js. It covers fail-fast architectures, type-safe database queries, request augmentation, and enterprise framework patterns.

## Module Structure

| Note | Title | Description | Baseline |
| :--- | :--- | :--- | :--- |
| **[00-node-types.md](./00-node-types.md)** | Node.js Types & Runtimes | Node 22 native type stripping (`--experimental-strip-types`), Buffer, streams, and globals. | Node 22 LTS |
| **[01-config-and-environment.md](./01-config-and-environment.md)** | Config & Environment | Fail-fast startup validation with Zod, declaration merging for `process.env`. | Node 20/22 LTS |
| **[02-express.md](./02-express.md)** | Web Framework Typing | Express Request/Response generics, Fastify TypeProvider (TypeBox), and Hono Zod routing. | Fastify / Hono |
| **[03-middleware.md](./03-middleware.md)** | Middleware & Augmentation | Request augmentation, `Express.Request` declaration merging, AsyncLocalStorage context. | Node 20/22 LTS |
| **[04-service-and-repository-layers.md](./04-service-and-repository-layers.md)** | Layered Architecture | Separation of Concerns, clean architecture boundaries, DTO mapping, and pure domain logic. | TS 5.0+ |
| **[05-databases.md](./05-databases.md)** | Database Typing | Prisma generated models, Drizzle ORM `$inferSelect`/`$inferInsert`, and Kysely query builders. | Drizzle / Prisma |
| **[06-redis-and-caching.md](./06-redis-and-caching.md)** | Redis & Caching | ioredis typed commands, schema validation on cached JSON, TTL helpers, and distributed locks. | Redis / ioredis |
| **[07-authentication-and-authorization.md](./07-authentication-and-authorization.md)** | Auth & Authorization | JWT claims typing, branded IDs (`UserId` vs `TenantId`), RBAC matrices, and ABAC rules. | Node 20/22 LTS |
| **[08-nestjs.md](./08-nestjs.md)** | NestJS Architecture | Decorators, metadata reflection (`emitDecoratorMetadata`), DTOs, and ExecutionContext. | NestJS v10/v11 |

## Learning Path & Prerequisites
- Complete **[16-type-safe-apis](../16-type-safe-apis/README.md)** and **[19-react-and-frontend](../19-react-and-frontend/README.md)** before this module.
- After completing this module, proceed to **[21-production-tooling](../21-production-tooling/README.md)**.
