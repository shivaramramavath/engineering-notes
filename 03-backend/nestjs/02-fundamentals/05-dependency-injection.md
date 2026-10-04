# Dependency Injection

Dependency injection (DI) is the mechanism that connects every part of a NestJS application. A class declares what it needs in its constructor, and Nest's container creates those objects and passes them in. You almost never write `new` for your own services. This file explains the idea, shows exactly how Nest resolves a constructor parameter (and why that depends on TypeScript's emitted metadata), covers tokens, optional dependencies, and scopes at a fundamentals level, and teaches you to read the most common Nest error message like a stack trace.

---

## Overview

**What it is.** A design pattern where objects receive their collaborators from the outside instead of creating them. **Inversion of control (IoC)**: the framework, not your class, controls object creation and wiring.

**Why it exists.** Classes that build their own dependencies are tightly coupled and hard to test, configure, or replace. With DI, a class says *what* it needs and something else decides *which implementation* and *how long it lives*.

**Where it is used.** Everywhere in Nest: controllers receive services, services receive repositories, guards receive `Reflector`, and so on.

**Why you should understand it.**

- It explains `Nest can't resolve dependencies`, the most common startup error.
- It is why you can replace a real service with a fake in tests in one line.
- It is why interfaces cannot be injected directly, and what to do instead.

---

## Mental Model

DI has three roles:

```text
 ┌────────────┐   1. declares need        ┌─────────────────┐
 │  Consumer  │ ────────────────────────► │   Container     │
 │ (class)    │   constructor(svc: Svc)   │  (Nest injector)│
 └────────────┘                           └────────┬────────┘
       ▲                                           │ 2. finds / creates a provider for token `Svc`
       │                                           ▼
       │            3. passes instance      ┌─────────────────┐
       └─────────────────────────────────── │    Provider     │
                                            │  (instance)     │
                                            └─────────────────┘
```

**Without DI:**

```typescript
class OrdersService {
  private readonly repo = new OrdersRepository(new Pool({ /* ... */ }));   // knows how to build everything
}
```

**With DI:**

```typescript
@Injectable()
class OrdersService {
  constructor(private readonly repo: OrdersRepository) {}                   // only says what it needs
}
```

The identity of what is needed is the **token**. For a class, the token is the class itself.

---

## Core Concepts

### Constructor Injection

```typescript
@Controller('cats')
export class CatsController {
  constructor(private readonly catsService: CatsService) {}
}
```

This one line does two things:

- `private readonly` is a TypeScript **parameter property**: it declares the member and assigns the argument.
- The type annotation `CatsService` is what Nest resolves against. TypeScript emits constructor parameter types as metadata (`design:paramtypes`), and the container reads that metadata to decide which provider to supply.

### Tokens

A **token** identifies a provider in the container.

| Token kind | Example | Injection |
|---|---|---|
| Class | `CatsService` | `constructor(private s: CatsService)` (implicit) |
| Abstract class | `PaymentGateway` | `constructor(private g: PaymentGateway)` |
| String | `'DB_CONNECTION'` | `constructor(@Inject('DB_CONNECTION') private db: Db)` |
| Symbol | `Symbol('CACHE')` | `constructor(@Inject(CACHE) private c: Cache)` |

Only classes have runtime type metadata, so only classes (including abstract classes) can be injected implicitly. Everything else needs `@Inject(token)`.

### Why Interfaces Cannot Be Injected

```typescript
interface AppConfig { port: number }

@Injectable()
class Server {
  constructor(private readonly config: AppConfig) {}   // fails
}
```

Interfaces and type aliases are erased at compile time. The emitted metadata for `AppConfig` is `Object`, which tells the container nothing. Startup fails with `Nest can't resolve dependencies`. The same happens when a class is imported with `import type`, because the import is erased.

Solutions:

```typescript
// 1. Abstract class as the token (type + runtime value)
export abstract class AppConfig { abstract readonly port: number; }
{ provide: AppConfig, useValue: { port: 3000 } }
constructor(private readonly config: AppConfig) {}

// 2. Explicit token
export const APP_CONFIG = Symbol('APP_CONFIG');
{ provide: APP_CONFIG, useValue: { port: 3000 } }
constructor(@Inject(APP_CONFIG) private readonly config: AppConfig) {}
```

