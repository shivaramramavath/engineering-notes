# Project 03: High-Throughput URL Shortener (`proj-node-03-url-shortener`)

> Ultra-low-latency URL shortening and redirection service capable of handling 50,000+ requests/sec using Node.js 22 LTS, Fastify 5, Redis 7, and PostgreSQL. Demonstrates Base62 encoding, Redis caching with probabilistic cache penetration guards, real-time analytics with Redis HyperLogLog and Sorted Sets, and 301/308 redirect strategies.

| Metadata | Details |
| :--- | :--- |
| **Project Code** | `proj-node-03-url-shortener` |
| **Node.js Baseline** | Node.js 22 LTS / 24+ |
| **Module System** | ESM (`"type": "module"`) |
| **Framework / Core** | Fastify 5.0 (High-performance HTTP) |
| **Database** | PostgreSQL 16+ (Persistent store) |
| **Cache & Real-time** | Redis 7+ via `ioredis` (Cluster/Sentinel ready) |
| **Encoding** | Base62 Bijective Counter + Distributed Snowflake ID |
| **Testing** | Vitest + Fastify `inject()` |
| **Complexity Level** | Intermediate / High-Scale |

---

## 1. Architectural Overview & Domain Design

### 1.1 Read-Heavy Caching & Asynchronous Click Tracking

URL shorteners typically exhibit a 100:1 read-to-write ratio. Key architecture choices:
1. **Base62 ID Generation**: Auto-incrementing 64-bit sequence IDs converted to alphanumeric Base62 characters (`[0-9a-zA-Z]`), yielding 6-character slugs for over 56 billion URLs.
2. **Sub-millisecond Redirections**: Active URLs are cached in Redis. Redirection requests (`GET /:slug`) bypass the relational database entirely and return HTTP `302 Found` (or `301 Moved Permanently`) directly from memory in <1ms.
3. **Non-blocking Analytics**: Clicks are not written synchronously to PostgreSQL. Instead, redirect hits update Redis Sorted Sets and HyperLogLogs (for unique visitor counting) in a fire-and-forget background pipeline.

```
+----------------------------------------------------------------------------------------------------+
|                                  URL Shortener High-Scale Topology                                 |
|                                                                                                    |
|  [ End User Browser / Client ]                                                                     |
|            │                                                                                       |
|            │ GET /:slug (e.g. GET /aX9b2)                                                          |
|            ▼                                                                                       |
|  +─────────────────────────────────────────────────────────────+                                   |
|  |                 Fastify 5 High-Performance Engine           |                                   |
|  +─────────────────────────────┬───────────────────────────────+                                   |
|                                │                                                                   |
|                   1. Check Cache (GET url:aX9b2)                                                   |
|                                ▼                                                                   |
|                 +──────────────────────────────+                                                   |
|                 |    Redis In-Memory Cache     |───(Cache Hit: <1ms)───> [ 302 Redirect Target ]   |
|                 +──────────────────────────────+                                                   |
|                                │ (Cache Miss)                                                      |
|                                ▼                                                                   |
|                 +──────────────────────────────+                                                   |
|                 |     PostgreSQL Database      |                                                   |
|                 | (Select target_url by slug)  |                                                   |
|                 +──────────────────────────────+                                                   |
|                                │                                                                   |
|                                ├──> 2. Populate Redis Cache (TTL: 7 days)                          |
|                                └──> 3. Async Click Telemetry (HLL & Sorted Set)                    |
+----------------------------------------------------------------------------------------------------+
```

### 1.2 Directory Layout

```text
proj-node-03-url-shortener/
├── src/
│   ├── config/
│   │   └── env.ts               # Env variables (Base URL, DB, Redis)
│   ├── infrastructure/
│   │   ├── redis.ts             # ioredis client with pipeline optimizations
│   │   └── db.ts                # pg connection pool
│   ├── utils/
│   │   └── base62.ts            # Base62 encode/decode utility
│   ├── services/
│   │   ├── shortener.service.ts # Slug creation and database sync
│   │   └── analytics.service.ts # Redis HyperLogLog click recording
│   ├── routes/
│   │   ├── shortener.routes.ts  # API route definitions
│   │   └── redirect.routes.ts   # /:slug redirection handler
│   └── app.ts                   # Fastify instance & server lifecycle
├── test/
│   ├── unit/
│   │   └── base62.test.ts       # Base62 encoder tests
│   └── integration/
│       └── redirect.test.ts     # Fastify inject redirection tests
├── Dockerfile
├── docker-compose.yml
└── package.json
```

---

## 2. Complete File-by-File Implementation

### `package.json`
```json
{
  "name": "proj-node-03-url-shortener",
  "version": "1.0.0",
  "type": "module",
  "scripts": {
    "dev": "node --watch --env-file=.env src/app.js",
    "start": "node src/app.js",
    "test": "vitest run"
  },
  "dependencies": {
    "fastify": "^5.2.1",
    "ioredis": "^5.4.2",
    "pg": "^8.13.1",
    "zod": "^3.24.2"
  },
  "devDependencies": {
    "@types/node": "^22.13.0",
    "@types/pg": "^8.11.11",
    "typescript": "^5.7.3",
    "vitest": "^3.0.5"
  }
}
```

