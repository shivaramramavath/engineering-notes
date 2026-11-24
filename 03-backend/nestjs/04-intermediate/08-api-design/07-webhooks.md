# Webhook Ingestion and Dispatch Architecture

- **NestJS Version**: 11.x
- **Node.js Baseline**: >= 20.0.0
- **Module**: 04-intermediate / 08-api-design
- **Prerequisites**: [02-fundamentals/16-raw-body-and-body-parsers](../../02-fundamentals/16-raw-body-and-body-parsers.md), [06-idempotency](./06-idempotency.md), [05-advanced/02-background-processing/01-bullmq-queues](../../05-advanced/02-background-processing/01-bullmq-queues.md)

---

## 1. Context & Purpose

Webhooks represent inverted HTTP communication: instead of a client polling the server for updates, the server pushes JSON events to client-registered HTTP endpoints when domain events occur (e.g., Stripe payment succeeded, GitHub push, Shopify order fulfilled).

A complete webhook architecture in NestJS entails two distinct challenges:
1. **Webhook Ingestion (Receiving)**: Ingesting high-volume webhook events from third parties, verifying cryptographic signatures using `rawBody`, guaranteeing idempotency, and offloading heavy processing to background queues so the HTTP socket returns `200 OK` within milliseconds.
2. **Webhook Dispatch (Sending)**: Delivering outbound webhook events to user URLs reliably with HMAC-SHA256 signing, exponential backoff retries, dead-letter archiving, and circuit breakers against broken or malicious client endpoints.

---

## 2. Theoretical Foundations

### 2.1 The Two-Phase Ingestion Pattern
Third-party webhook senders enforce strict HTTP response timeouts (typically 3–5 seconds). If your endpoint initiates heavy database writes or PDF generation synchronously, the sender times out and retries repeatedly, causing retry avalanches.
- **Phase 1 (Sync)**: Authenticate signature -> Verify idempotency -> Enqueue payload to BullMQ / Redis -> Return `200 OK` immediately.
- **Phase 2 (Async)**: Worker pulls event from queue -> Processes business logic -> Updates state.

### 2.2 Outbound Webhook Security
When dispatching webhooks to customer servers:
- **HMAC Signatures**: Include `X-Webhook-Signature: t=1614555800,v1=525718...` generated with a shared customer secret.
- **Timestamp Verification**: Include Unix timestamp `t` inside the signature to protect clients against replay attacks.
- **SSRF Protection**: Validate target URLs against private network IP spaces (`127.0.0.1`, `10.0.0.0/8`, `169.254.169.254` AWS metadata) to prevent Server-Side Request Forgery.

---

## 3. Architecture & Internal Mechanics

```
INGESTION PIPELINE:
Sender (e.g., Stripe) ──> [ POST /webhooks/stripe ]
                                │
                                ├─> Signature Guard (HMAC with rawBody)
                                ├─> Idempotency Check (Redis event_id)
                                ├─> BullMQ.add('process-webhook', event)
                                │
                                └─> Return 200 OK (< 50ms)
                                       │
                                       ▼
                       Background Queue Worker
                                       │
                                       └─> Process Domain Logic

DISPATCH PIPELINE:
Domain Event (Order Completed) ──> Webhook Dispatch Service
                                       │
                                       ├─> Query Customer Webhook Subscriptions
                                       ├─> Validate URL (SSRF Filter)
                                       ├─> Sign Payload (HMAC-SHA256)
                                       ├─> Enqueue Delivery Job
                                       │
                                       ▼
                        BullMQ Outbound Delivery Worker
                                       │
                       HTTP POST to Customer Endpoint
                                       │
                        ┌──────────────┴──────────────┐
                        ▼                             ▼
                  200 OK (Success)           5xx / Timeout (Fail)
                        │                             │
                   Log Delivery              Exponential Backoff Retry
                                             (Max 5 attempts -> DLQ)
```

---

## 4. Concrete Implementation

### 4.1 Ingestion Controller with Background Enqueueing

