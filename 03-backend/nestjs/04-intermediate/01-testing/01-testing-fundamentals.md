# Testing Fundamentals

Automated tests let you change code without fear. In Nest, the good news is that the framework's architecture (small classes with injected dependencies) makes most code easy to test. The skill is choosing **what to test at which level** and keeping tests fast, deterministic, and meaningful.

## The tooling

A project created with `nest new` ships with:

| Tool | Role |
|------|------|
| **Jest** | Test runner, assertions, mocking |
| **ts-jest** | Compiles TypeScript for Jest |
| **`@nestjs/testing`** | Builds a test DI container (`Test.createTestingModule`) |
| **Supertest** | Sends HTTP requests to your app in-process |

```bash
npm run test          # unit tests (src/**/*.spec.ts)
npm run test:watch    # re-run on change
npm run test:cov      # coverage report
npm run test:e2e      # end-to-end tests (test/*.e2e-spec.ts)
```

Conventions from the scaffold:

```text
src/
└── cats/
    ├── cats.service.ts
    └── cats.service.spec.ts      ← unit test next to the code
test/
├── app.e2e-spec.ts               ← e2e tests
└── jest-e2e.json                 ← separate Jest config for e2e
```

The unit-test Jest config lives in `package.json` (`rootDir: 'src'`, `testRegex: '.*\\.spec\\.ts$'`). E2E has its own config file so it can have different setup and timeouts. Other runners (for example Vitest) can work too, but this repository assumes Jest because the docs and tooling around Nest do.

## Three levels of tests

| Level | What runs | What's fake | Speed | Catches |
|-------|-----------|-------------|-------|---------|
| **Unit** | One class | All its dependencies | Milliseconds | Logic bugs, branching, error handling |
| **Integration** | Several real classes, usually with a real DB | External services (email, payments) | Seconds | Wiring, queries, transactions, constraints |
| **E2E** | Whole app over HTTP | Only third-party boundaries | Seconds to minutes | Routing, pipes/guards/filters, serialization, full flows |

A healthy suite has **many unit tests, fewer integration tests, and a small number of E2E tests** covering critical paths. Each level covers a blind spot of the others: unit tests can't tell you your SQL is wrong; E2E tests are too slow to cover every branch.

### What to test where

| Code | Best level |
|------|-----------|
| Business rules in services | Unit |
| Pure utility functions | Unit |
| Guards, pipes, interceptors, filters | Unit (+ a few E2E for wiring) |
| Repository queries, transactions, constraints | Integration (real DB) |
| Validation + serialization contract of an endpoint | E2E |
| Auth flows, critical user journeys | E2E |
| Controllers | Thin, so mostly covered by E2E; unit-test only if they contain logic |

Don't test the framework or libraries (that `@Get()` routes, that class-validator validates). Test **your** behavior.

## `Test.createTestingModule`

The core of Nest testing: build a module with only what the test needs.

```ts
// cats.service.spec.ts
import { Test, TestingModule } from '@nestjs/testing';
import { CatsService } from './cats.service';

describe('CatsService', () => {
  let service: CatsService;

  beforeEach(async () => {
    const moduleRef: TestingModule = await Test.createTestingModule({
      providers: [CatsService],
    }).compile();

    service = moduleRef.get(CatsService);
  });

  it('is defined', () => {
    expect(service).toBeDefined();
  });
});
```

- `createTestingModule` accepts the same metadata as `@Module` (`imports`, `providers`, `controllers`).
- `.compile()` resolves the dependency graph (async). **Unresolvable dependencies fail here**, exactly as at app startup.
- `moduleRef.get(Token)` retrieves a singleton provider. For request-scoped or transient providers use `await moduleRef.resolve(Token)` ([ModuleRef](../../03-core-concepts/04-modules-and-di/08-module-ref-and-lazy-loading.md)).
- To create a full app for HTTP tests: `moduleRef.createNestApplication()` ([E2E](./06-e2e-testing.md)).

