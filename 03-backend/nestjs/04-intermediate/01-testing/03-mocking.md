# Mocking

Mocking means replacing a real dependency with a controlled stand-in so a test can run fast, deterministically, and in isolation. Nest's DI makes this easy: you decide which provider a token resolves to. The skill is knowing **what to fake, how, and when to stop**.

Prerequisites: [Unit testing](./02-unit-testing.md), [custom providers](../../03-core-concepts/04-modules-and-di/05-custom-providers.md).

## Vocabulary

People say "mock" for everything. The distinctions help you choose:

| Term | What it is | Example |
|------|-----------|---------|
| **Stub** | Returns canned answers | `repo.findById.mockResolvedValue(user)` |
| **Mock** | Also verifies it was called a certain way | `expect(mailer.send).toHaveBeenCalledWith(...)` |
| **Spy** | Wraps a real function and records calls | `jest.spyOn(service, 'log')` |
| **Fake** | A simplified working implementation | In-memory repository |
| **Dummy** | Placeholder that's never really used | `{} as Logger` |

## Jest building blocks

```ts
const fn = jest.fn();                           // anonymous mock function
fn.mockReturnValue(1);                          // sync value
fn.mockResolvedValue({ id: 1 });                // promise resolving
fn.mockRejectedValue(new Error('boom'));        // promise rejecting
fn.mockResolvedValueOnce(a).mockResolvedValueOnce(b);   // different per call
fn.mockImplementation((x) => x * 2);

expect(fn).toHaveBeenCalledTimes(1);
expect(fn).toHaveBeenCalledWith('arg');
expect(fn).toHaveBeenLastCalledWith('arg');

const spy = jest.spyOn(service, 'method').mockReturnValue(1);   // replace a method on a real object
spy.mockRestore();                                               // put the original back

jest.mock('./some-module');                                      // auto-mock an entire module
```

### clear vs reset vs restore

Leaking mock state between tests is the #1 cause of confusing failures.

| Call | Clears calls/results | Removes implementations | Restores originals (spies) |
|------|----------------------|-------------------------|----------------------------|
| `mockClear()` / `jest.clearAllMocks()` | ✅ | ❌ | ❌ |
| `mockReset()` / `jest.resetAllMocks()` | ✅ | ✅ | ❌ |
| `mockRestore()` / `jest.restoreAllMocks()` | ✅ | ✅ | ✅ (for `spyOn`) |

Either set `clearMocks: true` (or `restoreMocks: true`) in the Jest config, or call the matching function in `afterEach`. Creating fresh mocks in `beforeEach` (as in the [unit testing](./02-unit-testing.md) examples) avoids the problem entirely.

## Mocking providers in a Nest test module

### `useValue`: the default

```ts
const moduleRef = await Test.createTestingModule({
  providers: [
    OrdersService,
    { provide: PaymentsService, useValue: { charge: jest.fn() } },
    { provide: ConfigService, useValue: { getOrThrow: jest.fn().mockReturnValue('test') } },
  ],
}).compile();
```

### Library tokens

Libraries often register providers under their own tokens. Use the helper they provide:

```ts
import { getRepositoryToken } from '@nestjs/typeorm';

{ provide: getRepositoryToken(User), useValue: { findOneBy: jest.fn(), save: jest.fn() } }
```

For Prisma, provide your `PrismaService` class token with a mock exposing the model methods you use. For Mongoose, `getModelToken(User.name)` ([Mongoose](../05-mongoose/README.md)).

### `useFactory` and `useClass`

Use a factory when the fake needs setup, or `useClass` to substitute an in-memory fake:

```ts
{ provide: UsersRepository, useClass: InMemoryUsersRepository }
```

### `overrideProvider` (and friends)

When you import a real module but need to replace part of it:

```ts
const moduleRef = await Test.createTestingModule({ imports: [OrdersModule] })
  .overrideProvider(PaymentsService).useValue(fakePayments)
  .overrideGuard(JwtAuthGuard).useValue({ canActivate: () => true })
  .overridePipe(ParseUUIDPipe).useValue({ transform: (v: unknown) => v })
  .overrideInterceptor(LoggingInterceptor).useValue({ intercept: (_c: any, next: any) => next.handle() })
  .overrideFilter(AllExceptionsFilter).useValue({ catch: jest.fn() })
  .compile();
```

Also available: `.useClass(...)` and `.useFactory({ factory, inject })` in place of `.useValue`. Overrides apply to the provider **wherever it's registered** in the graph, which is what makes them ideal for [integration](./05-integration-testing.md) and [E2E](./06-e2e-testing.md) tests: keep real wiring, fake only the edges (email, payment gateway, clock).

### Auto-mocking missing dependencies

For classes with many dependencies, `useMocker` supplies a mock for any unresolved token:

```ts
const moduleRef = await Test.createTestingModule({ controllers: [UsersController] })
  .useMocker((token) => {
    if (token === UsersService) return { findOne: jest.fn().mockResolvedValue({ id: '1' }) };
    if (typeof token === 'function') {
      const Mock = moduleMocker.generateFromMetadata(moduleMocker.getMetadata(token) as any);
      return new Mock();
    }
  })
  .compile();
```

