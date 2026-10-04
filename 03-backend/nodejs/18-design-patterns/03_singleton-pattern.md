# Singleton Pattern

A **singleton** guarantees that there is **exactly one instance** of something and provides a way to get it. Typical examples: a configuration object, a logger, a database connection pool, a cache.

In many languages this takes a private constructor and a static accessor. In JavaScript, **modules are already singletons**, so the pattern is mostly free, and also easy to misuse.

See also: [Module Pattern](./01_module-pattern.md), [ES Modules](../13_modules/01_es-modules.md), [Dependency Injection](./09_dependency-injection.md).

## The simplest singleton: a module

An ES module is evaluated **once**, and every importer receives the same exports. Export an instance and you have a singleton:

```js
// config.js
export const config = {
  env: process.env.NODE_ENV ?? 'development',
  port: Number(process.env.PORT ?? 3000),
};
```

```js
// a.js
import { config } from './config.js';
// b.js
import { config } from './config.js';
// both receive the very same object
```

The same applies to CommonJS: `require` caches the module's `exports` after the first load.

```js
// logger.js
class Logger {
  #lines = [];
  log(msg) { this.#lines.push(msg); console.log(msg); }
  get history() { return [...this.#lines]; }
}

export const logger = new Logger();      // created once, on first import
```

For most needs this is the best singleton: no ceremony, easy to read.

## Classic class-based singleton

If you want the class itself to enforce uniqueness:

```js
class Settings {
  static #instance = null;
  #values = new Map();

  constructor() {
    if (Settings.#instance) {
      throw new Error('Use Settings.getInstance()');
    }
  }

  static getInstance() {
    Settings.#instance ??= new Settings();
    return Settings.#instance;
  }

  get(key) { return this.#values.get(key); }
  set(key, value) { this.#values.set(key, value); }
}

const a = Settings.getInstance();
const b = Settings.getInstance();
console.log(a === b);        // true
```

A variant returns the existing instance from the constructor, so `new Settings()` always gives the same object:

```js
class Registry {
  static #instance;

  constructor() {
    if (Registry.#instance) return Registry.#instance;
    this.items = new Map();
    Registry.#instance = this;
  }
}

new Registry() === new Registry();     // true
```

This surprises readers (`new` normally means "a new object"), so prefer an explicit `getInstance()` or a module export.

## Closure-based singleton

```js
const getCache = (() => {
  let instance;

  return () => {
    instance ??= new Map();
    return instance;
  };
})();

getCache() === getCache();     // true
```

## Lazy vs eager initialization

| | Eager | Lazy |
|---|-------|------|
| Created | At module load | On first use |
| Code | `export const db = createDb();` | `getDb()` creates on first call |
| Pros | Simple; failures appear at startup | Saves startup time and resources if never used |
| Cons | Pays the cost even if unused; side effects at import time | Failures appear later, at first use |

Lazy initialization of an **async** resource must handle concurrent first calls. Cache the **promise**, not the result:

```js
let dbPromise;

export function getDb() {
  dbPromise ??= connect(process.env.DATABASE_URL).catch((err) => {
    dbPromise = undefined;               // allow retry after a failure
    throw err;
  });
  return dbPromise;
}

const db = await getDb();                // many callers share one connection attempt
```

Caching the result instead would let two simultaneous callers each start a connection.

## Singletons in practice

| Use case | Why one instance fits |
|----------|----------------------|
| Application configuration | One source of truth |
| Logger | One sink, consistent format |
| Database or HTTP connection pool | Pools exist to be shared |
| In-memory cache | A single store |
| Event bus | One place for app-wide events |
| Feature flags client | One connection to the flag service |
| Browser APIs: `window`, `document`, `localStorage` | The platform provides exactly one |

## Gotchas specific to JavaScript

### 1. Not global across everything

A module is a singleton **per module instance**, which is not always "per process":

