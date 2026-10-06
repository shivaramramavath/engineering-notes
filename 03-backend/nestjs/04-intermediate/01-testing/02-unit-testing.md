# Unit Testing

A unit test exercises **one class in isolation**: its dependencies are replaced with fakes, so a failure points at that class and nothing else. In Nest that mostly means testing **services** (where business logic lives) and the occasional controller or helper.

Prerequisites: [Testing fundamentals](./01-testing-fundamentals.md). The mocking techniques used below are explained fully in [Mocking](./03-mocking.md).

## Testing a service

Service under test:

```ts
// users.service.ts
@Injectable()
export class UsersService {
  constructor(
    private readonly repo: UsersRepository,
    private readonly hasher: HashingService,
  ) {}

  async register(dto: CreateUserDto) {
    const existing = await this.repo.findByEmail(dto.email);
    if (existing) throw new ConflictException('Email already registered');

    const passwordHash = await this.hasher.hash(dto.password);
    return this.repo.create({ email: dto.email, passwordHash });
  }

  async findOne(id: string) {
    const user = await this.repo.findById(id);
    if (!user) throw new NotFoundException(`User ${id} not found`);
    return user;
  }
}
```

Test:

```ts
// users.service.spec.ts
import { Test } from '@nestjs/testing';
import { ConflictException, NotFoundException } from '@nestjs/common';

describe('UsersService', () => {
  let service: UsersService;
  let repo: jest.Mocked<Pick<UsersRepository, 'findByEmail' | 'findById' | 'create'>>;
  let hasher: jest.Mocked<Pick<HashingService, 'hash'>>;

  beforeEach(async () => {
    repo = { findByEmail: jest.fn(), findById: jest.fn(), create: jest.fn() };
    hasher = { hash: jest.fn() };

    const moduleRef = await Test.createTestingModule({
      providers: [
        UsersService,
        { provide: UsersRepository, useValue: repo },
        { provide: HashingService, useValue: hasher },
      ],
    }).compile();

    service = moduleRef.get(UsersService);
  });

  describe('register', () => {
    const dto = { email: 'a@b.com', password: 'secret123' };

    it('creates a user with a hashed password', async () => {
      repo.findByEmail.mockResolvedValue(null);
      hasher.hash.mockResolvedValue('hashed');
      repo.create.mockResolvedValue({ id: '1', email: dto.email } as any);

      const result = await service.register(dto);

      expect(repo.create).toHaveBeenCalledWith({ email: dto.email, passwordHash: 'hashed' });
      expect(result).toEqual({ id: '1', email: dto.email });
    });

    it('never stores the plain password', async () => {
      repo.findByEmail.mockResolvedValue(null);
      hasher.hash.mockResolvedValue('hashed');

      await service.register(dto);

      expect(repo.create).not.toHaveBeenCalledWith(expect.objectContaining({ password: dto.password }));
    });

    it('throws ConflictException when the email exists', async () => {
      repo.findByEmail.mockResolvedValue({ id: '9' } as any);

      await expect(service.register(dto)).rejects.toThrow(ConflictException);
      expect(repo.create).not.toHaveBeenCalled();
    });
  });

  describe('findOne', () => {
    it('returns the user', async () => {
      repo.findById.mockResolvedValue({ id: '1' } as any);
      await expect(service.findOne('1')).resolves.toEqual({ id: '1' });
    });

    it('throws NotFoundException when missing', async () => {
      repo.findById.mockResolvedValue(null);
      await expect(service.findOne('1')).rejects.toThrow(NotFoundException);
    });
  });
});
```

What makes this a good unit test:

- Each `it` checks **one behavior**, named in plain language.
- It covers the **happy path and both failure branches**.
- It asserts outcomes (result, exception) and the **important side effects** (`create` called with a hash, never with the plain password, and not called on conflict), not every internal call.
- Mocks are re-created in `beforeEach`, so tests can't leak into each other.

## Controllers: keep them thin, test them lightly

A well-designed controller only maps HTTP concerns to a service call. Its interesting behavior (routing, validation, status codes, guards) isn't exercised by a unit test anyway; that's [E2E territory](./06-e2e-testing.md).

```ts
describe('UsersController', () => {
  let controller: UsersController;
  const service = { findOne: jest.fn() };

  beforeEach(async () => {
    const moduleRef = await Test.createTestingModule({
      controllers: [UsersController],
      providers: [{ provide: UsersService, useValue: service }],
    }).compile();
    controller = moduleRef.get(UsersController);
  });

  it('delegates to the service', async () => {
    service.findOne.mockResolvedValue({ id: '1' });
    await expect(controller.findOne('1')).resolves.toEqual({ id: '1' });
    expect(service.findOne).toHaveBeenCalledWith('1');
  });
});
```

