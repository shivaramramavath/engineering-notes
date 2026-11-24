# API Idempotency Implementation

- **NestJS Version**: 11.x
- **Node.js Baseline**: >= 20.0.0
- **Module**: 04-intermediate / 08-api-design
- **Prerequisites**: [02-fundamentals/03-controllers](../../02-fundamentals/03-controllers.md), [03-core-concepts/01-request-pipeline/04-interceptors](../../03-core-concepts/01-request-pipeline/04-interceptors.md), [00-prerequisites/04-http-and-rest-basics](../../00-prerequisites/04-http-and-rest-basics.md)

---

## 1. Context & Purpose

In distributed networks, HTTP client requests are susceptible to transient network timeouts, dropped TCP connections, and automated client retries. If a client attempts to charge a credit card via `POST /payments` and experiences a network timeout before receiving the response, retrying the request blindly can cause a catastrophic **double charge**.

While HTTP `GET`, `PUT`, and `DELETE` methods are defined as naturally idempotent in the HTTP specification, `POST` and `PATCH` are non-idempotent by default. To make critical mutation operations safe against retries, enterprise APIs require an **Idempotency Layer** using unique client-provided **Idempotency Keys** (e.g. `Idempotency-Key: <uuid>`), distributed atomic locks, and response caching.

---

## 2. Theoretical Foundations

An operation is **idempotent** if executing it multiple times produces the exact same server side effect and response as executing it once:
$$f(f(x)) = f(x)$$

### 2.1 The Idempotency Lifecycle
1. **First Attempt**: Client sends `Idempotency-Key: k`. Server acquires distributed lock on `k`, executes transaction, caches response, releases lock, and returns response.
2. **Concurrent Duplicate Attempt**: Client or another process sends `Idempotency-Key: k` while the first is still processing. Server detects lock active and returns `409 Conflict` (or blocks until completion).
3. **Subsequent Duplicate Attempt**: Client retries `Idempotency-Key: k` after completion. Server fetches cached response from Redis and returns it immediately with header `Idempotent-Replayed: true`, without executing business logic again.
4. **Mismatched Payload Attempt**: Client sends same key `k` with different body parameters. Server detects hash mismatch and rejects with `422 Unprocessable Entity` or `400 Bad Request`.

---

## 3. Architecture & Internal Mechanics

```
Client Request (POST /payments)
Headers: Idempotency-Key: 9b1deb4d...
Payload: { amount: 5000, recipient: "Alice" }
                    │
                    ▼
       Idempotency Interceptor
                    │
       Compute SHA256 of Payload
                    │
    Check Redis Key: `idemp:{key}`
                    │
    ┌───────────────┴───────────────┐
    │                               │
[Key Exists: COMPLETED]     [Key Does Not Exist]
    │                               │
Verify Body Hash Matches?           │
    ├─ No  ──> 400 Bad Request      │
    └─ Yes ──> Return Cached Res    ▼
               (Idempotent-Replayed) Acquire Redis Lock (SET NX PX 30000)
                                    │
                                    ▼
                         Execute Route Handler
                                    │
                                    ▼
                         Write Response to Redis
                         Status: COMPLETED
                         TTL: 24 Hours
                                    │
                                    ▼
                         Return Fresh Response
```

---

## 4. Concrete Implementation

### 4.1 Custom Decorator: `@Idempotent()`

```typescript
// src/common/decorators/idempotent.decorator.ts
import { SetMetadata } from '@nestjs/common';

export const IDEMPOTENT_KEY = 'IDEMPOTENT_METADATA';

export interface IdempotentOptions {
  ttlSeconds?: number;
  headerName?: string;
}

export const Idempotent = (options: IdempotentOptions = {}) =>
  SetMetadata(IDEMPOTENT_KEY, {
    ttlSeconds: options.ttlSeconds ?? 86400, // 24 hours default
    headerName: options.headerName ?? 'idempotency-key',
  });
```

---

### 4.2 Production Idempotency Interceptor with Redis