(In solution 2, `AppConfig` may be an interface, but import it with `import type`.)

### Custom Providers

How a token maps to a value is the provider definition.

| Form | Meaning |
|---|---|
| `{ provide: T, useClass: C }` | When `T` is requested, instantiate class `C` |
| `{ provide: T, useValue: v }` | Provide a constant or pre-built object |
| `{ provide: T, useFactory: (a, b) => ..., inject: [A, B] }` | Compute the value (sync or async), with injected arguments |
| `{ provide: T, useExisting: Other }` | Alias: `T` resolves to the same instance as `Other` |

```typescript
@Module({
  providers: [
    CatsService,                                                   // shorthand
    { provide: PaymentGateway, useClass: StripeGateway },
    { provide: 'API_URL', useValue: 'https://api.example.com' },
    {
      provide: 'DB',
      useFactory: async (cfg: ConfigService) => createClient(cfg.get('DATABASE_URL')),
      inject: [ConfigService],
    },
  ],
})
export class AppModule {}
```

Details: [Custom Providers](../03-core-concepts/04-modules-and-di/05-custom-providers.md).

### Optional Dependencies

```typescript
constructor(@Optional() @Inject('HTTP_OPTIONS') private readonly options?: HttpOptions) {}
```

If no provider is registered for the token, Nest injects `undefined` instead of failing. The class supplies defaults.

> **NestJS 12:** `@Optional()` markers are no longer inherited. A subclass with no constructor of its own keeps the parent's parameter types but loses their optional status, so a missing dependency now throws `UnknownDependenciesException` instead of becoming `undefined`. Declare a constructor in the subclass and redeclare `@Optional()`.

### Property-Based Injection

```typescript
@Injectable()
export class HttpService {
  @Inject('HTTP_OPTIONS')
  private readonly options!: HttpOptions;
}
```

Useful mostly when a base class needs a provider and you do not want every subclass to pass it through `super()`. Otherwise prefer constructor injection, because it states dependencies explicitly.

### Scopes (Overview)

| Scope | Instances | Notes |
|---|---|---|
| `DEFAULT` | One per application (singleton) | The default |
| `REQUEST` | One per incoming request | Bubbles up to everything that depends on it |
| `TRANSIENT` | One per consumer | Not shared |

Almost everything should be `DEFAULT`. Details: [Scopes and Request Context](../03-core-concepts/04-modules-and-di/07-scopes-and-request-context.md).

### Circular Dependencies (Overview)

If `A` needs `B` and `B` needs `A`, neither can be created first. Nest offers `forwardRef()` as an escape hatch, but the better fix is to restructure (extract a third provider). See [Circular Dependencies](../03-core-concepts/04-modules-and-di/04-circular-dependencies.md).

### Visibility Rules

Resolution is limited by modules. A provider is visible to a consumer when:

1. it is in the **same module's** `providers`, or
2. it is **exported** by a module the consumer's module **imports**, or
3. it is exported by a **global** module.

See [Modules](./02-modules.md).

---

## How It Works

A step-by-step resolution of `constructor(private readonly repo: OrdersRepository, private readonly gw: PaymentGateway)`:

```text
 COMPILE TIME (tsc / SWC with decorator metadata)
 ────────────────────────────────────────────────
 @Injectable() on OrdersService causes the compiler to emit:
     Reflect.defineMetadata('design:paramtypes', [OrdersRepository, PaymentGateway], OrdersService)

 STARTUP (NestFactory.create)
 ────────────────────────────
 1. Scan modules → register providers: OrdersService, OrdersRepository, { PaymentGateway → StripeGateway }
 2. Build the graph. For OrdersService read design:paramtypes → tokens [OrdersRepository, PaymentGateway]
 3. Resolve each token in the module's context:
        own providers → exports of imported modules → global modules
        found?  ─── no ──► throw UnknownDependenciesException
        │ yes
 4. Resolve the dependencies of THOSE providers first (depth-first, cycle detection)
 5. Instantiate in dependency order:  new OrdersRepository(...), new StripeGateway(...),
                                      then new OrdersService(repo, gw)
 6. Cache singletons. Hand the same instances to every consumer.
```

