# Module Patterns

Reusable ways to structure code with modules: from the pre-ESM **module pattern** to today's singletons, barrels, facades, lazy loading and strategies for avoiding circular dependencies.

## 1. The classic module pattern (IIFE + closure)

Before ES modules, an IIFE gave private scope and a public API.

```js
const Counter = (function () {
  let count = 0;                       // private
  function log() { console.log(count); }

  return {                              // public API
    inc() { count++; log(); },
    get value() { return count; },
  };
})();

Counter.inc();
```

Today the same idea is simply a file:

```js
// counter.js
let count = 0;                          // private to the module
const log = () => console.log(count);

export const inc = () => { count++; log(); };
export const getValue = () => count;
```

Unexported names are private.

## 2. Module as singleton

A module is evaluated once, so its top-level state is a shared instance.

```js
// db.js
import { createPool } from "some-driver";
export const pool = createPool(process.env.DATABASE_URL);   // created once for the whole app

// anywhere
import { pool } from "./db.js";
```

Simple and effective, but it is **global state**: harder to test and configure. Mitigate with factories (pattern 5) or dependency injection (pattern 6).

## 3. Named exports by default; one concept per module

```js
// user-service.js
export function createUser() {}
export function findUser() {}
export function deleteUser() {}
```

Guidelines:

- A module should have a **single responsibility**
- Export a small public surface; keep helpers unexported
- Name files after what they contain (`user-service.js`, `format-date.js`)
- Prefer **named** exports for consistency and refactoring; reserve `default` for single-purpose modules (components, entry points)

## 4. Barrel files (`index.js`)

A **barrel** re-exports a folder's public API.

```js
// components/index.js
export { Button } from "./Button.js";
export { Modal } from "./Modal.js";
export * from "./forms/index.js";

// usage
import { Button, Modal } from "./components/index.js";
```

| Pros | Cons |
|------|------|
| One import path, clear public API | Slower dev servers/test runs (loads the whole barrel) |
| Hides internal layout | Can defeat tree shaking if modules have side effects |
| | Encourages circular dependencies (siblings importing the barrel) |

Rules: use barrels at **package/feature boundaries**, not in every folder; modules **inside** a feature import siblings directly (`./Button.js`), never through their own barrel.

## 5. Factory modules (configurable, testable)

Export a function that builds the thing from its dependencies/config.

```js
// create-api.js
export function createApi({ baseUrl, fetchImpl = fetch, token }) {
  const headers = token ? { Authorization: `Bearer ${token}` } : {};
  return {
    async get(path) {
      const res = await fetchImpl(baseUrl + path, { headers });
      if (!res.ok) throw new Error(`HTTP ${res.status}`);
      return res.json();
    },
  };
}

// app wiring
export const api = createApi({ baseUrl: "https://api.example.com", token });

// test
const api = createApi({ baseUrl: "", fetchImpl: async () => ({ ok: true, json: async () => ({}) }) });
```

## 6. Dependency injection and a composition root

Modules receive collaborators instead of importing concrete implementations; **one place** wires everything.

```js
// services/order-service.js
export const createOrderService = ({ db, mailer, clock = () => new Date() }) => ({
  async place(order) {
    await db.insert("orders", { ...order, at: clock() });
    await mailer.send(order.email, "Order confirmed");
  },
});

// main.js  (composition root)
import { createOrderService } from "./services/order-service.js";
const orders = createOrderService({ db, mailer });
```

Benefits: easy tests, swappable implementations, fewer hidden singletons. See `20_design-patterns/09_dependency-injection.md`.

## 7. Facade / public API module

Expose a simplified interface over a complex subsystem.

```js
// payments/index.js  (the only file other code imports)
import { stripeClient } from "./internal/stripe.js";
import { retry } from "./internal/retry.js";

export async function charge(amount, token) {
  return retry(() => stripeClient.charge({ amount, source: token }));
}
```

Enforce boundaries with `package.json` `exports`, `#imports`, or lint rules (`eslint-plugin-import` `no-restricted-paths`, `eslint-plugin-boundaries`, `dependency-cruiser`).

## 8. Namespace imports for grouped utilities

```js
import * as math from "./math.js";
math.add(1, 2);
```

Good for readable call sites (`path.join`, `fs.readFile`); bundlers still tree-shake static member access.

## 9. Lazy loading with `import()`

```js
let chartModule;
async function showChart(data) {
  chartModule ??= await import("./chart.js");      // load once, on first use
  chartModule.render(data);
}

// route table
const pages = { home: () => import("./home.js"), admin: () => import("./admin.js") };
const { default: render } = await pages[name]();
```

Use for rarely used or heavy features, route-level code splitting, and optional functionality (feature flags).

## 10. Plugin / registry pattern

