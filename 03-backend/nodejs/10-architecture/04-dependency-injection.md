# Dependency Injection

Giving a piece of code the things it needs from the outside, instead of letting it reach out and grab them itself.

## The idea in one example

```js
// ❌ Hard-wired dependency: the service creates/imports its own collaborator
import { userRepository } from "./users.repository.js";      // concrete, fixed
import { sendgrid } from "../lib/sendgrid.js";                // concrete, fixed

export async function register(data) {
  const user = await userRepository.create(data);
  await sendgrid.send({ to: user.email, ... });
  return user;
}
```

```js
// ✅ Injected dependencies: the service is GIVEN what it needs
export function makeAuthService({ userRepository, mailer }) {
  return {
    async register(data) {
      const user = await userRepository.create(data);
      await mailer.sendWelcome(user.email);
      return user;
    },
  };
}
```

The second version doesn't know or care whether `userRepository` talks to PostgreSQL, MongoDB, or an array in a test, or whether `mailer` is SendGrid, SES, or a stub that records calls.

**Dependency Injection (DI)** is just this: *pass dependencies in; don't hard-code them.* **Inversion of Control (IoC)** is the broader principle: the code that *uses* a dependency no longer controls *which* one it gets. It's decided by whoever assembles the application.

> DI ≠ DI container. The pattern is about passing arguments. A container (or framework) is an optional tool that automates the passing. You can practice DI with zero libraries.

---

## Why it matters

| Benefit | How DI delivers it |
|---|---|
| **Testability** | Pass fakes/mocks instead of real databases and email providers |
| **Swappability** | Change PostgreSQL → MongoDB or SendGrid → SES by changing one line of wiring |
| **Explicit dependencies** | A function's parameters tell you everything it needs; nothing hidden behind imports |
| **Looser coupling** | Business logic depends on a *contract*, not a concrete library (the dependency inversion principle in `02-clean-architecture.md`) |
| **Configuration in one place** | Environment-specific choices (real vs fake, staging vs prod) live in a single file |

### The "D" in SOLID

The **Dependency Inversion Principle:** high-level modules (business logic) shouldn't depend on low-level modules (databases, SDKs); both should depend on abstractions. DI is the mechanism that puts that principle into practice.

---

## The problem with plain imports

In Node.js, `import` hard-wires a dependency at the module level:

```js
import { pool } from "../config/db.js";
```

It's simple, and for small apps that's fine. But it means:

- To test a service, you must **mock the module** (`jest.unstable_mockModule`, `mock-require`, `esmock`), which is awkward with ES modules, and ties tests to file paths.
- Dependencies are **invisible**: you have to read the whole file to learn what it uses.
- Singletons created at import time (DB connections, SDK clients) start connecting as a side effect of merely *importing* the file, which surprises tests.
- Swapping implementations means editing the file that uses it.

DI trades a little ceremony for control.

---

## Three ways to inject

### 1. Function parameters (factory functions): the simplest, idiomatic choice

```js
// users.service.js
export function makeUserService({ userRepository, passwordHasher, logger }) {
  async function register({ email, name, password }) {
    if (await userRepository.findByEmail(email)) {
      throw new AppError(409, "email_taken", "That email is already registered");
    }
    const passwordHash = await passwordHasher.hash(password);
    const user = await userRepository.create({ email, name, passwordHash });
    logger.info({ userId: user.id }, "User registered");
    return user;
  }

  async function getById(id) {
    const user = await userRepository.findById(id);
    if (!user) throw new AppError(404, "not_found", "User not found");
    return user;
  }

  return { register, getById };        // the public API of the service
}
```

The factory **closes over** its dependencies (see `03-javascript-for-node/03-closures.md`), so there's no `this` to manage and they're effectively private. This is the most natural style in JavaScript.

### 2. Constructor injection (classes)

```js
export class UserService {
  #userRepository;
  #passwordHasher;
  #logger;

  constructor({ userRepository, passwordHasher, logger }) {
    this.#userRepository = userRepository;
    this.#passwordHasher = passwordHasher;
    this.#logger = logger;
  }

  async register({ email, name, password }) {
    // uses this.#userRepository, etc.
  }
}

const service = new UserService({ userRepository, passwordHasher, logger });
```

