# API Versioning: URI, Header, and Media Type

- **NestJS Version**: 11.x
- **Node.js Baseline**: >= 20.0.0
- **Module**: 02-fundamentals
- **Prerequisites**: [03-controllers](./03-controllers.md), [10-routing-and-wildcards-express5-vs-fastify](./10-routing-and-wildcards-express5-vs-fastify.md)

---

## 1. Context & Purpose

Public and enterprise APIs inevitably evolve. Modifying response schemas, renaming fields, or deprecating endpoints without breaking mobile applications, partner integrations, or legacy clients requires structured **API Versioning**.

NestJS provides native, first-class API versioning at both controller and route-handler levels. It supports all standard industry versioning strategies: **URI Versioning** (`/v1/users`), **Header Versioning** (`X-API-Version: 1`), **Media Type Versioning** (`Accept: application/vnd.company.v1+json`), and **Custom Extractor Versioning**, allowing seamless backward compatibility without codebase fragmentation.

---

## 2. Theoretical Foundations

API Versioning contracts define how clients request a specific snapshot of an API's data contracts:

| Strategy | Client Request Mechanism | Pros | Cons |
|---|---|---|---|
| **URI** | Path prefix (`/v1/orders`) | Explicit, bookmarkable, easy CDN/proxy caching | Clutters URL space, violates pure REST identity |
| **Header** | Custom header (`X-Version: 1`) | Clean URLs, keeps resource identity pure | Harder to test in browsers, proxy header stripping |
| **Media Type** | `Accept: application/vnd.api.v1+json` | Pure RESTful content negotiation | Complex client configuration, caching nuances |
| **Query Param** | Query parameter (`/orders?v=1`) | Simple testing in browser | Clutters query string, cache fragmentation |

---

## 3. Architecture & Internal Mechanics

```
Incoming HTTP Request
        │
        ▼
NestJS Application Router
        │
        ├─> Reads app.enableVersioning({ type: ... })
        │
        ├─> Version Extractor resolves version string (e.g. "1" or "2")
        │
        ▼
Route Matching Pipeline
        │
        ├─> Compares extracted version against @Controller({ version: '...' })
        │   and @Get({ version: '...' }) metadata
        │
        ▼
Execute Matching Route Handler (v1 or v2)
```

The version extractor intercepts requests during routing resolution before middleware and guards run. If a route matches the path but fails the version condition, NestJS proceeds to evaluate alternative version candidates or returns a 404.

---

## 4. Concrete Implementation

### 4.1 Enabling Global Versioning

Versioning must be enabled in `main.ts`:

```typescript
// src/main.ts
import { NestFactory } from '@nestjs/core';
import { VersioningType } from '@nestjs/common';
import { AppModule } from './app.module';

async function bootstrap() {
  const app = await NestFactory.create(AppModule);

  // Strategy 1: URI Versioning (most common)
  app.enableVersioning({
    type: VersioningType.URI,
    defaultVersion: '1', // Routes without explicit version default to v1
    prefix: 'v',        // Generates /v1/..., /v2/...
  });

  await app.listen(3000);
}
bootstrap();
```

---

### 4.2 Controller and Route-Level Versioning

```typescript
// src/users/users.controller.ts
import { Controller, Get, Version, VERSION_NEUTRAL } from '@nestjs/common';

// Versioning applied to entire controller
@Controller({
  path: 'users',
  version: '1',
})
export class UsersV1Controller {
  @Get()
  findAll() {
    return [{ id: 1, name: 'Alice (v1 Schema)' }];
  }
}

@Controller({
  path: 'users',
  version: '2',
})
export class UsersV2Controller {
  @Get()
  findAll() {
    return [{ id: 'usr_1', fullName: 'Alice Smith', schemaVersion: 2 }];
  }
}

// Method-level versioning & Neutral endpoints
@Controller('health')
export class HealthController {
  // VERSION_NEUTRAL responds to any version prefix or request with no version
  @Version(VERSION_NEUTRAL)
  @Get()
  checkHealth() {
    return { status: 'healthy' };
  }
}
```

---

### 4.3 Alternative Versioning Strategies

#### Header Versioning
```typescript
app.enableVersioning({
  type: VersioningType.HEADER,
  header: 'X-API-Version',
});
```
Client Request:
```http
GET /users HTTP/1.1
Host: api.example.com
X-API-Version: 2
```

#### Media Type (Accept Header) Versioning
```typescript
app.enableVersioning({
  type: VersioningType.MEDIA_TYPE,
  key: 'v=',
});
```
Client Request:
```http
GET /users HTTP/1.1
Host: api.example.com
Accept: application/json;v=2
```

#### Custom Extractor Versioning
Extract versions from subdomains or custom auth tokens:
```typescript
app.enableVersioning({
  type: VersioningType.CUSTOM,
  extractor: (req: Request) => {
    // e.g., extract from subdomain: v2.api.domain.com
    const host = req.headers.host || '';
    const match = host.match(/^v(\d+)\./);
    return match ? match[1] : '1';
  },
});
```

