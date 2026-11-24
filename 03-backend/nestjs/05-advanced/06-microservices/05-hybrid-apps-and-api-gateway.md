# Hybrid Applications and API Gateway Pattern

- **NestJS Version**: 11.x
- **Node.js Baseline**: >= 20.0.0
- **Module**: 05-advanced / 06-microservices
- **Prerequisites**: [02-fundamentals/18-hybrid-applications-intro](../../02-fundamentals/18-hybrid-applications-intro.md), [01-microservices-architecture](./01-microservices-architecture.md)

---

## 1. Context & Purpose

Exposing internal microservices directly to the public internet creates severe security risks, protocol incompatibilities (browsers cannot easily speak binary gRPC or raw TCP), and client-side chatty network waterfalls.

An **API Gateway** acts as the single, unified front door for external clients (web browsers, mobile apps, third-party partners). It handles cross-cutting concerns—such as TLS termination, rate limiting, authentication, CORS, and request routing—and aggregates data from multiple downstream microservices. In NestJS, an API Gateway is frequently implemented as a **Hybrid Application**, combining an HTTP REST/GraphQL server with internal microservice transporters in a single unified deployment.

---

## 2. Theoretical Foundations

### 2.1 The Backend-for-Frontend (BFF) Pattern
Rather than building a one-size-fits-all API Gateway, teams build dedicated Gateways tailored to specific client needs:
- Web BFF: Optimized for rich desktop browsers.
- Mobile BFF: Compact payloads optimized for low-bandwidth mobile devices.

### 2.2 Aggregation and Fan-Out
When a client requests `GET /orders/dashboard`, the API Gateway fans out concurrent requests:
$$\text{Gateway} \longrightarrow \begin{cases} \text{Orders Service (gRPC)} \\ \text{User Profile Service (TCP)} \\ \text{Payment History Service (RabbitMQ)} \end{cases} \longrightarrow \text{Combine JSON} \longrightarrow \text{Client}$$

---

## 3. Architecture & Internal Mechanics

```
┌────────────────────────────────────────────────────────┐
│                   Public Web Clients                   │
│                HTTPS GET /api/v1/dashboard             │
└───────────────────────────┬────────────────────────────┘
                            │
                            ▼
┌────────────────────────────────────────────────────────┐
│             NestJS API Gateway (Hybrid App)            │
│  - Express 5 HTTP Server (Port 3000)                   │
│  - Authentication Guard & Rate Limiter                 │
│  - RxJS forkJoin() concurrent aggregator               │
└───────┬───────────────────┬────────────────────┬───────┘
        │ (gRPC)            │ (TCP)              │ (Redis)
        ▼                   ▼                    ▼
┌──────────────┐    ┌──────────────┐     ┌──────────────┐
│ Order Service│    │ User Service │     │Notification  │
└──────────────┘    └──────────────┘     └──────────────┘
```

---

## 4. Concrete Implementation

### 4.1 Initializing the Hybrid Gateway in `main.ts`

```typescript
// src/main.ts (API Gateway)
import { NestFactory } from '@nestjs/core';
import { Transport, MicroserviceOptions } from '@nestjs/microservices';
import { AppModule } from './app.module';

async function bootstrap() {
  // 1. Create standard HTTP server for public clients
  const app = await NestFactory.create(AppModule);

  // 2. Connect internal microservice transporter to receive internal broker events
  app.connectMicroservice<MicroserviceOptions>({
    transport: Transport.REDIS,
    options: {
      host: process.env.REDIS_HOST || 'localhost',
      port: 6379,
    },
  });

  // 3. Start microservice listener & HTTP server simultaneously
  await app.startAllMicroservices();
  await app.listen(process.env.PORT || 3000);

  console.log('API Gateway listening on HTTP port 3000 and Redis transporter.');
}
bootstrap();
```

---

### 4.2 Aggregating Downstream Microservices with RxJS `forkJoin`

```typescript
// src/dashboard/dashboard.service.ts
import { Injectable, Inject, Logger } from '@nestjs/common';
import { ClientProxy } from '@nestjs/microservices';
import { forkJoin, firstValueFrom, timeout, catchError, of } from 'rxjs';

@Injectable()
export class DashboardService {
  private readonly logger = new Logger(DashboardService.name);

  constructor(
    @Inject('USERS_SERVICE') private readonly usersClient: ClientProxy,
    @Inject('ORDERS_SERVICE') private readonly ordersClient: ClientProxy,
    @Inject('NOTIFICATIONS_SERVICE') private readonly notifyClient: ClientProxy,
  ) {}

  async getAggregatedUserDashboard(userId: string) {
    this.logger.log(`Aggregating dashboard data for user: ${userId}`);

    // Fan-out: Query downstream microservices concurrently via RxJS forkJoin!
    const dashboard$ = forkJoin({
      user: this.usersClient
        .send('get_user_profile', { id: userId })
        .pipe(timeout(3000)),
      orders: this.ordersClient
        .send('get_user_orders', { userId })
        .pipe(timeout(3000)),
      unreadNotificationsCount: this.notifyClient
        .send('get_unread_count', { userId })
        .pipe(
          timeout(2000),
          // Graceful degradation: if notifications fail, return 0 instead of crashing!
          catchError((err) => {
            this.logger.warn(`Notifications service failed: ${err.message}`);
            return of(0);
          }),
        ),
    });

    const result = await firstValueFrom(dashboard$);

    return {
      profile: result.user,
      recentOrders: result.orders,
      unreadNotifications: result.unreadNotificationsCount,
      generatedAt: new Date().toISOString(),
    };
  }
}
```

