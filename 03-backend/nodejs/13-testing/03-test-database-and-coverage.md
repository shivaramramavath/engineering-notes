# Test Database & Coverage

Giving tests a real, clean database — and measuring what your tests actually cover, and what coverage can't tell you.

> Examples use the Jest-style API shared with Vitest (`01-unit-and-integration-testing.md`). Replace `jest` with `vi` for Vitest.

# Part 1 — The Test Database

## Why a real database?

Fakes and mocks (`01`, `02`) are great for business rules, but they can't prove your **SQL, indexes, constraints, transactions, and type handling** work. Those are exactly the things that break in production:

| Only a real database reveals | Example |
|---|---|
| Unique constraints and foreign keys | A duplicate email actually fails with `23505` |
| Transaction behavior | A failed order rolls back the stock update |
| Query semantics | `ORDER BY`, `NULL` handling, case sensitivity, collations |
| Driver quirks | `pg` returns `BIGINT` and `NUMERIC` as strings; `DATE` vs `TIMESTAMPTZ` |
| Migrations | The schema your code expects is the schema the migrations produce |
| Concurrency | Deadlocks, lost updates, `SELECT ... FOR UPDATE SKIP LOCKED` |

So the rule for data you own: **test against the same kind of database you run in production.**

---

## The options

| Approach | Pros | Cons | Verdict |
|---|---|---|---|
| **In-memory substitute** (SQLite for Postgres, `mongodb-memory-server` for MongoDB) | No Docker, fast to start | SQL dialect, types, and features differ (JSONB, arrays, `ON CONFLICT`, locking, collations). Tests pass here and fail in production | Avoid for relational databases; acceptable for simple Mongo code |
| **Shared dev database** | Nothing to set up | Tests destroy your data, collide with teammates, depend on leftovers, can't run in parallel | Never |
| **Docker Compose test database** | Real engine, simple, fast to reuse | You must start it yourself; one instance shared across test workers | Great default |
| **Testcontainers** | Real engine, started and stopped *by the tests*; unique per run; works the same locally and in CI | Needs Docker available; container startup adds seconds | Excellent |
| **CI service container** | Real engine in CI with no extra code | CI only: you still need a local story | Pair with Compose or Testcontainers |
| **Mocking the driver** (`pool.query = jest.fn()`) | Fast | Proves nothing about SQL | Not a database test at all |

The recommendation: **a real PostgreSQL (or your production engine) in a container**, via Docker Compose for simplicity or Testcontainers for hands-off setup.

---

## Option A: Docker Compose

```yaml
# docker-compose.test.yml
services:
  postgres-test:
    image: postgres:16
    environment:
      POSTGRES_USER: test
      POSTGRES_PASSWORD: test
      POSTGRES_DB: app_test
    ports: ["5433:5432"]                   # different host port: never collides with your dev database
    tmpfs: ["/var/lib/postgresql/data"]    # keep data in RAM: much faster, and nothing to clean up
    healthcheck:
      test: ["CMD-SHELL", "pg_isready -U test -d app_test"]
      interval: 2s
      timeout: 3s
      retries: 20
  redis-test:
    image: redis:7
    ports: ["6380:6379"]
```

```json
{
  "scripts": {
    "test:db:up": "docker compose -f docker-compose.test.yml up -d --wait",
    "test:db:down": "docker compose -f docker-compose.test.yml down -v",
    "test:int": "npm run test:db:up && vitest run int.test api.test"
  }
}
```

```bash
# .env.test
NODE_ENV=test
DATABASE_URL=postgres://test:test@localhost:5433/app_test
REDIS_URL=redis://localhost:6380
JWT_ACCESS_SECRET=test-secret-test-secret-test-secret
```

**A safety check** that has saved many a production database: make the test setup **refuse to run against anything that doesn't look like a test database**, because the cleanup step is destructive.

```js
// test/setup.js
const url = new URL(process.env.DATABASE_URL);
if (!/_test$/.test(url.pathname.slice(1)) && url.hostname !== "localhost") {
  throw new Error(`Refusing to run tests against ${url.hostname}${url.pathname}`);
}
```

---

## Option B: Testcontainers