Only add controller unit tests when a controller contains real logic (for example mapping params, choosing a status). If it has none, skip them and rely on E2E tests. Calling `controller.findOne('1')` directly **bypasses** pipes, guards, interceptors, and filters, so it proves nothing about them.

## Instantiate directly when DI isn't the point

```ts
const service = new UsersService(repoMock as any, hasherMock as any);
```

Faster and simpler. Use `Test.createTestingModule` when you want the DI wiring, tokens, or `overrideProvider` involved. Either is fine; be consistent.

## Table-driven tests

For pure logic with many input/output pairs, `it.each` keeps tests compact:

```ts
describe('calculateShipping', () => {
  it.each([
    [0.5, 'domestic', 5],
    [0.5, 'international', 15],
    [10, 'domestic', 12],
  ])('weight %p kg to %s costs %p', (kg, zone, expected) => {
    expect(calculateShipping(kg, zone as any)).toBe(expected);
  });
});
```

Pure functions (pricing, formatting, parsing) are the easiest things to test. If a service method is hard to test, consider extracting its logic into a pure function.

## Controlling time and randomness

Never let a test depend on the real clock.

```ts
beforeEach(() => {
  jest.useFakeTimers().setSystemTime(new Date('2025-01-15T10:00:00Z'));
});
afterEach(() => jest.useRealTimers());

it('marks a token as expired after 15 minutes', () => {
  const token = service.issue();
  jest.advanceTimersByTime(16 * 60 * 1000);
  expect(service.isValid(token)).toBe(false);
});
```

Fake timers can interfere with promises/async code in some setups; if tests hang, either flush microtasks deliberately or inject a `Clock` provider and fake **that** instead. Same for randomness and IDs: inject a generator, or mock `crypto.randomUUID`.

## Testing async code and errors

```ts
await expect(service.findOne('x')).rejects.toThrow(NotFoundException);   // type
await expect(service.findOne('x')).rejects.toThrow('User x not found');  // message (substring)
await expect(service.findOne('x')).rejects.toMatchObject({ status: 404 });
```

Always `await` (or `return`) the assertion. A missing `await` makes the test pass before the promise settles. Add `expect.assertions(n)` when a test could silently skip assertions inside callbacks.

## What to assert (and not)

| Assert | Why |
|--------|-----|
| Return values | The contract of the method |
| Thrown exceptions (type and key message) | Error paths are behavior |
| Side effects that matter to the business (a record created with the right data, an email sent, nothing saved on failure) | Observable outcomes |
| **Avoid:** exact call order, call counts of incidental helpers, private methods | Locks tests to the implementation; breaks on refactors |

Don't test private methods directly. Test them through the public method that uses them. If they deserve their own tests, they probably deserve to be a separate class.

## Common mistakes

- **Not awaiting async assertions** (`expect(promise).rejects...` without `await`).
- **Sharing mocks across tests** without resetting, so call counts leak.
- **Testing the mock**: asserting that a mock returns what you told it to, with no logic of your own under test.
- **One giant test** covering many branches; failures are hard to diagnose.
- **Mirroring the implementation** in the test (same `if`s), which can't catch logic errors.
- **Using real timers, dates, or randomness.**
- **Calling a controller method directly and believing guards/pipes were tested.**
- **Only testing the happy path.**

## Debugging

- A test passes but shouldn't? Check for a missing `await`, or temporarily break the code to confirm the test fails.
- `Cannot read properties of undefined` on a mock: the mock lacks a method (`findById` not defined on the `useValue` object) or a `mockResolvedValue` wasn't set for that path.
- Mock call counts wrong across tests: add `jest.clearAllMocks()` in `afterEach` or enable `clearMocks: true` in the Jest config.
- Timeout with fake timers: you're awaiting a promise that depends on a timer you haven't advanced.

## Quick Summary

- Unit-test services: mock dependencies, cover the happy path and every error branch.
- One behavior per test, named clearly; Arrange, Act, Assert.
- Assert outcomes and meaningful side effects, not internals.
- Keep controllers thin; their routing, validation, and guards belong to E2E tests.
- Control time and randomness; always `await` async assertions.

## Next

[Mocking →](./03-mocking.md)