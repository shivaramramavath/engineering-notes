# Singleton and Factory

Both are **creational patterns** — they deal with *how objects come into existence*. Singleton answers "how do I make sure there's only one?" Factory answers "how do I create the right kind of object without the caller needing to know the details?"

---

# Part 1 — Singleton

## The problem

Some things should exist exactly once per process:

- A database connection pool (opening a new pool per request would exhaust the database — `07-databases/`)
- A logger configured once and used everywhere (`14-logging-observability/01-pino-and-structured-logging.md`)
- A Redis client, a config object, an in-memory cache

Creating a second copy wastes resources at best and causes subtle bugs at worst (two caches that disagree, two pools doubling your connection count).

## Singleton via the module cache (the idiomatic Node way)

In Node, you usually don't need a "Singleton class" at all. **Modules are cached after first load**: the first `require`/`import` runs the file and stores the exports; every later import gets the *same object*.

```js
// lib/db.js
import { Pool } from "pg";

const pool = new Pool({
  connectionString: process.env.DATABASE_URL,
  max: 10,
});

export default pool;
```

```js
// services/userService.js
import pool from "../lib/db.js";   // same instance

// services/orderService.js
import pool from "../lib/db.js";   // same instance — not a new pool
```

That's a singleton. No class, no `getInstance()`, no ceremony — the module system provides the guarantee. For the vast majority of Node backends, this is the right answer.

```js
// lib/logger.js
import pino from "pino";

export const logger = pino({ level: process.env.LOG_LEVEL ?? "info" });
```

## The classic class-based version

The textbook form, for when you need lazy creation or want to see the mechanism:

```js
class Config {
  static #instance = null;

  constructor() {
    if (Config.#instance) {
      return Config.#instance;      // hand back the existing one
    }
    this.values = loadConfigFromEnv();
    Config.#instance = this;
  }

  static getInstance() {
    if (!Config.#instance) {
      Config.#instance = new Config();
    }
    return Config.#instance;
  }

  get(key) {
    return this.values[key];
  }
}

const a = Config.getInstance();
const b = Config.getInstance();
console.log(a === b); // true
```

- `static #instance` is a **private static field** — outside code can't overwrite it
- **Lazy initialization** — the instance is created on first use, not at startup. Useful when creation is expensive and might never be needed.

## Lazy singleton with async setup

Connections are usually asynchronous. Cache the **promise**, not the result — otherwise two simultaneous callers can both start connecting:

```js
// lib/redis.js
import { createClient } from "redis";

let clientPromise = null;

export function getRedis() {
  if (!clientPromise) {
    const client = createClient({ url: process.env.REDIS_URL });
    clientPromise = client.connect().then(() => client);
  }
  return clientPromise;
}
```

```js
const redis = await getRedis();
await redis.set("key", "value");
```

Every caller awaits the *same* promise, so exactly one connection is ever opened — even under concurrent first calls.

## Caveats you need to know

**1. "Once per process" is not "once per system".**
Run four Node processes (the `cluster` module, PM2, or four containers behind a load balancer — `02-core-modules/10-cluster-and-worker-threads.md`, `15-performance/04-load-balancing-and-testing.md`) and you have four singletons. An in-memory singleton cache or counter is *not shared* between them. Anything that must be globally unique or consistent belongs in an external store like Redis or the database.

**2. The module cache is keyed by resolved path.**
If two different copies of the same package end up in `node_modules` (version conflicts), each has its own cache — and its own "singleton". Rare, but baffling when it happens.

**3. Singletons are hidden global state — and hurt testing.**
A module that imports `pool` directly is hard to test without a real database, and tests can leak state into each other through a shared instance. This is the main reason to prefer passing dependencies in (`10-architecture/04-dependency-injection.md`):

```js
// Harder to test: reaches out to a global
import pool from "../lib/db.js";
export const findUser = (id) => pool.query("SELECT ...", [id]);

// Easier to test: dependency passed in
export const makeUserRepo = (db) => ({
  findUser: (id) => db.query("SELECT ...", [id]),
});

// In production wiring: makeUserRepo(pool)
// In tests:             makeUserRepo(fakeDb)
```

The singleton still exists — it's created *once at the app's entry point* and handed down, rather than imported everywhere.

## When *not* to use it

- When the thing is cheap and stateless — just create it where needed
- When you'd be tempted to use a singleton purely for convenience ("I don't want to pass it around") — that's global state wearing a pattern's clothes
- For per-request data (the current user, a request ID) — a singleton is shared across *all* concurrent requests; per-request state belongs on `req` or in `AsyncLocalStorage` (`14-logging-observability/02-correlation-id.md`)

---

# Part 2 — Factory

## The problem

Creating an object often involves decisions: *which* class, *which* configuration, *which* dependencies. If every caller makes those decisions, the logic gets duplicated and every caller is coupled to every concrete class.

```js
// Without a factory: callers know about every concrete class
let notifier;
if (type === "email") notifier = new EmailNotifier(smtpConfig);
else if (type === "sms") notifier = new SmsNotifier(twilioConfig);
else if (type === "push") notifier = new PushNotifier(fcmConfig);
else throw new Error("Unknown type");
```

