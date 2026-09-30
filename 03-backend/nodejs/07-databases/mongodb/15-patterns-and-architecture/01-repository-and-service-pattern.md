# Repository & Service Pattern

Separating Mongoose-specific data access from framework-agnostic business logic — the same layering principle previewed in `08-errors/06-custom-application-errors.md`, covered here as a full architectural pattern.

## The problem this solves

```js
// a route handler doing everything at once
app.post("/users", async (req, res, next) => {
  try {
    const existing = await User.findOne({ email: req.body.email });
    if (existing) {
      return res.status(409).json({ error: "Email already in use" });
    }
    const hashedPassword = await bcrypt.hash(req.body.password, 10);
    const user = await User.create({ ...req.body, password: hashedPassword });
    await sendWelcomeEmail(user.email);
    res.status(201).json({ id: user.id, email: user.email });
  } catch (err) {
    next(err);
  }
});
```

This works, but mixes three different concerns in one place: HTTP handling (`req`/`res`), business rules (checking for an existing email, hashing a password, sending a welcome email), and raw Mongoose queries. As the app grows, this logic needs to be reused (a CLI import script, a background job, a test) without dragging along `req`/`res` — and testing it means either spinning up a real HTTP server or mocking Express entirely.

---

## The repository layer: isolating Mongoose specifics

```js
// repositories/userRepository.js
export const userRepository = {
  findByEmail(email) {
    return User.findOne({ email: email.toLowerCase() });
  },
  create(data) {
    return User.create(data);
  },
  findById(id) {
    return User.findById(id);
  },
};
```

A repository's job is narrow and specific: wrap Mongoose queries behind plain functions with names describing _what_ they do, not _how_. Nothing here knows about HTTP, password hashing, or emails — just data access.

---

## The service layer: business logic, no Mongoose or HTTP knowledge

```js
// services/userService.js
import { userRepository } from "../repositories/userRepository.js";
import { ConflictError } from "../errors/AppError.js";
import bcrypt from "bcrypt";
import { sendWelcomeEmail } from "./emailService.js";

export async function registerUser({ email, password, name }) {
  const existing = await userRepository.findByEmail(email);
  if (existing) {
    throw new ConflictError("Email already in use");
  }

  const hashedPassword = await bcrypt.hash(password, 10);
  const user = await userRepository.create({
    email,
    password: hashedPassword,
    name,
  });

  await sendWelcomeEmail(user.email);

  return user;
}
```

The service function expresses the actual business rule ("registering a user means: check for a duplicate, hash the password, create the record, send a welcome email") in plain language, calling the repository for data access rather than querying Mongoose directly.

---

## The controller/route layer: thin, HTTP-only

```js
// controllers/userController.js
import { registerUser } from "../services/userService.js";

export async function createUser(req, res, next) {
  try {
    const user = await registerUser(req.body);
    res.status(201).json({ id: user.id, email: user.email });
  } catch (err) {
    next(err);
  }
}
```

The route/controller layer only translates between HTTP and the service layer — it doesn't know or care that Mongoose exists underneath at all.

---

## Why this pays off

### 1. Reusable outside HTTP

```js
// a CLI import script — no Express, no req/res, same business logic
import { registerUser } from "../services/userService.js";

for (const record of importedRecords) {
  try {
    await registerUser(record);
  } catch (err) {
    console.log(`Skipped ${record.email}: ${err.message}`);
  }
}
```

`registerUser` works identically here, since it never depended on Express in the first place.

### 2. Testable without a real database or HTTP server

```js
// mocking the repository, testing the service in isolation
import { registerUser } from "../services/userService.js";

jest.mock("../repositories/userRepository.js", () => ({
  userRepository: {
    findByEmail: jest.fn().mockResolvedValue(null),
    create: jest.fn().mockResolvedValue({ id: "1", email: "test@example.com" }),
  },
}));
```

Full depth on this in `16-testing/02-mocking-and-fixtures.md` — the repository boundary is exactly what makes mocking data access clean, rather than needing to mock Mongoose's internals directly.

### 3. Mongoose-specific errors stay contained

```js
// only the repository/service boundary needs to know about Mongoose's error shapes
export async function registerUser({ email, password }) {
  try {
    return await userRepository.create({ email, password });
  } catch (err) {
    if (err.code === 11000) {
      throw new ConflictError("Email already in use");
    }
    throw err;
  }
}
```

Exactly the pattern from `08-errors/06-custom-application-errors.md` — the translation from Mongoose's raw errors to your own `AppError` classes happens once, at this boundary.

---

## How much of this structure does a given project actually need?

```
Small project (a handful of routes, simple CRUD):
  Controller → Model directly
  (repository/service layers would be premature structure)

Growing project (business rules, reused logic, real test coverage):
  Controller → Service → Repository → Model
```

This mirrors the guidance from the Node.js documentation earlier in this set (`10-architecture` in the broader Node guide): don't add layers a project doesn't need yet. A five-route CRUD API calling `Model.find()` directly from route handlers is completely reasonable — introduce a repository/service split once you notice logic being duplicated across routes, needing reuse outside HTTP, or becoming hard to test without a real database.

---

## A repository per model, a service per business capability

```
repositories/
├── userRepository.js
├── postRepository.js

services/
├── userService.js       (registerUser, authenticateUser, updateProfile, ...)
├── postService.js         (publishPost, archivePost, ...)
```

Repositories tend to map one-to-one with models; services tend to map to **business capabilities**, which may span multiple repositories (a `createOrder` service function might use both `orderRepository` and `productRepository` together, possibly within a transaction — `13-transactions/`).

## Common mistakes

- **Adding this structure to a genuinely tiny project** — three layers for five routes is more ceremony than value; start simple and introduce structure as real need appears.
- **Letting Mongoose-specific details (raw errors, query objects, `ObjectId`s) leak past the repository layer** into services or controllers — defeats the purpose of the boundary.
- **Putting business logic (password hashing, validation beyond what the schema handles, sending emails) inside the repository layer** — repositories should stay narrow: data access only. Business rules belong in services.
- **Making repository functions too generic/leaky** (e.g. exposing raw Mongoose query objects for the caller to further chain) — the point is a clean, purpose-named function per real use case (`findByEmail`, not a generic `find(filter)`).

## Quick summary

- Repository: thin wrapper around Mongoose queries, named by what they do — the only layer that knows Mongoose exists
- Service: business logic and rules, calling repositories, with no knowledge of HTTP or Mongoose specifics — reusable outside a web request entirely
- Controller/route: thin HTTP translation layer, calling services
- This structure earns its cost once logic needs reuse outside HTTP, needs isolated testing, or is duplicated across multiple routes — not before

## Next

**`02-soft-delete.md`** covers a full, production-ready implementation of one of the patterns referenced throughout this guide — soft delete, done properly.
