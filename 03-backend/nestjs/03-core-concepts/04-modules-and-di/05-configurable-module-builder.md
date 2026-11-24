# ConfigurableModuleBuilder and Dynamic Module Patterns

- **NestJS Version**: 11.x
- **Node.js Baseline**: >= 20.0.0
- **Module**: 03-core-concepts / 04-modules-and-di
- **Prerequisites**: [02-dynamic-modules](./02-dynamic-modules.md), [01-dependency-injection](./01-dependency-injection.md), [00-prerequisites/02-typescript-decorators](../../00-prerequisites/02-typescript-decorators.md)

---

## 1. Context & Purpose

Dynamic modules in NestJS allow modules to be configured dynamically with custom options (e.g. database credentials, API keys, cache timeouts) using asynchronous factories (`registerAsync`, `forRootAsync`). Historically, authoring dynamic modules required massive boilerplate: creating injection tokens, defining `ModuleAsyncOptions` interfaces, implementing `createAsyncProviders`, and wiring custom options factories manually.

NestJS introduced the **`ConfigurableModuleBuilder`** to eliminate this boilerplate entirely. With a few lines of code, it auto-generates the complete dynamic module infrastructure: synchronous `register()`, asynchronous `registerAsync()` with `useFactory`, `useClass`, and `useExisting`, and typed injection tokens.

---

## 2. Theoretical Foundations

A configurable module provides two consumer entrypoints:
1. **Synchronous Configuration**: Options passed directly as static objects (`register({ apiKey: '123' })`).
2. **Asynchronous Configuration**: Options derived at runtime from external services or environment configs (`registerAsync({ useFactory: (cfg: ConfigService) => ... })`).

Under the hood, `ConfigurableModuleBuilder` implements the Builder Pattern to synthesize:
- The base definition class (`ConfigurableModuleClass`).
- The injection token used by internal providers (`MODULE_OPTIONS_TOKEN`).
- The async options interfaces (`ASYNC_OPTIONS_TYPE`, `OPTIONS_TYPE`).

---

## 3. Architecture & Internal Mechanics

```
┌────────────────────────────────────────────────────────┐
│               ConfigurableModuleBuilder                │
│             new ConfigurableModuleBuilder<AuthOptions> │
└───────────────────────────┬────────────────────────────┘
                            │
              .build() generates dynamically
                            │
 ┌──────────────────────────┼───────────────────────────┐
 │                          │                           │
 ▼                          ▼                           ▼
ConfigurableModuleClass   MODULE_OPTIONS_TOKEN    ASYNC_OPTIONS_TYPE
- register()              (Injection Token)       - useFactory
- registerAsync()                                 - useClass
- forRoot() / forFeature()                        - useExisting
                            │
                            ▼
              Injectable into Internal Service
              constructor(@Inject(MODULE_OPTIONS_TOKEN) opts)
```

---

## 4. Concrete Implementation

### 4.1 Defining Module Options and Builder

```typescript
// src/mail/mail.module-definition.ts
import { ConfigurableModuleBuilder } from '@nestjs/common';

export interface MailModuleOptions {
  apiKey: string;
  fromEmail: string;
  sandbox?: boolean;
}

export const {
  ConfigurableModuleClass,
  MODULE_OPTIONS_TOKEN,
  OPTIONS_TYPE,
  ASYNC_OPTIONS_TYPE,
} = new ConfigurableModuleBuilder<MailModuleOptions>()
  .setClassPrefix('Mail')
  .setMethod('forRoot') // Renames register() to forRoot() and registerAsync() to forRootAsync()
  .setExtras(
    { isGlobal: false }, // Adds custom extra options like global flag
    (definition, extras) => ({
      ...definition,
      global: extras.isGlobal,
    }),
  )
  .build();
```

---

### 4.2 Implementing the Module

Extend `ConfigurableModuleClass` directly:

```typescript
// src/mail/mail.module.ts
import { Module } from '@nestjs/common';
import { ConfigurableModuleClass } from './mail.module-definition';
import { MailService } from './mail.service';

@Module({
  providers: [MailService],
  exports: [MailService],
})
export class MailModule extends ConfigurableModuleClass {}
```

---

### 4.3 Consuming Options Inside the Service

Inject `MODULE_OPTIONS_TOKEN`:

```typescript
// src/mail/mail.service.ts
import { Injectable, Inject, Logger } from '@nestjs/common';
import { MODULE_OPTIONS_TOKEN, MailModuleOptions } from './mail.module-definition';

@Injectable()
export class MailService {
  private readonly logger = new Logger(MailService.name);

  constructor(
    @Inject(MODULE_OPTIONS_TOKEN)
    private readonly options: MailModuleOptions,
  ) {}

  async sendMail(to: string, subject: string, content: string) {
    if (this.options.sandbox) {
      this.logger.log(`[Sandbox] Sending email from ${this.options.fromEmail} to ${to}: ${subject}`);
      return { sent: true, sandbox: true };
    }

    this.logger.log(`Sending live email via API key: ${this.options.apiKey.slice(0, 4)}***`);
    // Connect to SendGrid / Mailgun API
    return { sent: true, sandbox: false };
  }
}
```

---

### 4.4 Consuming the Generated Dynamic Module