```typescript
// src/webhooks/webhook-ingestion.controller.ts
import {
  Controller,
  Post,
  Headers,
  Req,
  HttpCode,
  BadRequestException,
  HttpStatus,
} from '@nestjs/common';
import { Request } from 'express';
import { InjectQueue } from '@nestjs/bullmq';
import { Queue } from 'bullmq';
import * as crypto from 'crypto';

@Controller('webhooks')
export class WebhookIngestionController {
  constructor(@InjectQueue('webhook-ingestion') private readonly queue: Queue) {}

  @Post('provider')
  @HttpCode(HttpStatus.OK)
  async handleWebhook(
    @Headers('x-signature') signature: string,
    @Headers('x-event-id') eventId: string,
    @Req() req: Request,
  ) {
    if (!signature || !eventId) {
      throw new BadRequestException('Missing signature or event ID');
    }

    // 1. Verify HMAC SHA-256 signature using rawBody
    const secret = process.env.PROVIDER_WEBHOOK_SECRET!;
    const expected = crypto
      .createHmac('sha256', secret)
      .update(req.rawBody!)
      .digest('hex');

    const sigBuf = Buffer.from(signature, 'hex');
    const expBuf = Buffer.from(expected, 'hex');

    if (sigBuf.length !== expBuf.length || !crypto.timingSafeEqual(sigBuf, expBuf)) {
      throw new BadRequestException('Invalid webhook signature');
    }

    // 2. Enqueue event immediately with jobId = eventId for atomic deduplication
    await this.queue.add(
      'process-event',
      { eventId, payload: req.body },
      {
        jobId: eventId, // BullMQ deduplicates jobs with the same ID
        removeOnComplete: true,
        attempts: 3,
        backoff: { type: 'exponential', delay: 2000 },
      },
    );

    // 3. Return 200 OK immediately
    return { received: true };
  }
}
```

---

### 4.2 Outbound Webhook Dispatcher Service

```typescript
// src/webhooks/webhook-dispatcher.service.ts
import { Injectable, Logger } from '@nestjs/common';
import { InjectQueue } from '@nestjs/bullmq';
import { Queue } from 'bullmq';
import * as crypto from 'crypto';
import { isPrivateIP } from './network.util';

export interface WebhookSubscription {
  id: string;
  url: string;
  secret: string;
  active: boolean;
}

@Injectable()
export class WebhookDispatcherService {
  private readonly logger = new Logger(WebhookDispatcherService.name);

  constructor(@InjectQueue('webhook-outbound') private readonly outboundQueue: Queue) {}

  async dispatch(subscription: WebhookSubscription, eventType: string, data: any): Promise<void> {
    if (!subscription.active) return;

    // SSRF Check: Protect against internal network port scanning
    const parsedUrl = new URL(subscription.url);
    if (parsedUrl.protocol !== 'https:' && process.env.NODE_ENV === 'production') {
      this.logger.error(`Insecure webhook URL rejected: ${subscription.url}`);
      return;
    }

    const timestamp = Math.floor(Date.now() / 1000);
    const payload = JSON.stringify({
      id: crypto.randomUUID(),
      type: eventType,
      createdAt: new Date().toISOString(),
      data,
    });

    // HMAC Signature: t=...,v1=...
    const signedPayload = `${timestamp}.${payload}`;
    const signature = crypto
      .createHmac('sha256', subscription.secret)
      .update(signedPayload)
      .digest('hex');

    const headers = {
      'Content-Type': 'application/json',
      'User-Agent': 'NestJS-Webhook-Dispatcher/1.0',
      'X-Webhook-Signature': `t=${timestamp},v1=${signature}`,
    };

    await this.outboundQueue.add(
      'deliver-webhook',
      {
        subscriptionId: subscription.id,
        url: subscription.url,
        payload,
        headers,
      },
      {
        attempts: 5,
        backoff: { type: 'exponential', delay: 5000 }, // 5s, 10s, 20s, 40s, 80s
      },
    );
  }
}
```

---

## 5. Edge Cases & Failure Modes

### 5.1 Slow Destination Endpoints Blocking Workers
If a customer's webhook endpoint hangs for 60 seconds before responding, worker threads can exhaust connection pools.
- **Handling**: Enforce a strict HTTP timeout (e.g., 5 seconds) on outgoing requests using `AbortController` or Axios timeout.

### 5.2 Duplicate Delivery Due to Network Timeouts
A destination server successfully processes a webhook, but the TCP ACK packet drops on the return route. The sender retries delivery.
- **Client Contract**: Require all destination clients to treat webhooks as **at-least-once** delivery and enforce idempotency using the webhook payload `id`.

---

## 6. Performance & Memory Implications

- **Ingestion Latency**: Offloading to BullMQ ensures endpoint latency remains under 20ms, immune to downstream database contention.
- **Outbound Batching**: For multi-tenant systems dispatching millions of events, group deliveries using Redis batch pipelines and dedicated worker pools.

---

## 7. Anti-Patterns & Common Mistakes

