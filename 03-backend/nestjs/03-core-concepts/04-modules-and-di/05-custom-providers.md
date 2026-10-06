# Custom Providers

The usual way to register a provider is to list its class:

```ts
providers: [CatsService]
```

That is shorthand. The full form is an object that says **what token to register** and **how to produce the value**:

```ts
providers: [{ provide: CatsService, useClass: CatsService }]
```

Custom providers let you inject constants, swap implementations, build values with logic, create aliases, and bring third-party objects into DI. Four recipes cover almost everything: `useValue`, `useClass`, `useFactory`, `useExisting`.

Prerequisites: [Providers and services](../../02-fundamentals/04-providers-and-services.md), [dependency injection](../../02-fundamentals/05-dependency-injection.md).

## `useValue`: inject a constant or ready-made object

```ts
const config = { apiUrl: 'https://api.example.com', retries: 3 };

providers: [{ provide: 'APP_CONFIG', useValue: config }]
```

```ts
constructor(@Inject('APP_CONFIG') private readonly config: typeof config) {}
```

Typical uses: configuration objects, third-party clients you construct yourself, and **mocks in tests**.

```ts
{ provide: UsersService, useValue: { findOne: jest.fn().mockResolvedValue(user) } }
```

The value is the same object every time; nothing is cloned.

## `useClass`: choose the implementation

```ts
providers: [
  {
    provide: PaymentGateway,
    useClass: process.env.NODE_ENV === 'production' ? StripeGateway : FakeGateway,
  },
]
```

Consumers ask for `PaymentGateway`; Nest instantiates whichever class you selected, with its own dependencies injected. Useful for environment-specific implementations, feature variants, and decorating a service.

Reading `process.env` in a decorator runs at import time; for anything driven by validated config use a factory instead ([timing trap](../03-configuration/01-configuration-basics.md)).

## `useFactory`: compute the value

A factory is a function Nest calls to create the provider. It can inject other providers and can be `async`.

```ts
providers: [
  {
    provide: 'DB_CLIENT',
    useFactory: async (config: ConfigService) => {
      const client = new DbClient(config.getOrThrow<string>('DATABASE_URL'));
      await client.connect();
      return client;
    },
    inject: [ConfigService],
  },
]
```

- `inject` lists the tokens passed to the factory, **in order**.
- An `async` factory is awaited before dependants are created, so bootstrap waits for it. If it rejects, the app fails to start. That is usually what you want for a database connection.
- Optional dependencies: `inject: [{ token: SomeToken, optional: true }]` passes `undefined` if it's not available.
- The factory is called **once** for a singleton provider (the default).

This is the mechanism behind every `forRootAsync` ([dynamic modules](./03-dynamic-modules.md)).

## `useExisting`: alias another provider

```ts
providers: [
  LoggerService,
  { provide: 'AliasedLogger', useExisting: LoggerService },
]
```

Both tokens resolve to the **same instance**. Use it to expose one implementation under several tokens, for example an interface token and the concrete class ([tokens](./06-injection-tokens-and-optional-dependencies.md)):

```ts
providers: [
  DatabaseUserRepository,
  { provide: USER_REPOSITORY, useExisting: DatabaseUserRepository },
]
```

Compare with `useClass`, which would create a **second** instance.

## Choosing

| You want to... | Use |
|----------------|-----|
| Inject a constant, config object, or mock | `useValue` |
| Swap which class satisfies a token | `useClass` |
| Build the value with logic, async setup, or injected inputs | `useFactory` |
| Reuse the same instance under another token | `useExisting` |

## Tokens: what `provide` accepts

`provide` is the **injection token**: a class, a string, or a symbol.

```ts
{ provide: CatsService, ... }          // class token: inject by type, no @Inject needed
{ provide: 'DB_CLIENT', ... }          // string token: requires @Inject('DB_CLIENT')
{ provide: DB_CLIENT, ... }            // symbol token: requires @Inject(DB_CLIENT)
```

Only class tokens work with plain type-based injection. Strings and symbols need `@Inject()`. Details and trade-offs are in [Injection tokens and optional dependencies](./06-injection-tokens-and-optional-dependencies.md).

