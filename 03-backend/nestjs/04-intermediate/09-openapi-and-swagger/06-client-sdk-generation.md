# Client SDK Generation from OpenAPI Specifications

- **NestJS Version**: 11.x
- **Node.js Baseline**: >= 20.0.0
- **Module**: 04-intermediate / 09-openapi-and-swagger
- **Prerequisites**: [01-swagger-setup](./01-swagger-setup.md), [02-decorators-and-dtos](./02-decorators-and-dtos.md), [00-prerequisites/01-typescript-essentials](../../00-prerequisites/01-typescript-essentials.md)

---

## 1. Context & Purpose

Maintaining frontend API clients (React, Next.js, React Native) or inter-service API wrappers by hand is slow and error-prone. Hand-crafted API clients frequently fall out of sync with backend DTOs, introducing silent runtime bugs when backend contracts change.

By combining `@nestjs/swagger` with modern OpenAPI code generators—such as **openapi-typescript**, **orval**, or **openapi-generator-cli**—engineering teams can automate end-to-end type safety. Every commit on the backend automatically produces strictly typed TypeScript fetch/Axios clients, React Query hooks, and request/response interfaces, eliminating manual client wiring.

---

## 2. Theoretical Foundations

Contract-Driven Development treats the OpenAPI specification (`swagger.json`) as the single source of truth:
1. **Source of Truth**: NestJS decorators (`@ApiProperty`, `@ApiResponse`) document the canonical data contracts.
2. **Spec Generation**: NestJS CLI swagger plugin exports the compiled OpenAPI 3.0/3.1 document.
3. **AST Code Generation**: Code generators read the JSON schema definitions and emit:
   - TypeScript `type` and `interface` models.
   - HTTP clients (using native `fetch` or Axios).
   - Reactive cache hooks (TanStack / React Query, SWR).

---

## 3. Architecture & Internal Mechanics

```
NestJS Backend (Source of Truth)
Controllers, DTOs, @ApiProperty()
              │
              ▼
   npm run swagger:export
              │
              ▼
   openapi-spec.json (OpenAPI 3.1)
              │
    ┌─────────┴─────────┐
    ▼                   ▼
orval / openapi-ts   openapi-generator-cli
    │                   │
    ▼                   ▼
TypeScript SDK       Java / Python / Go SDK
- Exact DTO Types    - Client Classes
- Typed Fetch Client - Data Models
- TanStack Query     - Serializers
    │                   │
    ▼                   ▼
Frontend Applications / Microservice Consumers
```

---

## 4. Concrete Implementation

### 4.1 Exporting the OpenAPI Spec via CLI Script

Create a standalone script that compiles the NestJS module context and writes `openapi.json` to disk without running an HTTP listener:

```typescript
// src/scripts/generate-openapi.ts
import { NestFactory } from '@nestjs/core';
import { SwaggerModule, DocumentBuilder } from '@nestjs/swagger';
import * as fs from 'fs';
import * as path from 'path';
import { AppModule } from '../app.module';

async function generateSpec() {
  const app = await NestFactory.create(AppModule, { logger: false });

  const config = new DocumentBuilder()
    .setTitle('Enterprise API')
    .setDescription('Auto-generated OpenAPI client specification')
    .setVersion('1.0.0')
    .addBearerAuth()
    .build();

  const document = SwaggerModule.createDocument(app, config);

  const outputPath = path.resolve(process.cwd(), 'openapi.json');
  fs.writeFileSync(outputPath, JSON.stringify(document, null, 2));

  console.log(`OpenAPI specification generated successfully at: ${outputPath}`);
  await app.close();
  process.exit(0);
}

generateSpec();
```

Add an export script to `package.json`:
```json
{
  "scripts": {
    "openapi:generate": "ts-node src/scripts/generate-openapi.ts"
  }
}
```

---

### 4.2 Automated Client SDK Generation with Orval

**Orval** is a TypeScript-first OpenAPI code generator that outputs clean types and TanStack Query hooks.

Install Orval:
```bash
npm install --save-dev orval
```

Create `orval.config.ts`:
```typescript
// orval.config.ts
import { defineConfig } from 'orval';

export default defineConfig({
  apiClient: {
    input: {
      target: './openapi.json',
    },
    output: {
      mode: 'split',
      target: './src/generated/api.ts',
      schemas: './src/generated/model',
      client: 'fetch', // Or 'axios', 'react-query', 'swr'
      baseUrl: 'https://api.example.com',
      override: {
        mutator: {
          path: './src/lib/custom-fetch.ts',
          name: 'customFetch',
        },
      },
    },
  },
});
```

Run code generation:
```bash
npx orval
```

---

### 4.3 Custom Fetch Mutator (Handling Bearer Tokens & Base URL)

```typescript
// src/lib/custom-fetch.ts
export async function customFetch<T>(url: string, options: RequestInit): Promise<T> {
  const token = typeof window !== 'undefined' ? localStorage.getItem('token') : null;

  const headers = new Headers(options.headers);
  if (token) {
    headers.set('Authorization', `Bearer ${token}`);
  }
  headers.set('Content-Type', 'application/json');

  const response = await fetch(url, { ...options, headers });

  if (!response.ok) {
    const errorBody = await response.json().catch(() => ({}));
    throw new Error(errorBody.message || `HTTP Error ${response.status}`);
  }

  return response.json();
}
```