```typescript
// src/common/interceptors/idempotency.interceptor.ts
import {
  Injectable,
  NestInterceptor,
  ExecutionContext,
  CallHandler,
  BadRequestException,
  ConflictException,
} from '@nestjs/common';
import { Reflector } from '@nestjs/core';
import { Observable, of } from 'rxjs';
import { tap } from 'rxjs/operators';
import { Request, Response } from 'express';
import * as crypto from 'crypto';
import { Redis } from 'ioredis';
import { IDEMPOTENT_KEY, IdempotentOptions } from '../decorators/idempotent.decorator';

interface CachedRecord {
  status: 'PENDING' | 'COMPLETED';
  payloadHash: string;
  statusCode: number;
  headers: Record<string, string>;
  body: any;
}

@Injectable()
export class IdempotencyInterceptor implements NestInterceptor {
  constructor(
    private readonly reflector: Reflector,
    private readonly redisClient: Redis,
  ) {}

  async intercept(context: ExecutionContext, next: CallHandler): Promise<Observable<any>> {
    const options = this.reflector.get<IdempotentOptions>(
      IDEMPOTENT_KEY,
      context.getHandler(),
    );

    // If endpoint is not decorated with @Idempotent, proceed normally
    if (!options) {
      return next.handle();
    }

    const http = context.switchToHttp();
    const req = http.getRequest<Request>();
    const res = http.getResponse<Response>();

    const rawKey = req.headers[options.headerName!];
    if (!rawKey || typeof rawKey !== 'string') {
      throw new BadRequestException(`Missing required header: ${options.headerName}`);
    }

    const redisKey = `idemp:${req.method}:${req.baseUrl + req.path}:${rawKey}`;
    const payloadHash = crypto
      .createHash('sha256')
      .update(JSON.stringify(req.body || {}))
      .digest('hex');

    // Atomic acquire with SET NX
    const lockAcquired = await this.redisClient.set(
      redisKey,
      JSON.stringify({ status: 'PENDING', payloadHash }),
      'EX',
      options.ttlSeconds!,
      'NX',
    );

    if (!lockAcquired) {
      // Key already exists: inspect cached state
      const existing = await this.redisClient.get(redisKey);
      if (!existing) {
        throw new ConflictException('Concurrent request conflict. Please retry.');
      }

      const cached: CachedRecord = JSON.parse(existing);

      if (cached.payloadHash !== payloadHash) {
        throw new BadRequestException(
          'Idempotency key reuse with different request payload is forbidden.',
        );
      }

      if (cached.status === 'PENDING') {
        throw new ConflictException(
          'A request with this idempotency key is currently being processed.',
        );
      }

      // Return cached response
      res.setHeader('Idempotent-Replayed', 'true');
      res.status(cached.statusCode);
      return of(cached.body);
    }

    // New request: execute handler and cache response
    return next.handle().pipe(
      tap({
        next: async (body) => {
          const record: CachedRecord = {
            status: 'COMPLETED',
            payloadHash,
            statusCode: res.statusCode || 200,
            headers: {},
            body,
          };
          await this.redisClient.set(
            redisKey,
            JSON.stringify(record),
            'EX',
            options.ttlSeconds!,
          );
        },
        error: async () => {
          // On server failure, release lock to allow client retry
          await this.redisClient.del(redisKey);
        },
      }),
    );
  }
}
```

---

### 4.3 Applying to Critical Controller Handlers

```typescript
// src/payments/payments.controller.ts
import { Controller, Post, Body, UseInterceptors } from '@nestjs/common';
import { Idempotent } from '../common/decorators/idempotent.decorator';
import { IdempotencyInterceptor } from '../common/interceptors/idempotency.interceptor';
import { PaymentsService } from './payments.service';

@Controller('payments')
@UseInterceptors(IdempotencyInterceptor)
export class PaymentsController {
  constructor(private readonly paymentsService: PaymentsService) {}

  @Post('charge')
  @Idempotent({ ttlSeconds: 86400 }) // 24 hours idempotency guarantee
  async chargeCreditCard(@Body() chargeDto: { amount: number; cardToken: string }) {
    return this.paymentsService.processCharge(chargeDto);
  }
}
```

---

## 5. Edge Cases & Failure Modes

### 5.1 Lock Starvation on Node Crash
If the backend process crashes while processing a request (during the `PENDING` state), the Redis lock will remain until the lock TTL expires.
- **Handling**: Use short lease timeouts for `PENDING` state (e.g. 30 seconds), extending to the full 24-hour TTL only when transitioned to `COMPLETED`.

### 5.2 Header Key Reuse Across Different Endpoints
If a client generates one UUID and uses it across `/orders` and `/refunds`.
- **Handling**: Prefix the Redis storage key with the HTTP method and route path:
  `idemp:${method}:${route}:${key}`.

---

## 6. Performance & Memory Implications

- **Redis Storage**: Caching 1 million idempotent responses at ~1KB each consumes ~1GB of Redis RAM. Ensure a strict TTL (typically 24 to 72 hours) and configure Redis with `volatile-lru` eviction policy.
- **Latency Overhead**: Adding the interceptor introduces two Redis roundtrips (~1–2ms total), negligible compared to third-party payment gateway latency (300–800ms).

