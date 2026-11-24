# Message Patterns and Event Handling

- **NestJS Version**: 11.x
- **Node.js Baseline**: >= 20.0.0
- **Module**: 05-advanced / 06-microservices
- **Prerequisites**: [01-microservices-architecture](./01-microservices-architecture.md), [02-transporters](./02-transporters.md)

---

## 1. Context & Purpose

In NestJS microservices, incoming messages are routed to handlers using **Patterns**. A pattern can be a simple string (`'create_order'`), a hierarchical dot-separated namespace (`'order.created'`), or a structured JavaScript object (`{ cmd: 'orders', action: 'create' }`).

Understanding the distinction between **Message Patterns** (`@MessagePattern()`) and **Event Patterns** (`@EventPattern()`), how pattern matching algorithms resolve handlers, and how to access transport contexts (`@Ctx()`, `@Payload()`) is fundamental for building clean microservice architectures.

---

## 2. Theoretical Foundations

### 2.1 The Two Processing Semantics
1. **Request-Response Semantic (`@MessagePattern`)**:
   - Implements Remote Procedure Call (RPC).
   - The caller sends a message and waits for an answer. The handler **must** return a value, an Observable, or a Promise.
   - If the handler returns `undefined`, NestJS sends `null` or empty frame back to acknowledge completion.
2. **Event-Driven Semantic (`@EventPattern`)**:
   - Implements Publish-Subscribe.
   - Fire-and-forget. The handler executes asynchronously without returning a response to the publisher.

### 2.2 Pattern Matching Mechanics
NestJS matches incoming messages against registered handlers:
- String patterns match by exact string equality.
- Object patterns (e.g. `{ cmd: 'user', action: 'get' }`) match by object key-value equivalence, regardless of key insertion order.

---

## 3. Architecture & Internal Mechanics

```
Incoming Message Packet
Pattern: { cmd: 'user', action: 'get' }, Data: { id: "10" }
                         │
                         ▼
             Pattern Matching Router
                         │
       ┌─────────────────┴─────────────────┐
       ▼                                   ▼
Match Found                         No Match Found
Execute Handler Function            Throw RpcException
(@MessagePattern({ ... }))          ("No handler for message")
       │
       ▼
Transforms Return Value into Response Packet
```

---

## 4. Concrete Implementation

### 4.1 Object Patterns vs String Patterns

```typescript
// src/users/users-microservice.controller.ts
import { Controller } from '@nestjs/common';
import {
  MessagePattern,
  EventPattern,
  Payload,
  Ctx,
  TcpContext,
} from '@nestjs/microservices';

@Controller()
export class UsersMicroserviceController {
  // 1. String Pattern: RPC Request-Response
  @MessagePattern('get_user_by_id')
  getUserById(@Payload() id: string) {
    return { id, name: 'Alice Smith' };
  }

  // 2. Structured Object Pattern: Clean Command Routing
  @MessagePattern({ cmd: 'users', action: 'update_status' })
  updateStatus(@Payload() data: { userId: string; active: boolean }) {
    console.log(`Updating status for user ${data.userId} to ${data.active}`);
    return { success: true };
  }

  // 3. Event Pattern: Asynchronous Broadcast Event
  @EventPattern('user.deleted')
  handleUserDeleted(@Payload() data: { userId: string }, @Ctx() context: TcpContext) {
    const socket = context.getSocketRef(); // Access underlying TCP socket if needed
    console.log(`Handling user deletion event for: ${data.userId}`);
  }
}
```

---

### 4.2 Pattern Validation with Pipes

Just like HTTP controllers, microservice message handlers support validation pipes:

```typescript
// src/users/dto/find-user.dto.ts
import { IsUUID } from 'class-validator';

export class FindUserDto {
  @IsUUID()
  id!: string;
}
```

```typescript
// In Controller:
@UsePipes(new ValidationPipe())
@MessagePattern('find_user')
findUser(@Payload() dto: FindUserDto) {
  // dto.id is strictly validated as a UUID before handler executes!
  return { id: dto.id };
}
```

