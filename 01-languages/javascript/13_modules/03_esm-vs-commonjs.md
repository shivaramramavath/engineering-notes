# ESM vs CommonJS

Node.js supports both module systems. This file compares them, explains **interop** in both directions, and shows how to configure `package.json` for apps and libraries.

## Side-by-side

| Feature | ESM | CommonJS |
|---------|-----|----------|
| Syntax | `import x from "y"`, `export` | `require("y")`, `module.exports` |
| Loading | static graph, async capable | synchronous, runtime |
| Static analysis / tree shaking | yes | limited |
| Exports | **live bindings** | value copies |
| Top-level `await` | yes | no |
| Strict mode | always | opt-in |
| Top-level `this` | `undefined` | `module.exports` |
| `__dirname`, `__filename`, `require` | not available (use `import.meta`) | available |
| File extensions in specifiers | required (relative paths) | optional |
| Directory imports (`./dir` → `index.js`) | not supported | supported |
| Conditional/dynamic loading | `await import()` | `require()` anywhere |
| JSON | `import x from "./x.json" with { type: "json" }` | `require("./x.json")` |
| Browser support | native | needs a bundler |
| Cycles | live bindings (TDZ pitfalls) | partial exports |
| Cache | module map by URL | `require.cache` by path |

## Choosing the system in Node

| Signal | Result |
|--------|--------|
| `.mjs` extension | ESM |
| `.cjs` extension | CommonJS |
| `.js` + nearest `package.json` has `"type": "module"` | ESM |
| `.js` + `"type": "commonjs"` or no `"type"` | CommonJS |
| `--input-type=module` for `--eval` / stdin | ESM |

```json
{ "name": "my-app", "type": "module" }
```

Recent Node versions can also detect ESM syntax in ambiguous `.js` files; do not depend on it. Be explicit.

## Importing CommonJS from ESM

Always works.

```js
// ESM
import fs from "node:fs";
import pkg from "./legacy.cjs";            // default import = module.exports
import { named } from "./legacy.cjs";      // works only if Node can statically detect the export names
```

- The **default** import is always `module.exports`
- **Named** imports rely on static analysis (`cjs-module-lexer`) and may be missing for dynamic exports. When in doubt use the default and destructure:

```js
import legacy from "./legacy.cjs";
const { named } = legacy;
```

## Using ESM from CommonJS

```js
// CommonJS: use dynamic import (returns a promise)
async function main() {
  const { default: chalk } = await import("chalk");     // ESM-only package
  console.log(chalk.green("ok"));
}
main();
```

- `require()` of an ES module historically throws `ERR_REQUIRE_ESM`
- Recent Node versions allow `require()` of **synchronous** ESM (no top-level `await`); check your Node version and docs before relying on it
- Alternatively keep CJS callers on a dual-published build of the library

## CommonJS features rebuilt in ESM

```js
// __dirname, __filename
import { fileURLToPath } from "node:url";
import path from "node:path";
const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);
// Node 20.11+: import.meta.dirname, import.meta.filename

// require inside ESM (for CJS-only needs or JSON on older Node)
import { createRequire } from "node:module";
const require = createRequire(import.meta.url);
const pkg = require("./package.json");

// "run if main"
import { pathToFileURL } from "node:url";
if (import.meta.url === pathToFileURL(process.argv[1]).href) main();
```

## `package.json` fields for module systems