---

## 7. Anti-Patterns & Common Mistakes

- **Caching Server Errors**: Storing 500 Internal Server Error responses in Redis as idempotent records. If a transient database timeout occurs, caching it prevents the client from ever succeeding on subsequent retries.
- **Omitting Payload Hash Validation**: Returning the cached output of an earlier \$100 charge when the client reuses the same key to charge \$500.
- **Generating Idempotency Keys on the Server**: The client must supply the key. Generating the key on the server provides zero protection against network drops between client and server.

---

## 8. Production Checklist & Operational Readiness

- [ ] Idempotency interceptor applied to all financial mutation endpoints (`POST /checkout`, `POST /charges`, `POST /transfers`).
- [ ] Payload hash computed using cryptographic SHA-256.
- [ ] Redis keys scoped by method, route path, and tenant ID.
- [ ] Lock released immediately on unhandled exceptions to permit legitimate retries.
- [ ] `Idempotent-Replayed: true` header included on cached responses.

---

## 9. Migration & Version History

- **IETF Draft**: Draft-ietf-httpapi-idempotency-key-header standards tracked.
- **NestJS 11.x**: Validated with RxJS 7.8 operators, Keyv stores, and Express 5 route context interception.

---

## 10. Verification & Testing

E2E testing idempotent replay behavior:

```typescript
import { Test, TestingModule } from '@nestjs/testing';
import { INestApplication } from '@nestjs/common';
import * as request from 'supertest';
import { PaymentsController } from './payments.controller';
import { PaymentsService } from './payments.service';

describe('Idempotency (e2e)', () => {
  let app: INestApplication;
  let chargeSpy: any;

  beforeAll(async () => {
    chargeSpy = vi.fn().mockResolvedValue({ id: 'ch_123', status: 'succeeded' });
    const moduleFixture: TestingModule = await Test.createTestingModule({
      controllers: [PaymentsController],
      providers: [{ provide: PaymentsService, useValue: { processCharge: chargeSpy } }],
    }).compile();

    app = moduleFixture.createNestApplication();
    await app.init();
  });

  afterAll(async () => {
    await app.close();
  });

  it('executes service on first attempt and replays on second attempt', async () => {
    const key = 'test-idem-key-001';
    const payload = { amount: 1000, cardToken: 'tok_visa' };

    // First attempt
    const res1 = await request(app.getHttpServer())
      .post('/payments/charge')
      .set('Idempotency-Key', key)
      .send(payload)
      .expect(201);

    expect(chargeSpy).toHaveBeenCalledTimes(1);
    expect(res1.headers['idempotent-replayed']).toBeUndefined();

    // Second attempt with exact same key and payload
    const res2 = await request(app.getHttpServer())
      .post('/payments/charge')
      .set('Idempotency-Key', key)
      .send(payload)
      .expect(200);

    // Service method should NOT be called a second time
    expect(chargeSpy).toHaveBeenCalledTimes(1);
    expect(res2.headers['idempotent-replayed']).toBe('true');
  });
});
```

---

## 11. Comprehensive Examples

### Client Implementation Pattern (TypeScript / Axios)

```typescript
// client/payment-client.ts
import axios from 'axios';
import { randomUUID } from 'crypto';

export async function submitPayment(amount: number, cardToken: string) {
  const idempotencyKey = randomUUID();

  // Retry up to 3 times with the exact same idempotency key
  for (let attempt = 1; attempt <= 3; attempt++) {
    try {
      const response = await axios.post(
        'https://api.example.com/payments/charge',
        { amount, cardToken },
        {
          headers: { 'Idempotency-Key': idempotencyKey },
          timeout: 5000,
        },
      );
      return response.data;
    } catch (err: any) {
      if (attempt === 3 || err.response?.status === 400) {
        throw err;
      }
      console.warn(`Payment attempt ${attempt} failed. Retrying...`);
    }
  }
}
```

---

## 12. Cross-References & Further Reading

- [03-core-concepts/01-request-pipeline/04-interceptors.md](../../03-core-concepts/01-request-pipeline/04-interceptors.md) - Interceptor architecture
- [07-webhooks.md](./07-webhooks.md) - Webhook processing and idempotency
- [IETF Idempotency-Key HTTP Header Specification](https://datatracker.ietf.org/doc/draft-ietf-httpapi-idempotency-key-header/)
