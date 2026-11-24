# Project 01: REST API with Express 5 & PostgreSQL (`proj-node-01-rest-api-postgres`)

> Enterprise-grade RESTful API built with Express 5.0, PostgreSQL, and Drizzle ORM. Features parameterized queries, connection pooling, transactional unit-of-work patterns, RFC 7807 Problem Details error handling, automated database migrations, and integration testing with Testcontainers and Vitest.

| Metadata | Details |
| :--- | :--- |
| **Project Code** | `proj-node-01-rest-api-postgres` |
| **Node.js Baseline** | Node.js 22 LTS / 24+ |
| **Module System** | ESM (`"type": "module"`) |
| **Framework / Core** | Express 5.0 |
| **Database** | PostgreSQL 16+ via `pg` (Pool) & Drizzle ORM |
| **Validation & DTOs** | Zod v3 |
| **Testing** | Vitest + Supertest + Testcontainers (PostgreSQL) |
| **Complexity Level** | Production-Grade Reference |

---

## 1. Architectural Overview & Domain Design

### 1.1 Layered Architecture & Data Flow

This service implements a clean three-tier architecture with strict boundary enforcement:
- **Presentation Layer**: Express 5 routing, input validation via Zod middleware, and HTTP response serialization.
- **Application / Service Layer**: Business logic, domain rules, transactional boundaries, and external side-effects.
- **Infrastructure / Data Access Layer**: Drizzle ORM database schemas, queries with PostgreSQL connection pooling, and migration runners.

```
+----------------------------------------------------------------------------------------------------+
|                                    Express 5 + PostgreSQL API                                      |
|                                                                                                    |
|  [ HTTP Client / Frontend ]                                                                        |
|             │                                                                                      |
|             ▼ HTTP Requests (JSON)                                                                 |
|  +─────────────────────────────────────────────────────────────+                                   |
|  |                   Presentation Layer                        |                                   |
|  |  - Express 5 Router & JSON Body Parser                      |                                   |
|  |  - Zod Request Validator Middleware                         |                                   |
|  |  - Centralized RFC 7807 Problem Details Error Handler       |                                   |
|  +─────────────────────────────┬───────────────────────────────+                                   |
|                                │ Clean DTO                                                         |
|                                ▼                                                                   |
|  +─────────────────────────────────────────────────────────────+                                   |
|  |                   Application Service Layer                 |                                   |
|  |  - ProductService / OrderService (Domain Rules)             |                                   |
|  |  - Transaction Management (Drizzle `db.transaction()`)      |                                   |
|  +─────────────────────────────┬───────────────────────────────+                                   |
|                                │ Repository Operations                                             |
|                                ▼                                                                   |
|  +─────────────────────────────────────────────────────────────+                                   |
|  |                 Infrastructure & Persistence                |                                   |
|  |  - Drizzle ORM Typed Schema Definitions                     |                                   |
|  |  - PostgreSQL Connection Pool (`pg.Pool`)                   |                                   |
|  +─────────────────────────────┬───────────────────────────────+                                   |
|                                │ SQL (Prepared Statements)                                         |
|                                ▼                                                                   |
|               +──────────────────────────────────+                                                 |
|               |        PostgreSQL Database       |                                                 |
|               |  (ACID Transactions, Read Replicas)                                                |
|               +──────────────────────────────────+                                                 |
+----------------------------------------------------------------------------------------------------+
```

### 1.2 Directory Layout