---

### 4.3 Returning RxJS Observables from Message Handlers

Message handlers can stream or transform data via RxJS Observables:

```typescript
// src/calc/calc.controller.ts
import { Controller } from '@nestjs/common';
import { MessagePattern, Payload } from '@nestjs/microservices';
import { Observable, from } from 'rxjs';
import { map } from 'rxjs/operators';

@Controller()
export class CalcController {
  @MessagePattern('compute_series')
  computeSeries(@Payload() data: { count: number }): Observable<number> {
    const numbers = Array.from({ length: data.count }, (_, i) => i + 1);
    // Emits values through RxJS pipeline
    return from(numbers).pipe(map((n) => n * 2));
  }
}
```

---

## 5. Edge Cases & Failure Modes

### 5.1 Pattern Collision
Registering two handlers with identical patterns:
```typescript
@MessagePattern('fetch_order') handlerOne() {}
@MessagePattern('fetch_order') handlerTwo() {}
```
NestJS throws an error during bootstrap: `Pattern "fetch_order" is already registered`.

### 5.2 Handlers Hanging Without Returning
If an async `@MessagePattern` handler awaits a hanging promise, the calling client blocks until its timeout triggers.
- **Rule**: Always enforce timeouts on all external I/O inside message handlers.

---

## 6. Performance & Memory Implications

- **Pattern Lookup**: Object patterns are normalized into deterministic JSON strings during bootstrap, allowing $O(1)$ hash map lookup when incoming packets arrive.
- **Allocation Efficiency**: `@Payload()` and `@Ctx()` decorators extract data without cloning, avoiding memory overhead.

---

## 7. Anti-Patterns & Common Mistakes

- **Using `@MessagePattern` for Broadcast Events**: Using `@MessagePattern` when no response is expected. Transporters like RabbitMQ or Kafka will waste resources setting up temporary reply queues that remain unused.
- **Returning Undefined to Indicate Error**: Returning `undefined` instead of throwing `RpcException`. The caller receives `null` and assumes success instead of triggering error handling.

---

## 8. Production Checklist & Operational Readiness

- [ ] Clear distinction between `@MessagePattern` (RPC) and `@EventPattern` (Events).
- [ ] DTOs validated using `@UsePipes(new ValidationPipe())`.
- [ ] Object patterns follow consistent naming standards (e.g. `{ cmd, action }`).
- [ ] Exception handling uses `RpcException`.

---

## 9. Migration & Version History

- **NestJS 7/8**: Standardized `@Payload()` and `@Ctx()` decorators across all transporters.

---

## 10. Verification & Testing

Unit testing a message pattern handler:

```typescript
import { UsersMicroserviceController } from './users-microservice.controller';

describe('UsersMicroserviceController', () => {
  let controller: UsersMicroserviceController;

  beforeEach(() => {
    controller = new UsersMicroserviceController();
  });

  it('handles get_user_by_id correctly', () => {
    const res = controller.getUserById('42');
    expect(res).toEqual({ id: '42', name: 'Alice Smith' });
  });
});
```

---

## 11. Comprehensive Examples

### Custom Payload Extractor Decorator

```typescript
// src/common/decorators/user-payload.decorator.ts
import { createParamDecorator, ExecutionContext } from '@nestjs/common';

export const MessageUserId = createParamDecorator(
  (data: unknown, ctx: ExecutionContext) => {
    const payload = ctx.switchToRpc().getData();
    return payload?.userId || payload?.id;
  },
);
```

Usage in handler:
```typescript
@MessagePattern('archive_account')
archiveAccount(@MessageUserId() userId: string) {
  console.log(`Archiving account: ${userId}`);
}
```

---

## 12. Cross-References & Further Reading

- [01-microservices-architecture.md](./01-microservices-architecture.md) - Architectural overview
- [02-transporters.md](./02-transporters.md) - Transporter mechanics
- [04-broker-transporters.md](./04-broker-transporters.md) - Message brokers
- [NestJS Message Patterns Guide](https://docs.nestjs.com/microservices/basics#patterns)
