# Integration Testing

An integration test runs **several real parts together**: typically your services and repositories against a **real database**, with only the outside world (email, payment providers, the clock) faked. There's no HTTP layer. You call providers directly from the test.

Unit tests can't tell you whether your query returns the right rows, whether a unique constraint fires, or whether a transaction rolls back. Integration tests can.

Prerequisites: [Testing fundamentals](./01-testing-fundamentals.md), [mocking](./03-mocking.md), [database foundations](../02-database-foundations/README.md).

## What belongs here

| Good integration targets | Why not unit |
|--------------------------|--------------|
| Repository queries, filters, sorting, pagination | Only a real DB proves the SQL/query is right |
| Constraints (unique, foreign key, not-null) and the errors they raise | Mocks can't enforce constraints ([database errors](../02-database-foundations/07-database-errors.md)) |
| Transactions and rollback behavior | Needs real transaction semantics ([transactions](../02-database-foundations/04-transactions.md)) |
| Services working through real repositories | Verifies modules are wired and data flows end to end |
| Migrations producing the schema your code expects | Schema drift is invisible to unit tests ([migrations](../02-database-foundations/05-migrations.md)) |

If a test needs HTTP concerns (routing, guards, validation, serialization), it's [E2E](./06-e2e-testing.md).

## Use a real database of the same kind as production

An in-memory substitute (SQLite for Postgres, for example) is fast but differs in types, JSON operators, constraints, case sensitivity, locking, and transaction behavior. You can end up with green tests and a broken production query.

| Option | Realism | Notes |
|--------|---------|-------|
| **Real DB in Docker** (Postgres/MySQL/Mongo container) | High | A compose file for local and CI; the usual choice |
| **Testcontainers** | High | Starts a throwaway container from test code; no manual setup, slower startup |
| **In-memory/embedded** (SQLite, `mongodb-memory-server`) | Medium/low | Quick for simple cases; verify behavior matches production for what you rely on |
| **Shared dev database** | n/a | **Don't.** Tests will destroy data and conflict |

## Setting up the test module

Import your real feature modules and the real DB module, pointed at a **test database**.

```ts
// users.integration.spec.ts
describe('UsersService (integration)', () => {
  let moduleRef: TestingModule;
  let service: UsersService;
  let dataSource: DataSource;

  beforeAll(async () => {
    moduleRef = await Test.createTestingModule({
      imports: [
        ConfigModule.forRoot({ envFilePath: '.env.test', isGlobal: true }),
        TypeOrmModule.forRootAsync({
          inject: [ConfigService],
          useFactory: (config: ConfigService) => ({
            type: 'postgres',
            url: config.getOrThrow('DATABASE_URL'),
            entities: [User],
            synchronize: false,          // schema comes from migrations (below)
          }),
        }),
        UsersModule,
      ],
    })
      .overrideProvider(MailService).useValue({ send: jest.fn() })   // fake the edges
      .compile();

    await moduleRef.init();               // runs lifecycle hooks (connections etc.)
    service = moduleRef.get(UsersService);
    dataSource = moduleRef.get(DataSource);
  });

  afterAll(async () => {
    await moduleRef.close();              // closes the DB connection so Jest can exit
  });

  beforeEach(async () => {
    await dataSource.query('TRUNCATE TABLE "user" RESTART IDENTITY CASCADE');
  });

  it('enforces unique emails at the database level', async () => {
    await service.register({ email: 'a@b.com', password: 'secret123' });
    await expect(service.register({ email: 'a@b.com', password: 'secret123' }))
      .rejects.toThrow(ConflictException);
  });
});
```

Notes:

- `beforeAll` builds the module **once per file** (connections are expensive); `beforeEach` resets **data**.
- Always `close()` in `afterAll`, or Jest hangs on open handles.
- Override only boundaries (`MailService`, payment client, clock). Keep the rest real.
- The same shape works with Prisma (use your `PrismaService`) and Mongoose (`MongooseModule.forRoot` with a test URI).

## Schema: use migrations, not `synchronize`

If the test DB is built with `synchronize: true` but production uses migrations, you're testing a schema that production never has. Apply real migrations before the suite:

```bash
# Prisma
npx prisma migrate deploy        # apply committed migrations to the test DB
npx prisma migrate reset --force # drop + recreate + apply (local test DB only!)

# TypeORM
npm run typeorm migration:run -- -d src/data-source.ts
```

Wire this into a Jest `globalSetup` file or a `pretest:int` npm script, so tests always run against the current schema. This also tests that your migrations work from scratch ([migrations](../02-database-foundations/05-migrations.md)).

## Keeping tests isolated: cleaning data