Testcontainers starts a throwaway container from inside your test code and tears it down afterward, with a random port, so nothing needs to be running beforehand and parallel runs don't clash.

```bash
npm install -D testcontainers @testcontainers/postgresql @testcontainers/redis
```

```js
// test/globalSetup.js: runs ONCE before all test files
import { PostgreSqlContainer } from "@testcontainers/postgresql";
import { RedisContainer } from "@testcontainers/redis";
import { migrate } from "../src/db/migrate.js";

export default async function globalSetup() {
  const pg = await new PostgreSqlContainer("postgres:16").start();
  const redis = await new RedisContainer("redis:7").start();

  process.env.DATABASE_URL = pg.getConnectionUri();                 // inherited by test workers
  process.env.REDIS_URL = redis.getConnectionUrl();

  await migrate(process.env.DATABASE_URL);                           // build the schema once

  // return a teardown function (Jest/Vitest call it after all tests)
  return async () => {
    await pg.stop();
    await redis.stop();
  };
}
```

```js
// jest.config / vitest.config
export default { globalSetup: "./test/globalSetup.js", testTimeout: 30_000 };
```

Notes:

- The first run **pulls images** (slow); later runs reuse them. Pin versions (`postgres:16`) to match production.
- Container startup adds a few seconds once per run, not per test, as long as you use global setup.
- It requires Docker (or a compatible runtime such as Podman or Colima) on every developer machine and CI runner.
- Environment variables set in Jest's `globalSetup` are visible to the test workers. In Vitest, the same pattern works, or you can use `provide`/`inject` to pass values to workers. Check your runner's docs for your version.
- Testcontainers also handles Kafka, MongoDB, MySQL, LocalStack (AWS), and more.

### MongoDB

```js
import { MongoDBContainer } from "@testcontainers/mongodb";
const mongo = await new MongoDBContainer("mongo:7").start();
process.env.MONGO_URL = `${mongo.getConnectionString()}/?directConnection=true`;
```

`mongodb-memory-server` downloads and runs a real `mongod` binary without Docker. It's convenient and close to production for typical queries, but it won't match your production version or replica-set features (transactions need a replica set: `MongoMemoryReplSet`). See `07-databases/mongodb/03-transactions.md`.

---

## Schema: run your real migrations

Tests should build the schema the **same way production does**, using your migration tool, so migrations are tested continuously and the test schema can't drift from reality.

```js
// test/helpers/db.js
import pg from "pg";

export const pool = new pg.Pool({ connectionString: process.env.DATABASE_URL, max: 5 });

export async function resetDatabase() {
  // Discover tables dynamically so you never forget to add a new one
  const { rows } = await pool.query(`
    SELECT tablename FROM pg_tables
    WHERE schemaname = 'public' AND tablename NOT IN ('migrations', 'knex_migrations', '_prisma_migrations')
  `);
  if (rows.length === 0) return;
  const tables = rows.map((r) => `"${r.tablename}"`).join(", ");
  await pool.query(`TRUNCATE ${tables} RESTART IDENTITY CASCADE`);
}
```

Run migrations **once** in global setup (`migrate up` against the test database), not before every test. (Avoid `sync({ force: true })`-style auto-schema shortcuts in ORMs for anything beyond throwaway prototypes: they skip your migrations, so nobody notices when a migration is broken.)

---

## Isolation: how tests avoid stepping on each other

Every test must start from a **known state** and must not be affected by other tests, including those running in parallel. There are four main strategies.

### Strategy 1: Truncate between tests (simple, robust)

```js
beforeEach(async () => { await resetDatabase(); });
afterAll(async () => { await pool.end(); });
```

- ✅ Easy to understand, works with any code (including code that opens its own connections and commits real transactions).
- ❌ `TRUNCATE` costs ~1–20 ms per test depending on table count. Fine for hundreds of tests, noticeable for tens of thousands.
- ❌ Requires **serial execution within a database**, because two test files truncating the same tables simultaneously will corrupt each other. Solve this with per-worker databases (Strategy 4).

### Strategy 2: Roll back a transaction per test (fastest)

Open a transaction before each test, run the test on that connection, and `ROLLBACK` afterward. Nothing is ever committed, so there's nothing to clean.