An explicit `@Inject('TOKEN')` writes a different token for that parameter index (stored as `self:paramtypes` metadata) and overrides the type-derived one.

You can see the idea in 40 lines in [TypeScript Decorators](../00-prerequisites/02-typescript-decorators.md) (the mini container), and the real implementation in [DI Internals](../06-internals/02-dependency-injection-internals.md).

---

## Basic Example

```typescript
// clock.ts: a tiny abstraction so tests can control time
export abstract class Clock {
  abstract now(): Date;
}

@Injectable()
export class SystemClock extends Clock {
  now() { return new Date(); }
}
```

```typescript
// greeting.service.ts
@Injectable()
export class GreetingService {
  constructor(private readonly clock: Clock) {}

  greet(name: string) {
    const hour = this.clock.now().getHours();
    return `${hour < 12 ? 'Good morning' : 'Hello'}, ${name}!`;
  }
}
```

```typescript
// greeting.module.ts
@Module({
  providers: [
    GreetingService,
    { provide: Clock, useClass: SystemClock },     // token Clock → implementation SystemClock
  ],
})
export class GreetingModule {}
```

```typescript
// greeting.service.spec.ts: replace the dependency in a test
const moduleRef = await Test.createTestingModule({
  providers: [
    GreetingService,
    { provide: Clock, useValue: { now: () => new Date('2026-01-01T08:00:00') } },
  ],
}).compile();

expect(moduleRef.get(GreetingService).greet('Ada')).toBe('Good morning, Ada!');
```

What this demonstrates:

1. `GreetingService` depends on the abstraction `Clock`, not on how time is produced.
2. The module decides the implementation (`SystemClock`).
3. A test decides a different one without changing `GreetingService`. That is the practical payoff of DI.

---

## Practical Examples

### 1. Basic: Standard Constructor Injection

```typescript
@Injectable()
export class UsersService {
  constructor(private readonly repo: UsersRepository) {}
}
```

Requires `UsersRepository` to be a registered, visible provider.

### 2. Common: Inject Configuration

```typescript
@Injectable()
export class MailService {
  constructor(private readonly config: ConfigService) {}   // from @nestjs/config
  send() { const host = this.config.get<string>('SMTP_HOST'); /* ... */ }
}
```

See [Configuration Basics](../03-core-concepts/03-configuration/01-configuration-basics.md).

### 3. Common: Inject by String Token

```typescript
// module
{ provide: 'REDIS', useFactory: () => new Redis(process.env.REDIS_URL) }

// consumer
constructor(@Inject('REDIS') private readonly redis: Redis) {}
```

Prefer a `Symbol` or an abstract class over a bare string to avoid typos and collisions.

### 4. Real-World: Async Factory Provider

```typescript
{
  provide: 'DB',
  useFactory: async (config: ConfigService) => {
    const client = new Client(config.getOrThrow('DATABASE_URL'));
    await client.connect();
    return client;
  },
  inject: [ConfigService],
}
```

Nest waits for the promise before creating dependants. Use this instead of doing async work in a constructor.

### 5. Real-World: Alias a Provider

```typescript
{ provide: 'LEGACY_MAILER', useExisting: MailService }
```

Both tokens resolve to the **same** instance.

### 6. Real-World: Swap a Dependency in a Test

```typescript
const moduleRef = await Test.createTestingModule({ imports: [OrdersModule] })
  .overrideProvider(PaymentGateway)
  .useValue({ charge: async () => 'fake-payment-id' })
  .compile();
```

`overrideProvider` replaces a registered provider while keeping the rest of the module real. See [Mocking](../04-intermediate/01-testing/03-mocking.md).

### 7. Edge Case: Interface Injection Failure

```typescript
interface Notifier { notify(msg: string): void }

@Injectable()
class Alerts {
  constructor(private readonly notifier: Notifier) {}   // Nest can't resolve dependencies of Alerts (?)
}
```

Fix with an abstract class token, or `@Inject(NOTIFIER)` plus a registered provider.