| Field | Purpose |
|-------|---------|
| `"type"` | `"module"` or `"commonjs"`: how `.js` files are interpreted |
| `"main"` | legacy entry point (CJS), used when `exports` is absent |
| `"module"` | **bundler** convention for an ESM entry (not used by Node) |
| `"exports"` | modern entry map with conditions; **encapsulates** the package |
| `"imports"` | private aliases starting with `#` (for the package's own use) |
| `"types"` | TypeScript declarations entry |
| `"sideEffects"` | tells bundlers which files can be tree-shaken (next file) |
| `"files"` | what to publish |
| `"engines"` | supported Node versions |

## The `exports` map

```json
{
  "name": "my-lib",
  "type": "module",
  "exports": {
    ".": {
      "types": "./dist/index.d.ts",
      "import": "./dist/index.js",
      "require": "./dist/index.cjs",
      "default": "./dist/index.js"
    },
    "./utils": "./dist/utils.js",
    "./package.json": "./package.json"
  }
}
```

Behavior:

- **Conditions** are matched in order: `types`, `import` (ESM consumers), `require` (CJS consumers), `node`, `browser`, `development`, `production`, `default`; put `types` first and `default` last
- Only listed subpaths can be imported (`my-lib/utils`); deep imports like `my-lib/dist/internal.js` are **blocked**
- Patterns: `"./features/*": "./src/features/*.js"`
- Supported by Node 12.7+/14+, bundlers, and TypeScript with `moduleResolution: "node16" | "nodenext" | "bundler"`

### Private imports

```json
{ "imports": { "#db": "./src/db/index.js", "#utils/*": "./src/utils/*.js" } }
```

```js
import { query } from "#db";             // stable internal alias, no `../../../` chains
```

## Publishing a library

| Strategy | Pros | Cons |
|----------|------|------|
| **ESM only** | simplest, modern, tree-shakable | CJS users need `import()` (or newer `require(esm)`) |
| **Dual (ESM + CJS)** | widest compatibility | build complexity, **dual package hazard** |
| **CJS only** | works everywhere in Node | no tree shaking, outdated |

### The dual package hazard

If an app loads both the ESM and CJS builds of the same package (through different dependency paths), the module is **instantiated twice**, so singletons, `instanceof` checks and caches break.

Mitigations:

- Publish ESM only, or make the CJS file a thin wrapper that loads the ESM
- Keep shared state out of the package, or in one format that both entry points re-export
- Avoid class identity checks across the boundary (use duck typing or symbols via `Symbol.for`)

### Building both formats

Tools: `tsup`, `unbuild`, `Rollup`, `tsc` (two outputs), `esbuild`.

```bash
tsup src/index.ts --format esm,cjs --dts
```

Check with `publint` and `arethetypeswrong` (`attw`) before publishing.

## TypeScript notes

| Setting | Meaning |
|---------|---------|
| `"module": "nodenext"` + `"moduleResolution": "nodenext"` | follows Node's ESM/CJS rules; relative imports need explicit `.js` extensions (even in `.ts` files) |
| `"moduleResolution": "bundler"` | resolution like bundlers (no mandatory extensions) |
| `"verbatimModuleSyntax": true` | keeps `import type` explicit |
| `"esModuleInterop": true` | smoother default-import behavior from CJS |

```ts
import type { User } from "./types.js";          // erased at compile time
import { load } from "./load.js";                 // ".js" even though the file is load.ts
```

## Interop pitfalls: default exports

CJS-to-ESM default mapping can produce a double default in transpiled code:

```js
// transpiled TS/Babel CJS marks __esModule
import mod from "./lib.cjs";
mod.default;                // sometimes needed when the CJS module was generated from ESM
```

When "undefined is not a function" appears after switching module formats, inspect `console.log(mod)` and adjust (`mod.default ?? mod`).

## Migration path CJS → ESM

1. Update Node to an active LTS and update dependencies (some are ESM-only now)
2. Add `"type": "module"`, rename config files that must stay CJS to `.cjs`
3. Replace `require`/`module.exports` with `import`/`export`
4. Add `.js` extensions to relative imports; replace directory imports with `./dir/index.js`
5. Replace `__dirname`/`__filename` with `import.meta` equivalents
6. Replace `require` of JSON with `import ... with { type: "json" }` (or `createRequire`)
7. Update test/tooling configs (Jest ESM mode, or switch to Vitest)
8. Run with `node --trace-warnings` and fix remaining interop issues

## Quick decision guide

| Situation | Choose |
|-----------|--------|
| New app or service | ESM (`"type": "module"`) |
| New library | ESM-first, add CJS build only if consumers need it |
| Existing CJS app | Keep until a clear reason; migrate gradually |
| Config files read by tools that expect CJS | `.cjs` |
| Need dynamic computed loading | `import()` |
| Need browser support | ESM (bundled or native) |

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| Mixing `require` and `import` in the same file | Errors in one of the systems | Pick one per file |
| Missing extensions in ESM relative imports | `ERR_MODULE_NOT_FOUND` | `./file.js` |
| Using `__dirname` in ESM | `ReferenceError` | `import.meta.dirname` |
| Named imports from CJS that Node cannot analyze | `SyntaxError: does not provide an export named` | Default import and destructure |
| Publishing dual packages carelessly | Duplicate instances | ESM-only or thin wrappers |
| Forgetting to list subpaths in `exports` | Consumers get `ERR_PACKAGE_PATH_NOT_EXPORTED` | Declare them |
| Relying on `"module"` field for Node | Node ignores it | Use `exports` |
| Putting `types` after `default` in `exports` | TypeScript may not pick it up | Order conditions: types first |
| Ignoring `"type"` | Wrong interpretation of `.js` | Set it explicitly |

## Key takeaways

- ESM: static, live bindings, async-capable; CJS: dynamic, synchronous, copied values
- ESM can import CJS freely; CJS needs `import()` (or newer `require(esm)`) for ESM
- Control module type with `.mjs`/`.cjs` or `"type"`; control entry points with `exports`
- Prefer ESM-only libraries; if you ship dual, beware duplicate module instances

**Next:** [Bundlers and Tree Shaking](./04_bundlers-and-tree-shaking.md)
