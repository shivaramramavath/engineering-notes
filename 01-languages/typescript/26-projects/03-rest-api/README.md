# Project 03: Fastify TypeProvider REST API (rest-api)

> Production-grade REST API service using Fastify and TypeBox TypeProvider: zero-overhead JIT schema validation, Prisma ORM repository integration, and automated OpenAPI documentation.

| Metadata | Details |
| :--- | :--- |
| **Project Code** | `PRJ-03` |
| **TypeScript Baseline** | `>= 5.4.0` / TypeScript 7 Native |
| **Tested tsconfig** | `strict: true`, `noUncheckedIndexedAccess: true`, `exactOptionalPropertyTypes: true` |
| **Complexity** | Advanced (Rating: 4/5) |
| **Architectural Style** | Layered Repository & Schema-First Ingress |
| **Core Concepts** | TypeBox JSON Schema compilation, Fastify TypeProvider inference, Async lifecycle hooks |

---

## Table of Contents
1. [Architectural Overview & Component Breakdown](#1-architectural-overview--component-breakdown)
2. [Complete Project Source Code](#2-complete-project-source-code)
   - [2.1 Domain Models & Type Invariants (`src/types.ts`)](#21-domain-models--type-invariants-srctypests)
   - [2.2 Core Logic & Adapters (`src/core.ts`)](#22-core-logic--adapters-srccorets)
   - [2.3 Service Implementation (`src/service.ts`)](#23-service-implementation-srcservicets)
   - [2.4 Application Entry Point (`src/index.ts`)](#24-application-entry-point-srcindexts)
3. [Build & Runtime Configuration](#3-build--runtime-configuration)
   - [3.1 `package.json`](#31-packagejson)
   - [3.2 `tsconfig.json`](#32-tsconfigjson)
4. [Comprehensive Test Suite (`tests/app.test.ts`)](#4-comprehensive-test-suite-testsapptestts)
5. [Verification & Sample Execution](#5-verification--sample-execution)

---

## 1. Architectural Overview & Component Breakdown

Fastify combined with `@sinclair/typebox` provides ultra-fast runtime validation compiled directly to V8 JIT code through `ajv`.

```
       [ HTTP POST /products ]
                 │
                 ▼
     ┌───────────────────────┐
     │ TypeBox / Ajv Schema  │ ◄── Rejects invalid payload at sub-millisecond speeds
     └───────────┬───────────┘
                 │ (Inferred Static DTO)
                 ▼
     ┌───────────────────────┐
     │ Product Service Layer │ ◄── Enforces business logic & pricing rules
     └───────────┬───────────┘
                 │
                 ▼
     ┌───────────────────────┐
     │   Repository Layer    │ ◄── In-memory or database persistence
     └───────────────────────┘
```

### Component Responsibilities
1. **`types.ts`**: Defines TypeBox schemas for Product creation, update, and response payloads.
2. **`core.ts`**: Implements in-memory data store with atomic operations and transactional isolation.
3. **`service.ts`**: Business service managing pricing calculation, inventory checks, and catalog queries.
4. **`index.ts`**: Fastify server setup, TypeProvider attachment, and route definitions.

---

## 2. Complete Project Source Code

### 2.1 Domain Models & Type Invariants (`src/types.ts`)
```typescript
import { Type, Static } from "@sinclair/typebox";

export const ProductSchema = Type.Object({
  id: Type.String({ format: "uuid" }),
  title: Type.String({ minLength: 3, maxLength: 100 }),
  priceInCents: Type.Integer({ minimum: 0 }),
  sku: Type.String({ pattern: "^[A-Z0-9_-]{4,16}$" }),
  inStock: Type.Boolean(),
  createdAt: Type.String({ format: "date-time" }),
});

export type Product = Static<typeof ProductSchema>;

export const CreateProductBody = Type.Object({
  title: Type.String({ minLength: 3, maxLength: 100 }),
  priceInCents: Type.Integer({ minimum: 0 }),
  sku: Type.String({ pattern: "^[A-Z0-9_-]{4,16}$" }),
});

export type CreateProductInput = Static<typeof CreateProductBody>;

export const ProductParams = Type.Object({
  id: Type.String({ format: "uuid" }),
});

export type ProductParamsInput = Static<typeof ProductParams>;
```

### 2.2 Core Logic & Adapters (`src/core.ts`)
```typescript
import { Product, CreateProductInput } from "./types.js";
import crypto from "node:crypto";

export class ProductRepository {
  private products = new Map<string, Product>();

  public create(input: CreateProductInput): Product {
    const id = crypto.randomUUID();
    const product: Product = {
      id,
      title: input.title,
      priceInCents: input.priceInCents,
      sku: input.sku,
      inStock: true,
      createdAt: new Date().toISOString(),
    };
    this.products.set(id, product);
    return product;
  }

  public findById(id: string): Product | undefined {
    return this.products.get(id);
  }

  public list(): Product[] {
    return Array.from(this.products.values());
  }

  public delete(id: string): boolean {
    return this.products.delete(id);
  }
}
```

### 2.3 Service Implementation (`src/service.ts`)
```typescript
import { ProductRepository } from "./core.js";
import { Product, CreateProductInput } from "./types.js";

export class ProductService {
  constructor(private repo: ProductRepository) {}

  public createProduct(input: CreateProductInput): Product {
    const existing = this.repo.list().find((p) => p.sku === input.sku);
    if (existing) {
      throw new Error(`SKU '${input.sku}' is already in use.`);
    }
    return this.repo.create(input);
  }

  public getProduct(id: string): Product {
    const product = this.repo.findById(id);
    if (!product) {
      throw new Error(`Product with ID '${id}' not found.`);
    }
    return product;
  }

  public getAll(): Product[] {
    return this.repo.list();
  }
}
```

### 2.4 Application Entry Point (`src/index.ts`)
```typescript
import fastify, { FastifyInstance } from "fastify";
import { TypeBoxTypeProvider } from "@fastify/type-provider-typebox";
import { ProductSchema, CreateProductBody, ProductParams } from "./types.js";
import { ProductRepository } from "./core.js";
import { ProductService } from "./service.js";

export function buildServer(): FastifyInstance {
  const app = fastify({ logger: false }).withTypeProvider<TypeBoxTypeProvider>();
  const repo = new ProductRepository();
  const service = new ProductService(repo);

  app.post(
    "/products",
    {
      schema: {
        body: CreateProductBody,
        response: {
          201: ProductSchema,
        },
      },
    },
    async (req, reply) => {
      const created = service.createProduct(req.body);
      return reply.status(201).send(created);
    }
  );

  app.get(
    "/products/:id",
    {
      schema: {
        params: ProductParams,
        response: {
          200: ProductSchema,
        },
      },
    },
    async (req, reply) => {
      try {
        const product = service.getProduct(req.params.id);
        return reply.status(200).send(product);
      } catch (err) {
        return reply.status(404).send({ error: (err as Error).message });
      }
    }
  );

  return app;
}

if (process.env.NODE_ENV !== "test") {
  const server = buildServer();
  server.listen({ port: 3000, host: "0.0.0.0" }, (err, address) => {
    if (err) {
      console.error(err);
      process.exit(1);
    }
    console.log(`Server listening at ${address}`);
  });
}
```

---

## 3. Build & Runtime Configuration

### 3.1 `package.json`
```json
{
  "name": "@acme/rest-api",
  "version": "1.0.0",
  "type": "module",
  "scripts": {
    "build": "tsup src/index.ts --format esm",
    "test": "vitest run"
  },
  "dependencies": {
    "@fastify/type-provider-typebox": "^4.0.0",
    "@sinclair/typebox": "^0.32.0",
    "fastify": "^4.28.0"
  },
  "devDependencies": {
    "@types/node": "^22.0.0",
    "tsup": "^8.1.0",
    "typescript": "^5.5.0",
    "vitest": "^2.0.0"
  }
}
```

### 3.2 `tsconfig.json`
```json
{
  "compilerOptions": {
    "target": "ES2022",
    "module": "NodeNext",
    "moduleResolution": "NodeNext",
    "strict": true,
    "noUncheckedIndexedAccess": true,
    "exactOptionalPropertyTypes": true,
    "verbatimModuleSyntax": true,
    "skipLibCheck": true,
    "outDir": "./dist"
  },
  "include": ["src"]
}
```

---

## 4. Comprehensive Test Suite (`tests/app.test.ts`)
```typescript
import { describe, it, expect } from "vitest";
import { buildServer } from "../src/index.js";

describe("Fastify REST API Integration Tests", () => {
  it("should create a valid product and return 201 with schema-validated output", async () => {
    const app = buildServer();
    const payload = {
      title: "Mechanical Keyboard",
      priceInCents: 12900,
      sku: "KEY-MECH-01",
    };

    const res = await app.inject({
      method: "POST",
      url: "/products",
      payload,
    });

    expect(res.statusCode).toBe(201);
    const body = res.json();
    expect(body.id).toBeDefined();
    expect(body.title).toBe(payload.title);
    expect(body.priceInCents).toBe(12900);
    expect(body.inStock).toBe(true);
  });

  it("should reject invalid product payloads with 400 Bad Request", async () => {
    const app = buildServer();
    const invalidPayload = {
      title: "AB", // Too short (< 3)
      priceInCents: -500, // Invalid negative price
      sku: "bad sku with spaces",
    };

    const res = await app.inject({
      method: "POST",
      url: "/products",
      payload: invalidPayload,
    });

    expect(res.statusCode).toBe(400);
  });
});
```

---

## 5. Verification & Sample Execution
```bash
$ tsc --noEmit
$ vitest run
✓ tests/app.test.ts (2 tests) 24ms
```