### 8. Edge Case: `import type` on an Injected Class

```typescript
import type { UsersService } from './users.service.js';   // erased

@Injectable()
class A { constructor(private readonly users: UsersService) {} }   // metadata says Object → unresolved
```

Use a normal import for classes that are injected.

### 9. Edge Case: Reading the `?`

```text
Nest can't resolve dependencies of the OrdersService (?, PaymentGateway). Please make sure that the argument UsersService at index [0] is available in the OrdersModule context.
```

The `?` marks the **first** parameter. The names listed beside it are the parameters that resolved. See "Error Anatomy" below.

---

## Syntax / API / Commands

| Item | Purpose |
|---|---|
| `constructor(private readonly x: X)` | Inject by class type |
| `@Inject(token)` | Inject by explicit token (parameter or property) |
| `@Optional()` | Allow absence |
| `{ provide, useClass }` / `useValue` / `useFactory` (+ `inject`) / `useExisting` | Custom providers |
| `@Injectable({ scope: Scope.REQUEST })` | Request-scoped provider |
| `forwardRef(() => X)` | Break a circular dependency |
| `ModuleRef.get(X)` / `.resolve(X)` | Retrieve providers dynamically (`resolve` for scoped) |
| `Test.createTestingModule({...}).overrideProvider(X).useValue(...)` | Replace a provider in tests |

---

## Important Rules

1. **Inject classes or explicit tokens.** Interfaces are erased and cannot be tokens.
2. **Every injected class needs `@Injectable()`** (or another class decorator) so constructor types are emitted.
3. **Use a normal `import` for injected classes.** `import type` erases them.
4. **The provider must be visible:** same module, exported by an imported module, or global.
5. **Never use `new` for your own providers.**
6. **Singletons are shared.** Do not store per-request state in them.
7. **Request scope bubbles up.** One request-scoped dependency makes all its dependants request-scoped.
8. **Constructors must be synchronous.** For async setup use an async factory or lifecycle hooks.
9. **Break cycles by design.** `forwardRef` is a last resort.
10. **In NestJS 12, redeclare `@Optional()` in subclass constructors.**

---

## Under the Hood

### The Injector

For each provider wrapper the injector:

1. Reads constructor parameter tokens from `design:paramtypes` (and overrides from `@Inject()` metadata, `self:paramtypes`).
2. Resolves each token against the module's provider map, then its imports' exports, then globals.
3. Awaits async providers before dependants.
4. Detects cycles (using `forwardRef` markers) and throws otherwise.
5. Constructs the instance and stores it on the wrapper (singletons) or builds per request/consumer (scoped).

### Why the Type Annotation Is the Token

TypeScript's `emitDecoratorMetadata` emits the **runtime value** of each parameter type: the class constructor for classes, `Object` for interfaces and unions, `Number`/`String` for primitives. The container uses that value as the lookup key. That is why the annotation, which looks like pure typing, is functionally an instruction.

### Why Request Scope Is Contagious

A singleton is created once at startup. It cannot hold a reference to something created per request. So if it depends on a request-scoped provider, Nest must rebuild the singleton per request too, and transitively all dependants. Request-scoped graphs are rebuilt for each request, which is the source of the performance cost.

### Optional Resolution

For `@Optional()` parameters the injector catches the "not found" case and passes `undefined`. In NestJS 12 it reads optional markers with `Reflect.getOwnMetadata`, so markers declared on a parent constructor are not seen from a subclass.

### Error Anatomy

```text
Nest can't resolve dependencies of the OrdersService (?, PaymentGateway).
Please make sure that the argument UsersService at index [0] is available in the OrdersModule context.
```

| Part | Meaning |
|---|---|
| `OrdersService` | The class being constructed |
| `(?, PaymentGateway)` | Its parameters. `?` is the unresolved one, names are resolved ones |
| `UsersService at index [0]` | The token that could not be found, and its position |
| `OrdersModule context` | The module where Nest looked |

If the token prints as `Object`, an interface or union was used. If it prints as `undefined`, a circular **file** import left the class undefined at decoration time.

---

## Common Patterns

### Depend on Abstractions

