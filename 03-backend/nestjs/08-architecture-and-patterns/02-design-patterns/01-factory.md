---
title: "The Factory Pattern in NestJS"
description: "Implementing Factory Method, Abstract Factory, dynamic useFactory providers, and ConfigurableModuleBuilder in NestJS"
tags: [nestjs, design-patterns, factory, use-factory, configurable-module-builder]
nestjs_version: "11.x"
node_version: ">=20.0.0"
express_version: "5.x"
---

# The Factory Pattern in NestJS

## Conceptual Overview
The Factory pattern abstracts object instantiation, allowing client code to request an object without coupling to its concrete constructor.

In NestJS, the Factory pattern is ubiquitous:
- **`useFactory` Providers**: Dynamic dependency instantiation with asynchronous initialization.
- **Factory Services**: Domain factories that construct complex aggregates or domain entities.
- **`ConfigurableModuleBuilder`**: Metaprogramming factory that generates standard dynamic module boilerplate (`forRoot`, `registerAsync`).

---

## Implementation Details & NestJS APIs

### 1. Dynamic Factory Provider with Async Resolution

```typescript
// src/database/connection.factory.ts
import { Module } from '@nestjs/common';
import { ConfigService } from '@nestjs/config';

export const DB_POOL = Symbol('DB_POOL');

@Module({
  providers: [
    {
      provide: DB_POOL,
      useFactory: async (config: ConfigService) => {
        const pool = await createConnectionPool(config.getOrThrow('DATABASE_URL'));
        return pool;
      },
      inject: [ConfigService],
    },
  ],
  exports: [DB_POOL],
})
export class DatabaseModule {}
```

---

## Key Takeaways & Cheat Sheet

- Use `useFactory` when dependency initialization requires asynchronous I/O or dynamic configuration.
- Use Domain Factories to enforce creation invariants for complex Aggregates.

---

## Cross-References
- [05-configurable-module-builder.md](../../03-core-concepts/04-modules-and-di/05-configurable-module-builder.md) - Configurable module builder
- [02-strategy.md](./02-strategy.md) - Strategy pattern