```js
// test/helpers/txn.js
import { pool } from "./db.js";

export async function withRollback(testFn) {
  const client = await pool.connect();
  await client.query("BEGIN");
  try {
    await testFn(client);                          // pass the transaction-bound client as `db`
  } finally {
    await client.query("ROLLBACK");
    client.release();
  }
}
```

```js
test("create then findByEmail round-trips", () =>
  withRollback(async (db) => {
    const repo = makeUserRepository({ db });        // repositories accept an injected executor (10-architecture/03)
    await repo.create({ email: "a@b.com", name: "A", passwordHash: "h" });
    expect(await repo.findByEmail("a@b.com")).not.toBeNull();
  }));
```

Or as hooks:

```js
let db, client;
beforeEach(async () => { client = await pool.connect(); await client.query("BEGIN"); db = client; });
afterEach(async () => { await client.query("ROLLBACK"); client.release(); });
```

- ✅ Very fast, perfectly clean, parallel-friendly (each test sees only its own uncommitted data).
- ❌ **Everything the test touches must use that one connection.** The code under test must accept the injected `db`, which is why DI and "pass a transaction handle" (`10-architecture/03-repository-and-service-pattern.md`) matter.
- ❌ Code that begins its **own transaction** (`withTransaction`) can use a **savepoint** instead (nested transactions via `SAVEPOINT`), which needs support in your helper.
- ❌ It can't test **commit behavior**: constraints deferred to commit (`DEFERRABLE INITIALLY DEFERRED`), triggers on commit, or **concurrency between connections** (two real connections racing).
- ❌ Doesn't work for **API tests** where the app uses its own pool, unless you inject the transaction-bound client into the whole app (`buildApp({ db: client })`), which works if everything uses the injected `db`.

### Strategy 3: Unique data per test (no cleanup needed)

Instead of resetting, make every test use **unique values** so tests can't collide:

```js
const email = `user-${crypto.randomUUID()}@example.com`;
const user = await createUser({ email });
```

- ✅ No cleanup cost; trivially parallel.
- ❌ Data accumulates (fine for a throwaway container; messy in a long-lived database).
- ❌ Tests that **count** rows or list "all" records see other tests' data. Scope queries by the test's own entities (`WHERE author_id = $1`).
- Best as a *complement* to truncation or rollback.

### Strategy 4: One database (or schema) per parallel worker

Jest and Vitest run test files in **parallel worker processes**. Give each worker its own database so they can truncate freely without interfering:

```js
// test/setup.js (runs in each worker)
import pg from "pg";

const workerId = process.env.JEST_WORKER_ID ?? process.env.VITEST_POOL_ID ?? "1";
const dbName = `app_test_${workerId}`;

// connect to the maintenance DB to create this worker's database from a pre-migrated template
const admin = new pg.Client({ connectionString: process.env.ADMIN_DATABASE_URL });
await admin.connect();
await admin.query(`DROP DATABASE IF EXISTS ${dbName}`);
await admin.query(`CREATE DATABASE ${dbName} TEMPLATE app_test_template`);     // template = migrated schema: copying is fast
await admin.end();

process.env.DATABASE_URL = replaceDbName(process.env.DATABASE_URL, dbName);
```

- Create `app_test_template` once in global setup (run migrations on it), and each worker clones it. `CREATE DATABASE ... TEMPLATE` is much faster than re-running migrations.
- A **schema per worker** (`SET search_path`) is a lighter variant, but extensions and some features are database-wide.
- ✅ Full parallelism with simple truncation inside each worker.
- ❌ More setup; `max_connections` must accommodate workers × pool size.

### Choosing

| Situation | Use |
|---|---|
| Starting out, modest test count | **Truncate** between tests, run DB tests serially (`--runInBand` / `--no-file-parallelism`) |
| Growing suite, repositories take an injected `db` | **Rollback per test** for repository/service tests |
| API tests where the app owns its connections | **Truncate** + **per-worker databases** for parallelism |
| Tests need real concurrency or commit semantics | Real committed data, **truncate** after, and unique data per test |
| Always | Add **unique data** (random emails/IDs) so ordering and leftovers don't matter |

---

## Seeding and fixtures

### Insert only what the test needs