---

### 4.4 Consuming Generated Types and Functions in Client Code

```typescript
// client/app.ts
import { getUsers, postUsers } from './generated/api';
import { CreateUserDto } from './generated/model';

async function main() {
  // Completely type-safe input DTO and return response!
  const newUser: CreateUserDto = {
    email: 'dev@example.com',
    name: 'Developer',
  };

  const created = await postUsers(newUser);
  console.log('Created user ID:', created.id);

  const allUsers = await getUsers();
  allUsers.forEach((u) => console.log(u.email));
}
```

---

## 5. Edge Cases & Failure Modes

### 5.1 Missing `operationId`
Code generators name generated client functions using the route's `operationId`. If `@ApiOperation({ operationId: 'getUsers' })` is missing, generators fall back to synthesizing names from paths (e.g. `usersControllerFindAll`), creating ugly, unstable client function names.
- **Remedy**: Configure `operationIdFactory` in Swagger configuration:
```typescript
const document = SwaggerModule.createDocument(app, config, {
  operationIdFactory: (controllerKey: string, methodKey: string) => methodKey,
});
```

### 5.2 Unspecified DTO Types Resulting in `any`
If DTO properties lack `@ApiProperty()` or the Nest CLI swagger plugin is disabled, fields emit as untyped objects, generating client types with `unknown` or `any`.
- **Remedy**: Always enable `@nestjs/swagger` compiler plugin in `nest-cli.json`:
```json
{
  "compilerOptions": {
    "plugins": ["@nestjs/swagger"]
  }
}
```

---

## 6. Performance & Memory Implications

- **Build Time**: Exporting the spec and running Orval completes in < 3 seconds.
- **Bundle Size**: Client generators emitting tree-shakeable functions produce lightweight bundles (typically 5–15KB), compared to heavy monolithic SDK packages.

---

## 7. Anti-Patterns & Common Mistakes

- **Committing Out-of-Sync Client Code**: Updating backend DTOs without regenerating client SDKs, breaking production deployments. Add a CI check: `npm run openapi:generate && git diff --exit-code`.
- **Exposing Internal Backend Entities in Public SDK**: Documenting database entities (with password hashes or internal foreign keys) directly in `@ApiResponse` instead of clean response DTOs.
- **Manually Editing Generated SDK Files**: Making manual changes inside `src/generated/`; any rerun of the generator silently obliterates those edits.

---

## 8. Production Checklist & Operational Readiness

- [ ] Swagger CLI plugin enabled in `nest-cli.json` to auto-generate schema types.
- [ ] `operationIdFactory` configured for clean function naming.
- [ ] CI pipeline runs specification generation and verifies no unstaged client diffs exist.
- [ ] Generated SDK published to internal npm registry or monorepo package workspace.
- [ ] Multi-tenant base URLs and authentication tokens configurable at client runtime.

---

## 9. Migration & Version History

- **OpenAPI 3.0 -> 3.1**: Modern generators support OpenAPI 3.1 with standard JSON Schema draft 2020-12 keywords (`nullable` replaced by `type: ['string', 'null']`).
- **NestJS 11.x**: `@nestjs/swagger` v11 fully supports OpenAPI 3.1 type definitions and Express 5 route introspection.

---

## 10. Verification & Testing

Verifying the exported specification in a test runner:

```typescript
import { Test } from '@nestjs/testing';
import { SwaggerModule, DocumentBuilder } from '@nestjs/swagger';
import { AppModule } from '../src/app.module';

describe('OpenAPI Generation Spec', () => {
  it('generates valid OpenAPI specification with valid paths and schemas', async () => {
    const app = await Test.createTestingModule({
      imports: [AppModule],
    }).compile();

    const nestApp = app.createNestApplication();
    const config = new DocumentBuilder().setTitle('Test API').build();
    const doc = SwaggerModule.createDocument(nestApp, config);

    expect(doc.openapi).toMatch(/^3\./);
    expect(doc.paths).toBeDefined();
    expect(Object.keys(doc.paths).length).toBeGreaterThan(0);
    await nestApp.close();
  });
});
```

---

## 11. Comprehensive Examples

### Automated GitHub Actions CI Workflow for SDK Publication

```yaml
# .github/workflows/generate-sdk.yml
name: Generate & Publish Client SDK

on:
  push:
    branches: [main]

jobs:
  sdk:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4

      - name: Setup Node.js
        uses: actions/setup-node@v4
        with:
          node-version: 22

      - run: npm ci
      - name: Generate OpenAPI Document
        run: npm run openapi:generate

      - name: Generate TypeScript SDK
        run: npx orval

      - name: Verify No Uncommitted Diffs
        run: |
          if [ -n "$(git status --porcelain src/generated)" ]; then
            echo "Client SDK is out of sync with backend DTOs! Run npm run openapi:generate."
            exit 1
          fi
```

---

## 12. Cross-References & Further Reading

- [01-swagger-setup.md](./01-swagger-setup.md) - SwaggerModule configuration
- [02-decorators-and-dtos.md](./02-decorators-and-dtos.md) - Documenting DTOs
- [Orval Documentation](https://orval.dev)
- [OpenAPI Generator Official](https://openapi-generator.tech)
