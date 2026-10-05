# Singleton Pattern

A singleton guarantees that **only one instance** of something exists and gives everyone access to it. Typical candidates: a configuration object, a logger, a database connection pool.

In JavaScript you rarely need to implement it by hand, because **a module is already a singleton**. The interesting part of this note is when that stops being true, and why singletons are often a design smell.

**Prerequisites:** [ES Modules](../13_modules/01_es-modules.md), [Classes](../05_this-and-oop/05_classes.md), [Private Fields](../05_this-and-oop/07_private-fields.md)

---

## The Idiomatic Way: Export an Instance

An ES module is evaluated once, and every importer receives the same bindings.

```js
// db.js
class Database {
  #connected = false;

  async connect(url) {
    if (this.#connected) return;
    // ...open the connection
    this.#connected = true;
  }

  query(sql, params) {
    if (!this.#connected) throw new Error("Call connect() first");
    // ...
  }
}

export const db = new Database();
```

```js
// a.js and b.js
import { db } from "./db.js"; // same object in both files
```

No static fields, no `getInstance()`. CommonJS works the same way through `require`'s module cache ([CommonJS](../13_modules/02_commonjs.md)).

---

## Class-Based Singleton (Lazy)

Use this when you want creation deferred until first use, or when the "one instance" rule should live in the class.

```js
class Config {
  static #instance;

  static get instance() {
    return (Config.#instance ??= new Config());
  }

  #values = new Map();

  get(key) { return this.#values.get(key); }
  set(key, value) { this.#values.set(key, value); }
}

Config.instance.set("env", "prod");
Config.instance === Config.instance; // true
```

JavaScript cannot stop someone from calling `new Config()` directly. The pattern is a convention, not a guarantee. If it matters, throw from the constructor when an instance already exists.

---

## When "One Instance" Is Not Actually One

This is where bugs come from.

| Situation | What happens |
| --- | --- |
| Two versions of the same package in `node_modules` | Each copy has its own module, so you get two singletons |
| Bundler duplicates a module into two chunks | Two instances in the browser |
| ESM import with different URLs (e.g. `./db.js?v=2`) | The module map is keyed by resolved URL, so you get a second instance |
| Multiple Node processes, cluster workers, serverless invocations | Each has its own memory. Nothing is shared |
| iframes, Web Workers | Each realm/thread has its own module instances |
| Test files in the same runner | Depends on the runner. Many isolate modules per file, others share, so state can leak between tests |

If the instance must be unique across processes, it's not a singleton problem anymore. It is shared state, and you need an external store (database, Redis, etc.).

---

## Why Singletons Are Often a Smell

A singleton is **global mutable state with a nicer name**.

- Code that imports it has a hidden dependency. You can't see it in function signatures.
- Tests share state and become order-dependent.
- You can't swap it for a fake without module-mocking tricks.

Compare:

```js
// hidden dependency
import { db } from "./db.js";
export const getUser = (id) => db.query("SELECT ...", [id]);

// explicit dependency
export const makeGetUser = (db) => (id) => db.query("SELECT ...", [id]);
```

The second version can still be fed the shared `db` in production, but tests pass in a fake. See [Dependency Injection](./09_dependency-injection.md): create the single instance at the app entry point and **pass it in**.

Good uses are things that are naturally unique and stateless or read-only from the caller's view: a frozen config, a logger, a connection pool owned by one place.

```js
export const config = Object.freeze({
  env: process.env.NODE_ENV ?? "development",
  port: Number(process.env.PORT ?? 3000),
});
```

---

## Common Mistakes

- **Implementing `getInstance()` in a module that is already a singleton.** Extra ceremony, no benefit.
- **Assuming one instance across processes or workers.**
- **Mutating a shared object from many places.** If it must change, funnel changes through methods. If it need not, freeze it.
- **Initializing at import time with side effects** (opening connections). It makes imports slow and tests hard. Prefer an explicit `connect()`/`init()` call.
- **Forgetting reset for tests.** If a singleton holds state, give it a reset method or avoid the singleton.

---

## Quick Summary

- In JS, `export const x = new X()` is the singleton. Modules are cached.
- Class-based `static #instance` is only worth it for lazy creation.
- "One per module cache" is not "one per app" once you have duplicate packages, workers, or multiple processes.
- Prefer creating one instance at the entry point and passing it in.

**Next:** [Builder Pattern](./04_builder-pattern.md)