```js
// test/helpers/factories.js: persisted builders
import { pool } from "./db.js";
import { buildUser, buildPost } from "./builders.js";      // in-memory builders from 01

export async function createUser(overrides = {}) {
  const u = buildUser(overrides);
  const { rows } = await pool.query(
    `INSERT INTO users (id, email, name, role, is_premium, password_hash)
     VALUES ($1, $2, $3, $4, $5, $6) RETURNING *`,
    [u.id, u.email, u.name, u.role, u.isPremium, overrides.passwordHash ?? "test-hash"]
  );
  return { ...u, ...rowToUser(rows[0]) };
}

export async function createPost(overrides = {}) {
  const author = overrides.authorId ? { id: overrides.authorId } : await createUser();
  const p = buildPost({ authorId: author.id, ...overrides });
  await pool.query("INSERT INTO posts (id, author_id, title, body, visibility) VALUES ($1,$2,$3,$4,$5)",
    [p.id, p.authorId, p.title, p.body, p.visibility ?? "public"]);
  return p;
}
```

Each test builds its own world explicitly, so the test file tells the whole story, with no mystery data from a shared seed script.

### Avoid giant shared fixtures

A global `seed.sql` that every test depends on creates **invisible coupling**: change one row for one test and 30 others break; and nobody knows which test needs which row. Use **factories per test**. Reserve seed data for **reference data** that production also has (countries, roles, plans), loaded once after migrations.

### Real database features worth testing deliberately

```js
test("the unique index prevents duplicate emails even under concurrency", async () => {
  const results = await Promise.allSettled(
    Array.from({ length: 10 }, () => repo.create({ email: "race@example.com", name: "A", passwordHash: "h" }))
  );
  expect(results.filter((r) => r.status === "fulfilled")).toHaveLength(1);
});

test("placing an order rolls back the stock decrement if a later step fails", async () => {
  const product = await createProduct({ stock: 5 });
  await expect(orderRepo.createWithStockUpdate({ ...input, failAfterStock: true })).rejects.toThrow();
  expect((await productRepo.findById(product.id)).stock).toBe(5);       // unchanged: the transaction rolled back
});

test("conditional stock update refuses to go negative", async () => {
  const product = await createProduct({ stock: 1 });
  const results = await Promise.all([decrementStock(product.id, 1), decrementStock(product.id, 1)]);
  expect(results.filter(Boolean)).toHaveLength(1);                       // `UPDATE ... WHERE stock >= $1` protects it
  expect((await productRepo.findById(product.id)).stock).toBe(0);
});
```

These tests need **separate connections and real commits**, so use truncate-style isolation for them rather than a single rolled-back transaction.

---

## Redis and other stores

```js
beforeEach(async () => { await redis.flushDb(); });          // clears the test Redis DB only
afterAll(async () => { await redis.quit(); });
```

- Use a **separate Redis DB index or instance** for tests, and never `flushAll` on a shared server.
- For BullMQ, use a **unique queue name per test** (or a unique prefix) and `queue.obliterate({ force: true })` afterward (`11-async-processing/01-queues-and-bullmq.md`).
- Give **rate limiters** a per-test key prefix, or flush between tests, so limits from one test don't spill into the next (`08-authentication-security/06-rate-limiting.md`).

---

## Parallelism, speed, and timeouts

- **Parallel files, serial tests within a file** is the default and works well with per-worker databases.
- If you're not ready for per-worker databases, run integration/API tests serially: Jest `--runInBand`; Vitest `--no-file-parallelism` (or `poolOptions`). Keep unit tests parallel by running them as a separate command.
- **Keep one pool per worker** (`max: 5` is plenty). Many workers × large pools can exhaust the database's `max_connections`.
- **Raise timeouts** for DB tests (`testTimeout: 30_000`) and especially for global setup, since container startup can take several seconds.
- Speed boosters: `tmpfs` for Postgres data, `fsync=off` / `synchronous_commit=off` on the test container (**tests only!**), `TRUNCATE` instead of `DELETE`, reuse containers across runs locally (Testcontainers "reuse" option), and don't re-run migrations per test.
- **Close everything** (`pool.end()`, `redis.quit()`, `server.close()`) in `afterAll`, or the runner hangs. Jest's `--detectOpenHandles` shows what's still open.