If a unit test fails with `Nest can't resolve dependencies of CatsService (?)`, your test module lacks a provider the class needs: provide a mock for it ([mocking](./03-mocking.md)).

### You don't always need the Nest container

For services, plain instantiation is often simpler and faster:

```ts
const repo = { find: jest.fn() };
const service = new CatsService(repo as any);
```

Using the testing module is better when you want to exercise DI wiring, tokens, or `overrideProvider`. Both are valid; pick consistently within a project.

## Anatomy of a good test

**Arrange → Act → Assert**:

```ts
it('returns the cat when it exists', async () => {
  // Arrange
  repo.findOneBy.mockResolvedValue({ id: 1, name: 'Tom' });

  // Act
  const result = await service.findOne(1);

  // Assert
  expect(result).toEqual({ id: 1, name: 'Tom' });
});
```

Qualities worth enforcing:

- **One behavior per test**; the test name states it: `'throws NotFoundException when the cat does not exist'`.
- **Independent**: no test relies on another having run first; each starts from a known state.
- **Deterministic**: no dependence on real time, randomness, network, or execution order. Control them (fake timers, injected clock, seeded data).
- **Tests behavior, not implementation**: assert on results and meaningful side effects, not on every internal call. Over-specified tests break on harmless refactors.
- **Readable failures**: assertions that say what was expected.

## Useful Jest basics

```ts
describe('group', () => {
  beforeAll(() => {});    // once per file/describe
  afterAll(() => {});
  beforeEach(() => {});   // before every test
  afterEach(() => {});

  it.each([[1, 2, 3], [2, 3, 5]])('adds %i + %i = %i', (a, b, sum) => {
    expect(a + b).toBe(sum);
  });
});

await expect(promise).rejects.toThrow(NotFoundException);
expect(fn).toHaveBeenCalledWith('x');
expect(obj).toMatchObject({ id: 1 });
```

Useful CLI flags: `--watch`, `-t "name pattern"`, `--runInBand` (serial; handy for DB tests), `--detectOpenHandles` (find why Jest doesn't exit), `--coverage`.

## Coverage: a tool, not a goal

`npm run test:cov` shows which lines ran, not whether they were **verified**. A test that calls code without meaningful assertions raises coverage and proves nothing. Use coverage to find untested branches and error paths, not to chase a number. Be suspicious of 100% achieved by mocks that mirror the implementation.

## Common mistakes

- **Testing the framework** (that decorators work) instead of your logic.
- **Over-mocking**, so tests pass while the real integration is broken ([mocking](./03-mocking.md)).
- **Shared mutable state** between tests (module-level variables, a DB not reset).
- **Not closing resources** (apps, DB connections), so Jest hangs.
- **Testing implementation details**: asserting call order and counts everywhere.
- **Relying on real time/network** causing flaky tests.
- **Skipping error paths.** Most production bugs live there.
- **Huge `beforeEach` setups** that make each test hard to understand.

## Debugging

- Run one test: `npx jest path/to/file.spec.ts -t "name"`.
- Failing only in a full run? Shared state or leftover mocks. Use `clearMocks`/`restoreMocks` in config or `afterEach` ([mocking](./03-mocking.md)).
- Jest won't exit? `--detectOpenHandles`, then close the app/connection in `afterAll`.
- Debug in an IDE or run `node --inspect-brk node_modules/.bin/jest --runInBand` and attach a debugger ([debugging](../../01-getting-started/06-debugging.md)).
- `Nest can't resolve dependencies` in a test: provide the missing token.

## Quick Summary

- Jest + `@nestjs/testing` + Supertest are the default stack.
- Three levels: unit (fast, mocked), integration (real DB, no HTTP), E2E (full HTTP). Many unit tests, a few E2E.
- `Test.createTestingModule({...}).compile()` builds a test DI container; `get()` for singletons, `resolve()` for scoped.
- Tests should be independent, deterministic, and assert behavior, not internals.
- Coverage shows what ran, not what's verified.

## Next

[Unit testing →](./02-unit-testing.md)