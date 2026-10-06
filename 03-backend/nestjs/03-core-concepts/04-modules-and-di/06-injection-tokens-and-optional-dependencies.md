# Injection Tokens and Optional Dependencies

Nest's DI container looks providers up by **token**. When you write `constructor(private cats: CatsService)`, the token is the class `CatsService`, taken from TypeScript's emitted type metadata. That works only for classes. Interfaces, primitives, and plain objects need an explicit token and `@Inject()`.

This note covers choosing tokens, injecting abstractions, and handling dependencies that may not exist.

Prerequisites: [Custom providers](./05-custom-providers.md).

## Why interfaces can't be injected directly

TypeScript interfaces are erased at runtime, so there's no value for Nest to use as a token.

```ts
interface PaymentGateway { charge(amount: number): Promise<void> }

@Injectable()
export class CheckoutService {
  constructor(private readonly gateway: PaymentGateway) {}   // ❌ emitted type is Object
}
```

Result:

```text
Nest can't resolve dependencies of the CheckoutService (?). Please make sure that the
argument Object at index [0] is available in the CheckoutModule context.
```

**"argument `Object` at index [0]"** is the tell: an interface, a type alias, or a primitive was used as a constructor type.

## Token options

### 1. Symbol (recommended for abstractions)

```ts
// payment.tokens.ts
export const PAYMENT_GATEWAY = Symbol('PAYMENT_GATEWAY');
```

```ts
providers: [{ provide: PAYMENT_GATEWAY, useClass: StripeGateway }]
```

```ts
constructor(@Inject(PAYMENT_GATEWAY) private readonly gateway: PaymentGateway) {}
```

Symbols are unique, so tokens can't collide by accident across modules or libraries.

### 2. String

```ts
{ provide: 'PAYMENT_GATEWAY', useClass: StripeGateway }
constructor(@Inject('PAYMENT_GATEWAY') private readonly gateway: PaymentGateway) {}
```

Simple, but two unrelated modules using the same string will clash, and typos fail only at startup. Fine for small apps; prefer symbols or abstract classes in shared code.

### 3. Abstract class (interface + token in one)

An abstract class exists at runtime **and** acts as a type, so no `@Inject()` is needed.

```ts
export abstract class PaymentGateway {
  abstract charge(amount: number): Promise<void>;
}

@Injectable()
export class StripeGateway extends PaymentGateway {
  async charge(amount: number) { /* ... */ }
}

providers: [{ provide: PaymentGateway, useClass: StripeGateway }]

constructor(private readonly gateway: PaymentGateway) {}   // ✅ works, fully typed
```

This is often the cleanest way to depend on an abstraction (ports/adapters). Trade-off: it's a class, so consumers can accidentally `extends` it, and it exists in the bundle.

### Comparison

| Token | Needs `@Inject()` | Collision-safe | Type-safe at injection | Best for |
|-------|-------------------|----------------|------------------------|----------|
| Class | No | Yes | Yes | Concrete services |
| Abstract class | No | Yes | Yes | Ports/abstractions |
| Symbol | Yes | Yes | Manual (you type the param) | Config values, SDKs, interfaces |
| String | Yes | No | Manual | Quick/simple cases |

With `@Inject(TOKEN)`, **you** assert the parameter's type; Nest can't verify that the provider matches it.

## Typical patterns

### Repository abstraction

```ts
export const USER_REPOSITORY = Symbol('USER_REPOSITORY');

export interface UserRepository {
  findById(id: string): Promise<User | null>;
}

@Module({
  providers: [
    TypeOrmUserRepository,
    { provide: USER_REPOSITORY, useExisting: TypeOrmUserRepository },
    UsersService,
  ],
  exports: [UsersService],
})
export class UsersModule {}

@Injectable()
export class UsersService {
  constructor(@Inject(USER_REPOSITORY) private readonly repo: UserRepository) {}
}
```

`UsersService` depends on the abstraction; swapping to another implementation (or an in-memory fake in tests) changes only the module wiring. See [repository pattern](../../04-intermediate/02-database-foundations/03-repository-pattern.md).

### Options/config objects

