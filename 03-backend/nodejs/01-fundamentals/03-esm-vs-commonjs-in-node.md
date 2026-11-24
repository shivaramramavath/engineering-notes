# Module System: CommonJS vs ES Modules

Node has two different module systems, and which one a project uses affects import/export syntax, whether `__dirname` is available, and how files get interpreted. Both are still common in the wild, which is why understanding both matters.

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
```

```js
// app.js
const { add } = require("./math.js");
console.log(add(2, 3));
```

- The Node default unless configured otherwise
- `require()` is **synchronous** — the module is fully loaded before execution continues
- `__dirname`/`__filename` are available automatically (see `02-global-objects.md`)
- File extensions are optional: `require("./math")` works without `.js`

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

- The same `import`/`export` syntax browsers use
- Imports are (mostly) **static**, resolved before the module runs — this is what enables tooling like tree-shaking in bundlers
- **File extensions are required**: `import "./math.js"`, never `import "./math"`
- `__dirname`/`__filename` are **not** available — a workaround is needed (below)
- Supports **top-level `await`** — using `await` outside an `async function`, directly at a module's top level

```js
// only valid in ESM
const data = await fetch("https://api.example.com/data").then((r) => r.json());
```

---

## How Node decides which one applies

### File extension

```
math.mjs   →  always ES Modules, regardless of package.json
math.cjs   →  always CommonJS, regardless of package.json
```

### `"type"` in `package.json`

```json
{
  "type": "module"
}
```

- `"type": "module"` — plain `.js` files in this project are ES Modules
- `"type": "commonjs"`, or the field omitted — plain `.js` files are CommonJS (the historical default)

New projects generally set `"type": "module"` from the start — see `00-setup/02-npm-and-package-json.md`.

---

## Named vs default exports

```js
// named exports
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

import add from "./math.js"; // any local name works, since it's the default
```

CommonJS's rough equivalent is less explicit about the distinction:

```js
module.exports = add; // like a "default export"
module.exports = { add, PI }; // like "named exports"
```

---

## Mixing the two: interop

### Requiring CommonJS from ESM — generally fine

```js
import someCjsPackage from "some-old-package";
```

Most CommonJS npm packages can be imported from an ESM project without any special handling — Node covers the common cases automatically.

### Requiring ESM from CommonJS — the harder direction

```js
// ❌ require() cannot synchronously load an ES Module
const esmPackage = require("some-esm-only-package");
```

```js
// ✅ dynamic import works, but is asynchronous
const esmPackage = await import("some-esm-only-package");
```

This asymmetry is one of the most common points of friction when a dependency migrates to being ESM-only while a project is still on CommonJS — `import()` (the dynamic, function-style form, usable even from CommonJS code) is the way through it.

---

## The `__dirname` gap in ES Modules

```js
// CommonJS — just works
console.log(__dirname);
```

```js
// ES Modules — needs this workaround
import path from "node:path";
import { fileURLToPath } from "node:url";

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);
```

`import.meta.url` is ESM's way of exposing the current module's location — `fileURLToPath` converts that `file://` URL into a normal filesystem path, which `path.dirname` then reduces to the containing folder.

---

## Which should a new project use?

**ES Modules**, in almost every case, for new projects:

- The standard JavaScript module system, not a Node-specific one
- Top-level `await` removes a lot of the async-IIFE boilerplate CommonJS needed
- Browsers, most modern tooling, and most actively-maintained packages have converged on it

Reach for CommonJS only when maintaining an existing CJS codebase, or when a specific dependency genuinely still requires it.

## Quick summary

|                   | CommonJS                       | ES Modules                                      |
| ----------------- | ------------------------------ | ----------------------------------------------- |
| Syntax            | `require()` / `module.exports` | `import` / `export`                             |
| Loading           | Synchronous                    | Static (mostly), plus dynamic `import()`        |
| `__dirname`       | Built in                       | Needs `fileURLToPath(import.meta.url)`          |
| Top-level `await` | No                             | Yes                                             |
| Import extensions | Optional                       | Required                                        |
| Set via           | Default, or `.cjs`             | `"type": "module"` in `package.json`, or `.mjs` |

- Default to ES Modules for anything new
- `import()` (dynamic form) is how CommonJS code loads an ESM-only package
- Check `package.json`'s `"type"` before assuming which syntax a `.js` file expects

## Next

**`04-environment-variables.md`** covers `process.env` and how configuration reaches a running Node app — regardless of which module system you're using.
