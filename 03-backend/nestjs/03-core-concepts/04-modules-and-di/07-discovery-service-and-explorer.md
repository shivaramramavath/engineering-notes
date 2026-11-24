# DiscoveryService and Metadata Explorer Patterns

- **NestJS Version**: 11.x
- **Node.js Baseline**: >= 20.0.0
- **Module**: 03-core-concepts / 04-modules-and-di
- **Prerequisites**: [01-dependency-injection](./01-dependency-injection.md), [00-prerequisites/06-reflect-metadata-and-decorator-mechanics](../../00-prerequisites/06-reflect-metadata-and-decorator-mechanics.md)

---

## 1. Context & Purpose

Framework authors and enterprise architects often need to build dynamic plugin systems, custom decorator scanners, or declarative event subscriptions. For example, how does `@nestjs/schedule` find all methods decorated with `@Cron()`? How does `@nestjs/event-emitter` automatically register all `@OnEvent()` handlers without requiring manual registration?

The answer lies in the **Discovery Module** (`DiscoveryService` and `MetadataScanner`). Provided by `@nestjs/core`, these services allow programmatic traversal of the entire compiled application graph, introspecting every controller, provider, and method prototype to extract custom metadata and wire dynamic runtime behaviors.

---

## 2. Theoretical Foundations

Metadata exploration occurs during the **`OnModuleInit`** lifecycle phase:
1. **Metadata Attachment**: Custom decorators (`SetMetadata`) attach metadata keys to class constructors or method prototypes.
2. **Graph Traversal (`DiscoveryService`)**: Inspects `ModulesContainer`, collecting all registered `InstanceWrapper` objects for controllers and providers.
3. **Prototype Reflection (`MetadataScanner`)**: Iterates through each instance's prototype chain, scanning method names and checking for attached metadata keys via `Reflector`.
4. **Dynamic Registration**: If matching metadata is found, the explorer registers the method callback with an event emitter, message broker, cron runner, or queue listener.

---

## 3. Architecture & Internal Mechanics

```
┌────────────────────────────────────────────────────────┐
│                   ModulesContainer                     │
│               (Internal NestJS DI Graph)               │
└───────────────────────────┬────────────────────────────┘
                            │
                            ▼
                     DiscoveryService
           .getControllers() / .getProviders()
                            │
              Returns InstanceWrapper[]
                            │
                            ▼
                     MetadataScanner
             .scanFromPrototype(instance, proto)
                            │
                            ▼
                        Reflector
        .get(CUSTOM_METADATA_KEY, methodRef)
                            │
              ┌─────────────┴─────────────┐
              ▼                           ▼
        [Metadata Found]          [No Metadata]
              │                           │
              ▼                           ▼
  Register with Event Bus /            Skip Method
  Worker / Scheduler Engine
```

---

## 4. Concrete Implementation

### 4.1 Building a Custom Audit Explorer

Let's build a complete custom decorator and explorer system: `@AuditAction('USER_LOGIN')`. Any method decorated with `@AuditAction` will automatically be intercepted and logged upon application startup.

#### Step 1: Define the Decorator

```typescript
// src/audit/audit.decorator.ts
import { SetMetadata } from '@nestjs/common';

export const AUDIT_ACTION_KEY = 'AUDIT_ACTION_METADATA';

export const AuditAction = (actionName: string) =>
  SetMetadata(AUDIT_ACTION_KEY, actionName);
```

#### Step 2: Implement the Explorer Service

