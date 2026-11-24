# Routing and Wildcards: Express 5 vs Fastify

- **NestJS Version**: 11.x
- **Node.js Baseline**: >= 20.0.0
- **Module**: 02-fundamentals
- **Prerequisites**: [03-controllers](./03-controllers.md), [07-request-data](./07-request-data.md), [00-prerequisites/04-http-and-rest-basics](../00-prerequisites/04-http-and-rest-basics.md)

---

## 1. Context & Purpose

Routing forms the contract between external HTTP clients and application controllers. In NestJS 11, routing mechanics underwent a fundamental shift with the adoption of **Express 5** as the default HTTP engine alongside ongoing support for **Fastify 5**.

Historically in Express 4, route definitions permitted unconstrained wildcard syntax (`*`), arbitrary regular expression fragments, and loose parameter delimiters. Express 5, powered by `path-to-regexp` v8, strictly requires **named wildcards** (e.g., `*filePath`) and rejects unescaped wildcards at route registration time. Fastify uses a radix-tree router (`find-my-way`) with its own distinct parameter parsing constraints. Understanding how route compilation differs between Express 5 and Fastify is essential for building transport-agnostic, bulletproof NestJS applications.

---

## 2. Theoretical Foundations

Routing engines convert string path patterns into compiled matching automata:
- **Express 5 (`path-to-regexp` v8)**: Compiles route strings into regular expressions using an explicit grammar. Wildcards capture arbitrary segments into named keys. Unnamed wildcards (`*`) produce compilation errors.
- **Fastify (`find-my-way`)**: Implements a deterministic **Radix Tree** (prefix tree). Route lookups occur in $O(k)$ time where $k$ is the path length, independent of the total number of registered routes. Wildcards are registered via `*` and accessed via `req.params['*']`.

| Characteristic | Express 5 (`@nestjs/platform-express`) | Fastify 5 (`@nestjs/platform-fastify`) |
|---|---|---|
| Routing Engine | `path-to-regexp` v8 (Regex-based) | `find-my-way` (Radix Tree) |
| Lookup Complexity | $O(N)$ sequential route regex matching | $O(k)$ radix tree traversal |
| Wildcard Syntax | Named: `@Get('files/*path')` | Asterisk: `@Get('files/*')` |
| Optional Params | Curly syntax: `@Get('users{/:id}')` | Query string or distinct routes |
| Regex Matching | Explicit regex delimiters or custom logic | Constrained regex inside param `(:id(\\d+))` |

---

## 3. Architecture & Internal Mechanics

```
Incoming Request URL: /api/v1/assets/images/logo.png
                             │
                             ▼
               NestJS Routing Resolution
                             │
              ┌──────────────┴──────────────┐
              ▼                             ▼
    Express 5 Adapter             Fastify Adapter
   (path-to-regexp v8)            (find-my-way)
  Pattern: /assets/*filePath     Pattern: /assets/*
  Captures:                      Captures:
  params: { filePath:            params: { '*':
    'images/logo.png' }            'images/logo.png' }
              │                             │
              └──────────────┬──────────────┘
                             ▼
             @Param('filePath') or @Param('*')
                             │
                             ▼
                  Controller Route Handler
```

### 3.1 The Express 5 Wildcard Rule
In NestJS 10 and Express 4, `@Get('assets/*')` matched everything following `assets/`.
In NestJS 11 and Express 5:
- `@Get('assets/*')` throws an unhandled error during startup: `TypeError: Unexpected * at ..., expected param name`.
- Every wildcard capture must have a variable name: `@Get('assets/*filePath')`. The captured string is then extracted using `@Param('filePath')`.

---

## 4. Concrete Implementation

### 4.1 Express 5 Routing in NestJS 11

```typescript
// src/assets/assets.controller.ts
import { Controller, Get, Param, NotFoundException } from '@nestjs/common';

@Controller('assets')
export class AssetsController {
  // Correct Express 5 named wildcard
  @Get('*filePath')
  getAsset(@Param('filePath') filePath: string) {
    if (!filePath) {
      throw new NotFoundException('Asset path required');
    }
    return {
      message: 'Serving asset',
      path: filePath,
    };
  }

  // Optional parameter syntax in Express 5
  @Get('users{/:id}')
  getUserOrAll(@Param('id') id?: string) {
    return id ? { user: `User #${id}` } : { users: [] };
  }
}
```

### 4.2 Cross-Platform Compatible Wildcards (Express & Fastify)

When designing libraries or services that must run on both Express and Fastify, abstract the parameter retrieval:

```typescript
// src/common/decorators/wildcard-param.decorator.ts
import { createParamDecorator, ExecutionContext } from '@nestjs/common';

