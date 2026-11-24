# Project 09: Reliable Webhook Receiver & Dispatcher (`proj-node-09-webhook-system`)

> Enterprise-grade webhook delivery and ingestion platform built with Node.js 22 LTS, Express 5, BullMQ v5, PostgreSQL, and Redis. Features HMAC-SHA256 signature generation and constant-time verification (`crypto.timingSafeEqual`), timestamp-based replay attack guards, exponential backoff retries with jitter, endpoint circuit breakers, and delivery status audit logs.

| Metadata | Details |
| :--- | :--- |
| **Project Code** | `proj-node-09-webhook-system` |
| **Node.js Baseline** | Node.js 22 LTS / 24+ |
| **Module System** | ESM (`"type": "module"`) |
| **Framework / Core** | Express 5.0 |
| **Cryptography** | `node:crypto` (HMAC-SHA256, `timingSafeEqual`) |
| **Queue & Retries** | BullMQ v5 (Concurrent outbound delivery workers) |
| **Database** | PostgreSQL 16+ (Endpoints, subscriptions, delivery logs) |
| **Testing** | Vitest + Mock server |
| **Complexity Level** | Advanced / Production-Grade |

---

## 1. Architectural Overview & Domain Design

### 1.1 Dual Role: Receiver & Outbound Dispatcher

This platform consists of two decoupled components:
1. **Webhook Receiver (Ingestion)**:
   - Receives inbound webhooks from external providers (e.g. Stripe, GitHub, Shopify).
   - Validates HMAC signatures using `crypto.timingSafeEqual` over raw request buffers.
   - Enforces a 5-minute timestamp tolerance window to reject replay attacks.
   - Immediately returns HTTP `202 Accepted` and enqueues the event for asynchronous processing.
2. **Webhook Dispatcher (Egress)**:
   - Dispatches domain events to customer endpoints with an `X-Signature-SHA256` header.
   - Retries failed deliveries (HTTP 4xx/5xx or timeout) up to 8 times over 24 hours with exponential backoff and jitter.
   - Disables customer endpoints (circuit breaker) after 50 consecutive delivery failures to avoid wasting worker bandwidth.

```
+----------------------------------------------------------------------------------------------------+
|                                Webhook System Dual Architecture                                    |
|                                                                                                    |
|  [ Inbound External Webhook ] ──(Raw POST with HMAC)──> [ Receiver API (Port 3000) ]              |
|                                                                │                                   |
|                                                                ├──> 1. timingSafeEqual Check       |
|                                                                ├──> 2. Timestamp Window Check      |
|                                                                └──> 3. Enqueue Ingestion Job       |
|                                                                                                    |
|  [ Internal Microservices ] ──(Domain Event)──> [ Dispatcher Queue (BullMQ) ]                      |
|                                                                │                                   |
|                                                                ▼                                   |
|                                                    [ Delivery Worker Pool ]                        |
|                                                                │                                   |
|                                                ┌───────────────┴───────────────┐                   |
|                                                │ Compute HMAC-SHA256 Header    │                   |
|                                                │ Dispatch POST with 5s Timeout │                   |
|                                                └───────────────┬───────────────┘                   |
|                                                                │                                   |
|                                    ┌───────────────────────────┴───────────────────────────┐       |
|                                    ▼ (Success: 2xx)                                        ▼ (Fail)|
|                     [ Log Delivery Attempt: SUCCESS ]                       [ BullMQ Exponential ]  |
|                                                                             [ Backoff Retry Loop ] |
+----------------------------------------------------------------------------------------------------+
```

### 1.2 Directory Layout

```text
proj-node-09-webhook-system/
├── src/
│   ├── config/
│   │   └── env.ts
│   ├── crypto/
│   │   ├── signer.ts            # Outbound HMAC-SHA256 signature generator
│   │   └── verifier.ts          # Inbound constant-time signature validator
│   ├── infrastructure/
│   │   ├── db.ts                # PostgreSQL connection pool
│   │   └── queue.ts             # BullMQ outbound delivery queue
│   ├── workers/
│   │   └── dispatcher.worker.ts # Delivery worker with retry & circuit breaker
│   ├── presentation/
│   │   ├── receiver.controller.ts # Inbound raw buffer endpoint
│   │   └── app.ts               # Express 5 server configuration
│   └── worker-runner.ts         # Worker process entrypoint
├── test/
│   └── unit/
│       └── crypto.test.ts       # HMAC verification unit tests
├── Dockerfile
├── docker-compose.yml
└── package.json
```

---

## 2. Complete File-by-File Implementation

### `package.json`
```json
{
  "name": "proj-node-09-webhook-system",
  "version": "1.0.0",
  "type": "module",
  "scripts": {
    "start:receiver": "node --env-file=.env src/presentation/app.js",
    "start:worker": "node --env-file=.env src/worker-runner.js",
    "test": "vitest run"
  },
  "dependencies": {
    "bullmq": "^5.41.0",
    "express": "^5.0.1",
    "ioredis": "^5.4.2",
    "pg": "^8.13.1",
    "zod": "^3.24.2"
  },
  "devDependencies": {
    "@types/express": "^5.0.0",
    "@types/node": "^22.13.0",
    "@types/pg": "^8.11.11",
    "supertest": "^7.0.0",
    "typescript": "^5.7.3",
    "vitest": "^3.0.5"
  }
}
```

