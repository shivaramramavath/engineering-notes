# NestJS Versions and Upgrade Guide

- **NestJS Version**: 11.x
- **Node.js Baseline**: >= 20.0.0 (Node.js 22 LTS recommended)
- **Module**: 01-getting-started
- **Prerequisites**: [01-what-is-nestjs](./01-what-is-nestjs.md), [02-first-steps](./02-first-steps.md), [00-prerequisites/05-nodejs-modules-esm-and-cjs](../00-prerequisites/05-nodejs-modules-esm-and-cjs.md)

---

## 1. Context & Purpose

Enterprise backend systems evolve continuously across framework generations. Upgrading major versions of NestJS is critical for accessing modern runtime performance optimizations, security patches, and language features. However, major releases (v8 -> v9 -> v10 -> v11) introduce breaking changes across routing engines (Express 4 to Express 5), caching architectures (`cache-manager` v6 with Keyv), exception hierarchies (`IntrinsicException`), and Node.js engine minimums.

This guide provides a comprehensive, production-grade upgrade roadmap. It covers breaking changes, automated migration tools, configuration refactorings, and step-by-step verification protocols for transitioning legacy applications smoothly to NestJS 11.

---

## 2. Theoretical Foundations

Framework lifecycle management balances backwards compatibility against technical debt. Major version transitions in NestJS follow semantic versioning (SemVer):
- **Major (X.0.0)**: Breaking changes in underlying HTTP adapters, DI resolution order, minimum Node.js runtime engines, or third-party core bindings.
- **Minor (X.Y.0)**: Backwards-compatible feature additions (e.g., new helper pipes, decorators, CLI commands).
- **Patch (X.Y.Z)**: Backwards-compatible bug fixes and internal optimizations.

Understanding the dependency tree of `@nestjs/core`, `@nestjs/common`, and the underlying platform adapter (`@nestjs/platform-express` or `@nestjs/platform-fastify`) prevents dependency drift and conflicting peer dependencies.

---

## 3. Architecture & Internal Mechanics

```
       NestJS 9.x / 10.x                             NestJS 11.x
┌──────────────────────────────┐              ┌──────────────────────────────┐
│       Node.js 16/18+         │              │       Node.js >= 20.0.0      │
├──────────────────────────────┤              ├──────────────────────────────┤
│     Express 4.x Engine       │              │     Express 5.x Engine       │
│  - Unnamed wildcards: '*'    │              │  - Named wildcards: '*path'  │
│  - Raw regex in string paths │              │  - Structured route params   │
├──────────────────────────────┤  Migration   ├──────────────────────────────┤
│    cache-manager v4 / v5     │ ───────────> │      cache-manager v6        │
│  - cache-manager-redis-store │              │  - Keyv unified storage      │
│  - Single/custom store API   │              │  - Multi-backend Keyv stores │
├──────────────────────────────┤              ├──────────────────────────────┤
│     Shutdown Lifecycle       │              │     Shutdown Lifecycle       │
│  - Topological root-first    │              │  - Reversed: dependents first│
├──────────────────────────────┤              ├──────────────────────────────┤
│        Custom Logging        │              │  Native ConsoleLogger JSON   │
│  - Verbose string formatters │              │  - { json: true } built-in   │
└──────────────────────────────┘              └──────────────────────────────┘
```

### 3.1 The Express 5 Transition
NestJS 11 ships with `@nestjs/platform-express` v11, which depends on **Express 5**. Express 5 updates its underlying `path-to-regexp` library:
- Raw wildcard routes like `@Get('files/*')` throw runtime routing errors. Express 5 requires named wildcards: `@Get('files/*path')`.
- RegEx syntax inside path strings requires explicit escaping or strict parameter patterns.
- Optional parameters must use curly braces or explicit modifiers: `@Get('users{/:id}')`.

### 3.2 The Keyv and cache-manager v6 Overhaul
NestJS 11 migrates `@nestjs/cache-manager` to `cache-manager` v6.
- In v6, `cache-manager` replaces fragmented store plugins with **Keyv**.
- Packages like `cache-manager-redis-store` are deprecated and incompatible. Keyv adapters such as `@keyv/redis` or Keyv instances are passed directly.

### 3.3 Reversed Shutdown Hook Execution
In NestJS 10 and earlier, `onApplicationShutdown` and `beforeApplicationShutdown` executed in dependency order. In NestJS 11, shutdown hooks execute in **reverse-topological order**: modules and services that depend on other services are shut down first, preventing active requests from attempting to use already-terminated database pools or message brokers.