Constructor injection is the conventional form in class-based code (NestJS, Angular, Java). Prefer passing **one object of named dependencies** over positional arguments: it reads well at the call site, and order can't be mixed up.

### 3. Parameter / setter / ambient injection (use sparingly)

```js
// Passing a dependency into one function call: fine for one-off needs like a transaction handle
await orderRepository.insert(order, tx);

// Setter injection (service.setLogger(...)): rarely a good idea; leaves objects half-built
// Ambient/global access (globalThis.logger, service locators): hides dependencies again
```

Use constructor/factory injection for things a component needs for its whole life. Use parameter passing for per-call data (a transaction, a request context).

---

## The composition root

If every component *receives* its dependencies, **something** must create and connect them. That place is the **composition root**: one module, near the entry point, that knows about every concrete implementation.

```js
// src/container.js (the composition root)
import { pool } from "./config/db.js";
import { redis } from "./config/redis.js";
import { logger } from "./config/logger.js";
import { env } from "./config/env.js";

import { makeUserRepository } from "./modules/users/users.repository.js";
import { makeOrderRepository } from "./modules/orders/orders.repository.js";
import { makeProductRepository } from "./modules/products/products.repository.js";

import { makePasswordHasher } from "./shared/passwordHasher.js";
import { makeSendGridMailer } from "./shared/mailers/sendgrid.js";
import { makeConsoleMailer } from "./shared/mailers/console.js";

import { makeUserService } from "./modules/users/users.service.js";
import { makeOrderService } from "./modules/orders/orders.service.js";

import { makeUsersController } from "./modules/users/users.controller.js";
import { makeOrdersController } from "./modules/orders/orders.controller.js";

export function buildContainer(overrides = {}) {
  // --- infrastructure (leaf nodes: no dependencies on our own code) ---
  const db = overrides.db ?? pool;

  // --- adapters ---
  const userRepository = overrides.userRepository ?? makeUserRepository({ db });
  const orderRepository = overrides.orderRepository ?? makeOrderRepository({ db });
  const productRepository = overrides.productRepository ?? makeProductRepository({ db });

  const mailer =
    overrides.mailer ??
    (env.NODE_ENV === "production"
      ? makeSendGridMailer({ apiKey: env.SENDGRID_API_KEY })
      : makeConsoleMailer({ logger }));

  const passwordHasher = overrides.passwordHasher ?? makePasswordHasher({ cost: 12 });

  // --- application services ---
  const userService = makeUserService({ userRepository, passwordHasher, mailer, logger });
  const orderService = makeOrderService({ orderRepository, productRepository, userRepository, mailer, logger, db });

  // --- delivery (HTTP) ---
  return {
    usersController: makeUsersController({ userService }),
    ordersController: makeOrdersController({ orderService }),
  };
}
```

```js
// src/app.js
import express from "express";
import { buildContainer } from "./container.js";
import { buildUsersRouter } from "./modules/users/users.routes.js";
import { buildOrdersRouter } from "./modules/orders/orders.routes.js";

export function buildApp(overrides) {
  const { usersController, ordersController } = buildContainer(overrides);

  const app = express();
  app.use(express.json());
  app.use("/api/v1/users", buildUsersRouter(usersController));
  app.use("/api/v1/orders", buildOrdersRouter(ordersController));
  // error handler last
  return app;
}
```

```js
// src/server.js: production entry point
import { buildApp } from "./app.js";
buildApp().listen(process.env.PORT ?? 3000);
```

```js
// routes receive their controller: no hidden imports
export function buildOrdersRouter(controller) {
  const router = Router();
  router.post("/", validate({ body: createOrderSchema }), controller.create);
  return router;
}
```

Rules of thumb for the composition root:

- **Only here** does code call `new ConcreteThing()` / `makeConcreteThing()` for application components.
- Build **leaf dependencies first** (database, clients), then the things that use them, working outward.
- Nothing outside it should `import` the container (that turns it into a service locator).
- Keep it boring: no logic, just construction and wiring.

---

## Testing: the payoff

### Unit testing a service with fakes