---

## 5. Edge Cases & Failure Modes

### 5.1 Multiple Version Arrays
A single handler can serve multiple non-contiguous versions:
```typescript
@Version(['1', '2'])
@Get('products')
getProducts() {
  return [];
}
```
If a client requests `/v3/products`, NestJS returns 404 because version 3 is unregistered.

### 5.2 Global Prefix Collision
If `app.setGlobalPrefix('api')` is combined with `type: VersioningType.URI`, the resulting URL path is:
`/api/v1/users`.
Order of registration matters: invoke `setGlobalPrefix` before `enableVersioning`.

---

## 6. Performance & Memory Implications

- **Routing Overhead**: Version checking adds negligible overhead (string comparison against cached metadata keys).
- **Controller Duplication**: Maintaining separate controller classes for v1 and v2 increases class metadata in memory slightly, but completely isolates business logic and prevents regression bugs.

---

## 7. Anti-Patterns & Common Mistakes

- **Hardcoding `/v1` in the Controller Path**: Using `@Controller('v1/users')` manually instead of using framework-level versioning. This breaks `VERSION_NEUTRAL` endpoints, OpenAPI documentation, and flexible version switching.
- **Branching Versions with `if-else` Inside Handlers**: Checking `if (req.headers['x-version'] === '2')` inside a service method. This creates spaghetti code and destroys maintainability.
- **Forgetting `defaultVersion`**: If `defaultVersion` is omitted, unversioned requests to versioned routes return 404.

---

## 8. Production Checklist & Operational Readiness

- [ ] Versioning strategy documented and agreed upon across API consumers.
- [ ] `defaultVersion` configured to preserve compatibility for existing legacy clients.
- [ ] Swagger / OpenAPI document configured with version exploration.
- [ ] Health and metrics endpoints decorated with `VERSION_NEUTRAL`.
- [ ] Deprecation headers (`Sunset`, `Deprecation`) returned on older versions.

---

## 9. Migration & Version History

- **NestJS 8.x**: Introduced native declarative API versioning (`VersioningType`).
- **NestJS 9/10**: Added custom version extractors and array-based multi-version support.
- **NestJS 11.x**: Full integration with Express 5 routing and OpenAPI version selectors.

---

## 10. Verification & Testing

E2E testing multiple API versions:

```typescript
import { Test, TestingModule } from '@nestjs/testing';
import { INestApplication, VersioningType } from '@nestjs/common';
import * as request from 'supertest';
import { UsersV1Controller, UsersV2Controller } from './users.controller';

describe('API Versioning (e2e)', () => {
  let app: INestApplication;

  beforeAll(async () => {
    const moduleFixture: TestingModule = await Test.createTestingModule({
      controllers: [UsersV1Controller, UsersV2Controller],
    }).compile();

    app = moduleFixture.createNestApplication();
    app.enableVersioning({
      type: VersioningType.URI,
      defaultVersion: '1',
    });
    await app.init();
  });

  afterAll(async () => {
    await app.close();
  });

  it('serves v1 response schema on /v1/users', () => {
    return request(app.getHttpServer())
      .get('/v1/users')
      .expect(200)
      .expect((res) => {
        expect(res.body[0].name).toBe('Alice (v1 Schema)');
      });
  });

  it('serves v2 response schema on /v2/users', () => {
    return request(app.getHttpServer())
      .get('/v2/users')
      .expect(200)
      .expect((res) => {
        expect(res.body[0].fullName).toBe('Alice Smith');
      });
  });
});
```

---

## 11. Comprehensive Examples

### Deprecation Interceptor Pattern for Old Versions

```typescript
// src/common/interceptors/deprecation.interceptor.ts
import {
  Injectable,
  NestInterceptor,
  ExecutionContext,
  CallHandler,
} from '@nestjs/common';
import { Observable } from 'rxjs';
import { Response } from 'express';

@Injectable()
export class DeprecationNoticeInterceptor implements NestInterceptor {
  constructor(private readonly sunsetDate: string) {}

  intercept(context: ExecutionContext, next: CallHandler): Observable<any> {
    const res = context.switchToHttp().getResponse<Response>();
    res.setHeader('Deprecation', 'true');
    res.setHeader('Sunset', this.sunsetDate);
    res.setHeader(
      'Link',
      '<https://api.example.com/docs/migration>; rel="sunset"',
    );
    return next.handle();
  }
}
```

---

## 12. Cross-References & Further Reading

- [03-controllers.md](./03-controllers.md) - Controller basics
- [04-intermediate/08-api-design/01-restful-standards.md](../04-intermediate/08-api-design/01-restful-standards.md) - REST API design standards
- [04-intermediate/09-openapi-and-swagger/01-swagger-setup.md](../04-intermediate/09-openapi-and-swagger/01-swagger-setup.md) - Documenting versioned APIs
- [NestJS Official Versioning Guide](https://docs.nestjs.com/techniques/versioning)
