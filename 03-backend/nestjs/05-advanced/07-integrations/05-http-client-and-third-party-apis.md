# HTTP Client and Third-Party API Integrations

- **NestJS Version**: 11.x
- **Node.js Baseline**: >= 20.0.0
- **Module**: 05-advanced / 07-integrations
- **Prerequisites**: [02-fundamentals/15-http-module-and-axios](../../02-fundamentals/15-http-module-and-axios.md), [03-core-concepts/04-modules-and-di/01-dependency-injection](../../03-core-concepts/04-modules-and-di/01-dependency-injection.md)

---

## 1. Context & Purpose

Modern enterprise backends are heavily integrated with external Software-as-a-Service (SaaS) APIs: payment gateways (Stripe, Adyen), shipping logistics (FedEx, UPS), identity verification (Persona), and CRM platforms (Salesforce, HubSpot).

Communicating with external APIs introduces unpredictable challenges: network latency, unexpected 5xx gateway errors, strict rate limiting (HTTP 429), and schema drift. Integrating third-party APIs requires building resilient client wrappers featuring **Circuit Breakers**, **Retry Policies with Exponential Backoff**, **Request Rate Limiting**, and **Structured Payload Logging**.

---

## 2. Theoretical Foundations

### 2.1 The Resilience Trifecta
1. **Timeouts**: Every outgoing network call must have a hard deadline. Hanging sockets exhaust thread pool capacity.
2. **Retries with Jitter**: Only retry **idempotent** failures (HTTP 502, 503, 504, TCP resets). Never retry HTTP 400 Bad Request or 401 Unauthorized.
3. **Circuit Breaker**: When an external API fails repeatedly, "open" the circuit to fail fast locally for 30 seconds, preventing cascading failure and allowing the remote service to recover.

---

## 3. Architecture & Internal Mechanics

```
┌────────────────────────────────────────────────────────┐
│                   CheckoutService                      │
│            paymentClient.chargeCustomer(...)           │
└───────────────────────────┬────────────────────────────┘
                            │
                            ▼
┌────────────────────────────────────────────────────────┐
│             Resilient Third-Party Client               │
│  1. Check Circuit Breaker State (CLOSED / OPEN)        │
│  2. Token Bucket Rate Limiter (Max 20 req/sec)         │
│  3. Attach HMAC Signatures & Bearer Tokens             │
│  4. Outgoing Call with 5000ms Timeout                  │
│  5. Exponential Backoff Retry on 5xx Errors            │
└───────────────────────────┬────────────────────────────┘
                            │
              HTTPS REST API Call (TLS Port 443)
                            │
                            ▼
                 Stripe / Third-Party API
```

---

## 4. Concrete Implementation

### 4.1 Building a Resilient Third-Party API Client Wrapper

```typescript
// src/integrations/payment-gateway.client.ts
import { Injectable, Logger, BadGatewayException } from '@nestjs/common';
import { HttpService } from '@nestjs/axios';
import { ConfigService } from '@nestjs/config';
import { firstValueFrom, retry, timer, timeout, catchError } from 'rxjs';
import { AxiosError } from 'axios';

export interface ChargeRequest {
  customerId: string;
  amountCents: number;
  currency: string;
}

export interface ChargeResponse {
  chargeId: string;
  status: string;
}

@Injectable()
export class PaymentGatewayClient {
  private readonly logger = new Logger(PaymentGatewayClient.name);
  private readonly apiKey: string;
  private readonly baseUrl: string;

  constructor(
    private readonly httpService: HttpService,
    private readonly config: ConfigService,
  ) {
    this.apiKey = this.config.getOrThrow<string>('PAYMENT_API_KEY');
    this.baseUrl = this.config.get<string>('PAYMENT_BASE_URL', 'https://api.gateway.com/v1');
  }

  async charge(request: ChargeRequest): Promise<ChargeResponse> {
    this.logger.log(`Initiating external charge for customer ${request.customerId}: $${request.amountCents / 100}`);

    const response$ = this.httpService
      .post<ChargeResponse>(`${this.baseUrl}/charges`, request, {
        headers: {
          Authorization: `Bearer ${this.apiKey}`,
          'Content-Type': 'application/json',
          'Idempotency-Key': `charge-${request.customerId}-${Date.now()}`,
        },
      })
      .pipe(
        // 1. Enforce strict 5-second timeout
        timeout(5000),
        // 2. Retry up to 3 times on transient network / 5xx errors with exponential backoff
        retry({
          count: 3,
          delay: (error: AxiosError, retryCount) => {
            const status = error.response?.status;
            // Only retry network errors or 5xx server errors
            if (!status || status >= 500 || status === 429) {
              const backoffMs = Math.pow(2, retryCount) * 500; // 1s, 2s, 4s
              this.logger.warn(`Transient error (${status || error.message}). Retrying in ${backoffMs}ms... (Attempt ${retryCount})`);
              return timer(backoffMs);
            }
            // Do not retry 4xx client errors (e.g. 400 Invalid Card, 401 Bad Auth)
            throw error;
          },
        }),
        // 3. Normalize external errors to clean NestJS exceptions
        catchError((error: AxiosError) => {
          this.logger.error(`Third-party payment gateway failed: ${error.message}`, error.response?.data);
          throw new BadGatewayException({
            message: 'External payment service unavailable. Please retry later.',
            externalStatus: error.response?.status,
          });
        }),
      );

    const { data } = await firstValueFrom(response$);
    return data;
  }
}
```