```js
import { makeUserService } from "../src/modules/users/users.service.js";

function setup({ existingUsers = [] } = {}) {
  const users = [...existingUsers];
  const sentEmails = [];

  const service = makeUserService({
    userRepository: {
      async findByEmail(email) { return users.find((u) => u.email === email) ?? null; },
      async create(data) { const u = { id: String(users.length + 1), ...data }; users.push(u); return u; },
    },
    passwordHasher: { hash: async (p) => `hashed:${p}` },           // instant, deterministic
    mailer: { sendWelcome: async (to) => sentEmails.push(to) },
    logger: { info() {}, error() {} },
  });

  return { service, users, sentEmails };
}

test("registers a new user with a hashed password", async () => {
  const { service, users } = setup();
  const user = await service.register({ email: "a@b.com", name: "A", password: "secret123" });

  expect(user.passwordHash).toBe("hashed:secret123");
  expect(users).toHaveLength(1);
});

test("rejects a duplicate email", async () => {
  const { service } = setup({ existingUsers: [{ id: "1", email: "a@b.com" }] });
  await expect(service.register({ email: "a@b.com", name: "A", password: "x" }))
    .rejects.toMatchObject({ code: "email_taken" });
});
```

No module mocking, no database, no network, no timers. Each test runs in milliseconds.

### Integration testing the whole app with overrides

```js
import request from "supertest";
import { buildApp } from "../src/app.js";

test("POST /orders returns 201 and emails the customer", async () => {
  const sent = [];
  const app = buildApp({
    mailer: { sendOrderConfirmation: async (to) => sent.push(to) },    // fake only the email provider
    // database stays real (a test DB), so SQL is exercised for real
  });

  const res = await request(app)
    .post("/api/v1/orders")
    .set("Authorization", `Bearer ${testToken}`)
    .send({ items: [{ productId: "p1", quantity: 1 }] });

  expect(res.status).toBe(201);
  expect(sent).toHaveLength(1);
});
```

Because `buildApp(overrides)` lets a test replace just the pieces it cares about, this scales from "everything real" to "everything fake". See `13-testing/02-api-testing-and-mocking.md`.

---

## Injecting the "boring" dependencies too

Time, randomness, and IDs are dependencies as well, and they make tests flaky when hard-coded.

```js
export function makeTokenService({ clock = () => Date.now(), randomBytes = crypto.randomBytes }) {
  return {
    issue(userId) {
      return { userId, expiresAt: clock() + 15 * 60_000, jti: randomBytes(16).toString("hex") };
    },
  };
}

// test: freeze time → deterministic expiry
const tokens = makeTokenService({ clock: () => 1_000_000, randomBytes: () => Buffer.alloc(16) });
expect(tokens.issue("u1").expiresAt).toBe(1_900_000);
```

Good candidates: `clock`, `idGenerator` (UUIDs), `randomBytes`, `logger`, `config`, HTTP clients, `fetch`.

### Configuration

Validate environment once at startup, then inject a typed `config` object rather than reading `process.env` all over the code:

```js
// config/env.js
import { z } from "zod";

export const env = z.object({
  NODE_ENV: z.enum(["development", "test", "production"]),
  DATABASE_URL: z.string().url(),
  SENDGRID_API_KEY: z.string().optional(),
}).parse(process.env);

// services get what they need via parameters: makeMailer({ apiKey: env.SENDGRID_API_KEY })
```

See `16-production/01-environment-management.md`.

---

## DI containers and frameworks

For small and mid-sized apps, hand-wiring in a composition root is clear and sufficient. As the graph grows (dozens of services), wiring becomes tedious, and containers can automate it.

| Tool | Style | Notes |
|---|---|---|
| **Manual wiring** | Factories + composition root | No dependencies, fully explicit. Start here. |
| **awilix** | Container with auto-registration and lifetimes | Popular Express-friendly choice; supports `SINGLETON`, `SCOPED`, `TRANSIENT` |
| **tsyringe**, **InversifyJS** | Decorator-based (TypeScript) | Needs `reflect-metadata` |
| **NestJS** | Framework with a built-in DI container and modules | DI and modular structure are first-class |

### A taste of awilix