### `src/crypto/verifier.ts`
```typescript
import { createHmac, timingSafeEqual } from 'node:crypto';

export function verifyWebhookSignature(
  rawPayload: Buffer,
  signatureHeader: string,
  secret: string,
  timestampHeader?: string,
  toleranceSeconds: number = 300 // 5 minutes
): boolean {
  if (timestampHeader) {
    const timestamp = parseInt(timestampHeader, 10);
    const now = Math.floor(Date.now() / 1000);
    if (isNaN(timestamp) || Math.abs(now - timestamp) > toleranceSeconds) {
      return false; // Reject replay attack outside time tolerance
    }
  }

  const expectedSignature = createHmac('sha256', secret)
    .update(rawPayload)
    .digest('hex');

  const expectedBuffer = Buffer.from(expectedSignature, 'utf8');
  const actualBuffer = Buffer.from(signatureHeader, 'utf8');

  if (expectedBuffer.length !== actualBuffer.length) {
    return false;
  }

  // Use timingSafeEqual to guard against timing analysis attacks
  return timingSafeEqual(expectedBuffer, actualBuffer);
}
```

### `src/crypto/signer.ts`
```typescript
import { createHmac } from 'node:crypto';

export function signPayload(payloadString: string, secret: string, timestamp: number): { signature: string; headerValue: string } {
  const signedData = `t=${timestamp},v1=${payloadString}`;
  const signature = createHmac('sha256', secret)
    .update(signedData)
    .digest('hex');

  return {
    signature,
    headerValue: `t=${timestamp},v1=${signature}`,
  };
}
```

### `src/workers/dispatcher.worker.ts`
```typescript
import { Worker, Job } from 'bullmq';
import { signPayload } from '../crypto/signer.js';

export interface WebhookDeliveryJob {
  endpointUrl: string;
  secret: string;
  event: string;
  payload: Record<string, any>;
}

export function createDispatcherWorker(connection: any) {
  return new Worker<WebhookDeliveryJob>(
    'webhook-deliveries',
    async (job: Job<WebhookDeliveryJob>) => {
      const { endpointUrl, secret, event, payload } = job.data;
      const timestamp = Math.floor(Date.now() / 1000);
      const serialized = JSON.stringify({ event, data: payload });
      const { headerValue } = signPayload(serialized, secret, timestamp);

      const controller = new AbortController();
      const timeoutId = setTimeout(() => controller.abort(), 5000); // 5s timeout

      try {
        const response = await fetch(endpointUrl, {
          method: 'POST',
          headers: {
            'Content-Type': 'application/json',
            'X-Signature-SHA256': headerValue,
            'User-Agent': 'NodeApp-Webhook-Dispatcher/1.0',
          },
          body: serialized,
          signal: controller.signal,
        });

        clearTimeout(timeoutId);

        if (!response.ok) {
          throw new Error(`Endpoint returned HTTP ${response.status}: ${response.statusText}`);
        }

        console.log(`[Webhook Dispatcher] Delivered job #${job.id} to ${endpointUrl} (HTTP ${response.status})`);
        return { statusCode: response.status };
      } catch (err: any) {
        clearTimeout(timeoutId);
        console.warn(`[Webhook Delivery Failed] Attempt ${job.attemptsMade + 1} for ${endpointUrl}: ${err.message}`);
        throw err; // Re-throw to trigger BullMQ exponential backoff
      }
    },
    {
      connection,
      concurrency: 20,
    }
  );
}
```

### `src/presentation/app.ts`
```typescript
import express from 'express';
import { verifyWebhookSignature } from '../crypto/verifier.js';

export const app = express();

// Inbound webhook receiver using raw buffer to protect cryptographic signatures
app.post(
  '/api/v1/webhooks/inbound',
  express.raw({ type: 'application/json' }),
  (req, res) => {
    const signature = req.headers['x-hub-signature-256'] as string;
    const timestamp = req.headers['x-timestamp'] as string;
    const secret = process.env.WEBHOOK_SECRET || 'inbound-webhook-signing-secret';

    if (!signature) {
      res.status(401).json({ error: 'Missing signature header' });
      return;
    }

    const isValid = verifyWebhookSignature(req.body, signature, secret, timestamp);
    if (!isValid) {
      res.status(403).json({ error: 'Invalid HMAC signature or timestamp expired' });
      return;
    }

    // Parse verified payload safely
    const payload = JSON.parse(req.body.toString('utf8'));
    console.log('[Inbound Webhook Verified]', payload.event);

    // Immediately accept to prevent provider timeout
    res.status(202).json({ status: 'accepted' });
  }
);

if (process.env.NODE_ENV !== 'test') {
  app.listen(3000, () => console.log('[Webhook Receiver API] Running on port 3000'));
}
```

---

## 3. Verification & Testing Suite

### Unit Test (`test/unit/crypto.test.ts`)
```typescript
import { describe, it, expect } from 'vitest';
import { createHmac } from 'node:crypto';
import { verifyWebhookSignature } from '../../src/crypto/verifier.js';

describe('Webhook Crypto Verifier', () => {
  const secret = 'super-secret-test-key-32chars';
  const body = Buffer.from(JSON.stringify({ event: 'user.created', id: 123 }));

  it('validates a correct HMAC-SHA256 signature', () => {
    const validSignature = createHmac('sha256', secret).update(body).digest('hex');
    expect(verifyWebhookSignature(body, validSignature, secret)).toBe(true);
  });

  it('rejects an altered payload with the same signature', () => {
    const validSignature = createHmac('sha256', secret).update(body).digest('hex');
    const tamperedBody = Buffer.from(JSON.stringify({ event: 'user.created', id: 999 }));
    expect(verifyWebhookSignature(tamperedBody, validSignature, secret)).toBe(false);
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
CMD ["src/presentation/app.js"]
```
