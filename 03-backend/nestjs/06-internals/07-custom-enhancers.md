---
title: "Custom Enhancers: DiscoveryService and Dynamic Metaprogramming"
description: "Building automated enhancers, dynamic route decorators, and framework-level cross-cutting plugins using NestJS DiscoveryService"
tags: [nestjs, internals, custom-enhancers, discovery-service, metaprogramming]
nestjs_version: "11.x"
node_version: ">=20.0.0"
express_version: "5.x"
---

# Custom Enhancers: DiscoveryService and Dynamic Metaprogramming

## Conceptual Overview
While traditional NestJS applications bind cross-cutting concerns using decorators like `@UseGuards()` or `@UseInterceptors()`, enterprise frameworks often require **declarative auto-discovery**.

Instead of requiring developers to manually register interceptors or middleware on hundreds of routes, you can build **Custom Enhancers** using `@nestjs/core` `DiscoveryService` and `MetadataScanner`.

Examples of automated enhancers include:
- Auto-registering audit logging across any method decorated with `@Audited()`.
- Auto-binding RabbitMQ / Kafka consumers based on method annotations without manual module wiring.
- Auto-injecting distributed tracing and metrics on all database repositories.

```
+-------------------------------------------------------------------------------+
|                       Dynamic Discovery Pipeline                              |
|                                                                               |
| 1. OnApplicationBootstrap                                                     |
|    |                                                                          |
|    v                                                                          |
| 2. discoveryService.getControllers() & getProviders()                         |
|    |                                                                          |
|    v                                                                          |
| 3. metadataScanner.getAllMethodNames(prototype)                               |
|    |                                                                          |
|    v                                                                          |
| 4. reflector.get(CUSTOM_METADATA_KEY, methodRef)                              |
|    |                                                                          |
|    +---> Metadata found?                                                      |
|          YES: Wrap method execution (Proxy / Interception / Register)         |
|          NO:  Skip method                                                     |
+-------------------------------------------------------------------------------+
```

---

## Core Mechanisms & Architecture

### 1. The `DiscoveryModule` Primitives
- `DiscoveryService`: Traverses the internal `NestContainer`, returning wrappers (`InstanceWrapper`) for every registered provider and controller.
- `MetadataScanner`: Iterates through the prototype chain of an object instance, identifying all callable method names.
- `Reflector`: Extracts metadata defined on discovered methods or classes.

---

## Implementation Details & NestJS APIs

### 1. Building an Auto-Registering Metric Enhancer

```typescript
// src/common/decorators/monitored.decorator.ts
import { SetMetadata } from '@nestjs/common';

export const MONITORED_METADATA = 'app:monitored';

export interface MonitoredOptions {
  metricName: string;
}

export const Monitored = (options: MonitoredOptions) =>
  SetMetadata(MONITORED_METADATA, options);
```

```typescript
// src/common/enhancers/monitored-discovery.service.ts
import { Injectable, OnApplicationBootstrap, Logger } from '@nestjs/common';
import { DiscoveryService, MetadataScanner, Reflector } from '@nestjs/core';
import { MONITORED_METADATA, MonitoredOptions } from '../decorators/monitored.decorator';

@Injectable()
export class MonitoredDiscoveryService implements OnApplicationBootstrap {
  private readonly logger = new Logger(MonitoredDiscoveryService.name);

  constructor(
    private readonly discoveryService: DiscoveryService,
    private readonly metadataScanner: MetadataScanner,
    private readonly reflector: Reflector,
  ) {}

  onApplicationBootstrap() {
    const providers = this.discoveryService.getProviders();

    for (const wrapper of providers) {
      const { instance } = wrapper;
      if (!instance || typeof instance !== 'object') continue;

      const prototype = Object.getPrototypeOf(instance);
      const methodNames = this.metadataScanner.getAllMethodNames(prototype);

      for (const methodName of methodNames) {
        const methodRef = instance[methodName];
        const options = this.reflector.get<MonitoredOptions>(
          MONITORED_METADATA,
          methodRef,
        );

        if (options) {
          this.logger.log(`Auto-wrapping method [${wrapper.name}.${methodName}] for metrics [${options.metricName}]`);
          this.wrapMethod(instance, methodName, options);
        }
      }
    }
  }

  private wrapMethod(instance: any, methodName: string, options: MonitoredOptions) {
    const originalMethod = instance[methodName];
    const logger = this.logger;

    instance[methodName] = async function (...args: any[]) {
      const start = performance.now();
      try {
        const result = await originalMethod.apply(this, args);
        const duration = (performance.now() - start).toFixed(2);
        logger.debug(`[Metric: ${options.metricName}] execution time: ${duration}ms`);
        return result;
      } catch (err) {
        logger.error(`[Metric: ${options.metricName}] failed after ${(performance.now() - start).toFixed(2)}ms`);
        throw err;
      }
    };
  }
}
```

```typescript
// src/common/enhancers/enhancer.module.ts
import { Module } from '@nestjs/common';
import { DiscoveryModule } from '@nestjs/core';
import { MonitoredDiscoveryService } from './monitored-discovery.service';

@Module({
  imports: [DiscoveryModule],
  providers: [MonitoredDiscoveryService],
})
export class EnhancerModule {}
```

---

## Production Patterns & Best Practices

### 1. Prototype Wrapping Safety
When wrapping prototype methods dynamically:
- Preserve `this` binding by using `originalMethod.apply(this, args)`.
- Re-attach original metadata so downstream reflection lookups (like Swagger or Guards) continue to function.

---

## Key Takeaways & Cheat Sheet

- Import `DiscoveryModule` from `@nestjs/core` to access `DiscoveryService` and `MetadataScanner`.
- Use `OnApplicationBootstrap` to execute discovery after all singletons are created.
- Scan prototypes using `metadataScanner.getAllMethodNames()`.
- Wrap methods using function proxying to apply cross-cutting behaviors dynamically.

---

## Cross-References
- [03-metadata-and-reflection.md](file:///03-backend/nestjs/06-internals/03-metadata-and-reflection.md) - Reflector and metadata reflection
- [01-how-nest-boots.md](file:///03-backend/nestjs/06-internals/01-how-nest-boots.md) - Bootstrap lifecycle and hooks
