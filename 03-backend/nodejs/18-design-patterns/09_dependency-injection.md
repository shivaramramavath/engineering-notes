# Dependency Injection

**Dependency injection (DI)** means a piece of code **receives** the things it depends on (database, logger, clock, HTTP client) from the outside, instead of **creating or importing** them itself. The code declares what it needs; something else decides which concrete implementation to supply.

DI is not a framework. It is a way of arranging code so that components are loosely coupled, easy to test, and easy to reconfigure.

See also: [Factory Pattern](./02_factory-pattern.md), [Strategy Pattern](./05_strategy-pattern.md), [Singleton Pattern](./03_singleton-pattern.md), [Adapter Pattern](./07_adapter-pattern.md), [Testing](../21_testing/00_README.md), [Mocking](../21_testing/05_mocking.md).

## The problem: hard-wired dependencies

```js
import { db } from './db.js';
import { sendEmail } from './mailer.js';

export async function registerUser(email) {
  const existing = await db.query('SELECT 1 FROM users WHERE email = ?', [email]);
  if (existing.length) throw new Error('Already registered');

  await db.query('INSERT INTO users (email, created_at) VALUES (?, ?)', [email, new Date()]);
  await sendEmail(email, 'Welcome!');
}
```

Problems:

- Testing requires a real database and a real mail server, or fragile module mocking
- The function is tied to one `db` and one mailer
- Time is a hidden dependency (`new Date()`): results change between runs
- The function's true dependencies are invisible from its signature

## The fix: pass dependencies in

```js
export function createRegistration({ db, mailer, clock = () => new Date() }) {
  return async function registerUser(email) {
    const existing = await db.query('SELECT 1 FROM users WHERE email = ?', [email]);
    if (existing.length) throw new Error('Already registered');

    await db.query('INSERT INTO users (email, created_at) VALUES (?, ?)', [email, clock()]);
    await mailer.send(email, 'Welcome!');
  };
}

// Production wiring
const registerUser = createRegistration({ db: realDb, mailer: smtpMailer });

// Test wiring
const sent = [];
const registerUser = createRegistration({
  db: fakeDb,
  mailer: { send: async (to, text) => sent.push({ to, text }) },
  clock: () => new Date('2026-01-01T00:00:00Z'),
});
```

The function no longer knows or cares where its dependencies came from. Its needs are explicit in its signature, and tests can substitute fakes with no mocking library.

## Forms of injection

### 1. Function parameters

The simplest form: pass the dependency as an argument.

```js
function getUser(db, id) {
  return db.query('SELECT * FROM users WHERE id = ?', [id]);
}
```

Good for small utilities; becomes noisy when many functions need the same dependencies.

### 2. Closure factory (partial application)

Bind dependencies once and return the working functions:

```js
function createUserService({ db, logger }) {
  return {
    async get(id) {
      logger.info(`Fetching user ${id}`);
      return db.query('SELECT * FROM users WHERE id = ?', [id]);
    },
    async remove(id) {
      logger.info(`Deleting user ${id}`);
      return db.query('DELETE FROM users WHERE id = ?', [id]);
    },
  };
}

const userService = createUserService({ db, logger });
```

Idiomatic JavaScript: no classes, no `this`, dependencies stay private in the closure.

### 3. Constructor injection

```js
class UserService {
  #db;
  #logger;

  constructor({ db, logger }) {
    this.#db = db;
    this.#logger = logger;
  }

  async get(id) {
    this.#logger.info(`Fetching user ${id}`);
    return this.#db.query('SELECT * FROM users WHERE id = ?', [id]);
  }
}

const userService = new UserService({ db, logger });
```

The object is fully usable as soon as it is constructed, and dependencies cannot change afterward. This is generally the preferred class-based form. Taking one object (`{ db, logger }`) keeps call sites readable and order-independent.

### 4. Setter or property injection

```js
class Report {
  setLogger(logger) { this.logger = logger; }
}
```

Dependencies can be missing or swapped at any time, so the object may be half-configured. Use only for genuinely optional dependencies.

### 5. Passing dependencies per call (context object)

```js
async function handleRequest(req, ctx) {      // ctx = { db, logger, user, ... }
  const user = await ctx.db.getUser(req.userId);
  ctx.logger.info('loaded user', user.id);
}
```

Useful for request-scoped values (current user, request id, transaction). In Node, `AsyncLocalStorage` offers implicit request context without threading a parameter through every function; use it sparingly because it hides data flow.

## The composition root

Somewhere you must create the concrete objects and wire them together. Do it in **one place**, close to the application's entry point, and nowhere else:

```js
// main.js: the composition root
import { createDb } from './infra/db.js';
import { createSmtpMailer } from './infra/mailer.js';
import { createLogger } from './infra/logger.js';
import { createUserService } from './services/user.js';
import { createRegistration } from './services/registration.js';
import { createApp } from './http/app.js';

const config = loadConfig(process.env);

const logger = createLogger(config.log);
const db = await createDb(config.db, { logger });
const mailer = createSmtpMailer(config.smtp);

const userService = createUserService({ db, logger });
const registerUser = createRegistration({ db, mailer });

const app = createApp({ userService, registerUser, logger });
app.listen(config.port);
```

Everything else in the codebase receives its dependencies as parameters and never calls `new Database()` or imports a concrete singleton. This is the only place that knows which implementations are used.

## What to inject

Inject anything that is **slow, non-deterministic, external, or likely to change**:

| Dependency | Why inject it |
|------------|---------------|
| Database, cache, queue clients | Replace with in-memory fakes in tests |
| HTTP clients, third-party SDKs | Avoid real network calls |
| File system | Avoid touching real disks |
| **Clock** (`Date.now`, `new Date`) | Deterministic time-based tests |
| **Random / ID generators** (`Math.random`, `crypto.randomUUID`) | Reproducible results |
| Logger | Silent or capturing logger in tests |
| Configuration | Different values per environment |
| Timers (`setTimeout`) | Fake timers or controlled delays |

Do **not** inject everything: pure helpers (`Math.max`, a string formatter you own), language built-ins, and stable value objects can be imported directly.

```js
// Inject the clock and ID generation
function createSessionManager({ now = Date.now, id = () => crypto.randomUUID() } = {}) {
  const sessions = new Map();
  return {
    create(user) {
      const session = { id: id(), user, expiresAt: now() + 3600_000 };
      sessions.set(session.id, session);
      return session;
    },
    isValid(sessionId) {
      const s = sessions.get(sessionId);
      return Boolean(s) && s.expiresAt > now();
    },
  };
}

// Test: time travel without fake-timer libraries
let t = 1_000_000;
const sessions = createSessionManager({ now: () => t, id: () => 'abc' });
const s = sessions.create('ada');
t += 3600_001;
sessions.isValid(s.id);       // false
```

## Interfaces and contracts

JavaScript has no interfaces, so the contract between a component and its dependency is a convention: "something with a `send(to, text)` method". Make that contract explicit:

- Document it (JSDoc `@typedef`) or declare an `interface` in TypeScript
- Write a **contract test** that every implementation (real and fake) must pass
- Keep dependency interfaces **small** and shaped by what the consumer needs, not by what the vendor library offers (adapters bridge the gap)

```js
/**
 * @typedef {Object} Mailer
 * @property {(to: string, text: string) => Promise<void>} send
 */

/** @param {{ mailer: Mailer }} deps */
export function createNotifier({ mailer }) { /* ... */ }
```

## Testing with DI

```js
import test from 'node:test';
import assert from 'node:assert/strict';

test('registerUser sends a welcome email', async () => {
  const sent = [];
  const db = {
    rows: [],
    async query(sql, params) {
      if (sql.startsWith('SELECT')) return this.rows.filter((r) => r.email === params[0]);
      this.rows.push({ email: params[0] });
      return [];
    },
  };
  const mailer = { send: async (to, text) => sent.push({ to, text }) };

  const registerUser = createRegistration({ db, mailer, clock: () => new Date(0) });
  await registerUser('ada@example.com');

  assert.deepEqual(sent, [{ to: 'ada@example.com', text: 'Welcome!' }]);
});

test('registerUser rejects duplicates', async () => {
  const db = { query: async () => [{ email: 'ada@example.com' }] };
  const registerUser = createRegistration({ db, mailer: { send: async () => {} } });
  await assert.rejects(() => registerUser('ada@example.com'), /Already registered/);
});
```

Compare this with module mocking (`vi.mock`, `jest.mock`, `proxyquire`): those work by patching the module system and depend on how things are imported. DI uses plain function arguments, so tests are simpler and less brittle.

## DI containers

For large applications, manually wiring dozens of objects becomes tedious. A **container** automates it: you register how to build each dependency, and the container resolves the graph.