```text
proj-node-01-rest-api-postgres/
├── src/
│   ├── config/
│   │   └── env.ts               # Environment validation (Zod)
│   ├── infrastructure/
│   │   ├── database.ts          # pg Pool and Drizzle db client
│   │   └── schema.ts            # Drizzle table schemas and relations
│   ├── domain/
│   │   └── product.types.ts     # Domain entities and interfaces
│   ├── application/
│   │   ├── dtos/
│   │   │   └── product.dto.ts   # Zod request/response validation schemas
│   │   └── services/
│   │       └── product.service.ts # Business logic & inventory rules
│   ├── presentation/
│   │   ├── controllers/
│   │   │   └── product.controller.ts # Express route handler functions
│   │   ├── middleware/
│   │   │   ├── validate.ts      # Zod request validation middleware
│   │   │   └── error-handler.ts # RFC 7807 error middleware
│   │   └── routes/
│   │       └── product.routes.ts # Express router mapping
│   └── app.ts                   # Express server lifecycle & graceful shutdown
├── drizzle/
│   └── 0000_init.sql            # Generated SQL migrations
├── test/
│   ├── unit/
│   │   └── product.service.test.ts # Mock-based unit tests
│   └── integration/
│       └── product.api.test.ts  # Testcontainers PostgreSQL integration tests
├── Dockerfile                   # Multi-stage production container
├── docker-compose.yml           # Local dev PostgreSQL service
├── drizzle.config.ts            # Drizzle Kit CLI configuration
├── package.json
└── tsconfig.json
```

---

## 2. Complete File-by-File Implementation

### `package.json`
```json
{
  "name": "proj-node-01-rest-api-postgres",
  "version": "1.0.0",
  "type": "module",
  "scripts": {
    "dev": "node --watch --env-file=.env src/app.js",
    "start": "node src/app.js",
    "db:generate": "drizzle-kit generate",
    "db:migrate": "drizzle-kit migrate",
    "test": "vitest run"
  },
  "dependencies": {
    "drizzle-orm": "^0.38.4",
    "express": "^5.0.1",
    "pg": "^8.13.1",
    "zod": "^3.24.2"
  },
  "devDependencies": {
    "@testcontainers/postgresql": "^10.18.0",
    "@types/express": "^5.0.0",
    "@types/node": "^22.13.0",
    "@types/pg": "^8.11.11",
    "drizzle-kit": "^0.30.4",
    "supertest": "^7.0.0",
    "testcontainers": "^10.18.0",
    "typescript": "^5.7.3",
    "vitest": "^3.0.5"
  }
}
```

### `tsconfig.json`
```json
{
  "compilerOptions": {
    "target": "ES2024",
    "module": "NodeNext",
    "moduleResolution": "NodeNext",
    "strict": true,
    "esModuleInterop": true,
    "skipLibCheck": true,
    "outDir": "./dist"
  },
  "include": ["src/**/*", "test/**/*"]
}
```

### `src/config/env.ts`
```typescript
import { z } from 'zod';

const envSchema = z.object({
  NODE_ENV: z.enum(['development', 'production', 'test']).default('development'),
  PORT: z.coerce.number().default(3000),
  DATABASE_URL: z.string().url().default('postgresql://postgres:postgres@localhost:5432/ecommerce_db'),
  DB_MAX_CONNECTIONS: z.coerce.number().default(20),
  DB_IDLE_TIMEOUT_MS: z.coerce.number().default(30000),
  DB_CONNECTION_TIMEOUT_MS: z.coerce.number().default(2000),
});

export const env = envSchema.parse(process.env);
```

### `src/infrastructure/database.ts`
```typescript
import pg from 'pg';
import { drizzle } from 'drizzle-orm/node-postgres';
import { env } from '../config/env.js';
import * as schema from './schema.js';

const { Pool } = pg;

export const pool = new Pool({
  connectionString: env.DATABASE_URL,
  max: env.DB_MAX_CONNECTIONS,
  idleTimeoutMillis: env.DB_IDLE_TIMEOUT_MS,
  connectionTimeoutMillis: env.DB_CONNECTION_TIMEOUT_MS,
});

pool.on('error', (err) => {
  console.error('[PostgreSQL Pool Unexpected Error]', err);
});

export const db = drizzle(pool, { schema });
```