```typescript
// src/audit/audit-explorer.service.ts
import { Injectable, OnModuleInit, Logger } from '@nestjs/common';
import { DiscoveryService, MetadataScanner, Reflector } from '@nestjs/core';
import { AUDIT_ACTION_KEY } from './audit.decorator';

@Injectable()
export class AuditExplorerService implements OnModuleInit {
  private readonly logger = new Logger(AuditExplorerService.name);

  constructor(
    private readonly discoveryService: DiscoveryService,
    private readonly metadataScanner: MetadataScanner,
    private readonly reflector: Reflector,
  ) {}

  onModuleInit() {
    this.exploreAuditActions();
  }

  private exploreAuditActions() {
    // 1. Retrieve all registered providers and controllers
    const wrappers = [
      ...this.discoveryService.getControllers(),
      ...this.discoveryService.getProviders(),
    ];

    for (const wrapper of wrappers) {
      const { instance } = wrapper;
      if (!instance || !Object.getPrototypeOf(instance)) {
        continue;
      }

      const prototype = Object.getPrototypeOf(instance);

      // 2. Scan prototype methods using MetadataScanner
      const methodNames = this.metadataScanner.getAllMethodNames(prototype);

      for (const methodName of methodNames) {
        const methodRef = instance[methodName];

        // 3. Extract custom metadata using Reflector
        const auditAction = this.reflector.get<string>(
          AUDIT_ACTION_KEY,
          methodRef,
        );

        if (auditAction) {
          this.logger.log(
            `Discovered @AuditAction("${auditAction}") on ${wrapper.name}.${methodName}()`,
          );

          // 4. Bind dynamic runtime wrapper or registration
          this.registerAuditHook(instance, methodName, auditAction);
        }
      }
    }
  }

  private registerAuditHook(instance: any, methodName: string, action: string) {
    const originalMethod = instance[methodName];

    // Wrap the original method with auditing logic
    instance[methodName] = async function (...args: any[]) {
      console.log(`[AUDIT EVENT] Executing action: ${action}`);
      return originalMethod.apply(this, args);
    };
  }
}
```

---

### 4.3 Registering `DiscoveryModule` in the Module

```typescript
// src/audit/audit.module.ts
import { Module } from '@nestjs/common';
import { DiscoveryModule } from '@nestjs/core';
import { AuditExplorerService } from './audit-explorer.service';

@Module({
  imports: [DiscoveryModule], // Provides DiscoveryService & MetadataScanner
  providers: [AuditExplorerService],
  exports: [AuditExplorerService],
})
export class AuditModule {}
```

---

### 4.4 Using the Custom Decorator in Any Controller or Service

```typescript
// src/users/users.service.ts
import { Injectable } from '@nestjs/common';
import { AuditAction } from '../audit/audit.decorator';

@Injectable()
export class UsersService {
  @AuditAction('USER_PASSWORD_RESET')
  async resetPassword(email: string) {
    return { status: 'Password reset initiated for ' + email };
  }
}
```

When NestJS boots, `AuditExplorerService` automatically detects `@AuditAction('USER_PASSWORD_RESET')` and wraps the method seamlessly!

---

## 5. Edge Cases & Failure Modes

### 5.1 Request-Scoped Providers in Discovery
`DiscoveryService.getProviders()` returns wrappers for request-scoped providers, but `wrapper.instance` is initially `null` or `undefined` because singletons do not exist at startup.
- **Handling**: Always check `if (!instance) continue;` or handle dynamic resolution via `ModuleRef.resolve()`.

### 5.2 Method Scanner on Non-Class Prototypes
Scanning plain object literals or dynamic proxy objects without a clean prototype chain throws `TypeError: Object.getPrototypeOf is not a function`.
- **Handling**: Validate `typeof instance === 'object' && instance !== null`.

---

## 6. Performance & Memory Implications

- **Startup Execution**: Discovery runs once during application startup in `onModuleInit()`. Traversal of 500 providers takes ~5–12ms, adding zero overhead to the live request pipeline.
- **Memory Footprint**: Wrappers reference existing DI graph objects, adding no additional memory overhead.

---

## 7. Anti-Patterns & Common Mistakes

- **Running Discovery on Every Request**: Calling `discoveryService.getProviders()` inside an HTTP interceptor or middleware. Discovery must only run once during module initialization.
- **Mutating Method Signatures Carelessly**: Overriding `instance[methodName]` without preserving `apply(this, args)` or losing the original function's return promise.
- **Hardcoding Method Lists**: Manually maintaining lists of audited methods instead of using declarative metadata reflection.