---

### 4.3 Gateway HTTP Controller

```typescript
// src/dashboard/dashboard.controller.ts
import { Controller, Get, Param, UseGuards } from '@nestjs/common';
import { DashboardService } from './dashboard.service';

@Controller('api/v1/dashboard')
export class DashboardController {
  constructor(private readonly dashboardService: DashboardService) {}

  @Get(':userId')
  async getDashboard(@Param('userId') userId: string) {
    return this.dashboardService.getAggregatedUserDashboard(userId);
  }
}
```

---

## 5. Edge Cases & Failure Modes

### 5.1 Slow Downstream Service Blocking Aggregation
If one downstream service takes 10 seconds to respond, the entire `forkJoin` hangs.
- **Handling**: Always apply the RxJS `timeout()` operator on every child request.

### 5.2 Partial Degradation Strategy
If a non-critical microservice (e.g. recommended products, unread alerts) fails, failing the entire user dashboard is poor user experience.
- **Remedy**: Use `catchError()` to provide fallback mock values or empty arrays for optional components.

---

## 6. Performance & Memory Implications

- **Concurrent Fan-Out**: Querying 3 services in parallel via `forkJoin` takes only as long as the single slowest service ($\max(T_1, T_2, T_3)$), rather than sequential waterfalls ($T_1 + T_2 + T_3$).
- **Network Compression**: Internal microservice payloads travel across high-speed internal VPC subnets with $< 1\text{ms}$ latency.

---

## 7. Anti-Patterns & Common Mistakes

- **Awaiting Microservices Sequentially**: Writing `const user = await ...; const orders = await ...;`. This creates slow sequential waterfalls. Always query in parallel using `forkJoin` or `Promise.all`.
- **Putting Business Logic in the Gateway**: Adding heavy database operations or business transactions inside the API Gateway. The Gateway should strictly route, authenticate, and aggregate.

---

## 8. Production Checklist & Operational Readiness

- [ ] Every downstream microservice call guarded by `timeout()`.
- [ ] Non-critical services configured with graceful fallback (`catchError()`).
- [ ] Correlation IDs generated at Gateway and forwarded to all child services.
- [ ] Global rate limiting applied at the Gateway entrypoint.

---

## 9. Migration & Version History

- **NestJS 6+**: Supported hybrid applications with `startAllMicroservices()`.
- **NestJS 11.x**: Validated with Express 5 HTTP router and unified reverse shutdown hooks.

---

## 10. Verification & Testing

Unit testing aggregate fan-out logic:

```typescript
import { DashboardService } from './dashboard.service';
import { of } from 'rxjs';

describe('DashboardService (Aggregation)', () => {
  let service: DashboardService;
  let mockUsers: any;
  let mockOrders: any;
  let mockNotify: any;

  beforeEach(() => {
    mockUsers = { send: vi.fn().mockReturnValue(of({ name: 'Alice' })) };
    mockOrders = { send: vi.fn().mockReturnValue(of([{ id: 'ord_1' }])) };
    mockNotify = { send: vi.fn().mockReturnValue(of(3)) };

    service = new DashboardService(mockUsers, mockOrders, mockNotify);
  });

  it('aggregates data concurrently from all three microservices', async () => {
    const data = await service.getAggregatedUserDashboard('u1');

    expect(data.profile).toEqual({ name: 'Alice' });
    expect(data.recentOrders.length).toBe(1);
    expect(data.unreadNotifications).toBe(3);
  });
});
```

---

## 11. Comprehensive Examples

### Correlation ID Forwarding Interceptor

```typescript
// src/common/interceptors/correlation-id.interceptor.ts
import { Injectable, NestInterceptor, ExecutionContext, CallHandler } from '@nestjs/common';
import { Observable } from 'rxjs';
import { randomUUID } from 'crypto';

@Injectable()
export class CorrelationIdInterceptor implements NestInterceptor {
  intercept(context: ExecutionContext, next: CallHandler): Observable<any> {
    const req = context.switchToHttp().getRequest();
    const correlationId = req.headers['x-correlation-id'] || randomUUID();

    // Attach correlationId to request context
    req.correlationId = correlationId;

    const res = context.switchToHttp().getResponse();
    res.setHeader('X-Correlation-ID', correlationId);

    return next.handle();
  }
}
```

---

## 12. Cross-References & Further Reading

- [01-microservices-architecture.md](./01-microservices-architecture.md) - Microservices architecture
- [02-fundamentals/18-hybrid-applications-intro.md](../../02-fundamentals/18-hybrid-applications-intro.md) - Hybrid app basics
- [06-grpc-fundamentals-and-protobuf.md](./06-grpc-fundamentals-and-protobuf.md) - High-speed gRPC
- [API Gateway Pattern (Microservices.io)](https://microservices.io/patterns/apigateway.html)
