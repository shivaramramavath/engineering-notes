# HTTP Module and Axios Integration

- **NestJS Version**: 11.x
- **Node.js Baseline**: >= 20.0.0
- **Module**: 02-fundamentals
- **Prerequisites**: [04-providers-and-services](./04-providers-and-services.md), [05-dependency-injection](./05-dependency-injection.md), [00-prerequisites/03-nodejs-async-and-event-loop](../00-prerequisites/03-nodejs-async-and-event-loop.md)

---

## 1. Context & Purpose

Modern backend architectures rarely live in isolation. NestJS microservices and API gateways constantly communicate with external third-party APIs (Stripe, Twilio, SendGrid), internal microservices, and upstream legacy systems via outgoing HTTP requests.

The `@nestjs/axios` package wraps the battle-tested **Axios** HTTP client into an idiomatic, injectable NestJS provider: `HttpService`. It provides declarative module configuration, centralized timeout handling, automatic request/response interceptors (for authentication headers, tracing IDs, and logging), and returns reactive RxJS `Observable` responses that easily convert to Promises via `firstValueFrom`.

---

## 2. Theoretical Foundations

Outgoing HTTP requests require resilient transport configuration:
- **Connection Reuse & Keep-Alive**: Reusing TCP connections via HTTP agents reduces handshake latency (avoiding repeated SYN-ACK and TLS handshakes).
- **Timeouts**: Every outgoing call must enforce strict connection and read timeouts to prevent thread pool starvation.
- **Interceptors**: Cross-cutting concerns—such as injecting OAuth bearer tokens, propagating W3C `traceparent` headers, or retrying idempotent failures—should be centralized in interceptors rather than duplicated in service calls.
- **Observables vs Promises**: `HttpService` returns RxJS `Observable<AxiosResponse<T>>`. While NestJS controllers and services can consume Observables directly, converting them via `firstValueFrom(obs$)` allows clean `async`/`await` ergonomics.

---

## 3. Architecture & Internal Mechanics

```
┌────────────────────────────────────────────────────────┐
│                     PaymentService                     │
│      await firstValueFrom(this.httpService.post(...))  │
└───────────────────────────┬────────────────────────────┘
                            │
                            ▼
               HttpService (@nestjs/axios)
                            │
               Axios Request Interceptor
               - Appends Authorization: Bearer <token>
               - Appends X-Correlation-ID: <uuid>
                            │
                            ▼
               Node http.Agent (Keep-Alive Pool)
                            │
                            ▼
               External Third-Party Service
                            │
                            ▼
               Axios Response Interceptor
               - Logs timing and latency
               - Normalizes error payloads
                            │
                            ▼
             RxJS Observable<AxiosResponse<T>>
```

---

## 4. Concrete Implementation

### 4.1 Installing Dependencies

```bash
npm install --save @nestjs/axios axios
```

### 4.2 Registering `HttpModule`

Register `HttpModule` either synchronously or asynchronously with custom timeout and agent settings:

```typescript
// src/payments/payments.module.ts
import { Module } from '@nestjs/common';
import { HttpModule } from '@nestjs/axios';
import { ConfigModule, ConfigService } from '@nestjs/config';
import * as http from 'http';
import * as https from 'https';
import { PaymentsService } from './payments.service';

@Module({
  imports: [
    HttpModule.registerAsync({
      imports: [ConfigModule],
      useFactory: async (configService: ConfigService) => ({
        timeout: configService.get<number>('HTTP_TIMEOUT', 5000),
        maxRedirects: 3,
        // Reuse TCP sockets with keep-alive
        httpAgent: new http.Agent({ keepAlive: true, maxSockets: 100 }),
        httpsAgent: new https.Agent({ keepAlive: true, maxSockets: 100 }),
      }),
      inject: [ConfigService],
    }),
  ],
  providers: [PaymentsService],
  exports: [PaymentsService],
})
export class PaymentsModule {}
```

---

### 4.3 Consuming `HttpService` in Providers

```typescript
// src/payments/payments.service.ts
import { Injectable, Logger, BadGatewayException } from '@nestjs/common';
import { HttpService } from '@nestjs/axios';
import { AxiosError } from 'axios';
import { firstValueFrom, catchError } from 'rxjs';

interface PaymentIntent {
  id: string;
  status: string;
  amount: number;
}

@Injectable()
export class PaymentsService {
  private readonly logger = new Logger(PaymentsService.name);

  constructor(private readonly httpService: HttpService) {}

  async createPayment(amount: number, currency: string): Promise<PaymentIntent> {
    const url = 'https://api.stripe.com/v1/payment_intents';

    const { data } = await firstValueFrom(
      this.httpService
        .post<PaymentIntent>(
          url,
          { amount, currency },
          {
            headers: {
              Authorization: `Bearer ${process.env.STRIPE_SECRET_KEY}`,
            },
          },
        )
        .pipe(
          catchError((error: AxiosError) => {
            this.logger.error(
              `Payment gateway error: ${error.message}`,
              error.response?.data,
            );
            throw new BadGatewayException('Payment gateway unavailable');
          }),
        ),
    );

    return data;
  }
}
```

---

### 4.4 Centralized Request & Response Interceptors

Configure logging and correlation tracing across all outgoing HTTP calls:

```typescript
// src/payments/http-logger.service.ts
import { Injectable, OnModuleInit } from '@nestjs/common';
import { HttpService } from '@nestjs/axios';

@Injectable()
export class HttpLoggerService implements OnModuleInit {
  constructor(private readonly httpService: HttpService) {}

  onModuleInit() {
    const axiosInstance = this.httpService.axiosRef;

    axiosInstance.interceptors.request.use((config) => {
      config.headers['X-Request-Start'] = Date.now().toString();
      return config;
    });

    axiosInstance.interceptors.response.use(
      (response) => {
        const start = Number(response.config.headers['X-Request-Start']);
        const duration = Date.now() - start;
        console.log(`[HTTP Outgoing] ${response.config.method?.toUpperCase()} ${response.config.url} took ${duration}ms`);
        return response;
      },
      (error) => Promise.reject(error),
    );
  }
}
```