---

## 8. Production Checklist & Operational Readiness

- [ ] `DiscoveryModule` imported from `@nestjs/core`.
- [ ] Discovery logic placed strictly inside `onModuleInit()` or `onApplicationBootstrap()`.
- [ ] Safe checks for `!instance` and valid prototypes implemented.
- [ ] Wrapped methods preserve the original `this` context and promise chaining.

---

## 9. Migration & Version History

- **NestJS 7/8**: Introduced official `DiscoveryModule` and `DiscoveryService`.
- **NestJS 10.x**: Redesigned `MetadataScanner.getAllMethodNames` to support modern ES class prototypes and SWC output.
- **NestJS 11.x**: Support for introspection across reverse-lifecycle teardown hooks.

---

## 10. Verification & Testing

Unit testing discovery exploration:

```typescript
import { Test, TestingModule } from '@nestjs/testing';
import { DiscoveryModule } from '@nestjs/core';
import { AuditExplorerService } from './audit-explorer.service';
import { UsersService } from '../users/users.service';

describe('AuditExplorerService', () => {
  let explorer: AuditExplorerService;
  let usersService: UsersService;

  beforeEach(async () => {
    const module: TestingModule = await Test.createTestingModule({
      imports: [DiscoveryModule],
      providers: [AuditExplorerService, UsersService],
    }).compile();

    explorer = module.get<AuditExplorerService>(AuditExplorerService);
    usersService = module.get<UsersService>(UsersService);

    // Trigger onModuleInit manually
    explorer.onModuleInit();
  });

  it('successfully discovers and instruments decorated methods', async () => {
    const consoleSpy = vi.spyOn(console, 'log');
    await usersService.resetPassword('test@example.com');
    expect(consoleSpy).toHaveBeenCalledWith(
      expect.stringContaining('[AUDIT EVENT] Executing action: USER_PASSWORD_RESET'),
    );
  });
});
```

---

## 11. Comprehensive Examples

### Building a Declarative RabbitMQ Event Subscriber Explorer

```typescript
// src/rabbitmq/rabbitmq-explorer.service.ts
import { Injectable, OnModuleInit } from '@nestjs/common';
import { DiscoveryService, MetadataScanner, Reflector } from '@nestjs/core';

export const RABBIT_SUBSCRIBE_KEY = 'RABBIT_SUBSCRIBE_META';

@Injectable()
export class RabbitMQExplorerService implements OnModuleInit {
  constructor(
    private readonly discovery: DiscoveryService,
    private readonly metadataScanner: MetadataScanner,
    private readonly reflector: Reflector,
  ) {}

  onModuleInit() {
    const providers = this.discovery.getProviders();

    for (const wrapper of providers) {
      const { instance } = wrapper;
      if (!instance || !Object.getPrototypeOf(instance)) continue;

      const methods = this.metadataScanner.getAllMethodNames(Object.getPrototypeOf(instance));

      for (const method of methods) {
        const queueName = this.reflector.get<string>(RABBIT_SUBSCRIBE_KEY, instance[method]);
        if (queueName) {
          console.log(`Binding queue [${queueName}] to ${wrapper.name}.${method}()`);
          // Wire consumer: channel.consume(queueName, (msg) => instance[method](msg));
        }
      }
    }
  }
}
```

---

## 12. Cross-References & Further Reading

- [01-dependency-injection.md](./01-dependency-injection.md) - DI graph mechanics
- [00-prerequisites/06-reflect-metadata-and-decorator-mechanics.md](../../00-prerequisites/06-reflect-metadata-and-decorator-mechanics.md) - Metadata reflection mechanics
- [06-internals/03-metadata-and-reflection.md](../../06-internals/03-metadata-and-reflection.md) - Deep dive into Nest internals
- [NestJS DiscoveryModule API Documentation](https://docs.nestjs.com)
