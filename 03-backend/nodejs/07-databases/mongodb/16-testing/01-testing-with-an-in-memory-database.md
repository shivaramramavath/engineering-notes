# Testing with an In-Memory Database

`mongodb-memory-server` — spins up a real, temporary MongoDB instance for tests to run against, giving you genuine Mongoose behavior (casting, validation, indexes, middleware) without needing a shared test database or mocking Mongoose itself.

## Why not just mock Mongoose entirely?

```js
// mocking Mongoose's Model methods directly
jest.mock("../models/User.js");
User.findOne.mockResolvedValue({ email: "test@example.com" });
```

This works for testing code that merely _calls_ Mongoose methods, but tells you nothing about whether your actual schema's validation rules, indexes, or middleware hooks behave correctly — a mock just returns whatever you told it to, regardless of whether the real schema would have rejected that data. `mongodb-memory-server` runs real Mongoose against a real (if temporary) database, so validation, casting, unique indexes, and `pre`/`post` hooks all genuinely execute.

---

## Install

```bash
npm install -D mongodb-memory-server
```

---

## Basic setup

```js
// test/setup.js
import { MongoMemoryServer } from "mongodb-memory-server";
import mongoose from "mongoose";

let mongoServer;

export async function connectTestDB() {
  mongoServer = await MongoMemoryServer.create();
  const uri = mongoServer.getUri();
  await mongoose.connect(uri);
}

export async function closeTestDB() {
  await mongoose.disconnect();
  await mongoServer.stop();
}

export async function clearTestDB() {
  const collections = mongoose.connection.collections;
  for (const key in collections) {
    await collections[key].deleteMany({});
  }
}
```

`MongoMemoryServer.create()` downloads (on first run) and starts an actual, temporary MongoDB binary, giving you a real connection URI — from Mongoose's perspective, it's indistinguishable from a genuine MongoDB deployment.

---

## Using it in a test suite

```js
// user.test.js
import { connectTestDB, closeTestDB, clearTestDB } from "./setup.js";
import User from "../models/User.js";

beforeAll(async () => {
  await connectTestDB();
});

afterEach(async () => {
  await clearTestDB(); // start each test with a clean slate
});

afterAll(async () => {
  await closeTestDB();
});

test("creates a user with valid data", async () => {
  const user = await User.create({ name: "Alice", email: "alice@example.com" });
  expect(user.name).toBe("Alice");
});

test("rejects a user with an invalid email", async () => {
  await expect(
    User.create({ name: "Alice", email: "not-an-email" }),
  ).rejects.toThrow("ValidationError");
});

test("rejects a duplicate email", async () => {
  await User.create({ email: "alice@example.com", name: "Alice" });
  await expect(
    User.create({ email: "alice@example.com", name: "Alice 2" }),
  ).rejects.toMatchObject({ code: 11000 });
});
```

That last test is a genuinely important one — it verifies the _actual_ unique index and duplicate-key behavior from `08-errors/04-duplicate-key-errors.md`, something a mocked Mongoose could never meaningfully exercise, since the uniqueness constraint lives at the database level, not in application code.

---

## `beforeEach`/`afterEach` for isolation between tests

```js
afterEach(async () => {
  await clearTestDB();
});
```

Clearing all collections between tests (rather than, say, only clearing what one specific test created) keeps tests independent — one test's leftover data should never affect another's assertions. For a large test suite, deleting every document in every collection after each test is usually fast enough not to matter, given the in-memory nature of the database.

---

## Testing middleware hooks for real

```js
test("hashes the password before saving", async () => {
  const user = await User.create({
    email: "a@example.com",
    password: "plaintext123",
  });
  expect(user.password).not.toBe("plaintext123"); // the pre("save") hook from 10-middleware-hooks/03- actually ran
  expect(await bcrypt.compare("plaintext123", user.password)).toBe(true);
});
```

Because this runs against a real MongoDB instance with real Mongoose model instances, the actual `pre("save")` hashing hook (`10-middleware-hooks/03-common-hook-patterns.md`) genuinely executes — this is exactly the kind of test a fully-mocked approach couldn't provide confidence in.

---

## Testing indexes

```js
test("has a unique index on email", async () => {
  const indexes = await User.collection.getIndexes();
  expect(indexes).toHaveProperty("email_1");
});
```

Confirms the index actually exists as expected — relevant especially given `autoIndex` behaves differently between development/test and production (`04-schemas/07-indexes-in-schemas.md`); worth explicitly ensuring indexes are built in the test environment (`MongoMemoryServer` uses Mongoose's default `autoIndex: true` behavior unless configured otherwise, which is usually fine for a temporary test database).

---

## Speed considerations

```js
// one shared in-memory server across an entire test file/suite — faster than starting a new one per test
beforeAll(async () => {
  await connectTestDB(); // once per file, not once per test
});
```

Starting a fresh `MongoMemoryServer` instance is relatively fast, but still meaningfully slower than a plain in-process mock — share one instance across all tests in a file (or even an entire suite, depending on your test runner's configuration) rather than creating a new one per individual test, and rely on `clearTestDB()` between tests for isolation instead.

## Common mistakes

- **Starting a new `MongoMemoryServer` for every individual test** — much slower than necessary; share one instance per test file/suite and clear data between tests instead.
- **Forgetting to clear collections between tests** — leftover data from one test can cause confusing, order-dependent failures in another.
- **Relying entirely on `mongodb-memory-server` and never unit-testing business logic in isolation** — integration tests are slower and shouldn't be the only layer of testing; `02-mocking-and-fixtures.md` covers the faster, complementary unit-test approach.
- **Not testing things that specifically depend on real database behavior** (unique indexes, casting edge cases, middleware) — these are exactly the tests where mocking would give false confidence, and an in-memory database is the right tool.

## Quick summary

- `mongodb-memory-server` provides a real, temporary MongoDB instance, giving genuine Mongoose behavior — actual validation, casting, unique-index enforcement, and middleware execution — rather than mocked approximations
- Share one server instance per test file/suite for speed, clearing collections between individual tests for isolation
- This is the right tool specifically for verifying behavior that depends on the real database (duplicate-key errors, hooks, indexes) — not a replacement for fast, mocked unit tests of pure business logic

## Next

**`02-mocking-and-fixtures.md`** covers the complementary, faster approach: mocking the repository layer for pure unit tests, and building reusable test data.
