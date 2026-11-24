# Raw Body and Body Parsers

- **NestJS Version**: 11.x
- **Node.js Baseline**: >= 20.0.0
- **Module**: 02-fundamentals
- **Prerequisites**: [07-request-data](./07-request-data.md), [11-platform-express-vs-fastify](./11-platform-express-vs-fastify.md), [00-prerequisites/04-http-and-rest-basics](../00-prerequisites/04-http-and-rest-basics.md)

---

## 1. Context & Purpose

In standard REST APIs, incoming HTTP request bodies are parsed automatically by middleware into parsed JavaScript objects (`req.body`). However, critical integrations—such as **Webhook Signature Verification** (Stripe, GitHub, Shopify, Slack) and cryptographic signature validation—require access to the exact, unparsed, byte-for-byte **Raw Body** (`Buffer`).

If a parser parses the body to JSON first, field order, indentation, or Unicode escapes may change. Re-serializing with `JSON.stringify(req.body)` alters the byte signature, causing cryptographic signature verification to fail and resulting in rejected webhooks.

NestJS provides a built-in `rawBody: true` option that preserves the raw buffer on `req.rawBody` while simultaneously providing parsed JSON in `req.body`.

---

## 2. Theoretical Foundations

Cryptographic webhook verification uses HMAC (Hash-based Message Authentication Code), typically SHA-256:
$$\text{HMAC-SHA256}(\text{RawBytes}, \text{SecretKey}) == \text{SignatureHeader}$$

Any mutation of `RawBytes`—including trimming whitespace, normalizing CRLF to LF, reordering JSON keys, or floating-point precision changes—produces an entirely different digest. Therefore, the raw byte stream from the TCP socket must be intercepted and captured before or during body parsing.

---

## 3. Architecture & Internal Mechanics

```
Incoming Request Stream (Bytes)
              │
              ▼
    NestJS Body Parser Layer
    (when rawBody: true is enabled)
              │
              ├─> Intercepts raw stream chunks
              ├─> Buffers exact bytes onto req.rawBody (Buffer)
              │
              ▼
    Standard Parser (JSON / URL-encoded)
              │
              ├─> Parses bytes into JavaScript object
              ├─> Sets req.body = { ... }
              │
              ▼
    Controller / Route Handler
    - @Body() dto: StripeWebhookDto   <-- Accesses parsed JSON
    - @Req() req.rawBody              <-- Accesses raw Buffer for HMAC verification
```

---

## 4. Concrete Implementation

### 4.1 Enabling `rawBody` Globally

Configure `rawBody: true` in `main.ts`:

```typescript
// src/main.ts
import { NestFactory } from '@nestjs/core';
import { AppModule } from './app.module';

async function bootstrap() {
  const app = await NestFactory.create(AppModule, {
    rawBody: true, // Preserves req.rawBody across all requests
  });

  await app.listen(3000);
}
bootstrap();
```

---

### 4.2 Webhook Signature Verification Controller (Stripe Example)

```typescript
// src/webhooks/stripe-webhook.controller.ts
import {
  Controller,
  Post,
  Headers,
  Req,
  BadRequestException,
  HttpCode,
} from '@nestjs/common';
import { Request } from 'express';
import Stripe from 'stripe';

const stripe = new Stripe(process.env.STRIPE_SECRET_KEY as string, {
  apiVersion: '2023-10-16',
});

@Controller('webhooks')
export class StripeWebhookController {
  @Post('stripe')
  @HttpCode(200)
  handleWebhook(
    @Headers('stripe-signature') signature: string,
    @Req() req: Request,
  ) {
    if (!signature) {
      throw new BadRequestException('Missing stripe-signature header');
    }

    // Access the raw byte buffer captured by NestJS
    const rawBody = req.rawBody;
    if (!rawBody) {
      throw new BadRequestException('Raw body buffer unavailable');
    }

    let event: Stripe.Event;

    try {
      // Cryptographically verify signature using raw buffer
      event = stripe.webhooks.constructEvent(
        rawBody,
        signature,
        process.env.STRIPE_WEBHOOK_SECRET as string,
      );
    } catch (err: any) {
      throw new BadRequestException(`Webhook signature verification failed: ${err.message}`);
    }

    // Process verified event safely
    switch (event.type) {
      case 'payment_intent.succeeded':
        const paymentIntent = event.data.object as Stripe.PaymentIntent;
        console.log(`Payment succeeded: ${paymentIntent.id}`);
        break;
      default:
        console.log(`Unhandled event type: ${event.type}`);
    }

    return { received: true };
  }
}
```

---

### 4.3 Custom Raw Body Param Decorator

Create a reusable decorator for clean controller signatures:

```typescript
// src/common/decorators/raw-body.decorator.ts
import { createParamDecorator, ExecutionContext, BadRequestException } from '@nestjs/common';
import { Request } from 'express';

export const RawBody = createParamDecorator(
  (data: unknown, ctx: ExecutionContext): Buffer => {
    const req = ctx.switchToHttp().getRequest<Request>();
    if (!req.rawBody) {
      throw new BadRequestException(
        'Raw body not available. Ensure rawBody: true is enabled in NestFactory.create()',
      );
    }
    return req.rawBody;
  },
);
```

Usage:
```typescript
@Post('github')
handleGitHubWebhook(
  @Headers('x-hub-signature-256') signature: string,
  @RawBody() rawBody: Buffer,
) {
  // Verify GitHub HMAC SHA256 using rawBody Buffer
}
```

---

### 4.4 Configuring Custom Body Size Limits

To prevent Denial of Service (DoS) attacks via oversized payloads:

```typescript
// src/main.ts (Express)
import { NestFactory } from '@nestjs/core';
import { json, urlencoded } from 'express';
import { AppModule } from './app.module';

async function bootstrap() {
  const app = await NestFactory.create(AppModule, { rawBody: true });

  // Custom limit for large webhook or file payloads
  app.use(json({ limit: '5mb' }));
  app.use(urlencoded({ extended: true, limit: '5mb' }));

  await app.listen(3000);
}
bootstrap();
```

---

## 5. Edge Cases & Failure Modes

### 5.1 Fastify Raw Body Compatibility
In Fastify (`@nestjs/platform-fastify`), enabling `rawBody: true` requires installing `fastify-raw-body`:
```bash
npm install --save fastify-raw-body
```
Register the plugin in `main.ts` or pass `rawBody: true` to the `NestFactory` options. Without this plugin, `req.rawBody` remains undefined in Fastify applications.

### 5.2 Third-Party Body Parsers Overriding Raw Body
If custom middleware (e.g. legacy `body-parser`) is registered manually after `NestFactory.create()`, it can overwrite `req.body` or consume the request stream before Nest's raw-body collector runs.
- **Rule**: Avoid registering external `body-parser` packages directly; rely on Nest's built-in options.

---

## 6. Performance & Memory Implications

- **Memory Overhead**: Storing both `req.rawBody` (Buffer) and `req.body` (Object) doubles the memory footprint of each incoming request while it is being processed.
- **Garbage Collection**: Buffers are freed immediately after the HTTP request lifecycle terminates. For endpoints processing multi-megabyte payloads, consider streaming directly to disk rather than retaining them in `rawBody`.

---

## 7. Anti-Patterns & Common Mistakes

- **Re-stringifying Parsed JSON for Verification**: Calling `stripe.webhooks.constructEvent(JSON.stringify(req.body), ...)` when verification fails. This fails unpredictably in production due to key ordering.
- **Disabling Global Body Parser Entirely**: Disabling body parsing framework-wide just for one webhook route, forcing all standard controllers to parse JSON manually.
- **Ignoring Payload Limits**: Setting `limit: '100mb'` globally, leaving the backend vulnerable to memory exhaustion attacks.

---

## 8. Production Checklist & Operational Readiness

- [ ] `rawBody: true` enabled in `NestFactory.create()` options.
- [ ] Webhook signature secrets loaded securely from environment variables.
- [ ] Payload size limits explicitly set (e.g., 2MB–5MB).
- [ ] Webhook controllers return status 200 promptly before launching long background jobs.
- [ ] Constant-time comparison (`crypto.timingSafeEqual`) used for HMAC comparison to prevent timing attacks.

---

## 9. Migration & Version History

- **NestJS 8.x**: First added official `rawBody: true` support to `NestApplicationOptions`.
- **NestJS 10.x**: Improved Fastify raw body integration and memory cleanup.
- **NestJS 11.x**: Validated with Express 5 internal body-parser mechanics.

---

## 10. Verification & Testing

Testing webhook signature verification with a simulated raw buffer:

```typescript
import { Test, TestingModule } from '@nestjs/testing';
import { INestApplication } from '@nestjs/common';
import * as request from 'supertest';
import * as crypto from 'crypto';
import { StripeWebhookController } from './stripe-webhook.controller';

describe('Stripe Webhook (e2e)', () => {
  let app: INestApplication;
  const secret = 'whsec_test_secret';

  beforeAll(async () => {
    process.env.STRIPE_WEBHOOK_SECRET = secret;
    const moduleFixture: TestingModule = await Test.createTestingModule({
      controllers: [StripeWebhookController],
    }).compile();

    app = moduleFixture.createNestApplication({ rawBody: true });
    await app.init();
  });

  afterAll(async () => {
    await app.close();
  });

  it('rejects webhooks with missing signature', () => {
    return request(app.getHttpServer())
      .post('/webhooks/stripe')
      .send({ id: 'evt_123' })
      .expect(400);
  });
});
```

---

## 11. Comprehensive Examples

### Generic HMAC Verification Guard

```typescript
// src/common/guards/hmac-verification.guard.ts
import {
  Injectable,
  CanActivate,
  ExecutionContext,
  UnauthorizedException,
} from '@nestjs/common';
import * as crypto from 'crypto';
import { Request } from 'express';

@Injectable()
export class HmacVerificationGuard implements CanActivate {
  canActivate(context: ExecutionContext): boolean {
    const req = context.switchToHttp().getRequest<Request>();
    const signature = req.headers['x-signature-sha256'] as string;
    const rawBody = req.rawBody;

    if (!signature || !rawBody) {
      throw new UnauthorizedException('Missing signature or raw body');
    }

    const secret = process.env.WEBHOOK_SECRET || 'secret';
    const computedHmac = crypto
      .createHmac('sha256', secret)
      .update(rawBody)
      .digest('hex');

    const signatureBuffer = Buffer.from(signature, 'hex');
    const computedBuffer = Buffer.from(computedHmac, 'hex');

    if (
      signatureBuffer.length !== computedBuffer.length ||
      !crypto.timingSafeEqual(signatureBuffer, computedBuffer)
    ) {
      throw new UnauthorizedException('Invalid cryptographic signature');
    }

    return true;
  }
}
```

---

## 12. Cross-References & Further Reading

- [07-request-data.md](./07-request-data.md) - Request decorators & data extraction
- [04-intermediate/08-api-design/07-webhooks.md](../04-intermediate/08-api-design/07-webhooks.md) - Webhook architecture & security
- [Stripe Webhook Signature Verification Guide](https://stripe.com/docs/webhooks/signatures)