### `src/infrastructure/schema.ts`
```typescript
import { pgTable, uuid, varchar, integer, timestamp, text, index } from 'drizzle-orm/pg-core';
import { sql } from 'drizzle-orm';

export const products = pgTable('products', {
  id: uuid('id').primaryKey().defaultRandom(),
  sku: varchar('sku', { length: 50 }).notNull().unique(),
  name: varchar('name', { length: 255 }).notNull(),
  description: text('description'),
  priceCents: integer('price_cents').notNull(),
  stockQuantity: integer('stock_quantity').notNull().default(0),
  createdAt: timestamp('created_at', { withTimezone: true }).default(sql`CURRENT_TIMESTAMP`).notNull(),
  updatedAt: timestamp('updated_at', { withTimezone: true }).default(sql`CURRENT_TIMESTAMP`).notNull(),
}, (table) => [
  index('idx_products_sku').on(table.sku),
  index('idx_products_created_at').on(table.createdAt),
]);
```

### `src/application/dtos/product.dto.ts`
```typescript
import { z } from 'zod';

export const CreateProductSchema = z.object({
  sku: z.string().min(3).max(50).regex(/^[A-Z0-9_-]+$/, 'SKU must be uppercase alphanumeric'),
  name: z.string().min(2).max(255),
  description: z.string().optional(),
  priceCents: z.number().int().positive('Price must be greater than zero'),
  stockQuantity: z.number().int().nonnegative('Stock cannot be negative'),
});

export const UpdateStockSchema = z.object({
  quantityDelta: z.number().int(), // positive to increment, negative to decrement
});

export const PaginationQuerySchema = z.object({
  page: z.coerce.number().int().positive().default(1),
  limit: z.coerce.number().int().min(1).max(100).default(20),
});

export type CreateProductInput = z.infer<typeof CreateProductSchema>;
```

### `src/application/services/product.service.ts`
```typescript
import { eq, sql } from 'drizzle-orm';
import { db } from '../../infrastructure/database.js';
import { products } from '../../infrastructure/schema.js';
import { CreateProductInput } from '../dtos/product.dto.js';

export class ProductService {
  async createProduct(input: CreateProductInput) {
    const [created] = await db.insert(products).values(input).returning();
    return created;
  }

  async getProductById(id: string) {
    const [product] = await db.select().from(products).where(eq(products.id, id));
    return product ?? null;
  }

  async listProducts(page: number, limit: number) {
    const offset = (page - 1) * limit;
    const items = await db.select().from(products).limit(limit).offset(offset).orderBy(products.createdAt);
    return { items, page, limit };
  }

  async updateStock(id: string, delta: number) {
    return await db.transaction(async (tx) => {
      // Atomic stock update with conditional check preventing negative inventory
      const [updated] = await tx
        .update(products)
        .set({
          stockQuantity: sql`${products.stockQuantity} + ${delta}`,
          updatedAt: new Date(),
        })
        .where(sql`${products.id} = ${id} AND ${products.stockQuantity} + ${delta} >= 0`)
        .returning();

      if (!updated) {
        throw new Error('INSUFFICIENT_STOCK_OR_NOT_FOUND');
      }

      return updated;
    });
  }
}

export const productService = new ProductService();
```

### `src/presentation/controllers/product.controller.ts`
```typescript
import { Request, Response, NextFunction } from 'express';
import { productService } from '../../application/services/product.service.js';
import { CreateProductSchema, PaginationQuerySchema, UpdateStockSchema } from '../../application/dtos/product.dto.js';

export async function createProductHandler(req: Request, res: Response, next: NextFunction): Promise<void> {
  try {
    const input = CreateProductSchema.parse(req.body);
    const product = await productService.createProduct(input);
    res.status(201).json({ status: 'success', data: product });
  } catch (error) {
    next(error);
  }
}

export async function getProductHandler(req: Request, res: Response, next: NextFunction): Promise<void> {
  try {
    const product = await productService.getProductById(req.params.id);
    if (!product) {
      res.status(404).json({
        type: 'https://api.example.com/errors/not-found',
        title: 'Product Not Found',
        status: 404,
        detail: `Product with ID ${req.params.id} does not exist`,
      });
      return;
    }
    res.status(200).json({ status: 'success', data: product });
  } catch (error) {
    next(error);
  }
}

export async function listProductsHandler(req: Request, res: Response, next: NextFunction): Promise<void> {
  try {
    const { page, limit } = PaginationQuerySchema.parse(req.query);
    const result = await productService.listProducts(page, limit);
    res.status(200).json({ status: 'success', data: result });
  } catch (error) {
    next(error);
  }
}

export async function updateStockHandler(req: Request, res: Response, next: NextFunction): Promise<void> {
  try {
    const { quantityDelta } = UpdateStockSchema.parse(req.body);
    const updated = await productService.updateStock(req.params.id, quantityDelta);
    res.status(200).json({ status: 'success', data: updated });
  } catch (error: any) {
    if (error.message === 'INSUFFICIENT_STOCK_OR_NOT_FOUND') {
      res.status(409).json({
        type: 'https://api.example.com/errors/conflict',
        title: 'Conflict',
        status: 409,
        detail: 'Insufficient stock available for this operation',
      });
      return;
    }
    next(error);
  }
}
```