Abstract class (or token) plus `useClass` so implementations can change (real vs fake, vendor A vs B).

### Inject Time, Randomness, and IDs

`Clock`, `IdGenerator` providers make tests deterministic.

### Factory Providers for Clients

`useFactory` with `inject: [ConfigService]` to build SDK or database clients from configuration.

### Alias Providers for Migration

`useExisting` to keep an old token working while moving to a new one.

### Compose in Modules, Not in Classes

Wiring decisions (which implementation, which config) live in module definitions. Classes stay unaware.

### Constructor Parameters as a Design Smell Detector

Many constructor parameters usually signal a class doing too much. Split it.

---

## Common Mistakes

| Mistake | Symptom | Why It Happens | Fix |
|---|---|---|---|
| Interface as constructor type | `can't resolve ... Object` | Interface erased | Abstract class or `@Inject(token)` |
| `import type` on an injected class | Unresolved dependency | Import erased | Normal import |
| Missing `@Injectable()` | Dependencies `undefined` | No metadata emitted | Add it |
| Provider not registered or not visible | `can't resolve ... in the XModule context` | Missing in `providers`, `exports`, or `imports` | Fix module wiring |
| `new Service()` | Dependencies missing, untestable | Bypasses container | Inject |
| Bare string tokens everywhere | Typos, collisions | Easy to mistype | Use `Symbol` or abstract class tokens, export constants |
| Async work in the constructor | Race conditions | Constructors cannot be awaited | `onModuleInit` or async `useFactory` |
| Circular file imports | `undefined` in the error message | Evaluation order | Break the cycle, import directly |
| Circular provider dependency | Startup error | A needs B needs A | Extract a third provider, `forwardRef` as a last resort |
| Request scope used casually | Slowdown, per-request construction | Scope bubbling | Prefer singleton plus explicit context |
| Subclass loses `@Optional()` (v12) | `UnknownDependenciesException` | Markers not inherited | Own constructor with `@Optional()` |
| Overriding the wrong token in tests | Real implementation still used | Token mismatch | Override the exact token registered |
| Injecting concrete clients everywhere | Hard to replace | No abstraction | Wrap in your own provider |

---

## Debugging

### The Checklist (copy it)

```text
Nest can't resolve dependencies of X (?)
□ Is the missing class listed in some module's `providers`?
□ Is it in that module's `exports`?
□ Does X's module import that module (or is it global)?
□ Does the class have @Injectable()?
□ Is the constructor type a CLASS? (token prints "Object" → interface/union)
□ Normal import, not `import type`? (token prints "Object" or missing)
□ Circular file import? (token prints "undefined")
□ Did you switch builder/transformer? Is emitDecoratorMetadata still emitted?
□ Is it a subclass whose parent used @Optional()? (v12: redeclare it)
□ Test only? Did you add the provider to Test.createTestingModule providers/imports?
```

### Commands and Tools

```bash
npx tsc --showConfig | grep -E 'experimentalDecorators|emitDecoratorMetadata'
grep -n "design:paramtypes" dist/**/orders.service.js     # is metadata emitted? (path varies)
```

```typescript
console.log(Reflect.getMetadata('design:paramtypes', OrdersService));
// [ [class UsersService], [class PaymentGateway] ]  healthy
// [ [Function: Object] ]                              interface or union used
// [ undefined ]                                       circular import
```

Use Nest REPL `debug()` or Devtools to see registered providers per module. See [Debugging](../01-getting-started/06-debugging.md).

---

## Performance

- Resolution happens once at startup for singletons. After that, injection is just a reference.
- Request-scoped graphs are rebuilt per request, adding allocations and CPU. On high-traffic paths this matters.
- Large numbers of providers lengthen startup. Rarely a problem for servers, sometimes for serverless.
- Prefer passing request context explicitly (arguments, `AsyncLocalStorage`) over request-scoped providers when performance matters.

---

## Security

- **DI can hide dependencies.** A class that quietly receives a privileged provider (a database client with admin rights, a secrets store) widens its power. Keep providers narrow.
- **Global modules widen exposure** to everything in the process.
- **Do not inject secrets as broad objects** (the whole `process.env`). Inject only the validated values a class needs.
- **Tests that override providers** must not leak into production wiring. Keep test modules separate.
- **Token strings are not secrets.** They are lookup keys, nothing more.