| Strategy | How | Trade-offs |
|----------|-----|-----------|
| **Truncate tables** in `beforeEach` | `TRUNCATE ... RESTART IDENTITY CASCADE` for all tables | Simple, reliable; slower with many tables |
| **Transaction rollback** | Begin a transaction per test, roll back after | Fast; but code that manages its own transactions or uses multiple connections is awkward to test |
| **Fresh database/schema per file or worker** | Create a schema/DB named by `JEST_WORKER_ID` | Strong isolation, enables parallelism; more setup |
| **Delete only what you created** | Track created IDs | Fragile; leftovers leak |

Truncation is the best default. Keep a helper:

```ts
export async function resetDatabase(ds: DataSource) {
  const tables = ds.entityMetadatas.map((m) => `"${m.tableName}"`).join(', ');
  await ds.query(`TRUNCATE TABLE ${tables} RESTART IDENTITY CASCADE`);
}
```

## Parallelism: the most common integration-test trap

Jest runs **test files in parallel workers**. If every worker shares one database, files truncate each other's data and you get flaky, order-dependent failures.

Options:

- Run DB tests serially: `jest --runInBand` (simple; slower).
- **One database or schema per worker**, derived from `process.env.JEST_WORKER_ID`, created in setup.
- Keep integration tests in their own Jest project/config (`jest-integration.json`) so unit tests stay parallel and fast.

## Test data

Prefer small **builders/factories** over fixtures that mirror the whole database:

```ts
export const aUser = (overrides: Partial<User> = {}): Partial<User> => ({
  email: `user-${Math.random().toString(36).slice(2)}@test.com`,
  passwordHash: 'hash',
  ...overrides,
});

await repo.save(aUser({ role: 'admin' }));
```

Each test creates exactly the data it needs, so it's readable and independent. Avoid depending on a big shared seed.

## Safety: never run against the wrong database

Truncating tables is destructive. Guard it:

```ts
// jest global setup or helper
const url = process.env.DATABASE_URL ?? '';
if (!/test/i.test(new URL(url).pathname)) {
  throw new Error(`Refusing to run integration tests against non-test database: ${url}`);
}
```

Use a dedicated `.env.test`, and never point it at a shared or production database. Don't print the URL with credentials in logs.

## Faking the outside world

| Boundary | Approach |
|----------|----------|
| Email/SMS/push | Override the provider with a recorder (`jest.fn()` or in-memory outbox) |
| Payment/third-party APIs | Override your wrapper provider; optionally use `nock`/`msw` for real client code |
| Clock | Inject a clock provider, or fake timers |
| Queues (BullMQ) | Override the queue producer, or use a real Redis for queue-specific tests |
| Cache | Use the in-memory store or a real Redis, consistent with production behavior you rely on |

Also test **transaction rollback** explicitly: make a step in the middle throw, then assert the earlier writes were not persisted.

## Speed

- Build the module once per file (`beforeAll`), reset data per test.
- Keep integration tests focused; don't re-test business branches already covered by unit tests.
- Run unit tests on every save, integration tests before pushing/in CI, using separate npm scripts.

## Common mistakes

- **In-memory DB that differs from production** and hides real bugs.
- **`synchronize: true`** in tests while production uses migrations.
- **Shared database across parallel Jest workers.**
- **Not closing the module/connection**, leaving Jest hanging.
- **Order-dependent tests** relying on data left by earlier tests.
- **Running against a non-test database** (and wiping it).
- **Mocking the repository in "integration" tests**, which makes them unit tests with extra steps.
- **Huge shared seed data** that nobody understands.
- **Forgetting `await moduleRef.init()`**, so connections or hooks don't start.

## Debugging

- Flaky failures that pass when run alone: parallel workers sharing a DB or leftover state. Use `--runInBand` to confirm, then isolate per worker.
- `Jest did not exit one second after the test run`: unclosed connections; run `--detectOpenHandles` and close in `afterAll`.
- Constraint errors from earlier test data: your cleanup isn't running (check `beforeEach`, foreign key order, `CASCADE`).
- "relation does not exist": migrations weren't applied to the test DB.
- Slow suite: too many tests at this level, or a full migrate/reset per file instead of once globally.

## Quick Summary

- Integration tests use real modules with a real database (same engine as production) and fake only external boundaries.
- Build the schema with migrations; reset data per test (truncate is the sturdy default).
- Parallel Jest workers need separate databases/schemas, or run serially.
- Use data builders, a `.env.test`, and a guard against running on non-test databases.
- Always close the module in `afterAll`; keep these tests focused on queries, constraints, transactions, and wiring.

## Next

[E2E testing →](./06-e2e-testing.md)