```yaml
# faster Postgres for tests ONLY: never use these settings in production
command: ["postgres", "-c", "fsync=off", "-c", "synchronous_commit=off", "-c", "full_page_writes=off"]
```

---

## Running in CI

Provide the same infrastructure with a **service container** (GitHub Actions example, `16-production/05-ci-cd.md`):

```yaml
# .github/workflows/test.yml
jobs:
  test:
    runs-on: ubuntu-latest
    services:
      postgres:
        image: postgres:16
        env: { POSTGRES_USER: test, POSTGRES_PASSWORD: test, POSTGRES_DB: app_test }
        ports: ["5432:5432"]
        options: >-
          --health-cmd "pg_isready -U test -d app_test" --health-interval 5s --health-timeout 5s --health-retries 10
      redis:
        image: redis:7
        ports: ["6379:6379"]
    env:
      DATABASE_URL: postgres://test:test@localhost:5432/app_test
      REDIS_URL: redis://localhost:6379
      JWT_ACCESS_SECRET: ci-test-secret-ci-test-secret-ci
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-node@v4
        with: { node-version: 22, cache: npm }
      - run: npm ci
      - run: npm run db:migrate
      - run: npm test -- --coverage
```

If you use Testcontainers instead, the runner just needs Docker, which GitHub's `ubuntu-latest` provides, and no service block is required.

---

# Part 2 — Code Coverage

## What coverage measures

**Code coverage** reports which parts of your code were *executed* while the tests ran.

| Metric | Question it answers |
|---|---|
| **Line / statement** | Was this line executed? |
| **Branch** | Was *each side* of every `if`/`?:`/`&&`/`switch` taken? |
| **Function** | Was each function called at least once? |

**Branch coverage** is the most informative. A line like `return user.isPremium ? a : b` can be 100% line-covered while one branch never ran.

```js
export function shippingCost(order) {
  if (order.total > 5000) return 0;              // branch A
  if (order.country !== "IN") return 2500;       // branch B
  return 500;                                    // branch C
}

test("free shipping over 5000", () => expect(shippingCost({ total: 6000, country: "IN" })).toBe(0));
// line coverage: 66%  |  branch coverage: 50%: B and C never tested
```

---

## Generating coverage

### Vitest

```bash
npm install -D @vitest/coverage-v8
```

```js
// vitest.config.js
export default defineConfig({
  test: {
    coverage: {
      provider: "v8",                                    // uses V8's built-in coverage: fast, no instrumentation step
      reporter: ["text", "html", "lcov"],               // terminal table, browsable report, CI/Codecov format
      include: ["src/**/*.js"],
      exclude: ["src/**/*.test.js", "src/server.js", "src/config/**", "src/db/migrations/**"],
      thresholds: { lines: 80, functions: 80, branches: 75, statements: 80 },    // build FAILS below these
    },
  },
});
```

```bash
npx vitest run --coverage
open coverage/index.html          # annotated source: red lines = never executed
```

### Jest

```json
{
  "scripts": { "test:coverage": "NODE_OPTIONS=--experimental-vm-modules jest --coverage" },
  "jest": {
    "coverageProvider": "v8",
    "collectCoverageFrom": ["src/**/*.js", "!src/**/*.test.js", "!src/server.js"],
    "coverageReporters": ["text", "lcov", "html"],
    "coverageThreshold": { "global": { "branches": 75, "functions": 80, "lines": 80, "statements": 80 } }
  }
}
```

### Node's built-in runner and `c8`

```bash
node --test --experimental-test-coverage       # built in (experimental)
npx c8 --reporter=text --reporter=lcov node --test    # c8 wraps V8 coverage for any Node process
```

### Reading the report

```
-----------------------|---------|----------|---------|---------|-------------------
File                   | % Stmts | % Branch | % Funcs | % Lines | Uncovered Line #s
-----------------------|---------|----------|---------|---------|-------------------
 orders/pricing.js     |     100 |      100 |     100 |     100 |
 orders/orders.service |      92 |       80 |     100 |      92 | 48-52
 auth/auth.service.js  |      61 |       40 |      75 |      61 | 30-44,71-80
-----------------------|---------|----------|---------|---------|-------------------
```