---

## Production Considerations

- **Wiring is configuration.** Environment-dependent implementation choices (real vs fake, vendor A vs B) belong in module definitions driven by validated config.
- **Fail fast at startup.** DI errors surface at bootstrap, so deploy health checks catch them before traffic. Keep optional dependencies truly optional.
- **Keep scopes explicit.** Audit request-scoped providers: each one has a runtime cost.
- **Shutdown:** providers holding resources need `onModuleDestroy`/`onApplicationShutdown` and `app.enableShutdownHooks()`.
- **Upgrades:** review `@Optional()` inheritance and hook ordering when moving to NestJS 12.

---

## Best Practices

### Recommended

```typescript
export abstract class PaymentGateway {
  abstract charge(amountCents: number): Promise<string>;
}

@Module({
  providers: [
    OrdersService,
    { provide: PaymentGateway, useClass: StripeGateway },
  ],
})
export class OrdersModule {}

@Injectable()
export class OrdersService {
  constructor(private readonly payments: PaymentGateway) {}
}
```

### Avoid

```typescript
@Injectable()
export class OrdersService {
  private readonly payments = new StripeGateway(process.env.STRIPE_KEY!);   // hard-wired, untestable
}

interface Mailer { send(): void }
constructor(private readonly mailer: Mailer) {}                              // interface: cannot resolve
```

Why: the recommended version depends on an abstraction chosen by the module and replaceable in tests. The avoided versions hard-wire an implementation or give the container nothing to resolve.

Additional guidance:

- Depend on the narrowest abstraction you need.
- Export token constants from one file (`export const CACHE = Symbol('CACHE')`).
- Keep provider factories small. Move logic into classes.
- Prefer composition through DI over inheritance.
- Write at least one test that replaces a dependency, to make sure your class is actually decoupled.

---

## Version / Compatibility Notes

| Item | NestJS 12 | Earlier |
|---|---|---|
| `@Optional()` inheritance | Read with `Reflect.getOwnMetadata`. **Not inherited** by subclasses | Inherited |
| Lifecycle hook ordering | By component hierarchy level | Previous ordering |
| Decorator model | Legacy (`experimentalDecorators`) with `emitDecoratorMetadata` in generated `tsconfig` | Same |
| `isolatedModules` in generated ESM tsconfig | On. Classes injected: normal `import`. Types and interfaces: `import type` | n/a |
| Provider forms and tokens | Unchanged | Same |
| `HealthIndicatorService` (Terminus) | Replaces the removed legacy health indicator API | Legacy API deprecated in v11 |

*Verify against the official migration guide.*

---

## Real-World Use Cases

- **Swapping implementations** by environment (real vs fake payment, in-memory vs Redis cache).
- **Unit testing** services with mocked repositories and clients.
- **Configuration injection** (`ConfigService`, typed config objects).
- **Plugin architectures** using `DiscoveryService`/`ModuleRef` and multi-provider tokens.
- **Cross-cutting services** (logging, metrics) injected into guards, interceptors, and filters.
- **Library modules** exposing configurable providers via `forRoot()`.

---

## Interview Questions

### Beginner

1. What is dependency injection?
   - Providing a class's dependencies from outside (through the constructor) instead of the class creating them.
2. How does a Nest controller get its service?
   - The container instantiates the provider and passes it via the controller's constructor.
3. Why do you not write `new Service()` in Nest?
   - It bypasses the container, so dependencies, sharing, and test replacement are lost.
4. What does `@Injectable()` do?
   - Marks a class as injectable and makes TypeScript emit its constructor parameter types.

### Intermediate

1. How does Nest know which provider to inject?
   - It reads the constructor parameter types emitted as metadata (`design:paramtypes`) and uses each class as a token, or the token given with `@Inject()`.
2. Why can't an interface be injected, and what are the alternatives?
   - Interfaces are erased. Use an abstract class as a token, or a string/symbol token with `@Inject()`.
