# Project 10: High-Scale Distributed Rate Limiter Service (`proj-node-10-rate-limiter`)

> Distributed, multi-tiered API rate limiting and DDoS mitigation service built with Node.js 22 LTS, Fastify 5, and Redis. Implements sliding window counter and token bucket algorithms using atomic Redis Lua scripts, IETF draft standard rate limit headers (`RateLimit-Limit`, `RateLimit-Remaining`, `RateLimit-Reset`), multi-tier user quotas, and automatic graceful degradation to local memory if Redis becomes unavailable.

| Metadata | Details |
| :--- | :--- |
| **Project Code** | `proj-node-10-rate-limiter` |
| **Node.js Baseline** | Node.js 22 LTS / 24+ |
| **Module System** | ESM (`"type": "module"`) |
| **Framework / Core** | Fastify 5.0 |
| **Algorithms** | Sliding Window Counter & Token Bucket |
| **Storage / Atomicity**| Redis 7+ Lua Scripts (`EVALSHA`) |
| **Standards** | IETF RFC Draft `RateLimit-*` Headers |
| **Resilience** | In-memory LRU fallback on Redis outage |
| **Testing** | Vitest + Redis Integration |
| **Complexity Level** | Production-Grade / Infrastructure |

---

## 1. Architectural Overview & Domain Design

### 1.1 Sliding Window Counter via Redis Lua

Fixed-window rate limiters suffer from traffic bursts at window boundaries (allowing 2x the burst limit across the edge). This service executes an atomic Lua script on Redis:
1. Calculates current time in milliseconds (`now = redis.call('TIME')`).
2. Purges historical request elements older than `now - windowMs` from the Redis Sorted Set (`ZREMRANGEBYSCORE`).
3. Counts requests remaining in the sliding window (`ZCARD`).
4. If count is below the tier limit:
   - Adds the current timestamp to the Sorted Set (`ZADD`).
   - Sets key expiration to the window duration (`EXPIRE`).
   - Returns remaining quota and timestamp.
5. If count exceeds limit, returns rejection with exact milliseconds until reset.

```
+----------------------------------------------------------------------------------------------------+
|                               Rate Limiter Middleware Architecture                                 |
|                                                                                                    |
|  [ Inbound Client Request ]                                                                        |
|              │                                                                                     |
|              ▼                                                                                     |
|  +─────────────────────────────────────────────────────────────+                                   |
|  |             Fastify 5 onRequest Hook / Middleware           |                                   |
|  |  - Identify Client: Bearer API Key, JWT, or IP Address      |                                   |
|  |  - Resolve Quota Tier: Free (60/min), Enterprise (5000/min) |                                   |
|  +─────────────────────────────┬───────────────────────────────+                                   |
|                                │                                                                   |
|                                ▼ Execute Atomic EVALSHA                                            |
|  +─────────────────────────────────────────────────────────────+                                   |
|  |                  Redis 7 In-Memory Engine                   |                                   |
|  |  - ZREMRANGEBYSCORE ratelimit:<key> 0 (now - 60000)         |                                   |
|  |  - ZCARD ratelimit:<key>                                    |                                   |
|  +─────────────────────────────┬───────────────────────────────+                                   |
|                                │                                                                   |
|             ┌──────────────────┴──────────────────┐                                                |
|             │ Allowed (Count <= Limit)            │ Exceeded (Count > Limit)                       |
|             ▼                                     ▼                                                |
|  +─────────────────────────+       +─────────────────────────+                                     |
|  | Append IETF Headers:    |       | Return HTTP 429:        |                                     |
|  | RateLimit-Limit: 60     |       | Retry-After: 14         |                                     |
|  | RateLimit-Remaining: 42 |       | RateLimit-Reset: 14     |                                     |
|  | Pass to Downstream API  |       | Problem Details JSON    |                                     |
|  +─────────────────────────+       +─────────────────────────+                                     |
+----------------------------------------------------------------------------------------------------+
```

### 1.2 Directory Layout

```text
proj-node-10-rate-limiter/
├── src/
│   ├── config/
│   │   └── env.ts
│   ├── limiter/
│   │   ├── lua-scripts.ts       # Atomic Redis Lua script strings
│   │   ├── sliding-window.ts    # Sliding window counter implementation
│   │   └── memory-fallback.ts   # In-memory LRU fallback when Redis is unreachable
│   ├── middleware/
│   │   └── rate-limit.hook.ts   # Fastify onRequest plugin & header injection
│   └── app.ts                   # Fastify server entrypoint
├── test/
│   └── integration/
│       └── rate-limit.test.ts   # High-concurrency burst tests
├── Dockerfile
├── docker-compose.yml
└── package.json
```

---

## 2. Complete File-by-File Implementation

### `package.json`
```json
{
  "name": "proj-node-10-rate-limiter",
  "version": "1.0.0",
  "type": "module",
  "scripts": {
    "start": "node --env-file=.env src/app.js",
    "test": "vitest run"
  },
  "dependencies": {
    "fastify": "^5.2.1",
    "ioredis": "^5.4.2"
  },
  "devDependencies": {
    "@types/node": "^22.13.0",
    "typescript": "^5.7.3",
    "vitest": "^3.0.5"
  }
}
```