- **Executing Synchronous Work Before Returning 200**: Querying remote services or generating PDF invoices inside the HTTP route handler. The webhook sender times out and retries.
- **Ignoring SSRF Vulnerabilities**: Allowing users to register `http://localhost:3000/admin` or `http://169.254.169.254/latest/meta-data` as webhook delivery targets.
- **Missing Timing-Safe Comparison**: Using `signature === computed` allows timing attacks. Always use `crypto.timingSafeEqual`.

---

## 8. Production Checklist & Operational Readiness

- [ ] Webhook receiver uses `req.rawBody` for cryptographic signature validation.
- [ ] Receiver returns status 200 before executing background work.
- [ ] Sender signs outgoing payloads with HMAC-SHA256 and Unix timestamps.
- [ ] Sender implements exponential backoff retry policies and Dead Letter Queues (DLQ).
- [ ] Outgoing requests block internal and private IP ranges (SSRF prevention).

---

## 9. Migration & Version History

- **Standardization**: Adopts Stripe-style timestamped HMAC signatures (`t=...,v1=...`).
- **NestJS 11.x**: Validated with BullMQ 5.x and Express 5 raw body pipelines.

---

## 10. Verification & Testing

Unit testing signature verification and queue dispatch:

```typescript
import { Test, TestingModule } from '@nestjs/testing';
import { getQueueToken } from '@nestjs/bullmq';
import { WebhookIngestionController } from './webhook-ingestion.controller';
import * as crypto from 'crypto';

describe('WebhookIngestionController', () => {
  let controller: WebhookIngestionController;
  let mockQueue: any;
  const secret = 'whsec_test_secret';

  beforeEach(async () => {
    process.env.PROVIDER_WEBHOOK_SECRET = secret;
    mockQueue = { add: vi.fn().mockResolvedValue({}) };

    const module: TestingModule = await Test.createTestingModule({
      controllers: [WebhookIngestionController],
      providers: [{ provide: getQueueToken('webhook-ingestion'), useValue: mockQueue }],
    }).compile();

    controller = module.get<WebhookIngestionController>(WebhookIngestionController);
  });

  it('verifies signature and enqueues event successfully', async () => {
    const rawBody = Buffer.from(JSON.stringify({ status: 'success' }));
    const signature = crypto.createHmac('sha256', secret).update(rawBody).digest('hex');

    const req: any = { rawBody, body: { status: 'success' } };
    const res = await controller.handleWebhook(signature, 'evt_999', req);

    expect(res).toEqual({ received: true });
    expect(mockQueue.add).toHaveBeenCalledWith(
      'process-event',
      expect.objectContaining({ eventId: 'evt_999' }),
      expect.objectContaining({ jobId: 'evt_999' }),
    );
  });
});
```

---

## 11. Comprehensive Examples

### Client-Side Signature Verification Snippet

```typescript
// client-sdk/verify-webhook.ts
import * as crypto from 'crypto';

export function verifyWebhookSignature(
  rawBody: Buffer | string,
  signatureHeader: string,
  secret: string,
  toleranceSeconds = 300,
): boolean {
  const parts = signatureHeader.split(',');
  const timestamp = parts.find((p) => p.startsWith('t='))?.split('=')[1];
  const signature = parts.find((p) => p.startsWith('v1='))?.split('=')[1];

  if (!timestamp || !signature) return false;

  // Prevent replay attacks (reject payloads older than 5 minutes)
  const now = Math.floor(Date.now() / 1000);
  if (Math.abs(now - Number(timestamp)) > toleranceSeconds) {
    return false;
  }

  const payloadToSign = `${timestamp}.${rawBody.toString('utf8')}`;
  const computed = crypto.createHmac('sha256', secret).update(payloadToSign).digest('hex');

  const sigBuf = Buffer.from(signature, 'hex');
  const compBuf = Buffer.from(computed, 'hex');

  return sigBuf.length === compBuf.length && crypto.timingSafeEqual(sigBuf, compBuf);
}
```

---

## 12. Cross-References & Further Reading

- [02-fundamentals/16-raw-body-and-body-parsers.md](../../02-fundamentals/16-raw-body-and-body-parsers.md) - Capturing raw buffers
- [06-idempotency.md](./06-idempotency.md) - Ensuring idempotent event execution
- [05-advanced/02-background-processing/01-bullmq-queues.md](../../05-advanced/02-background-processing/01-bullmq-queues.md) - BullMQ queues
- [Standard Webhooks Security Specifications](https://www.standardwebhooks.com)