export const WildcardParam = createParamDecorator(
  (defaultKey: string, ctx: ExecutionContext): string => {
    const req = ctx.switchToHttp().getRequest();
    // Fastify stores wildcard under '*'
    // Express 5 stores wildcard under the named identifier
    return req.params[defaultKey] || req.params['*'] || '';
  },
);
```

Controller usage:
```typescript
@Controller('storage')
export class StorageController {
  // Express 5 uses named *key; Fastify tolerates *
  @Get('*objectPath')
  downloadObject(@WildcardParam('objectPath') objectPath: string) {
    return { objectPath };
  }
}
```

---

## 5. Edge Cases & Failure Modes

### 5.1 Literal Asterisk or Colons in URLs
If an API path must match a literal asterisk or colon (e.g., `/search:export` or `/calc/*`), Express 5 treats unescaped colons as parameters.
- **Remedy**: Escape literal characters in Express 5 or handle them via custom middleware.

### 5.2 Trailing Slash Discrepancies
By default, Express 5 treats `/users` and `/users/` identically under default route matching, but Fastify's Radix tree distinguishes them unless `ignoreTrailingSlash: true` is configured in Fastify options during bootstrap:
```typescript
const app = await NestFactory.create<NestFastifyApplication>(
  AppModule,
  new FastifyAdapter({ ignoreTrailingSlash: true }),
);
```

---

## 6. Performance & Memory Implications

- **Radix Tree (Fastify)**: Constant lookup overhead regardless of whether you have 10 routes or 1,000 routes. High suitability for high-throughput microservices.
- **Linear Regex (Express)**: Route matching scans registered handlers in the order they were mounted. Placing hot, high-frequency routes at the top of controller definitions reduces CPU regex evaluation cycles.

---

## 7. Anti-Patterns & Common Mistakes

- **Using Legacy Express 4 `*` in NestJS 11**: Upgrading a repository and leaving `@Get('*')` will crash application bootstrap on Express 5.
- **Overlapping Wildcard Routes with Static Paths**: Defining `@Get('*catchAll')` above `@Get('profile')` in the same controller. In Express, the wildcard matches first, starving the static route.
- **Relying on Non-Standard Parameter Modifiers Across Adapters**: Writing complex regex patterns in route strings that work in Express but throw in Fastify's `find-my-way` router.

---

## 8. Production Checklist & Operational Readiness

- [ ] All wildcard routes use named wildcards (`*paramName`).
- [ ] No bare `@Get('*')` or `@All('*')` exist in controllers.
- [ ] Fastify applications configure `ignoreTrailingSlash: true` if clients send mixed URLs.
- [ ] Route parameters are validated with validation pipes (`ParseUUIDPipe`, `ParseIntPipe`).
- [ ] E2E tests verify wildcard resolution with nested subpaths (e.g., `/assets/sub/dir/file.png`).

---

## 9. Migration & Version History

- **NestJS 8/9/10**: Shipped with Express 4 by default, supporting raw `*` wildcards.
- **NestJS 11.x**: Upgraded to Express 5 by default. Requires named wildcards (`*name`) and new parameter delimiters. Fastify 5 adapter updated in parallel.

---

## 10. Verification & Testing

E2E testing named wildcard routing in NestJS 11:

```typescript
import { Test, TestingModule } from '@nestjs/testing';
import { INestApplication } from '@nestjs/common';
import * as request from 'supertest';
import { AssetsController } from './assets.controller';

describe('Wildcard Routing (e2e)', () => {
  let app: INestApplication;

  beforeAll(async () => {
    const moduleFixture: TestingModule = await Test.createTestingModule({
      controllers: [AssetsController],
    }).compile();

    app = moduleFixture.createNestApplication();
    await app.init();
  });

  afterAll(async () => {
    await app.close();
  });

  it('matches nested paths via named wildcard', () => {
    return request(app.getHttpServer())
      .get('/assets/images/icons/avatar.png')
      .expect(200)
      .expect((res) => {
        expect(res.body.path).toBe('images/icons/avatar.png');
      });
  });
});
```

---

## 11. Comprehensive Examples

### Complete Catch-All Fallback Controller

```typescript
// src/fallback/fallback.controller.ts
import { Controller, All, Param, NotFoundException } from '@nestjs/common';

@Controller()
export class FallbackController {
  // Named wildcard catch-all route placed at the end of the routing registry
  @All('*unmatchedPath')
  handleCatchAll(@Param('unmatchedPath') unmatchedPath: string) {
    throw new NotFoundException({
      statusCode: 404,
      error: 'Not Found',
      message: `The endpoint /${unmatchedPath} does not exist on this server.`,
      timestamp: new Date().toISOString(),
    });
  }
}
```

---

## 12. Cross-References & Further Reading

- [03-controllers.md](./03-controllers.md) - Controller definition and decorators
- [11-platform-express-vs-fastify.md](./11-platform-express-vs-fastify.md) - Choosing between HTTP platform engines
- [01-getting-started/07-nestjs-versions-and-upgrade-guide.md](../01-getting-started/07-nestjs-versions-and-upgrade-guide.md) - Express 5 breaking changes
- [path-to-regexp GitHub Documentation](https://github.com/pillarjs/path-to-regexp)