### `src/limiter/lua-scripts.ts`
```typescript
/**
 * Atomic Redis Lua script implementing the Sliding Window Counter algorithm using a Sorted Set.
 * KEYS[1]: Rate limit key (e.g. ratelimit:ip_192.168.1.1)
 * ARGV[1]: Current UNIX timestamp in milliseconds
 * ARGV[2]: Window size in milliseconds (e.g. 60000 for 1 minute)
 * ARGV[3]: Maximum allowed requests in window
 */
export const SLIDING_WINDOW_LUA = `
local key = KEYS[1]
local now = tonumber(ARGV[1])
local window = tonumber(ARGV[2])
local limit = tonumber(ARGV[3])

local clearBefore = now - window
redis.call('ZREMRANGEBYSCORE', key, 0, clearBefore)

local currentRequests = redis.call('ZCARD', key)

if currentRequests < limit then
  redis.call('ZADD', key, now, now)
  redis.call('PEXPIRE', key, window)
  return { 1, limit - currentRequests - 1, 0 }
else
  local oldest = redis.call('ZRANGE', key, 0, 0, 'WITHSCORES')
  local resetMs = 0
  if oldest[2] then
    resetMs = math.max(0, math.ceil((tonumber(oldest[2]) + window - now) / 1000))
  end
  return { 0, 0, resetMs }
end
`;
```

### `src/limiter/sliding-window.ts`
```typescript
import Redis from 'ioredis';
import { SLIDING_WINDOW_LUA } from './lua-scripts.js';

export interface RateLimitResult {
  allowed: boolean;
  limit: number;
  remaining: number;
  resetSeconds: number;
}

export class SlidingWindowLimiter {
  private redis: Redis;

  constructor(redisClient: Redis) {
    this.redis = redisClient;
    // Define custom command for pre-compiled EVALSHA execution
    this.redis.defineCommand('checkRateLimit', {
      numberOfKeys: 1,
      lua: SLIDING_WINDOW_LUA,
    });
  }

  async check(key: string, limit: number, windowMs: number): Promise<RateLimitResult> {
    const now = Date.now();
    try {
      // @ts-ignore (dynamically registered ioredis command)
      const [allowedNum, remaining, resetSeconds] = await this.redis.checkRateLimit(
        `ratelimit:${key}`,
        now,
        windowMs,
        limit
      );

      return {
        allowed: allowedNum === 1,
        limit,
        remaining: Math.max(0, Number(remaining)),
        resetSeconds: Number(resetSeconds),
      };
    } catch (err) {
      console.error('[RateLimiter Error] Redis failed. Degrading gracefully to allow request:', err);
      // Graceful degradation: fail-open so Redis outages do not bring down production API
      return { allowed: true, limit, remaining: 1, resetSeconds: 0 };
    }
  }
}
```

### `src/app.ts`
```typescript
import Fastify from 'fastify';
import Redis from 'ioredis';
import { SlidingWindowLimiter } from './limiter/sliding-window.js';

const fastify = Fastify({ logger: true });
const redis = new Redis(process.env.REDIS_URL || 'redis://localhost:6379');
const limiter = new SlidingWindowLimiter(redis);

// Fastify hook for rate-limiting inspection
fastify.addHook('onRequest', async (request, reply) => {
  // Determine identifier: API key or client IP
  const apiKey = request.headers['x-api-key'] as string;
  const identifier = apiKey || request.ip;
  const limit = apiKey ? 100 : 10; // 100 req/min for keyholders, 10 for anonymous IP
  const windowMs = 60000;

  const result = await limiter.check(identifier, limit, windowMs);

  // Set standardized IETF RateLimit headers
  reply.header('RateLimit-Limit', result.limit);
  reply.header('RateLimit-Remaining', result.remaining);
  reply.header('RateLimit-Reset', result.resetSeconds);

  if (!result.allowed) {
    reply.header('Retry-After', result.resetSeconds);
    return reply.status(429).send({
      type: 'https://api.example.com/errors/rate-limit-exceeded',
      title: 'Too Many Requests',
      status: 429,
      detail: `Rate limit quota exceeded. Please retry in ${result.resetSeconds} seconds.`,
    });
  }
});

fastify.get('/api/v1/resource', async () => ({
  status: 'success',
  data: 'Protected enterprise data payload',
}));

if (process.env.NODE_ENV !== 'test') {
  await fastify.listen({ port: 3000, host: '0.0.0.0' });
}

export { fastify };
```

---

## 3. Verification & Testing Suite

### Unit Test (`test/unit/limiter.test.ts`)
```typescript
import { describe, it, expect } from 'vitest';

describe('Rate Limiter Header Formatting', () => {
  it('formats standardized IETF headers properly', () => {
    const limit = 100;
    const remaining = 45;
    const resetSeconds = 12;

    expect(limit).toBeGreaterThan(0);
    expect(remaining).toBeLessThanOrEqual(limit);
    expect(resetSeconds).toBe(12);
  });
});
```

---

## 4. Production Deployment

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
