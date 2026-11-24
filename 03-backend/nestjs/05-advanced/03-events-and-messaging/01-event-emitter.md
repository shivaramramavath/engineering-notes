# In-Process Event Emitter Architecture

- **NestJS Version**: 11.x
- **Node.js Baseline**: >= 20.0.0
- **Module**: 05-advanced / 03-events-and-messaging
- **Prerequisites**: [00-prerequisites/03-nodejs-async-and-event-loop](../../00-prerequisites/03-nodejs-async-and-event-loop.md), [03-core-concepts/04-modules-and-di/01-dependency-injection](../../03-core-concepts/04-modules-and-di/01-dependency-injection.md)

---

## 1. Context & Purpose

In modular monoliths, tight coupling between distinct feature modules destroys maintainability. For example, when a user completes registration in `AuthModule`, multiple downstream side effects must occur:
- `MailModule` sends a welcome email.
- `AnalyticsModule` logs a conversion event.
- `ReferralModule` credits the referrer's account.

If `AuthService` injects `MailService`, `AnalyticsService`, and `ReferralService` directly, it violates the Single Responsibility Principle, creates tangled circular module dependencies, and slows down registration response times.

**`@nestjs/event-emitter`** provides a decoupled, in-process Publish-Subscribe (Pub/Sub) event bus built on `eventemitter2`. It supports wildcards, asynchronous listeners (`async: true`), ordered executions, and strict event class typing.

---

## 2. Theoretical Foundations

The **Observer / Pub-Sub Pattern** decouples event producers from consumers:
- **Producer (Publisher)**: Emits a named domain event containing an immutable payload (`this.eventEmitter.emit('user.registered', event)`). The producer has zero knowledge of who is listening or how many listeners exist.
- **Consumer (Subscriber)**: Decorates methods with `@OnEvent('user.registered')`. Listeners execute independently.
- **Execution Modes**:
  - **Synchronous**: Listeners execute sequentially in the same call stack before the emitter returns.
  - **Asynchronous (`async: true`)**: Listeners return Promises and can execute concurrently or in the background without blocking the caller.

---

## 3. Architecture & Internal Mechanics

```
┌────────────────────────────────────────────────────────┐
│                      AuthService                       │
│    await this.eventEmitter.emitAsync('user.created',   │
│             new UserCreatedEvent(user))                │
└───────────────────────────┬────────────────────────────┘
                            │
                            ▼
              EventEmitter2 In-Process Bus
                            │
 ┌──────────────────────────┼───────────────────────────┐
 │                          │                           │
 ▼                          ▼                           ▼
MailListener               AnalyticsListener           AuditListener
@OnEvent('user.created')   @OnEvent('user.created')    @OnEvent('user.*')
(Sends welcome email)      (Tracks user acquisition)   (Wildcard listener)
```

---

## 4. Concrete Implementation

### 4.1 Installing Dependencies

```bash
npm install --save @nestjs/event-emitter
```

### 4.2 Configuring `EventEmitterModule`

```typescript
// src/app.module.ts
import { Module } from '@nestjs/common';
import { EventEmitterModule } from '@nestjs/event-emitter';
import { AuthModule } from './auth/auth.module';
import { NotificationsModule } from './notifications/notifications.module';

@Module({
  imports: [
    EventEmitterModule.forRoot({
      wildcard: true,        // Enable wildcard matching (e.g. 'order.*')
      delimiter: '.',        // Delimiter used in event namespaces
      maxListeners: 20,      // Max listeners before warning
      verboseMemoryLeak: true,
      ignoreErrors: false,
    }),
    AuthModule,
    NotificationsModule,
  ],
})
export class AppModule {}
```

---

### 4.3 Defining Type-Safe Event Classes

```typescript
// src/auth/events/user-registered.event.ts
export class UserRegisteredEvent {
  constructor(
    public readonly userId: string,
    public readonly email: string,
    public readonly registeredAt: Date = new Date(),
  ) {}
}
```

---

### 4.4 Publishing Events in Services

```typescript
// src/auth/auth.service.ts
import { Injectable, Logger } from '@nestjs/common';
import { EventEmitter2 } from '@nestjs/event-emitter';
import { UserRegisteredEvent } from './events/user-registered.event';

@Injectable()
export class AuthService {
  private readonly logger = new Logger(AuthService.name);

  constructor(private readonly eventEmitter: EventEmitter2) {}

  async registerUser(email: string, name: string) {
    this.logger.log(`Registering user: ${email}`);

    // 1. Create user in database...
    const userId = 'usr_' + Date.now();

    // 2. Publish domain event to in-process bus
    const event = new UserRegisteredEvent(userId, email);

    // emitAsync awaits all async listeners; emit() is fire-and-forget
    await this.eventEmitter.emitAsync('user.registered', event);

    return { id: userId, email, status: 'CREATED' };
  }
}
```

---

### 4.5 Subscribing with `@OnEvent()`

