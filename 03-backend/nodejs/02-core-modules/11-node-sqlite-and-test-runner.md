# Module System: CommonJS vs ES Modules

Node has two different module systems, and which one a project uses affects import/export syntax, `__dirname` availability, and how files are interpreted. Understanding both matters because plenty of existing code, tutorials, and packages still use the older one.

```
CommonJS (CJS)   →  Node's original module system — require() / module.exports
ES Modules (ESM)  →  the standard JavaScript module system — import / export
```

---

## CommonJS (the original)

```js
// math.js
function add(a, b) {
  return a + b;
}

module.exports = { add };
// or: module.exports.add = add;
```

```js
// app.js
const { add } = require("./math.js");
console.log(add(2, 3));
```

- Files are treated as CommonJS by default unless configured otherwise
- `require()` is **synchronous** — the required module is fully loaded before execution continues
- `__dirname` and `__filename` are available automatically
- No file extension required in most cases: `require("./math")` works without `.js`

---

## ES Modules (the modern standard)

```js
// math.js
export function add(a, b) {
  return a + b;
}
```

```js
// app.js
import { add } from "./math.js";
console.log(add(2, 3));
```

- `import`/`export` is the same module syntax browsers use — a Node ESM file and browser JavaScript module share the same syntax
- Imports are (mostly) **static** — resolved before the module runs, which enables tooling like tree-shaking in bundlers
- **File extensions are required**: `import "./math.js"`, not `import "./math"`
- `__dirname`/`__filename` are **not** available — see the `path`/`url` workaround below (covered fully in `02-path.md`)
- Supports **top-level `await`** — `await` outside an `async function`, at the top level of a module

```js
// only works in ESM
const data = await fetch("https://api.example.com/data").then((r) => r.json());
```

---

## How Node decides which one you're using

### `.mjs` / `.cjs` extensions

```
math.mjs   →  always treated as ES Modules
math.cjs   →  always treated as CommonJS
```

### `"type"` in `package.json`

```json
{
  "type": "module"
}
```

- `"type": "module"` — `.js` files in this project are treated as **ES Modules**
- `"type": "commonjs"` (or the field omitted entirely) — `.js` files are treated as **CommonJS** (the default)

This is why a project's `package.json` matters so much for which syntax is valid — the dotenv/envalid/Express examples throughout this documentation set assume `"type": "module"`, since that's the modern default for new projects.

---

## Named vs default exports

```js
// named exports — import exactly what you name
export function add(a, b) {
  return a + b;
}
export const PI = 3.14159;

import { add, PI } from "./math.js";
```

```js
// default export — one primary thing per module
export default function add(a, b) {
  return a + b;
}

import add from "./math.js"; // any name works, since it's the default
```

CommonJS has a rough equivalent, but it's less clean about the distinction:

```js
module.exports = add; // like a "default export"
module.exports = { add, PI }; // like "named exports"
```

---

## Mixing the two: interop

### Requiring CommonJS from ESM

```js
// works — ESM can import CommonJS modules
import someCjsPackage from "some-old-package";
```

Most npm packages still published as CommonJS can be imported from an ESM project without issue — Node handles the interop automatically for the common cases.

### Requiring ESM from CommonJS — the harder direction

```js
// ❌ doesn't work — require() cannot load an ES Module synchronously
const esmPackage = require("some-esm-only-package");
```

```js
// ✅ dynamic import works, but is asynchronous
const esmPackage = await import("some-esm-only-package");
```

This asymmetry (ESM can import CJS more easily than the reverse) is one of the most common friction points when a dependency you rely on migrates to being ESM-only — it's the reason `import()` (the dynamic, function-style version) exists even in CommonJS code.

---

## The `__dirname` situation

Covered in full in `02-path.md`, but the short version:

```js
// CommonJS — just works
console.log(__dirname);
```

```js
// ES Modules — needs a workaround
import path from "node:path";
import { fileURLToPath } from "node:url";

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);
```

---

## Which should a new project use?

**ES Modules**, for new projects, in almost all cases:

- It's the standard JavaScript module system, not a Node-specific one
- Top-level `await` alone removes a lot of the "wrap everything in an async IIFE" boilerplate CommonJS projects needed
- The broader JavaScript ecosystem (browsers, most modern tooling, most actively-maintained packages) has converged on it

Stick with CommonJS only when maintaining an existing CJS codebase, or when a specific dependency genuinely requires it (increasingly rare).

## Quick summary

|                           | CommonJS                       | ES Modules                                                |
| ------------------------- | ------------------------------ | --------------------------------------------------------- |
| Syntax                    | `require()` / `module.exports` | `import` / `export`                                       |
| Loading                   | Synchronous                    | Static (mostly) / supports dynamic `import()`             |
| `__dirname`               | Built in                       | Needs `fileURLToPath(import.meta.url)`                    |
| Top-level `await`         | No                             | Yes                                                       |
| File extension in imports | Optional                       | Required                                                  |
| Set via                   | Default, or `.cjs` extension   | `"type": "module"` in `package.json`, or `.mjs` extension |

- New projects should default to ES Modules
- `import()` (dynamic) is the way to load an ESM-only package from CommonJS code
- Check `package.json`'s `"type"` field before assuming which syntax a `.js` file expects

## Section complete

That covers Node's core built-in modules — the foundation everything else in this documentation set (Express, BullMQ, database drivers, and more) is built on top of.
