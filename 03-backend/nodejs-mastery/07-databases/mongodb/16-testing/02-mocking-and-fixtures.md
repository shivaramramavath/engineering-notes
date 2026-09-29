# Mocking & Fixtures

The faster, complementary testing approach to `01-testing-with-an-in-memory-database.md`: mocking the repository layer entirely for pure unit tests of business logic, and building reusable test data instead of duplicating setup across every test.

## Mocking the repository layer

Building directly on `15-patterns-and-architecture/01-repository-and-service-pattern.md`'s separation of concerns — because the service layer only talks to a repository (never Mongoose directly), mocking that repository is enough to unit-test the service's business logic in complete isolation.

```js
// services/userService.js
import { userRepository } from "../repositories/userRepository.js";
import { ConflictError } from "../errors/AppError.js";

export async function registerUser({ email, password }) {
  const existing = await userRepository.findByEmail(email);
  if (existing) {
    throw new ConflictError("Email already in use");
  }
  return userRepository.create({
    email,
    password: await hashPassword(password),
  });
}
```

```js
// userService.test.js
import { registerUser } from "../services/userService.js";
import { userRepository } from "../repositories/userRepository.js";

jest.mock("../repositories/userRepository.js");

test("throws ConflictError if the email is already registered", async () => {
  userRepository.findByEmail.mockResolvedValue({
    id: "1",
    email: "taken@example.com",
  });

  await expect(
    registerUser({ email: "taken@example.com", password: "x" }),
  ).rejects.toThrow("Email already in use");

  expect(userRepository.create).not.toHaveBeenCalled(); // confirms it didn't proceed past the check
});

test("creates a user when the email is available", async () => {
  userRepository.findByEmail.mockResolvedValue(null);
  userRepository.create.mockResolvedValue({
    id: "1",
    email: "new@example.com",
  });

  const user = await registerUser({ email: "new@example.com", password: "x" });

  expect(user.email).toBe("new@example.com");
  expect(userRepository.create).toHaveBeenCalledWith(
    expect.objectContaining({ email: "new@example.com" }),
  );
});
```

No database at all — real or in-memory — is involved. These tests run in milliseconds and verify the actual business logic: does `registerUser` correctly check for a duplicate, and does it correctly call `create` with the right data when it should proceed.

---

## Why this is faster and more targeted than an integration test

|          | In-memory DB (`01-`)                                        | Mocked repository (this file)                                   |
| -------- | ----------------------------------------------------------- | --------------------------------------------------------------- |
| Speed    | Slower — a real (if temporary) database round trip          | Very fast — no I/O at all                                       |
| Tests    | Real Mongoose behavior: casting, validation, indexes, hooks | Pure business logic: branching, error handling, call sequencing |
| Good for | Verifying the schema/database layer itself works correctly  | Verifying service-layer decisions are correct, in isolation     |

Neither replaces the other — a healthy test suite typically has many fast, mocked unit tests for business logic, plus a smaller number of slower integration tests specifically covering the things that genuinely need a real database to verify (duplicate-key behavior, middleware actually running, index existence).

---

## Fixtures: reusable test data

```js
// test/fixtures/users.js
export function buildUserData(overrides = {}) {
  return {
    name: "Test User",
    email: `test-${Date.now()}@example.com`, // unique per call, avoids collisions across tests
    password: "SecurePass123",
    ...overrides,
  };
}
```

```js
test("creates a user", async () => {
  const user = await User.create(buildUserData({ name: "Alice" }));
  expect(user.name).toBe("Alice");
});

test("creates an admin user", async () => {
  const admin = await User.create(buildUserData({ role: "admin" }));
  expect(admin.role).toBe("admin");
});
```

A **factory function** like `buildUserData` centralizes what a "valid test user" looks like, with sensible defaults and an easy way to override just the fields a specific test cares about — rather than every test file duplicating a full, verbose object literal (which also means every test breaks identically whenever the schema gains a new required field, unless the factory is updated once, centrally).

---

## Fixtures for related data

```js
// test/fixtures/posts.js
import { buildUserData } from "./users.js";
import User from "../../models/User.js";
import Post from "../../models/Post.js";

export async function createUserWithPosts(postCount = 3) {
  const user = await User.create(buildUserData());
  const posts = await Post.create(
    Array.from({ length: postCount }, (_, i) => ({
      title: `Post ${i + 1}`,
      author: user._id,
    })),
  );
  return { user, posts };
}
```

```js
test("finds all posts by an author", async () => {
  const { user, posts } = await createUserWithPosts(5);
  const found = await Post.find({ author: user._id });
  expect(found).toHaveLength(5);
});
```

A higher-level fixture that actually persists related data (using the real database from `01-testing-with-an-in-memory-database.md`, since this involves genuine relationships) — useful for integration tests that need a realistic, multi-document scenario set up quickly and consistently.

---

## Mocking Mongoose model methods directly (a lighter alternative to repository mocking)

```js
jest.spyOn(User, "findOne").mockResolvedValue({ email: "test@example.com" });
```

If a codebase doesn't have a repository layer (per `15-patterns-and-architecture/01-repository-and-service-pattern.md`'s guidance that it's not always warranted), you can mock Mongoose's own model methods directly — functionally similar, but ties the test more tightly to Mongoose's specific API rather than to a clean abstraction your own code defined. Reasonable for a smaller project without the repository layer; the repository-mocking approach above is cleaner once that layer exists.

---

## Testing error-handling middleware in isolation

```js
import { errorHandler } from "../middleware/errorHandler.js";
import { ConflictError } from "../errors/AppError.js";

test("errorHandler maps ConflictError to a 409 response", () => {
  const err = new ConflictError("Email already in use");
  const req = {};
  const res = {
    status: jest.fn().mockReturnThis(),
    json: jest.fn(),
  };

  errorHandler(err, req, res, jest.fn());

  expect(res.status).toHaveBeenCalledWith(409);
  expect(res.json).toHaveBeenCalledWith({ error: "Email already in use" });
});
```

A plain function with mocked `req`/`res` objects — no Express server, no database — verifying the centralized error mapping from `08-errors/05-turning-errors-into-friendly-responses.md` behaves correctly for each error type.

## Common mistakes

- **Mocking Mongoose's model methods when a repository layer already exists** — mock the repository instead, keeping tests decoupled from Mongoose's specific API surface.
- **Duplicating full test-data object literals across many test files** instead of a shared fixture/factory function — becomes a maintenance burden the moment the schema changes.
- **Using a non-unique, hardcoded value (like a fixed email) across multiple tests** that share a database — causes duplicate-key collisions between unrelated tests; generate unique values per fixture call (as `buildUserData` does with `Date.now()`).
- **Only writing mocked unit tests, never integration tests** — misses genuine bugs in validation, indexes, or middleware that only a real database can surface.
- **Only writing integration tests, never mocked unit tests** — a slow test suite, and harder to pinpoint exactly which business-logic branch failed versus a database-level issue.

## Quick summary

- Mocking the repository layer lets you unit-test service-layer business logic (branching, error handling, call sequencing) in milliseconds, with no database at all
- Fixture/factory functions centralize what "valid test data" looks like, with overridable defaults — avoiding duplicated, schema-fragile object literals across test files
- Mocked unit tests and real (in-memory) integration tests are complementary, not substitutes for each other — use both for a genuinely healthy test suite

## Section complete

That covers testing a Mongoose-backed application — real-database integration testing and fast, mocked unit testing, plus reusable fixtures. **`17-production`** covers deploying and operating this application for real.