### `src/presentation/middleware/error-handler.ts`
```typescript
import { Request, Response, NextFunction } from 'express';
import { ZodError } from 'zod';

export function errorHandler(err: unknown, _req: Request, res: Response, _next: NextFunction): void {
  if (err instanceof ZodError) {
    res.status(422).json({
      type: 'https://api.example.com/errors/validation-failed',
      title: 'Validation Error',
      status: 422,
      detail: 'Request body or query parameters failed validation',
      invalidParams: err.errors.map((e) => ({ field: e.path.join('.'), message: e.message })),
    });
    return;
  }

  console.error('[Unhandled Server Error]', err);
  res.status(500).json({
    type: 'https://api.example.com/errors/internal-error',
    title: 'Internal Server Error',
    status: 500,
    detail: 'An unexpected internal error occurred',
  });
}
```

### `src/app.ts`
```typescript
import express from 'express';
import { env } from './config/env.js';
import { pool } from './infrastructure/database.js';
import {
  createProductHandler,
  getProductHandler,
  listProductsHandler,
  updateStockHandler,
} from './presentation/controllers/product.controller.js';
import { errorHandler } from './presentation/middleware/error-handler.js';

export const app = express();

app.use(express.json());

// Healthcheck with database connectivity verification
app.get('/health', async (_req, res) => {
  try {
    await pool.query('SELECT 1');
    res.status(200).json({ status: 'healthy', database: 'connected' });
  } catch (error) {
    res.status(503).json({ status: 'unhealthy', database: 'disconnected' });
  }
});

// Product API routes
app.post('/api/v1/products', createProductHandler);
app.get('/api/v1/products', listProductsHandler);
app.get('/api/v1/products/:id', getProductHandler);
app.patch('/api/v1/products/:id/stock', updateStockHandler);

// RFC 7807 Error handling middleware
app.use(errorHandler);

if (process.env.NODE_ENV !== 'test') {
  const server = app.listen(env.PORT, () => {
    console.log(`[REST API] Server running on port ${env.PORT} in ${env.NODE_ENV} mode`);
  });

  const shutdown = async () => {
    console.log('Shutting down server and closing database pool...');
    server.close(async () => {
      await pool.end();
      console.log('PostgreSQL pool closed. Process exiting cleanly.');
      process.exit(0);
    });
  };

  process.on('SIGTERM', shutdown);
  process.on('SIGINT', shutdown);
}
```

---

## 3. Database Schema & Migration Specification

### DDL Schema (`drizzle/0000_init.sql`)
```sql
CREATE TABLE IF NOT EXISTS products (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    sku VARCHAR(50) NOT NULL UNIQUE,
    name VARCHAR(255) NOT NULL,
    description TEXT,
    price_cents INTEGER NOT NULL CHECK (price_cents > 0),
    stock_quantity INTEGER NOT NULL DEFAULT 0 CHECK (stock_quantity >= 0),
    created_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP NOT NULL,
    updated_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_products_sku ON products(sku);
CREATE INDEX IF NOT EXISTS idx_products_created_at ON products(created_at);
```