```ts
{ provide: MAIL_OPTIONS, useValue: { from: 'noreply@example.com' } }
constructor(@Inject(MAIL_OPTIONS) private readonly options: MailOptions) {}
```

Primitives and plain objects always need a token.

## Property-based injection

```ts
@Injectable()
export class LegacyService {
  @Inject(PAYMENT_GATEWAY)
  private readonly gateway: PaymentGateway;
}
```

Supported, but **prefer constructor injection**: dependencies are explicit, instances are never half-initialized, and unit tests can pass mocks directly. Use property injection mainly when extending a base class whose constructor you don't want to forward through.

## Optional dependencies

Normally a missing provider is a startup error. `@Optional()` tells Nest that the dependency may be absent; the parameter is `undefined` instead.

```ts
import { Injectable, Optional, Inject } from '@nestjs/common';

@Injectable()
export class ReportService {
  constructor(
    @Optional() @Inject(METRICS) private readonly metrics?: MetricsClient,
  ) {}

  run() {
    this.metrics?.increment('report.run');   // handle the absent case
    // ...
  }
}
```

For class tokens, `@Optional()` alone is enough:

```ts
constructor(@Optional() private readonly cache?: CacheService) {}
```

Good uses: telemetry/metrics hooks, optional plug-ins, library code that should work with or without an integration.

Use sparingly:

- It hides misconfiguration. If the dependency is actually required, a startup error is **better** than a silent `undefined`.
- Every call site must handle `undefined`. Type the parameter as optional (`?:` or `| undefined`) so TypeScript enforces it.
- `@Optional()` makes the provider's *absence* acceptable; it doesn't suppress errors from a provider that exists but fails to construct.

In factories, the equivalent is `inject: [{ token: X, optional: true }]` ([custom providers](./05-custom-providers.md)).

## Other built-in tokens worth knowing

| Token | Gives you |
|-------|-----------|
| `ModuleRef` | Programmatic access to the container ([ModuleRef](./08-module-ref-and-lazy-loading.md)) |
| `REQUEST` (from `@nestjs/core`) | The current request; makes the provider request-scoped ([scopes](./07-scopes-and-request-context.md)) |
| `HttpAdapterHost` | Access to the underlying HTTP adapter ([exception filters](../01-request-pipeline/08-exception-filters.md)) |
| `APP_GUARD`, `APP_PIPE`, `APP_INTERCEPTOR`, `APP_FILTER` | Registers global pipeline components, not for injecting |
| `ConfigService` / `registerAs(...).KEY` | Configuration ([custom configuration](../03-configuration/03-custom-configuration.md)) |

## Common mistakes

- **Using an interface as a constructor type** and getting `argument Object at index [0]`.
- **Forgetting `@Inject()`** with string/symbol tokens.
- **Typos in string tokens**, which only fail at startup.
- **Defining the same symbol in two places** (`Symbol('X')` twice creates two different tokens). Export the token constant from one file and import it everywhere.
- **Not exporting the token** from the providing module.
- **Overusing `@Optional()`** to silence resolution errors instead of fixing wiring.
- **Forgetting to handle `undefined`** from an optional dependency.
- **Using `useClass` vs `useExisting` incorrectly** when mapping an interface token to a concrete class that's also registered (creates two instances).

## Debugging

- `argument Object at index [N]`: the Nth constructor parameter is typed with an interface/primitive. Add a token and `@Inject()`, or switch to an abstract class.
- `argument X at index [N] is available in the YModule context`: the token isn't provided/exported/imported in that module (see [visibility rules](./01-feature-and-shared-modules.md)).
- Different instance than expected: log in constructors; check `useClass` vs `useExisting`.
- Symbol mismatch: `console.log(TOKEN.toString())` is the same for different symbols with the same description, so compare identity by importing from a single file.

## Quick Summary

- DI tokens can be classes, abstract classes, symbols, or strings; interfaces can't be tokens because they're erased.
- `argument Object at index [N]` means an interface or primitive was used as a type: add a token and `@Inject()`.
- Prefer symbols or abstract classes for abstractions; define each token once and import it.
- `@Optional()` yields `undefined` when a provider is missing; use it only for truly optional integrations.
- Prefer constructor injection over property injection.

## Next

[Scopes and request context →](./07-scopes-and-request-context.md)