Copy that into five places and adding a new channel means editing five places.

## Simple Factory — a function that returns the right thing

```js
// notifications/createNotifier.js
import { EmailNotifier } from "./EmailNotifier.js";
import { SmsNotifier } from "./SmsNotifier.js";
import { PushNotifier } from "./PushNotifier.js";

export function createNotifier(type, config) {
  switch (type) {
    case "email": return new EmailNotifier(config.smtp);
    case "sms":   return new SmsNotifier(config.twilio);
    case "push":  return new PushNotifier(config.fcm);
    default:
      throw new Error(`Unknown notifier type: ${type}`);
  }
}
```

```js
const notifier = createNotifier(user.preferredChannel, config);
await notifier.send(user, "Your order shipped");
```

The caller doesn't know — or care — which class it got. It only relies on a shared **contract**: every notifier has a `send(user, message)` method. (In JavaScript the contract is by convention; in TypeScript you'd formalize it with an `interface` — see `17-typescript/02-interfaces-and-generics.md`.)

A **factory function** is almost always enough in JavaScript. You rarely need a `NotifierFactory` *class*.

## Factory with a registry (no more switch statements)

A `switch` has to be edited for every new type. A registry lets new types plug themselves in:

```js
const registry = new Map();

export function registerNotifier(type, factoryFn) {
  registry.set(type, factoryFn);
}

export function createNotifier(type, config) {
  const factoryFn = registry.get(type);
  if (!factoryFn) throw new Error(`Unknown notifier type: ${type}`);
  return factoryFn(config);
}

// Each module registers itself
registerNotifier("email", (config) => new EmailNotifier(config.smtp));
registerNotifier("sms",   (config) => new SmsNotifier(config.twilio));
```

Adding Slack notifications now means writing one new file and one `registerNotifier` call — `createNotifier` itself never changes. (This idea — code that is *open to extension but closed to modification* — is the "O" in SOLID.)

## Factory functions that return configured objects

In JavaScript, "factory" often just means a function that builds and returns an object, using closures instead of classes:

```js
export function createHttpClient({ baseURL, timeoutMs = 5000, apiKey }) {
  return {
    async get(path) {
      const res = await fetch(`${baseURL}${path}`, {
        headers: { Authorization: `Bearer ${apiKey}` },
        signal: AbortSignal.timeout(timeoutMs),
      });
      if (!res.ok) throw new Error(`HTTP ${res.status}`);
      return res.json();
    },
  };
}

const stripe = createHttpClient({ baseURL: "https://api.stripe.com", apiKey: process.env.STRIPE_KEY });
const github = createHttpClient({ baseURL: "https://api.github.com", apiKey: process.env.GH_TOKEN });
```

Two independent clients from one definition, each with private configuration captured in a closure (`03-javascript-for-node/03-closures.md`).

## A realistic example: factory for environment-specific services

```js
// storage/createStorage.js
export function createStorage(env) {
  if (env.NODE_ENV === "test") return new InMemoryStorage();
  if (env.STORAGE_DRIVER === "s3") return new S3Storage(env.S3_BUCKET);
  return new LocalDiskStorage(env.UPLOAD_DIR);
}
```

The upload code (`06-express/07-file-upload.md`) just calls `storage.save(file)`. Local development writes to disk, production writes to S3, tests write to memory — and none of the calling code changes. This is exactly the kind of swap that makes factories pay off, and it pairs naturally with the Strategy pattern in the next file.

## Factory vs Singleton together

They combine well: a factory decides *what* to build, a singleton ensures it's built *once*:

```js
let storage;
export function getStorage() {
  storage ??= createStorage(process.env);
  return storage;
}
```

## When *not* to use it

- When there's only one concrete type and no realistic prospect of another — `new EmailNotifier(config)` is clearer than `createNotifier("email", config)`
- When the "factory" is just a constructor with extra steps
- When a plain object literal would do

## Common mistakes

- **Using a Singleton class when a module export would do** — Node's module cache already gives you this.
- **Assuming a singleton is shared across processes** — each cluster worker or container has its own.
- **Caching the connected client instead of the connecting promise** — concurrent first calls open duplicate connections.
- **Hiding global state in singletons everywhere** — makes testing painful; create at the entry point and pass dependencies in.
- **Singleton for per-request data** — it's shared by every concurrent request. Use `req` or `AsyncLocalStorage`.
- **Factories with giant `switch` statements that keep growing** — move to a registry.
- **Building a factory for one type** — premature abstraction.

## Quick summary

- **Singleton** — one shared instance per process. In Node, a module-level export *is* a singleton
- Cache the **promise** for async setup; remember "once per process" ≠ "once per system"
- Prefer creating shared instances once at startup and passing them in, for testability
- **Factory** — a function that decides which object to build so callers don't have to
- Use a registry to avoid ever-growing `switch` statements
- In JavaScript, a plain factory *function* is nearly always enough — no class needed

## Next

**`02-strategy-and-observer.md`** covers behavioral patterns: swapping algorithms at runtime (Strategy) and broadcasting changes to listeners (Observer).