```typescript
// src/notifications/notifications.listener.ts
import { Injectable, Logger } from '@nestjs/common';
import { OnEvent } from '@nestjs/event-emitter';
import { UserRegisteredEvent } from '../auth/events/user-registered.event';

@Injectable()
export class NotificationsListener {
  private readonly logger = new Logger(NotificationsListener.name);

  // Synchronous or asynchronous listener
  @OnEvent('user.registered', { async: true })
  async handleWelcomeEmail(event: UserRegisteredEvent) {
    this.logger.log(`[Listener] Sending welcome email to ${event.email} (User ID: ${event.userId})`);
    await new Promise((r) => setTimeout(r, 100)); // Simulate sending email
  }

  // Wildcard listener catching all user events
  @OnEvent('user.*', { async: true })
  handleAllUserEvents(payload: any) {
    this.logger.log(`[Audit Trail] Received user event: ${JSON.stringify(payload)}`);
  }
}
```

---

## 5. Edge Cases & Failure Modes

### 5.1 Unhandled Exceptions in `async: true` Listeners
If a listener configured with `async: true` throws an unhandled error during `this.eventEmitter.emit()` (fire-and-forget), the exception becomes an unhandled promise rejection that can crash the Node.js process.
- **Handling**: Always wrap listener logic in `try/catch` or use `await this.eventEmitter.emitAsync(...)` with centralized error filtering.

### 5.2 Transaction Boundaries
If `AuthService` emits an event inside an uncommitted SQL transaction, and the listener immediately queries the database for that user, the query will return `null` because the transaction has not committed yet.
- **Rule**: Emit domain events **after** the database transaction successfully commits.

---

## 6. Performance & Memory Implications

- **Near-Zero Latency**: Emitting an in-process event takes $< 0.01\text{ms}$. There is no network serialization or socket roundtrip.
- **Memory Footprint**: Listeners are registered as function pointers on the central `EventEmitter2` instance, consuming negligible RAM.

---

## 7. Anti-Patterns & Common Mistakes

- **Using In-Process Events for Distributed Communication**: Emitting an in-process event and expecting a worker on another server pod to receive it. In-process events are strictly local to that single Node.js process!
- **Modifying Mutable Event Payloads**: Allowing Listener A to mutate properties on the event object, causing Listener B to receive mutated data. Always treat event payloads as immutable.
- **Exceeding Max Listeners Warning**: Registering dozens of anonymous listeners dynamically without unregistering them, causing Node.js memory leak warnings.

---

## 8. Production Checklist & Operational Readiness

- [ ] Events declared as strongly typed classes with `readonly` properties.
- [ ] Domain events emitted **after** database transactions commit.
- [ ] Asynchronous listeners wrapped in `try/catch` to prevent unhandled rejections.
- [ ] `wildcard: true` enabled only if namespace pattern matching is required.

---

## 9. Migration & Version History

- **NestJS 7.x**: Introduced `@nestjs/event-emitter` based on `eventemitter2`.
- **NestJS 11.x**: Validated with modern Node 20/22 event loop mechanics and SWC metadata emission.

---

## 10. Verification & Testing

Unit testing event emission and reception:

```typescript
import { Test, TestingModule } from '@nestjs/testing';
import { EventEmitter2 } from '@nestjs/event-emitter';
import { AuthService } from './auth.service';
import { UserRegisteredEvent } from './events/user-registered.event';

describe('AuthService (EventEmitter)', () => {
  let service: AuthService;
  let mockEmitter: any;

  beforeEach(async () => {
    mockEmitter = {
      emitAsync: vi.fn().mockResolvedValue([]),
    };

    const module: TestingModule = await Test.createTestingModule({
      providers: [
        AuthService,
        { provide: EventEmitter2, useValue: mockEmitter },
      ],
    }).compile();

    service = module.get<AuthService>(AuthService);
  });

  it('emits user.registered event with correct payload', async () => {
    await service.registerUser('alex@example.com', 'Alex');

    expect(mockEmitter.emitAsync).toHaveBeenCalledWith(
      'user.registered',
      expect.any(UserRegisteredEvent),
    );
  });
});
```

---

## 11. Comprehensive Examples

### Transactional Outbox Integration with EventEmitter

```typescript
// src/common/events/transactional-emitter.service.ts
import { Injectable } from '@nestjs/common';
import { EventEmitter2 } from '@nestjs/event-emitter';
import { DataSource } from 'typeorm';

@Injectable()
export class TransactionalEventService {
  constructor(
    private readonly dataSource: DataSource,
    private readonly eventEmitter: EventEmitter2,
  ) {}

  async runInTransactionAndEmit(
    action: () => Promise<any>,
    eventName: string,
    eventPayload: any,
  ) {
    const queryRunner = this.dataSource.createQueryRunner();
    await queryRunner.connect();
    await queryRunner.startTransaction();

    try {
      const result = await action();
      await queryRunner.commitTransaction();

      // Emit event strictly AFTER transaction commits!
      await this.eventEmitter.emitAsync(eventName, eventPayload);
      return result;
    } catch (err) {
      await queryRunner.rollbackTransaction();
      throw err;
    } finally {
      await queryRunner.release();
    }
  }
}
```

---

## 12. Cross-References & Further Reading

- [02-messaging-fundamentals.md](./02-messaging-fundamentals.md) - Distributed messaging comparison
- [05-cqrs-module.md](./05-cqrs-module.md) - CQRS EventBus pattern
- [EventEmitter2 Documentation](https://github.com/EventEmitter2/EventEmitter2)
