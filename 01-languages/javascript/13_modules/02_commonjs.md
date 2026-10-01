# CommonJS

**CommonJS (CJS)** is Node.js's original module system, based on `require()` and `module.exports`. A huge amount of existing code and tooling (config files, older packages) still uses it.

```js
// math.js
const PI = 3.14159;
function add(a, b) { return a + b; }
module.exports = { PI, add };

// main.js
const { PI, add } = require("./math");
console.log(add(1, 2));
```

## Core API

| Name | Meaning |
|------|---------|
| `require(id)` | load a module and return its `module.exports` |
| `module.exports` | the value exported (any type) |
| `exports` | shortcut reference to `module.exports` |
| `module` | object describing this module (`id`, `filename`, `loaded`, `children`, `paths`) |
| `__filename`, `__dirname` | absolute path of this file / its directory |
| `require.cache` | cache of loaded modules |
| `require.resolve(id)` | resolve to an absolute path without loading |
| `require.main` | the entry module (`require.main === module` in the script run by `node`) |

## Exporting

```js
// replace the whole export
module.exports = function greet() {};
module.exports = class User {};
module.exports = { a, b };

// add properties
exports.add = (a, b) => a + b;
exports.PI = 3.14;
module.exports.sub = (a, b) => a - b;
```

`exports` is just a **reference to** `module.exports`. Reassigning it breaks the link:

```js
exports = { a: 1 };            // BUG: module.exports is still the original empty object
module.exports = { a: 1 };     // correct
```

## Importing

```js
const fs = require("fs");                      // built-in (or "node:fs")
const express = require("express");            // from node_modules
const local = require("./local");              // relative: tries ./local, ./local.js, ./local.json, ./local.node, ./local/index.js
const { add } = require("./math");             // destructure
const config = require("./config.json");       // JSON is parsed automatically
```

## The module wrapper

Node wraps each file in a function, which is why `require`, `module`, `exports`, `__dirname` and `__filename` appear to be globals:

```js
(function (exports, require, module, __filename, __dirname) {
  // your file's code
});
```

Consequences:

- Top-level variables are **file-local**, not global
- Top-level `this` is `module.exports` (`this === exports` at the top of a CJS file)
- Top-level `return` is allowed (it just returns from the wrapper)

## Resolution algorithm (simplified)

1. Core module (`fs`, `path`, `node:*`)? use it
2. Starts with `./`, `../` or `/`? resolve as a file, then as a directory (`package.json` `main`, then `index.js`)
3. Otherwise search `node_modules` in the current directory, then each parent directory up to the root
4. Package entry points are chosen via `package.json` `exports` (if present) or `main`

```js
require.resolve("express");          // "/project/node_modules/express/index.js"
```

## Caching

A module is executed **once**; later `require` calls return the cached `module.exports`.

```js
// counter.js
let n = 0;
module.exports = { inc: () => ++n };

require("./counter").inc();          // 1
require("./counter").inc();          // 2  (same instance)

delete require.cache[require.resolve("./counter")];   // force re-load next time (rarely needed; useful in tests)
```

The cache key is the **resolved filename**, so different paths to the same file share one instance; different symlinks or duplicated packages may not.

## Synchronous loading

`require` blocks while it reads and runs the file. Fine on servers at startup, unsuitable for browsers (bundlers resolve it ahead of time). Conditional and dynamic requires are possible:

```js
if (process.env.NODE_ENV === "development") {
  require("./dev-tools");
}
const plugin = require(`./plugins/${name}`);
```

## Values are copies, not live bindings

```js
// counter.js
let count = 0;
module.exports = { count, inc() { count++; } };

// main.js
const { count, inc } = require("./counter");
inc();
console.log(count);                  // 0  (copied at require time)

// to expose live values, use getters or functions
module.exports = { get count() { return count; }, inc };
```

## Circular dependencies

```js
// a.js
exports.loaded = false;
const b = require("./b");
console.log("in a, b.loaded =", b.loaded);
exports.loaded = true;

// b.js
const a = require("./a");            // gets a's PARTIAL exports so far ({ loaded: false })
console.log("in b, a.loaded =", a.loaded);   // false
exports.loaded = true;
```

On a cycle, `require` returns whatever has been exported **so far**. Replacing `module.exports` after requiring a cyclic dependency causes the other side to hold a stale object. Avoid cycles, or require lazily inside functions.

## `require.main`

```js
if (require.main === module) {
  main();                            // run only when executed directly: node script.js
}
module.exports = { run };            // still importable by tests
```

ESM equivalent: `if (import.meta.url === pathToFileURL(process.argv[1]).href) main();` (or `import.meta.main` in Deno/Bun and newer Node).

## Error handling

```js
try {
  const optional = require("optional-dependency");
} catch (err) {
  if (err.code !== "MODULE_NOT_FOUND") throw err;   // distinguish missing module from errors inside it
}
```

## Where CommonJS still matters

| Case | Notes |
|------|-------|
| Old npm packages | many still ship CJS only |
| Config files | `.eslintrc.cjs`, `webpack.config.js` (CJS in many setups), `jest.config.cjs` |
| Scripts that must run on older Node | no ESM tooling needed |
| `.cjs` files inside ESM packages | explicit CJS opt-in |
| Dynamic `require` patterns | plugin loaders (consider `import()`) |

## Writing portable libraries

```js
// UMD-style detection (legacy)
(function (root, factory) {
  if (typeof define === "function" && define.amd) define([], factory);
  else if (typeof module === "object" && module.exports) module.exports = factory();
  else root.MyLib = factory();
})(typeof self !== "undefined" ? self : this, function () {
  return { hello() {} };
});
```

Today: publish ESM (optionally with a CJS build) using `package.json` `exports` (see the next file) instead of UMD.

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| `exports = {...}` | Does nothing | `module.exports = {...}` |
| Mixing `exports.x` and `module.exports = ...` | Earlier `exports.x` assignments get lost | Choose one style per file |
| Expecting live bindings | Values are copied | Export getters/functions |
| Cyclic requires | Partial exports, `undefined` values | Restructure, lazy `require` |
| Dynamic requires in bundled code | Bundlers cannot analyze | Static paths or `import()` with patterns |
| Relying on `require.cache` hacks | Fragile, version dependent | Dependency injection, fresh module state |
| `__dirname` in ESM | `ReferenceError` | `import.meta.dirname` |
| Requiring huge modules at startup | Slow boot | Lazy require inside functions |
| `require` of ESM in older Node | `ERR_REQUIRE_ESM` | `await import()` or upgrade |

## Key takeaways

- CommonJS is synchronous: `require()` returns `module.exports`, cached per resolved file
- `exports` is only an alias of `module.exports`; do not reassign it
- Values are copied at require time, and cycles see partial exports
- New code should prefer ESM; know CJS for legacy and tooling files

**Next:** [ESM vs CommonJS](./03_esm-vs-commonjs.md)