(`moduleMocker` comes from `jest-mock`: `new ModuleMocker(global)`.) Community packages like `@golevelup/ts-jest` offer `createMock<T>()` for deeply auto-mocked, typed objects. They're convenient, but be aware they return a truthy mock for **any** property, so a typo or a forgotten stub won't fail loudly. Evaluate before adopting.

## Typed mocks

Untyped `jest.fn()` objects let tests drift away from the real API. Keep them tied to the class:

```ts
type MockOf<T> = { [K in keyof T]?: jest.Mock };

const repo: MockOf<UsersRepository> = { findById: jest.fn() };

// or, when you want the real signatures:
let repo: jest.Mocked<Pick<UsersRepository, 'findById' | 'create'>>;
```

If you rename `findById` in the class, the typed mock stops compiling instead of silently passing.

## Fakes: often better than mocks

For repositories, an in-memory fake lets service tests assert **state** instead of call patterns:

```ts
class InMemoryUsersRepository implements UsersRepositoryPort {
  private users = new Map<string, User>();

  async findByEmail(email: string) {
    return [...this.users.values()].find((u) => u.email === email) ?? null;
  }
  async create(data: Omit<User, 'id'>) {
    const user = { id: String(this.users.size + 1), ...data };
    this.users.set(user.id, user);
    return user;
  }
}

it('rejects a duplicate email', async () => {
  await service.register({ email: 'a@b.com', password: 'x' });
  await expect(service.register({ email: 'a@b.com', password: 'x' })).rejects.toThrow(ConflictException);
});
```

This test survives refactors (it doesn't care *how* the repository is called) and reads like a scenario. Fakes need an interface to implement ([tokens and abstractions](../../03-core-concepts/04-modules-and-di/06-injection-tokens-and-optional-dependencies.md)), and they can themselves be wrong, so don't use them where only a real DB can prove correctness (queries, constraints), as that's [integration testing](./05-integration-testing.md).

## Mocking things you don't own

- **Wrap third-party SDKs in your own provider** (`StripeClient`, `MailService`) and mock *that*. Mocking deep SDK shapes (`stripe.charges.create`) couples tests to a library's internals.
- **HTTP calls:** mock your service that wraps `HttpService`, or intercept at the network level with a tool like `nock` or `msw` for tests that should exercise real request code.
- **Time/randomness/IDs:** inject a clock/ID generator, or use fake timers ([unit testing](./02-unit-testing.md)).
- **The file system, environment, and `process.env`:** isolate per test and restore afterwards.

## Request-scoped providers

`moduleRef.get()` throws for scoped providers; use `resolve()`:

```ts
const service = await moduleRef.resolve(TenantService);
```

Mock the `REQUEST` token with `{ provide: REQUEST, useValue: { headers: { 'x-tenant-id': 't1' } } }` ([scopes](../../03-core-concepts/04-modules-and-di/07-scopes-and-request-context.md)).

## When to stop mocking

Mocks buy isolation and cost realism. Signs you've gone too far:

- Tests contain more mock setup than assertions.
- A refactor that keeps behavior identical breaks many tests.
- All tests pass but the app is broken in staging (your mocks encode wrong assumptions about the real thing).
- You're mocking your own simple classes (pure helpers, mappers). Use the real ones.

Mock **boundaries** (databases, networks, clocks, third parties), not every collaborator.

## Common mistakes

- **Leaking mock state** between tests; use fresh mocks or `clearMocks`.
- **Mocking the class under test** or asserting only that mocks return what you configured.
- **Forgetting to provide a mock** for a dependency (`Nest can't resolve dependencies`).
- **Wrong token** (`UsersRepository` vs `getRepositoryToken(User)` vs a symbol).
- **Untyped mocks** that drift from the real API.
- **`jest.mock()` hoisting surprises:** calls are hoisted above imports; factory functions can't reference out-of-scope variables unless prefixed with `mock`.
- **Auto-mocks that return truthy values for everything**, hiding missing stubs.
- **Mocking SDK internals** instead of wrapping the SDK.

## Debugging

- `Nest can't resolve dependencies of X (?)`: provide the missing token (check the exact token the class injects).
- A mock returns `undefined`: you didn't stub that method for this test, or a reset removed the implementation.
- `received number of calls: 0`: the code took a different branch, or you asserted on a different mock instance than the one injected.
- Print `mock.mock.calls` to see how it was actually called.
- Intermittent failures across files: shared module-level mocks; move setup into `beforeEach`.

## Quick Summary

- Stub (canned answer), mock (verify calls), spy (wrap real), fake (working simplification).
- Provide mocks via `useValue`/`useClass`/`useFactory`, or replace parts of a real module with `overrideProvider`/`overrideGuard`/etc.
- Use library token helpers (`getRepositoryToken`) and typed mocks.
- Prefer in-memory fakes for repositories when you want state-based assertions.
- Wrap third-party SDKs, mock your wrapper, and mock boundaries, not everything.

## Next

[Testing pipeline components →](./04-testing-pipeline-components.md)