---

## 4. Concrete Implementation & Migration Playbooks

### 4.1 Upgrading from NestJS 10 to NestJS 11

#### Step 1: Verify Node.js Engine
Ensure your environment runs Node.js 20 or 22:
```bash
node -v # Must be >= v20.0.0
```
Update your `package.json`:
```json
{
  "engines": {
    "node": ">=20.0.0"
  }
}
```

#### Step 2: Update Core Dependencies
Update all official `@nestjs/*` packages simultaneously to avoid peer dependency mismatches:

```bash
npm install @nestjs/common@^11.0.0 @nestjs/core@^11.0.0 @nestjs/platform-express@^11.0.0 reflect-metadata@^0.2.2 rxjs@^7.8.1
npm install --save-dev @nestjs/cli@^11.0.0 @nestjs/schematics@^11.0.0 @types/node@^22.0.0 typescript@^5.6.0
```

#### Step 3: Refactor Route Wildcards
Scan controllers for raw asterisk routes:

```typescript
// ❌ Deprecated in NestJS 10 (Express 4 style, broken in NestJS 11 / Express 5)
@Controller('assets')
export class AssetsController {
  @Get('*')
  serveAllAssets(@Req() req: Request) {
    return this.assetsService.find(req.url);
  }
}

// ✅ Correct in NestJS 11 (Express 5 named wildcard)
@Controller('assets')
export class AssetsController {
  @Get('*filePath')
  serveAllAssets(@Param('filePath') filePath: string) {
    return this.assetsService.find(filePath);
  }
}
```

#### Step 4: Refactor Caching Configuration
Update `CacheModule` configuration for `cache-manager` v6 and Keyv:

```typescript
// ❌ NestJS 10 legacy cache configuration
import { Module } from '@nestjs/common';
import { CacheModule } from '@nestjs/cache-manager';
import * as redisStore from 'cache-manager-redis-store';

@Module({
  imports: [
    CacheModule.register({
      store: redisStore,
      host: 'localhost',
      port: 6379,
      ttl: 60,
    }),
  ],
})
export class LegacyAppModule {}

// ✅ NestJS 11 Keyv configuration
import { Module } from '@nestjs/common';
import { CacheModule } from '@nestjs/cache-manager';
import { createKeyv } from '@keyv/redis';

@Module({
  imports: [
    CacheModule.registerAsync({
      useFactory: () => ({
        stores: [
          createKeyv('redis://localhost:6379'),
        ],
        ttl: 60000, // cache-manager v6 uses milliseconds for TTL
      }),
    }),
  ],
})
export class ModernAppModule {}
```

#### Step 5: Enable Native JSON Logging
Leverage the built-in structured logger in `main.ts`:

```typescript
import { NestFactory } from '@nestjs/core';
import { ConsoleLogger } from '@nestjs/common';
import { AppModule } from './app.module';

async function bootstrap() {
  const app = await NestFactory.create(AppModule, {
    logger: new ConsoleLogger({
      json: process.env.NODE_ENV === 'production',
      colors: process.env.NODE_ENV !== 'production',
      compact: false,
    }),
  });

  app.enableShutdownHooks();
  await app.listen(process.env.PORT ?? 3000);
}
bootstrap();
```

---

## 5. Edge Cases & Failure Modes

### 5.1 Route Parameter Colons in Query Strings
In Express 5, unescaped colons in route paths or patterns may be treated as route parameter identifiers. If your API path contains literal colons (e.g., `/rpc:call`), escape them properly or use strict path mapping.

### 5.2 Microservice Client Connection Access
When interacting with raw underlying transporter clients (such as NATS or Kafka connections) in NestJS 11, use `.unwrap()` rather than internal private properties:
```typescript
const nativeKafkaClient = clientProxy.unwrap<Kafka>();
```

### 5.3 Exception Filtering Hierarchy
NestJS 11 introduces `IntrinsicException` as an abstract superclass for all core HTTP exceptions. If you have custom global exception filters checking class prototypes:
```typescript
// Ensure your filter handles IntrinsicException and HttpException correctly
if (exception instanceof HttpException) {
  // Handles standard HTTP exceptions
}
```

---

## 6. Performance & Memory Implications

1. **Express 5 Routing Performance**: Route compilation with `path-to-regexp` v8 is faster and performs strict route parsing, preventing memory bloat from unbounded regex caches.
2. **Reverse Shutdown Order**: Clean database connection draining prevents hung TCP sockets and OS-level memory/handle leaks upon SIGTERM signals in Kubernetes pods.
3. **Keyv Caching Pipeline**: Keyv uses unified buffer and serialization pipelines, reducing V8 heap allocations during high-throughput Redis caching cycles.