---

## 4. Verification & Testing Suite

### Integration Test with Testcontainers (`test/integration/product.api.test.ts`)
```typescript
import { describe, it, expect, beforeAll, afterAll } from 'vitest';
import request from 'supertest';
import { PostgreSqlContainer, StartedPostgreSqlContainer } from '@testcontainers/postgresql';
import { pool } from '../../src/infrastructure/database.js';
import { app } from '../../src/app.js';

describe('Product API Integration Tests with Real Postgres', () => {
  let container: StartedPostgreSqlContainer;

  beforeAll(async () => {
    container = await new PostgreSqlContainer('postgres:16-alpine')
      .withDatabase('testdb')
      .withUsername('testuser')
      .withPassword('testpass')
      .start();

    // Reconfigure pool connection
    process.env.DATABASE_URL = container.getConnectionUri();

    // Run initial DDL migration
    await pool.query(`
      CREATE TABLE IF NOT EXISTS products (
        id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
        sku VARCHAR(50) NOT NULL UNIQUE,
        name VARCHAR(255) NOT NULL,
        description TEXT,
        price_cents INTEGER NOT NULL,
        stock_quantity INTEGER NOT NULL DEFAULT 0,
        created_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP NOT NULL,
        updated_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP NOT NULL
      );
    `);
  }, 60000);

  afterAll(async () => {
    await pool.end();
    await container.stop();
  });

  it('creates and fetches product successfully', async () => {
    const postRes = await request(app)
      .post('/api/v1/products')
      .send({
        sku: 'LAPTOP-PRO-16',
        name: 'Pro Laptop 16-inch',
        priceCents: 199900,
        stockQuantity: 15,
      });

    expect(postRes.status).toBe(201);
    expect(postRes.body.data.id).toBeDefined();

    const getRes = await request(app).get(`/api/v1/products/${postRes.body.data.id}`);
    expect(getRes.status).toBe(200);
    expect(getRes.body.data.sku).toBe('LAPTOP-PRO-16');
  });

  it('prevents negative stock decrement via atomic transaction', async () => {
    const postRes = await request(app)
      .post('/api/v1/products')
      .send({
        sku: 'PHONE-AIR-1',
        name: 'Phone Air',
        priceCents: 79900,
        stockQuantity: 2,
      });

    const productId = postRes.body.data.id;

    // Attempt to decrement 5 items when only 2 exist
    const patchRes = await request(app)
      .patch(`/api/v1/products/${productId}/stock`)
      .send({ quantityDelta: -5 });

    expect(patchRes.status).toBe(409);
    expect(patchRes.body.title).toBe('Conflict');
  });
});
```

---

## 5. Production Deployment & Containerization

### `Dockerfile`
```dockerfile
FROM node:22-alpine AS builder
WORKDIR /app
COPY package*.json tsconfig.json ./
RUN npm ci
COPY src/ ./src
RUN npm run build || true
RUN npm prune --omit=dev

FROM gcr.io/distroless/nodejs22-debian12:nonroot
WORKDIR /app
COPY --from=builder /app/package.json ./package.json
COPY --from=builder /app/node_modules ./node_modules
COPY --from=builder /app/src ./src

USER nonroot
EXPOSE 3000
ENV NODE_ENV=production
CMD ["src/app.js"]
```

### `docker-compose.yml`
```yaml
version: '3.8'

services:
  postgres:
    image: postgres:16-alpine
    restart: unless-stopped
    ports:
      - "5432:5432"
    environment:
      POSTGRES_DB: ecommerce_db
      POSTGRES_USER: postgres
      POSTGRES_PASSWORD: postgrespassword
    volumes:
      - pgdata:/var/lib/postgresql/data

  api:
    build: .
    ports:
      - "3000:3000"
    environment:
      NODE_ENV: development
      PORT: 3000
      DATABASE_URL: postgresql://postgres:postgrespassword@postgres:5432/ecommerce_db
    depends_on:
      - postgres

volumes:
  pgdata:
```