| Situation | Result |
|-----------|--------|
| Same URL imported twice with different query strings (`./a.js?v=1`) | Two instances |
| ESM and CJS copies of the same package both loaded (dual package hazard) | Two instances |
| Multiple versions of a package in `node_modules` | One per version |
| Worker threads, child processes, cluster workers | Each has its own copy |
| Several browser tabs or iframes | Each has its own copy |
| Serverless: separate invocations or containers | Separate copies, sometimes reused |
| Hot module reload in dev | Module re-evaluated; state may reset |

If you need one instance across processes, use an external store (Redis, a database) instead.

### 2. Shared mutable state

A singleton is global state with a nicer name. Changes by one caller affect everyone:

```js
import { config } from './config.js';
config.port = 9999;                      // every importer now sees 9999
```

Mitigations: freeze it, expose read-only getters, or keep mutation behind explicit methods.

```js
export const config = Object.freeze({ env: 'production', port: 3000 });
```

### 3. Hidden dependencies and testing

Code that imports a singleton directly is coupled to it:

```js
import { db } from './db.js';

export async function getUser(id) {
  return db.query('SELECT * FROM users WHERE id = ?', [id]);   // hard to replace in tests
}
```

Tests share the same instance and state, so one test can affect another, and you cannot easily substitute a fake. Prefer **dependency injection** and use the singleton only at the application's edge (composition root):

```js
export function createUserService(db) {
  return {
    getUser: (id) => db.query('SELECT * FROM users WHERE id = ?', [id]),
  };
}

// production wiring (once, at startup)
import { db } from './db.js';
export const userService = createUserService(db);

// test
const service = createUserService(fakeDb);
```

See [Dependency Injection](./09_dependency-injection.md).

### 4. Import-time side effects

```js
// connects as soon as anything imports this file
export const db = await connect(process.env.DATABASE_URL);
```

Importing the module now has side effects: tests that merely import it hit the network, and failures crash at import. Prefer a factory plus an explicit `init()`, or a lazy getter.

## Singleton vs just using one instance

You can have **one instance in practice** without a singleton mechanism: create it once at startup and pass it around. That is often better:

```js
// main.js (composition root)
const logger = createLogger();
const db = await createDb({ logger });
const app = createApp({ db, logger });
```

There is exactly one logger and one database, but nothing global enforces it, so tests can create more. The difference is **enforcement versus convention**: a true singleton prevents a second instance; passing one instance around merely does not create another.

## Resetting for tests

If you must keep a singleton, give yourself a way out:

```js
let instance;

export function getCache() {
  instance ??= createCache();
  return instance;
}

export function __resetCacheForTests() {
  instance = undefined;
}
```

Or have test runners isolate modules (`vi.resetModules()` in Vitest, `jest.resetModules()` in Jest).

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| Singleton as a convenient global variable | Hidden coupling; unpredictable state | Pass dependencies explicitly |
| Mutable shared state | Action at a distance, race conditions in async code | Freeze, or mutate only through methods |
| Hard-importing singletons everywhere | Impossible to substitute in tests | Dependency injection |
| Assuming one instance across workers, tabs, or processes | Each has its own copy | External shared store |
| Side effects (connect, read files) at import time | Slow startup, test pain, unhandled failures at load | Lazy getter or explicit `init()` |
| Caching the resolved value of an async init | Concurrent first calls create duplicates | Cache the promise |
| Not clearing a cached failed promise | Permanent failure after one error | Reset on rejection |
| Dual package (ESM and CJS) loading twice | Two "singletons" | One entry format, or a shared global key |
| Enforcing uniqueness with `throw` in the constructor | Awkward API | Module export or `getInstance()` |

## Key takeaways

- A singleton guarantees a single shared instance; in JavaScript an exported instance from a **module** already does this
- Use a module export first; use `getInstance()` or closures only if you need lazy creation or class-level enforcement
- For async resources, cache the **promise** and reset it on failure
- "Singleton" means one per module instance, not one per universe: workers, tabs, and duplicate packages each get their own
- Singletons are global state: they couple code and complicate testing
- Prefer creating one instance at startup and injecting it; reserve true singletons for things that must be unique

**Next:** [Builder Pattern](./04_builder-pattern.md)