The valuable part is the **"Uncovered Line #s"** column: open the HTML report and look at *which* error paths and branches nothing exercises.

---

## What coverage can and can't tell you

> **Low coverage tells you code is definitely untested. High coverage does NOT tell you code is well tested.**

```js
// 100% line and branch coverage, and it verifies NOTHING
test("calculateTotal runs", () => {
  calculateTotal({ isPremium: true }, [{ priceCents: 1000, quantity: 2 }]);    // no assertion!
});
```

Coverage counts *execution*, not *verification*. Things it can't see:

- **Missing assertions** (above) or weak ones (`expect(result).toBeDefined()`)
- **Untested behavior that no code exists for:** a missing validation or authorization check leaves nothing to be "uncovered"
- **Bad inputs and edge cases** that follow the same line through different values (`0`, `-1`, `NaN`, `""`)
- **Integration correctness:** both sides covered doesn't mean they agree
- **Concurrency and timing** issues

### Goodhart's law

*"When a measure becomes a target, it ceases to be a good measure."* A mandated 90% often produces tests written to touch lines (assertion-free, mock-everything) rather than to catch bugs. Teams hitting the number by gaming it end up with *worse* suites that feel safe.

### Sensible use

| Do | Don't |
|---|---|
| Use coverage to **find untested code** (error paths, branches) | Treat the percentage as a quality score |
| Set a **modest threshold as a floor** (70–85%) to prevent decay | Demand 100% (it incentivizes junk tests and slows change) |
| Ratchet up gradually: "coverage must not decrease" on PRs | Block merges on an arbitrary global target with no context |
| Aim for **higher coverage in critical code** (money, auth, permissions, state machines) | Spend equal effort on config, glue code, and generated files |
| Review the **uncovered lines in a pull request's diff** (Codecov/Coveralls "patch coverage") | Only look at the global number |
| Exclude genuinely untestable boilerplate **explicitly** (`/* c8 ignore next */`, `exclude`) with a reason | Hide real logic with ignore comments |

A useful rule of thumb: **coverage of the *changed lines* in each pull request matters more than the repository-wide number.**

### Combine unit, integration, and API coverage

Coverage from different test types can be merged (`nyc merge`, `c8` with the same `--temp-directory`, or by running everything in one command). Judging unit tests alone undersells you: a repository exercised only by integration tests shows 0% in the unit report but is well tested overall.

---

## Mutation testing: testing the tests

Coverage asks, "Did the tests *run* this code?" **Mutation testing** asks the better question: "If this code were *wrong*, would a test *fail*?"

A mutation tool (**Stryker** for JavaScript) makes small deliberate bugs ("mutants") in a copy of your code: flips `>` to `>=`, replaces `+` with `-`, removes a `return`, negates a condition. Then it runs your tests against each mutant:

- **Killed:** a test failed, which is good: the tests noticed the bug.
- **Survived:** all tests still passed, which is bad: that behavior isn't actually verified.

```bash
npm install -D @stryker-mutator/core @stryker-mutator/vitest-runner     # or jest-runner
npx stryker init
npx stryker run
```

```
Mutant survived: src/pricing.js:3
- if (user.isPremium) total = Math.round(total * 0.9);
+ if (!user.isPremium) total = Math.round(total * 0.9);       ← no test caught the inverted condition
```

The **mutation score** (killed ÷ total) reveals test quality far better than line coverage. It's slow (it re-runs tests per mutant), so run it on critical modules, on a schedule, or only on changed files (`--incremental`), not on every commit.

---

## Coverage in CI

```yaml
- run: npm test -- --coverage
- uses: codecov/codecov-action@v4          # or Coveralls, SonarCloud: tracks trends, comments on PRs with patch coverage
  with:
    files: ./coverage/lcov.info
    fail_ci_if_error: false
```

- Upload `lcov.info`; let the service comment on pull requests with the **diff coverage** (how much of the *new* code is tested).
- Fail the build on **threshold regressions**, not on missing a perfect number.
- Keep the HTML report as a CI artifact so reviewers can browse it.

---

## Beyond the percentage: is my suite healthy?