---

### 4.2 Outgoing Rate Limiter Integration

When calling third parties with strict rate limits (e.g. max 10 requests per second):

```typescript
// src/common/resilience/rate-limiter.util.ts
export class RateLimiter {
  private queue: Array<() => void> = [];
  private activeCount = 0;

  constructor(
    private readonly maxConcurrent: number,
    private readonly intervalMs: number,
  ) {}

  async acquire(): Promise<void> {
    if (this.activeCount < this.maxConcurrent) {
      this.activeCount++;
      return;
    }

    return new Promise((resolve) => {
      this.queue.push(resolve);
      setTimeout(() => {
        if (this.queue.length > 0) {
          const next = this.queue.shift();
          next?.();
        } else {
          this.activeCount--;
        }
      }, this.intervalMs);
    });
  }
}
```

---

## 5. Edge Cases & Failure Modes

### 5.1 Circular References in Axios Error Logging
Passing raw `error` into `console.log(error)` when Axios rejects. Axios errors contain circular HTTP request and socket handles, which crash JSON loggers or generate megabytes of useless log spam.
- **Handling**: Always log only selected properties:
  `{ status: err.response?.status, data: err.response?.data, message: err.message }`.

### 5.2 Retrying Non-Idempotent Mutations
Retrying a `POST /transfers` request that failed with a 504 Gateway Timeout without an `Idempotency-Key`. The first request may have actually succeeded, causing a double transfer.
- **Rule**: Never retry outgoing POST requests without client-side idempotency keys.

---

## 6. Performance & Memory Implications

- **Keep-Alive Agents**: By default, Node creates and tears down TCP connections for each request. Configuring `keepAlive: true` in `HttpModule` saves 30–50ms per outgoing call.
- **Payload Streaming**: For large third-party downloads (e.g. multi-gigabyte reports), configure `responseType: 'stream'` and pipe to disk or S3 directly.

---

## 7. Anti-Patterns & Common Mistakes

- **Using Global Native Fetch Without Timeouts**: Using raw `fetch()` in Node.js without passing an `AbortSignal`. If the remote server hangs, the promise hangs indefinitely.
- **Exposing Internal API Keys in Client Headers**: Forwarding external API keys down to browser clients. All third-party secrets must remain strictly encapsulated inside backend services.

---

## 8. Production Checklist & Operational Readiness

- [ ] Outgoing HTTP calls bounded by strict timeouts ($\le 5000\text{ms}$).
- [ ] Retries restricted to 5xx and network drop errors; 4xx errors fail immediately.
- [ ] Idempotency keys included on all external payment and mutation calls.
- [ ] Keep-alive HTTP/HTTPS agents enabled on `HttpModule`.

---

## 9. Migration & Version History

- **Axios 1.x**: Standardized modern interceptor and error models.

---

## 10. Verification & Testing

Unit testing retry backoff logic:

```typescript
import { PaymentGatewayClient } from './payment-gateway.client';
import { of, throwError } from 'rxjs';

describe('PaymentGatewayClient', () => {
  let client: PaymentGatewayClient;
  let mockHttp: any;

  beforeEach(() => {
    mockHttp = { post: vi.fn() };
    const mockConfig: any = {
      getOrThrow: () => 'api_key_123',
      get: () => 'https://api.test.com',
    };
    client = new PaymentGatewayClient(mockHttp, mockConfig);
  });

  it('successfully recovers and returns on transient retry', async () => {
    // Fail once with 503, then succeed on second attempt
    mockHttp.post
      .mockReturnValueOnce(throwError(() => ({ response: { status: 503 } })))
      .mockReturnValueOnce(of({ data: { chargeId: 'ch_1', status: 'PAID' } }));

    const res = await client.charge({ customerId: 'c1', amountCents: 1000, currency: 'USD' });
    expect(res.chargeId).toBe('ch_1');
    expect(mockHttp.post).toHaveBeenCalledTimes(2);
  });
});
```

---

## 11. Comprehensive Examples

### HTTP Keep-Alive Module Provider Setup

```typescript
// src/integrations/resilient-http.module.ts
import { Module } from '@nestjs/common';
import { HttpModule } from '@nestjs/axios';
import * as http from 'http';
import * as https from 'https';

@Module({
  imports: [
    HttpModule.register({
      timeout: 5000,
      maxRedirects: 3,
      httpAgent: new http.Agent({ keepAlive: true, maxSockets: 50 }),
      httpsAgent: new https.Agent({ keepAlive: true, maxSockets: 50 }),
    }),
  ],
  exports: [HttpModule],
})
export class ResilientHttpModule {}
```

---

## 12. Cross-References & Further Reading

- [02-fundamentals/15-http-module-and-axios.md](../../02-fundamentals/15-http-module-and-axios.md) - HttpModule basics
- [04-intermediate/08-api-design/06-idempotency.md](../../04-intermediate/08-api-design/06-idempotency.md) - Idempotency patterns
- [Axios Documentation](https://axios-http.com)