## Exporting custom providers

Export by **token**, or by passing the whole provider object:

```ts
const dbProvider = { provide: 'DB_CLIENT', useFactory: () => new DbClient() };

@Module({
  providers: [dbProvider],
  exports: ['DB_CLIENT'],        // or: exports: [dbProvider]
})
export class DbModule {}
```

Forgetting this is the custom-provider version of the classic "can't resolve dependencies" error.

## Practical examples

### Third-party SDK as a provider

```ts
export const STRIPE = Symbol('STRIPE');

@Module({
  providers: [
    {
      provide: STRIPE,
      useFactory: (config: ConfigService) => new Stripe(config.getOrThrow('STRIPE_KEY')),
      inject: [ConfigService],
    },
    BillingService,
  ],
  exports: [BillingService],
})
export class BillingModule {}

@Injectable()
export class BillingService {
  constructor(@Inject(STRIPE) private readonly stripe: Stripe) {}
}
```

The SDK is now injectable and mockable, and its construction lives in one place.

### Overriding in tests

```ts
const moduleRef = await Test.createTestingModule({ imports: [BillingModule] })
  .overrideProvider(STRIPE)
  .useValue({ charges: { create: jest.fn() } })
  .compile();
```

`overrideProvider` replaces the provider registered under that token ([mocking](../../04-intermediate/01-testing/03-mocking.md)).

### Several implementations behind one token (strategy)

Nest has no built-in "multi-provider". Aggregate with a factory:

```ts
export const NOTIFIERS = Symbol('NOTIFIERS');

providers: [
  EmailNotifier,
  SmsNotifier,
  {
    provide: NOTIFIERS,
    useFactory: (email: EmailNotifier, sms: SmsNotifier) => [email, sms],
    inject: [EmailNotifier, SmsNotifier],
  },
]
```

See [strategy pattern](../../08-architecture-and-patterns/02-design-patterns/02-strategy.md).

## Important behavior

- **Singleton by default.** `useClass`, `useFactory`, `useValue`, and `useExisting` all produce one shared instance unless you change the scope ([scopes](./07-scopes-and-request-context.md)).
- **Factories run at bootstrap** (for singletons), in dependency order.
- `useValue` objects are not instantiated by Nest; lifecycle hooks (`onModuleInit`, etc.) on a plain object won't be called, whereas a `useClass` instance participates in lifecycle.
- Class-token providers can be overridden by registering the same token in a more local module, but this causes confusion. Avoid duplicating tokens.

## Common mistakes

- **Using `useClass` where you meant `useExisting`**, creating a second instance and splitting state.
- **Forgetting `inject`** (factory receives nothing) or listing it in the wrong order.
- **Forgetting to export** the token from the module.
- **Injecting a string/symbol token without `@Inject()`**, leading to an unresolved `Object` dependency.
- **Heavy side effects in a synchronous factory** that should be `async` and awaited.
- **Reading `process.env` in `useClass` conditions** instead of using config-driven factories.
- **Expecting lifecycle hooks on `useValue` objects.**

## Debugging

- `Nest can't resolve dependencies of X (?)... argument DB_CLIENT at index [0]`: token not provided, not exported, or not imported.
- Factory got `undefined` params: `inject` is missing or its token isn't resolvable in this module's context.
- Two instances behaving independently: you used `useClass` (or declared the class twice) instead of `useExisting`.
- App hangs at startup: an `async` factory is waiting on something that never resolves (network, DB).

## Quick Summary

- `providers: [X]` is shorthand for `{ provide: X, useClass: X }`.
- `useValue` for constants/mocks, `useClass` to swap implementations, `useFactory` for computed/async values with `inject`, `useExisting` to alias.
- Tokens can be classes, strings, or symbols; the latter two need `@Inject()`.
- Export custom providers by token; override them in tests with `overrideProvider`.
- Providers are singletons unless scoped otherwise.

## Next

[Injection tokens and optional dependencies →](./06-injection-tokens-and-optional-dependencies.md)