Questions worth asking more than "what's the coverage?":

| Question | Healthy answer |
|---|---|
| Do tests fail when I break something on purpose? | Yes: try deleting a validation check, or inverting an `if`, and see |
| How long does the suite take? | Unit < ~30 s; full suite a few minutes; run on every PR |
| How often does a test fail for no reason (flaky)? | Practically never, because flaky tests get fixed or deleted immediately |
| Can a new teammate run all tests with one command? | Yes: `npm test` (and containers start by themselves or via one script) |
| When a bug reaches production, do we add a regression test? | Always |
| Are the scary parts (money, auth, data deletion) covered by tests that actually assert? | Yes, ideally verified with mutation testing |
| Do tests break whenever we refactor without changing behavior? | No, because they assert on behavior, not implementation |

### Tracking flaky tests

- Retry once in CI to *detect* flakiness (`retry: 1` in Vitest/Jest `jest.retryTimes(1)`), but **log it and fix it**, because retries shouldn't hide it.
- Common causes: shared state/order dependence, real clocks and sleeps, leftover data from other tests, un-awaited promises, port or file collisions, resource limits on CI machines, unseeded randomness.
- Quarantine a flaky test (`test.skip` with a ticket and an owner) rather than letting it erode trust in red builds.

---

## Common mistakes

```js
// ❌ SQLite (or another engine) standing in for production PostgreSQL → dialect/feature differences hide bugs
// ❌ tests run against the dev or a shared database → destroyed data, collisions, order dependence
// ❌ no safety check before TRUNCATE; one wrong DATABASE_URL and you wipe a real database
// ❌ rebuilding the schema with ORM auto-sync instead of real migrations
// ❌ running migrations before every test (slow) instead of once in global setup (or cloning a template database)
// ❌ shared mutable seed data that every test silently depends on
// ❌ parallel test files truncating the same database simultaneously → random failures
// ❌ forgetting to close pools/clients/servers → the runner never exits ("did not exit one second after the test run")
// ❌ rollback-per-test while the code under test uses its own connections: nothing is rolled back, data leaks
// ❌ relying on real `sleep` instead of polling (`waitFor`) or controllable clocks
// ❌ chasing 100% coverage with assertion-free tests
// ❌ excluding hard-to-test files from coverage rather than testing them
// ❌ a global threshold nobody understands, set so high that people write junk tests to hit it
// ❌ treating a high coverage number as proof of quality
```

## Checklist

**Database**
- [ ] Tests run against the same engine as production, in a container (Compose, Testcontainers, or a CI service)
- [ ] Never against a shared or developer database; a safety check blocks non-test URLs before destructive operations
- [ ] Schema built by your real migrations, run once per run (or cloned from a migrated template)
- [ ] An isolation strategy chosen: truncate, rollback-per-test, unique data, and/or per-worker databases
- [ ] Data created with per-test factories; shared seeds only for reference data
- [ ] Constraint, transaction, and concurrency behavior tested against the real database
- [ ] Redis/queues/limiters isolated by unique names, prefixes, or flushes
- [ ] All pools, clients, and servers closed in `afterAll`; timeouts raised for DB suites
- [ ] The same setup works locally (one command) and in CI

**Coverage**
- [ ] Coverage generated (v8 provider) with text + lcov + HTML reports
- [ ] A modest threshold acts as a floor; PRs are judged on the coverage of changed lines
- [ ] Uncovered branches in critical code (auth, money, permissions) reviewed and tested
- [ ] Assertions are meaningful; mutation testing considered for critical modules
- [ ] Flaky tests fixed or quarantined promptly, never ignored

## Wrap-up

This completes `13-testing/`. The through-line: **test behavior, not implementation** (`00`); **unit-test rules fast, integration-test seams for real, and keep fakes honest** (`01`); **drive the whole app over HTTP, and fake only what you don't own** (`02`); **use a real, isolated database, and treat coverage as a map of what's untested rather than a grade** (`03`).

## Next

Section **`14-logging-observability/`**: structured logging with Pino, correlation IDs, metrics and Prometheus, tracing with OpenTelemetry, and Grafana. Tests tell you the code works *before* release; observability tells you how it behaves *in* production.