#### Synchronous Consumption:
```typescript
@Module({
  imports: [
    MailModule.forRoot({
      apiKey: 'live-key-12345',
      fromEmail: 'noreply@example.com',
      sandbox: false,
    }),
  ],
})
export class AppModule {}
```

#### Asynchronous Consumption with ConfigService:
```typescript
@Module({
  imports: [
    MailModule.forRootAsync({
      imports: [ConfigModule],
      useFactory: (config: ConfigService) => ({
        apiKey: config.getOrThrow<string>('MAIL_API_KEY'),
        fromEmail: config.get<string>('MAIL_FROM', 'info@example.com'),
        sandbox: config.get<boolean>('MAIL_SANDBOX', true),
        isGlobal: true, // From our custom setExtras builder config!
      }),
      inject: [ConfigService],
    }),
  ],
})
export class AppModule {}
```

---

## 5. Edge Cases & Failure Modes

### 5.1 Circular Injection with `inject: [...]`
In `registerAsync`, injecting a service that directly or indirectly imports the dynamic module creates circular dependency failures.
- **Handling**: Ensure configuration providers (e.g. `ConfigService`) are in standalone leaf modules.

### 5.2 Token Collision When Instantiating Multiple Instances
If a dynamic module is registered multiple times with different options (e.g., two distinct Redis connections), the default `MODULE_OPTIONS_TOKEN` collides because both registrations use the same symbol token.
- **Handling**: For multi-instance dynamic modules, create factory functions that generate distinct dynamic tokens per connection name.

---

## 6. Performance & Memory Implications

- **Zero Runtime Overhead**: `ConfigurableModuleBuilder` executes entirely at module compilation time during NestJS bootstrapping. Once the DI graph compiles, execution speed is identical to manual providers.
- **Code Reduction**: Replaces 100+ lines of fragile manual provider wiring per dynamic module with ~20 lines of declarative code.

---

## 7. Anti-Patterns & Common Mistakes

- **Writing Manual `createAsyncProviders` in New Code**: Manually implementing legacy async boilerplate for new libraries instead of using `ConfigurableModuleBuilder`.
- **Forgetting `inject: [...]` in `useFactory`**: Writing parameters in `useFactory` without listing matching provider classes in the `inject` array.
- **Hardcoding Class Names in Tokens**: Relying on string-based tokens instead of the generated `MODULE_OPTIONS_TOKEN`.

---

## 8. Production Checklist & Operational Readiness

- [ ] Module options validated using TypeScript interfaces or Zod schemas.
- [ ] Module supports both synchronous (`forRoot`) and asynchronous (`forRootAsync`) configurations.
- [ ] `setExtras` configured if the module needs an `isGlobal` flag.
- [ ] Sensitive keys masked in logger output.
- [ ] Asynchronous factories handle environment variable absence gracefully via `getOrThrow`.

---

## 9. Migration & Version History

- **NestJS 9.x**: Introduced `ConfigurableModuleBuilder` in `@nestjs/common`.
- **NestJS 10/11**: Enhanced type inference for extras and full ESM export compatibility.

---

## 10. Verification & Testing

Testing dynamic module registration with mocks:

```typescript
import { Test, TestingModule } from '@nestjs/testing';
import { MailModule } from './mail.module';
import { MailService } from './mail.service';

describe('MailModule (ConfigurableModuleBuilder)', () => {
  it('registers synchronously and provides configured service', async () => {
    const module: TestingModule = await Test.createTestingModule({
      imports: [
        MailModule.forRoot({
          apiKey: 'test-key',
          fromEmail: 'test@example.com',
          sandbox: true,
        }),
      ],
    }).compile();

    const service = module.get<MailService>(MailService);
    expect(service).toBeDefined();

    const result = await service.sendMail('user@example.com', 'Hello', 'Test body');
    expect(result.sandbox).toBe(true);
  });
});
```

---

## 11. Comprehensive Examples

### Building a Reusable Storage Module with Builder Extras

```typescript
// src/storage/storage.module-definition.ts
import { ConfigurableModuleBuilder } from '@nestjs/common';

export interface StorageOptions {
  bucket: string;
  region: string;
  endpoint?: string;
}

export interface StorageExtraOptions {
  isGlobal?: boolean;
}

export const {
  ConfigurableModuleClass,
  MODULE_OPTIONS_TOKEN,
  OPTIONS_TYPE,
  ASYNC_OPTIONS_TYPE,
} = new ConfigurableModuleBuilder<StorageOptions>()
  .setMethod('forRoot')
  .setExtras<StorageExtraOptions>({ isGlobal: false }, (def, extras) => ({
    ...def,
    global: extras.isGlobal,
  }))
  .build();
```

---

## 12. Cross-References & Further Reading

- [02-dynamic-modules.md](./02-dynamic-modules.md) - Dynamic module fundamentals
- [01-dependency-injection.md](./01-dependency-injection.md) - DI tokens and providers
- [03-configuration/01-config-module.md](../03-configuration/01-config-module.md) - Asynchronous configuration patterns
- [NestJS Official Configurable Module Builder Guide](https://docs.nestjs.com/fundamentals/dynamic-modules#configurable-module-builder)