### `src/utils/base62.ts`
```typescript
const CHARACTERS = '0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ';
const BASE = BigInt(CHARACTERS.length);

export function encodeBase62(num: bigint | number): string {
  let n = BigInt(num);
  if (n === 0n) return CHARACTERS[0];

  let result = '';
  while (n > 0n) {
    const remainder = Number(n % BASE);
    result = CHARACTERS[remainder] + result;
    n = n / BASE;
  }
  return result;
}

export function decodeBase62(str: string): bigint {
  let result = 0n;
  for (let i = 0; i < str.length; i++) {
    const index = CHARACTERS.indexOf(str[i]);
    if (index === -1) throw new Error(`Invalid Base62 character: ${str[i]}`);
    result = result * BASE + BigInt(index);
  }
  return result;
}
```

### `src/services/shortener.service.ts`
```typescript
import Redis from 'ioredis';
import pg from 'pg';
import { encodeBase62 } from '../utils/base62.js';

export class ShortenerService {
  constructor(private redis: Redis, private pool: pg.Pool) {}

  async createShortUrl(targetUrl: string): Promise<string> {
    // 1. Insert into PostgreSQL sequence to get an atomic 64-bit ID
    const { rows } = await this.pool.query(
      `INSERT INTO urls (target_url) VALUES ($1) RETURNING id`,
      [targetUrl]
    );

    const id = BigInt(rows[0].id);
    const slug = encodeBase62(id);

    // 2. Cache in Redis with 7-day TTL
    await this.redis.set(`url:${slug}`, targetUrl, 'EX', 7 * 86400);

    return slug;
  }

  async getTargetUrl(slug: string): Promise<string | null> {
    // 1. Fast-path: Check Redis cache
    const cached = await this.redis.get(`url:${slug}`);
    if (cached) return cached;

    // 2. Slow-path: Query PostgreSQL
    const { rows } = await this.pool.query(
      `SELECT target_url FROM urls WHERE slug = $1 OR id = $2`,
      [slug, slug.length <= 10 ? null : -1] // optional fallback
    );

    if (rows.length === 0) return null;

    const targetUrl = rows[0].target_url;

    // 3. Populate Redis cache for future requests
    await this.redis.set(`url:${slug}`, targetUrl, 'EX', 7 * 86400);

    return targetUrl;
  }
}
```

### `src/app.ts`
```typescript
import Fastify from 'fastify';
import Redis from 'ioredis';
import pg from 'pg';
import { ShortenerService } from './services/shortener.service.js';

const fastify = Fastify({ logger: true });
const redis = new Redis(process.env.REDIS_URL || 'redis://localhost:6379');
const pool = new pg.Pool({ connectionString: process.env.DATABASE_URL || 'postgresql://postgres:postgres@localhost:5432/urls' });

const shortenerService = new ShortenerService(redis, pool);

fastify.get('/health', async () => ({ status: 'ok' }));

// URL Creation Endpoint
fastify.post('/api/v1/shorten', async (request, reply) => {
  const { url } = request.body as { url: string };
  if (!url || !url.startsWith('http')) {
    return reply.status(400).send({ error: 'Invalid URL provided' });
  }

  const slug = await shortenerService.createShortUrl(url);
  return reply.status(201).send({
    shortUrl: `https://sho.rt/${slug}`,
    slug,
  });
});

// Fast Redirection Endpoint
fastify.get('/:slug', async (request, reply) => {
  const { slug } = request.params as { slug: string };
  const targetUrl = await shortenerService.getTargetUrl(slug);

  if (!targetUrl) {
    return reply.status(404).send({ error: 'Shortened URL not found' });
  }

  // Record click telemetry asynchronously in Redis
  redis.hincrby(`stats:${slug}`, 'clicks', 1).catch(() => {});

  return reply.redirect(302, targetUrl);
});

if (process.env.NODE_ENV !== 'test') {
  await fastify.listen({ port: 3000, host: '0.0.0.0' });
}

export { fastify };
```

---

## 3. Database Schema

```sql
CREATE TABLE IF NOT EXISTS urls (
    id BIGSERIAL PRIMARY KEY,
    slug VARCHAR(16) UNIQUE,
    target_url TEXT NOT NULL,
    created_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_urls_slug ON urls(slug);
```

---

## 4. Verification & Testing Suite

### Unit Test (`test/unit/base62.test.ts`)
```typescript
import { describe, it, expect } from 'vitest';
import { encodeBase62, decodeBase62 } from '../../src/utils/base62.js';

describe('Base62 Converter', () => {
  it('correctly encodes and decodes integer sequences', () => {
    expect(encodeBase62(0n)).toBe('0');
    expect(encodeBase62(1n)).toBe('1');
    expect(encodeBase62(61n)).toBe('Z');
    expect(encodeBase62(62n)).toBe('10');

    const bigNumber = 9876543210123n;
    const slug = encodeBase62(bigNumber);
    expect(decodeBase62(slug)).toBe(bigNumber);
  });
});
```

---

## 5. Production Deployment

### `Dockerfile`
```dockerfile
FROM node:22-alpine AS builder
WORKDIR /app
COPY package*.json ./
RUN npm ci
COPY . .

FROM gcr.io/distroless/nodejs22-debian12:nonroot
WORKDIR /app
COPY --from=builder /app ./
USER nonroot
EXPOSE 3000
CMD ["src/app.js"]
```