---

## 7. Anti-Patterns & Common Mistakes

- **Partial Upgrades**: Updating `@nestjs/core` to v11 while keeping `@nestjs/platform-express` at v10. This leads to conflicting runtime contexts and injection failures.
- **Ignoring Express 5 Route Breakages**: Deploying NestJS 11 with `@Get('*')` routes without automated regression tests on wildcard routing endpoints.
- **Using Deprecated Redis Stores**: Retaining `cache-manager-redis-store` causes runtime instantiation errors on startup under `cache-manager` v6.
- **Forgetting `enableShutdownHooks()`**: Failing to enable shutdown hooks prevents the reverse-topological teardown cycle from executing cleanly.

---

## 8. Production Checklist & Operational Readiness

- [ ] Node.js runtime on all target build and container environments is >= 20.0.0.
- [ ] All `@nestjs/*` dependencies in `package.json` are on matching major versions (`^11.0.0`).
- [ ] TypeScript is upgraded to >= 5.6.0.
- [ ] All route wildcards refactored from `*` to `*paramName`.
- [ ] Caching layers refactored to Keyv stores with millisecond TTL values.
- [ ] `app.enableShutdownHooks()` verified in `main.ts`.
- [ ] Integration test suite executed against Express 5 route handlers.

---

## 9. Migration & Version History

| Release | Baseline Node | Key Highlights & Breaking Changes |
|---|---|---|
| **NestJS 8** | Node 12+ / 14+ | RxJS 7 support, upgraded CLI, revised versioning APIs. |
| **NestJS 9** | Node 14+ / 16+ | Fastify 4 support, ConfigModule typed environments, REPL introduced. |
| **NestJS 10** | Node 16+ / 18+ | SWC compiler integration, Vitest compatibility, dropped Node 12/14. |
| **NestJS 11** | Node 20+ / 22+ | Express 5 default, cache-manager v6 / Keyv, reversed shutdown order, `ConsoleLogger` JSON formatting, `IntrinsicException`. |

---

## 10. Verification & Testing

Verify that your upgraded application compiles cleanly and responds to routing correctly:

```typescript
import { Test, TestingModule } from '@nestjs/testing';
import { INestApplication } from '@nestjs/common';
import * as request from 'supertest';
import { AppModule } from './app.module';

describe('NestJS 11 Upgrade Verification (e2e)', () => {
  let app: INestApplication;

  beforeAll(async () => {
    const moduleFixture: TestingModule = await Test.createTestingModule({
      imports: [AppModule],
    }).compile();

    app = moduleFixture.createNestApplication();
    await app.init();
  });

  afterAll(async () => {
    await app.close();
  });

  it('correctly resolves Express 5 named wildcards', () => {
    return request(app.getHttpServer())
      .get('/assets/css/styles.css')
      .expect(200)
      .expect((res) => {
        expect(res.body.resolvedPath).toBe('css/styles.css');
      });
  });
});
```

---

## 11. Comprehensive Examples

### Complete Migration Script
A shell script demonstrating automated verification of package versions before upgrading:

```bash
#!/usr/bin/env bash
set -euo pipefail

echo "=== Verifying Upgrade Compatibility for NestJS 11 ==="

NODE_VER=$(node -v | cut -d'v' -f2 | cut -d'.' -f1)
if [ "$NODE_VER" -lt 20 ]; then
  echo "Error: Node.js version 20 or higher is required. Found: $(node -v)"
  exit 1
fi

echo "Node.js version OK: $(node -v)"

echo "Scanning for invalid Express 4 wildcards in controllers..."
grep -rn "@Get('\*')" src/ || true
grep -rn '@Get("\*")' src/ || true

echo "Upgrade check complete."
```

---

## 12. Cross-References & Further Reading

- [01-what-is-nestjs.md](./01-what-is-nestjs.md) - Architecture and philosophy of NestJS
- [02-fundamentals/10-routing-and-wildcards-express5-vs-fastify.md](../02-fundamentals/10-routing-and-wildcards-express5-vs-fastify.md) - Deep dive into Express 5 routing
- [05-advanced/01-caching/01-cache-manager-basics.md](../05-advanced/01-caching/01-cache-manager-basics.md) - Keyv architecture in NestJS 11
- [Official NestJS 11 Migration Guide](https://docs.nestjs.com/migration-guide)