```js
class Container {
  #factories = new Map();
  #instances = new Map();

  register(name, factory, { singleton = true } = {}) {
    this.#factories.set(name, { factory, singleton });
    return this;
  }

  resolve(name) {
    if (this.#instances.has(name)) return this.#instances.get(name);

    const entry = this.#factories.get(name);
    if (!entry) throw new Error(`Nothing registered for "${name}"`);

    const instance = entry.factory(this);                  // factory receives the container
    if (entry.singleton) this.#instances.set(name, instance);
    return instance;
  }
}

const container = new Container()
  .register('config', () => loadConfig(process.env))
  .register('logger', (c) => createLogger(c.resolve('config').log))
  .register('db', (c) => createDb(c.resolve('config').db))
  .register('userService', (c) => createUserService({
    db: c.resolve('db'),
    logger: c.resolve('logger'),
  }));

const userService = container.resolve('userService');
```

Real-world containers: **InversifyJS**, **tsyringe**, **Awilix**, and the built-in providers of **NestJS** and **Angular**. They add automatic resolution by type or name, lifetimes (singleton, per-request, transient), and decorators.

Things to watch:

- A container is a **service locator** if components call `container.resolve(...)` themselves (a hidden dependency again). Keep resolution at the composition root and pass plain dependencies to components
- Detect **circular dependencies** (A needs B needs A): redesign, or inject lazily
- Async initialization (connecting to a database) needs async factories or an explicit startup phase

## Service locator vs dependency injection

| | Dependency injection | Service locator |
|---|----------------------|-----------------|
| How dependencies arrive | Passed in (constructor/parameters) | The component asks a global registry |
| Dependencies visible in the signature | Yes | No |
| Testing | Pass fakes | Must configure the global registry |
| Coupling | To an interface | To the locator |

Prefer injection. A locator is acceptable at the very edge (framework glue), not inside business logic.

## Lifetimes

| Lifetime | Meaning | Example |
|----------|---------|---------|
| **Singleton** | One instance for the whole application | Database pool, config, logger |
| **Scoped** (per request) | One instance per request or unit of work | Transaction, current user context |
| **Transient** | A new instance every time | Stateless helpers, short-lived objects |

Never inject a **scoped** or request-specific object into a **singleton** (the singleton would keep using the first request's data forever).

## DI and other patterns

| Pattern | How DI helps |
|---------|--------------|
| **Strategy** | The strategy is injected rather than chosen inside |
| **Factory** | Factories receive dependencies and create objects that use them |
| **Adapter** | Adapters implement the interface a component asks for |
| **Decorator** | Wrap a dependency (logging, caching) before injecting it |
| **Singleton** | Create one instance at the composition root and pass it, instead of a global |
| **Observer** | Inject the event bus rather than importing a global one |

```js
// Decorate, then inject
const cachedDb = withCache(realDb, { ttlMs: 30_000 });
const userService = createUserService({ db: cachedDb, logger });
```

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| Creating dependencies inside the component (`new Database()`) | Not testable or reconfigurable | Receive them from outside |
| Hard-importing a concrete singleton in business logic | Hidden coupling; shared state in tests | Inject it; wire at the composition root |
| Service locator inside components | Hidden dependencies | Pass dependencies explicitly |
| Constructors with 8+ dependencies | The class does too much | Split responsibilities; group related deps |
| Injecting everything, including trivial pure helpers | Ceremony with no benefit | Inject only external, slow, non-deterministic, or variable things |
| Fakes that differ from real behavior | Tests pass; production fails | Contract tests that run against both |
| Over-mocking (asserting every call) | Tests mirror the implementation and break on refactors | Prefer in-memory fakes; assert outcomes |
| Circular dependencies | Cannot construct the graph | Redesign, extract a third component, or inject lazily |
| Injecting a request-scoped object into a singleton | Data leaks across requests | Pass per call, or create per scope |
| Two composition roots (wiring scattered across files) | Hard to see what is used | One root, near the entry point |
| Framework or container everywhere in a small app | Complexity out of proportion | Plain function parameters and factories |
| Injecting a raw third-party client with a huge API | Components depend on vendor details | Inject a small adapter you define |

## Key takeaways

- DI means components receive their dependencies instead of creating or importing concrete ones
- In JavaScript the simplest forms are function parameters, closure factories, and constructor injection with an object argument
- Wire everything in one **composition root** near the entry point; elsewhere, just use what you are given
- Inject external, slow, non-deterministic, or changeable things: databases, network, file system, clock, randomness, logging, config
- Tests become simple: pass fakes, a fixed clock, and capture outputs, with no module-mocking tricks
- Keep dependency interfaces small, documented, and verified with contract tests
- Containers help in large apps, but avoid turning them into a service locator
- Do not over-engineer: small programs often need nothing more than passing arguments

**Next:** [Testing](../21_testing/00_README.md)
