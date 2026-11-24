# OpenAPI-First Development & SDK Generation in Node.js

> Contract-first backend engineering: Writing OpenAPI 3.1 specifications, runtime request validation with `@scalar/openapi-parser`, and generating type-safe client SDKs with openapi-typescript.

| Metadata | Details |
| :--- | :--- |
| **Node.js Baseline** | Node.js 22 LTS / 24+ |
| **Specifications** | OpenAPI 3.1 / Swagger |
| **Tooling** | `openapi-typescript`, `@scalar/hono-api-reference`, Orval |

---

## 1. Why OpenAPI-First?

In code-first development, developers write route handlers and generate Swagger annotations as an afterthought, leading to out-of-sync documentation.
**In OpenAPI-First Development**:
1. Teams agree on the API contract (`openapi.yaml`) before writing code.
2. The contract generates server-side route interfaces and client SDKs automatically.
3. Automated tools validate incoming HTTP requests and outgoing responses against the specification at runtime.

---

## 2. Generating TypeScript Interfaces with `openapi-typescript`

```bash
npx openapi-typescript ./openapi.yaml -o ./src/api-types.ts
```

Consuming generated schemas in backend controllers:
```typescript
import type { paths } from './api-types';

type GetUserResponse = paths['/users/{id}']['get']['responses']['200']['content']['application/json'];

export async function getUserHandler(userId: string): Promise<GetUserResponse> {
  return { id: userId, email: 'user@example.com' };
}
```

---

## 3. Serving Interactive API Documentation

Using modern interactive documentation engines like `@scalar/api-reference`:

```javascript
import { apiReference } from '@scalar/express-api-reference';

app.use(
  '/reference',
  apiReference({
    spec: { url: '/openapi.json' },
    theme: 'purple',
  })
);
```
Interactive testing, curl commands, and request generators are served directly from the backend.