```bash
npm install awilix
```

```js
import { createContainer, asFunction, asValue, Lifetime } from "awilix";

const container = createContainer({ injectionMode: "PROXY" });   // dependencies injected as one destructured object

container.register({
  db: asValue(pool),
  logger: asValue(logger),
  userRepository: asFunction(makeUserRepository, { lifetime: Lifetime.SINGLETON }),
  userService: asFunction(makeUserService, { lifetime: Lifetime.SINGLETON }),
});

// awilix reads the destructured parameter NAMES ({ userRepository, logger }) and supplies matching registrations
const userService = container.resolve("userService");
```

### Lifetimes: which objects are shared?

| Lifetime | Meaning | Typical use |
|---|---|---|
| **Singleton** | One instance for the whole app | DB pool, Redis client, stateless services, config |
| **Scoped** | One instance per request (or unit of work) | Request context, current user, a per-request transaction |
| **Transient** | A new instance every time | Stateful helpers, rarely needed |

A classic bug: a **singleton that depends on request-scoped data** (for example, caching `req.user` in a singleton service). One user's data leaks into another's request. Pass request-specific data as **arguments to method calls**, or use a scoped container created per request (`app.use((req, res, next) => { req.scope = container.createScope(); next(); })`).

For async request context without threading arguments everywhere, Node offers `AsyncLocalStorage` (`14-logging-observability/02-correlation-id.md`).

### Should you use a container?

- **Manual wiring:** preferred for clarity until it genuinely hurts (20+ services, repeated boilerplate).
- **Container:** adds magic (resolution by name, lifetimes) and failure modes (`Cannot resolve 'xyz'` at runtime). Worth it in large codebases; overkill in small ones.
- **NestJS** gives you DI whether or not you'd have chosen it, part of its appeal and its opinionated weight.

---

## Common mistakes

```js
// ❌ Service locator: resolving dependencies from a global container INSIDE business code
import { container } from "../container.js";
export async function placeOrder() {
  const repo = container.resolve("orderRepository");   // hidden dependency again: DI in name only
}

// ❌ Injecting the whole container into everything ("just give me everything")
makeOrderService(container);          // can't tell what it really needs; tests must build it all

// ❌ Constructor over-injection: 10+ dependencies → the class does too much; split it
// ❌ Creating dependencies inside the "injected" function anyway
export function makeService({ userRepository }) {
  const mailer = new SendGridMailer(process.env.KEY);   // not injected → untestable
}

// ❌ Interfaces/abstractions for every tiny thing "for DI's sake"
// ❌ Singleton holding per-request state (cross-user data leaks)
// ❌ Two composition roots that wire the same things differently (production ≠ tests in surprising ways)
// ❌ Side effects at import time (connecting to the DB when a file is merely imported)
```

```js
// ✅ Keep the dependency list honest: if it's long, the unit is too big
makeOrderService({ orderRepository, productRepository, userRepository, mailer, logger, db });
//                └─ 6 is already a lot; consider splitting placeOrder / cancelOrder / refundOrder
```

---

## TypeScript note

DI and TypeScript fit together well: declare each dependency as an `interface`, and the compiler verifies every implementation and fake matches the contract.

```ts
interface UserRepository {
  findByEmail(email: string): Promise<User | null>;
  create(data: NewUser): Promise<User>;
}

export function makeUserService(deps: { userRepository: UserRepository; logger: Logger }) { /* ... */ }
```

Full treatment in `17-typescript/02-interfaces-and-generics.md`.

## Checklist

- [ ] Components receive dependencies via parameters or constructors, not hard-coded imports
- [ ] One composition root builds everything; no other code imports it
- [ ] `buildApp(overrides)` (or similar) lets tests replace individual pieces
- [ ] Time, randomness, IDs, config, and loggers are injectable
- [ ] No singleton holds request-specific state
- [ ] No service locator calls inside business logic
- [ ] Dependency lists stay short; long ones trigger a split
- [ ] No connections or side effects at import time

## Next

**`05-modular-monolith-vs-microservices.md`** zooms out from classes and functions to the whole system: how to split a growing application into modules, and when (and whether) to split it into separate services.