---

## 5. Edge Cases & Failure Modes

### 5.1 Unhandled Axios Exceptions
Axios rejects promises on HTTP error status codes (4xx, 5xx) by default. If not caught, an unhandled AxiosError contains circular references (including Node socket objects) that can crash custom JSON loggers or leak sensitive auth headers in stack traces.
- **Handling**: Always catch errors using RxJS `catchError` or standard `try/catch`, extracting only `error.response?.status` and `error.response?.data`.

### 5.2 Thread Pool Starvation via Hanging Requests
Omitting `timeout` in `HttpModule.register()` means outgoing requests wait indefinitely if the remote server hangs, exhausting available Node.js sockets.
- **Rule**: Always enforce an explicit timeout (e.g., 3000ms–10000ms).

---

## 6. Performance & Memory Implications

- **Socket Pooling**: By default, Node's global agent uses `keepAlive: false`, tearing down TCP connections on every request. Configuring `keepAlive: true` reduces network round-trip time (RTT) by up to 50–70% for frequent internal microservice calls.
- **Memory Consumption**: Large outgoing responses stream into memory buffers. For multi-megabyte payloads, configure `responseType: 'stream'` and stream directly.

---

## 7. Anti-Patterns & Common Mistakes

- **Importing Global Axios Directly**: Using `import axios from 'axios'` directly inside service files instead of injecting `HttpService`. This bypasses dependency injection, makes unit testing difficult, and prevents centralized configuration.
- **Ignoring `firstValueFrom`**: Forgetting to convert the `Observable` and awaiting the Observable directly. An `Observable` is not a `Promise`; awaiting it without conversion returns the Observable itself or undefined.
- **Logging Sensitive Auth Headers**: Serializing `error.config` into production logs, inadvertently leaking API bearer tokens.

---

## 8. Production Checklist & Operational Readiness

- [ ] Outgoing timeout explicitly configured (e.g., `<= 5000ms`).
- [ ] Keep-alive HTTP/HTTPS agents configured with socket limits.
- [ ] Correlation IDs forwarded on all outgoing HTTP headers.
- [ ] Sensitive headers redacted before logging request/response payloads.
- [ ] Circuit breaker or retry logic applied for idempotent requests.

---

## 9. Migration & Version History

- **NestJS 7.x**: Deprecated built-in `HttpModule` in `@nestjs/common` and migrated to `@nestjs/axios`.
- **NestJS 10.x**: Full Axios 1.x compatibility.
- **NestJS 11.x**: Standardized on RxJS 7.8+ with native `firstValueFrom` recommendations, fully compatible with Node 22 native fetch while preserving Axios interceptor advantages.

---

## 10. Verification & Testing

Unit testing a service that depends on `HttpService`:

```typescript
import { Test, TestingModule } from '@nestjs/testing';
import { HttpService } from '@nestjs/axios';
import { of } from 'rxjs';
import { AxiosResponse } from 'axios';
import { PaymentsService } from './payments.service';

describe('PaymentsService', () => {
  let service: PaymentsService;
  let httpService: HttpService;

  beforeEach(async () => {
    const mockHttpService = {
      post: vi.fn(),
    };

    const module: TestingModule = await Test.createTestingModule({
      providers: [
        PaymentsService,
        { provide: HttpService, useValue: mockHttpService },
      ],
    }).compile();

    service = module.get<PaymentsService>(PaymentsService);
    httpService = module.get<HttpService>(HttpService);
  });

  it('successfully creates payment intent', async () => {
    const mockResponse: AxiosResponse = {
      data: { id: 'pi_123', status: 'succeeded', amount: 5000 },
      status: 200,
      statusText: 'OK',
      headers: {},
      config: {} as any,
    };

    vi.spyOn(httpService, 'post').mockReturnValue(of(mockResponse));

    const result = await service.createPayment(5000, 'USD');
    expect(result.id).toBe('pi_123');
  });
});
```

---

## 11. Comprehensive Examples

### Resilient Client with Retry Logic

```typescript
// src/common/http/resilient-caller.ts
import { HttpService } from '@nestjs/axios';
import { Injectable } from '@nestjs/common';
import { firstValueFrom, retry, timer } from 'rxjs';

@Injectable()
export class ResilientCallerService {
  constructor(private readonly httpService: HttpService) {}

  async fetchWithRetry<T>(url: string, attempts = 3): Promise<T> {
    const response$ = this.httpService.get<T>(url).pipe(
      retry({
        count: attempts,
        delay: (error, retryCount) => {
          // Exponential backoff: 200ms, 400ms, 800ms...
          const backoffTime = Math.pow(2, retryCount) * 100;
          return timer(backoffTime);
        },
      }),
    );

    const { data } = await firstValueFrom(response$);
    return data;
  }
}
```

---

## 12. Cross-References & Further Reading

- [04-providers-and-services.md](./04-providers-and-services.md) - Provider patterns
- [05-dependency-injection.md](./05-dependency-injection.md) - Injecting external modules
- [05-advanced/06-microservices/01-microservices-overview.md](../05-advanced/06-microservices/01-microservices-overview.md) - Internal microservice communications
- [NestJS Axios Guide](https://docs.nestjs.com/techniques/http-module)
- [Axios Official Documentation](https://axios-http.com)