3. What are `useClass`, `useValue`, `useFactory`, and `useExisting`?
   - Provider forms: instantiate a class, supply a value, compute a value (sync or async, with injected args), or alias another provider.
4. What does `@Optional()` do?
   - Injects `undefined` when no provider exists for the token.
5. How do you replace a dependency in a test?
   - Provide a fake under the same token in `Test.createTestingModule`, or use `overrideProvider(...).useValue(...)`.

### Advanced

1. Walk through how the container resolves a dependency across modules.
   - Own providers, then exports of imported modules (including re-exports), then global modules. If found, resolve its dependencies first, then instantiate. Otherwise throw `UnknownDependenciesException`.
2. Why does request scope bubble up?
   - Singletons are created once, so anything depending on a per-request instance must itself be created per request.
3. Decode: `Nest can't resolve dependencies of X (?, Y)`.
   - X's first parameter could not be resolved, Y resolved. The message names the failing token, index, and module context. `Object` as the token suggests an interface, `undefined` a circular import.
4. What changed for `@Optional()` in NestJS 12?
   - Not inherited. Subclasses must declare a constructor and redeclare `@Optional()`, or a missing dependency throws.
5. How would you design for swappable infrastructure?
   - Abstract class tokens, `useClass` or `useFactory` selected by validated configuration, and tests that override the token.
6. Why must constructors be synchronous and what do you do instead?
   - Nest cannot await them. Use async factory providers or lifecycle hooks.

---

## Quick Reference

```text
Inject              constructor(private readonly x: X) {}     class type = token
Explicit token      @Inject('TOKEN') / @Inject(SYMBOL) / abstract class
Optional            @Optional()      (v12: not inherited by subclasses)
Providers           { provide, useClass | useValue | useFactory(+inject) | useExisting }
Visibility          same module · exported by imported module · global module
Scopes              DEFAULT (singleton) · REQUEST (bubbles up) · TRANSIENT
Requires            @Injectable() · emitDecoratorMetadata · normal import for injected classes
Cannot inject       interfaces · type aliases · import type'd classes
Async setup         useFactory(async) or lifecycle hooks, never the constructor
Error decoder       Object → interface/union · undefined → circular import · ? → failing index
Test                Test.createTestingModule(...).overrideProvider(X).useValue(fake)
```

---

## Key Takeaways

- DI means classes declare needs and the container supplies them. Nest wires everything this way.
- The constructor parameter's type annotation is the lookup token, because TypeScript emits it as metadata.
- Only classes (including abstract classes) carry runtime types. Use abstract classes or `@Inject(token)` to inject abstractions.
- Visibility is module-based: own providers, exports of imports, global exports.
- Singleton by default. Request scope is contagious and costly.
- `Nest can't resolve dependencies` tells you the class, the index, and the module. `Object` and `undefined` in the message are strong clues.
- DI's main payoff is replaceability: different implementations in different environments and fakes in tests.
- In NestJS 12, redeclare `@Optional()` in subclass constructors.

---

## Related Topics

```text
04 Providers and Services
      ↓
[05 Dependency Injection]
      ↓
06 Decorators  →  03-core-concepts/04 Custom Providers, Scopes, Circular Dependencies
      ↓
06-internals/02 Dependency Injection Internals
```

- [Fundamentals Overview](./README.md)
- [Modules](./02-modules.md)
- [Providers and Services](./04-providers-and-services.md)
- [Decorators](./06-decorators.md)
- [TypeScript Decorators (mini container)](../00-prerequisites/02-typescript-decorators.md)
- [Custom Providers](../03-core-concepts/04-modules-and-di/05-custom-providers.md)
- [Injection Tokens and Optional Dependencies](../03-core-concepts/04-modules-and-di/06-injection-tokens-and-optional-dependencies.md)
- [Scopes and Request Context](../03-core-concepts/04-modules-and-di/07-scopes-and-request-context.md)
- [Circular Dependencies](../03-core-concepts/04-modules-and-di/04-circular-dependencies.md)
- [DI Internals](../06-internals/02-dependency-injection-internals.md)
- [Mocking](../04-intermediate/01-testing/03-mocking.md)