```js
// registry.js
const handlers = new Map();
export const register = (type, fn) => handlers.set(type, fn);
export const handle = (type, ...args) => (handlers.get(type) ?? (() => { throw new Error(`no handler for ${type}`); }))(...args);

// plugins/csv.js
import { register } from "../registry.js";
register("csv", parseCsv);                 // side-effect registration on import
```

Side-effect imports must be listed in `sideEffects` so bundlers keep them. Prefer **explicit registration** calls in the composition root over import side effects.

## 11. Configuration modules

```js
// config.js
const required = (name) => {
  const value = process.env[name];
  if (!value) throw new Error(`Missing environment variable ${name}`);
  return value;
};

export const config = Object.freeze({
  port: Number(process.env.PORT ?? 3000),
  databaseUrl: required("DATABASE_URL"),
});
```

Fail fast at startup, validate with a schema (Zod, Valibot), never import secrets into browser bundles.

## 12. Avoiding circular dependencies

Symptoms: `undefined` imports, `ReferenceError` in TDZ, or "works only in some import orders".

```
a.js ──► b.js ──► a.js        (cycle)
```

Fixes:

| Technique | Idea |
|-----------|------|
| **Extract shared code** | move what both need into a third module `c.js` |
| **Dependency inversion** | depend on an interface/callback passed in rather than importing the concrete module |
| **Lazy access** | use the import inside functions, not at module top level |
| **Merge modules** | if two modules are inseparable, they are probably one module |
| **Layering rules** | allow imports only downward (UI → services → data), enforce with tooling |
| **Barrels discipline** | never import your own barrel from inside the same feature |

```js
// before: user.js imports order.js and order.js imports user.js
// after
// types.js      (shared constants/types)
// user.js       → imports types.js
// order.js      → imports user.js and types.js
```

Detect cycles: `madge --circular src`, `dependency-cruiser`, ESLint `import/no-cycle`.

## 13. Side-effect-free modules

Prefer modules that only **define** things; run actions from explicit functions or the entry file.

```js
// bad
import "./analytics.js";                   // starts tracking just by importing
// good
import { initAnalytics } from "./analytics.js";
initAnalytics({ id });
```

Benefits: tree shaking, testability, predictable startup order.

## 14. Re-export and compatibility layers

```js
// old path kept for backward compatibility
export { newFunction as oldFunction } from "./new-location.js";
export * from "./v2/index.js";
```

Useful in libraries and during large refactors.

## 15. Testing modules

```js
// Vitest: mock a module
import { vi, test } from "vitest";
vi.mock("./mailer.js", () => ({ send: vi.fn() }));

// reset module state between tests
vi.resetModules();
const { counter } = await import("./counter.js");   // fresh instance
```

Hard-to-test modules usually have hidden global state or import-time side effects: switch to factories or injection.

## 16. Project structure

```
src/
  features/
    users/
      index.js            // public API of the feature
      user-service.js
      user-repo.js
      user-routes.js
    orders/
  shared/
    http/
    logging/
  main.js                 // composition root
```

| Strategy | Notes |
|----------|-------|
| **By feature** (recommended) | related code lives together, clear boundaries |
| By layer (`controllers/`, `services/`, `models/`) | simple at first, spreads a feature across folders |
| Shared code | in `shared/` or a package; avoid becoming a dumping ground |
| Path aliases (`@/`, `#imports`) | avoid `../../../` chains; configure in bundler/TS/package.json |

## 17. Legacy module formats (recognize, do not write)

| Format | Where |
|--------|-------|
| **IIFE/global namespace** | old script-tag code (`window.MyLib`) |
| **AMD** (`define([...], fn)`, RequireJS) | older browser apps |
| **UMD** | libraries supporting AMD, CJS and globals |
| **SystemJS** | transitional loader |

Modern code uses ESM (with a bundler or natively), plus CJS only where required.

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| Hidden global state in module-level variables | Order-dependent bugs, hard tests | Factories, DI, explicit init |
| Barrel files everywhere | Slow builds, cycles | Barrels only at feature boundaries |
| Import-time side effects | Surprising behavior, tree shaking loss | Explicit `init()` calls |
| Circular dependencies | `undefined`/TDZ errors | Extract shared modules, layer imports |
| Deep relative import chains | Brittle paths | Aliases or `#imports` |
| Giant "utils" modules | Low cohesion | Split by purpose |
| Default exports with inconsistent names | Hard to search/refactor | Named exports |
| Importing internals across features | Tight coupling | Public API + boundary lint rules |
| Reading config at import time in libraries | Cannot reconfigure/test | Accept config in factories |

## Key takeaways

- A file is a module: unexported names are private, and top-level state is a shared singleton
- Prefer named exports, small public surfaces, and one responsibility per module
- Use factories and dependency injection for configuration and testability
- Use barrels sparingly, lazy-load heavy features with `import()`, and keep modules free of import-time side effects
- Break circular dependencies by extracting shared code and enforcing layers

**Next:** [DOM and Browser](../14_dom-and-browser/00_README.md)
