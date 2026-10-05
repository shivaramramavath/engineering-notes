# ES Modules and CommonJS

JavaScript has two module systems: **ES modules** (ESM, `import`/`export`) and **CommonJS** (CJS, `require`/`module.exports`). TypeScript lets you write ESM syntax and then emits either one, depending on config. The hard part is not the syntax. It is that Node.js decides per file which system applies, and TypeScript's settings must agree with it.

**Prerequisites:**
- [Imports and exports](./00-imports-and-exports.md)
- [Compiler options](../13-compiler-and-tsconfig/00-compiler-options.md)

---

## The two systems side by side

| | CommonJS | ES modules |
|---|---|---|
| Syntax | `require()`, `module.exports` | `import`, `export` |
| Loading | synchronous | asynchronous, statically analyzable |
| Imports | copied values (a snapshot of `exports`) | live bindings |
| Top-level `await` | no | yes |
| `__dirname`, `__filename`, `require` | available | not available |
| Node default | `.js` files with no `"type"` field, `.cjs` | `.mjs`, or `.js` when `"type": "module"` |
| `import()` | yes | yes |

## What TypeScript does with your code

You write ESM syntax in `.ts` files. The `module` option picks the output format:

```ts
// source
import { add } from "./math";
export const result = add(1, 2);
```

```js
// module: "commonjs"  ->  emitted
const math_1 = require("./math");
exports.result = (0, math_1.add)(1, 2);

// module: "esnext" / "es2022"  ->  emitted (essentially unchanged)
import { add } from "./math";
export const result = add(1, 2);
```

`module` controls the **emit** format. `moduleResolution` controls **how import specifiers are looked up**. They must be a sensible pair. The common combinations:

| Situation | `module` | `moduleResolution` |
|---|---|---|
| Node.js app or library (CJS or ESM, per package) | `nodenext` | `nodenext` |
| App compiled by a bundler (Vite, webpack, esbuild) | `esnext` (or `preserve`) | `bundler` |
| Legacy CJS-only Node project | `commonjs` | `node10` (formerly `node`) |

For Node.js code, `nodenext` is the one that mirrors Node's real behavior. `bundler` is deliberately more permissive (extensionless imports, `exports` map support) and is wrong for code that runs directly in Node. See [target, module, and lib](../13-compiler-and-tsconfig/02-target-module-and-lib.md) and [module resolution and paths](../13-compiler-and-tsconfig/03-module-resolution-and-paths.md).

## How Node decides which system a file uses

Under `module: "nodenext"`, TypeScript follows Node's rules:

```text
file extension / package.json "type"      treated as
------------------------------------      ----------
.mts  / .mjs                              ESM
.cts  / .cjs                              CommonJS
.ts   / .js  with "type": "module"        ESM
.ts   / .js  with "type": "commonjs"      CommonJS
.ts   / .js  with no "type" field         CommonJS
```

The nearest `package.json` wins. The decision is per file, and TypeScript emits matching output.

```json
{
  "type": "module"
}
```

## ESM in Node: the rules that trip people

**Relative imports need the file extension, and it is the _output_ extension.** You write `.js` in a `.ts` file, because that is the file that will exist at runtime:

```ts
// src/main.ts
import { add } from "./math.js";   // refers to math.ts at compile time
```

Omitting it gives error TS2835 ("Relative import paths need explicit file extensions...") under `nodenext` when the importing file is ESM. CJS files can still omit extensions.

From TS 5.7, `rewriteRelativeImportExtensions` lets you write `.ts` extensions in imports and have them rewritten to `.js` on emit. `allowImportingTsExtensions` is the related option for projects that do not emit JavaScript at all (they let a bundler or runtime handle `.ts` files).

**No `__dirname` or `__filename`.** Use `import.meta`:

```ts
import { fileURLToPath } from "node:url";
import path from "node:path";

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);
```

Recent Node versions also provide `import.meta.dirname` and `import.meta.filename`; check your Node version before relying on them.

**Top-level `await` works only in ESM**, and needs a `target` of ES2017+ together with an ESM-emitting `module` setting.

## Interop: importing CommonJS from ESM and the reverse

CommonJS has no concept of a default export. `module.exports = fn` exports the function itself. Two flags decide how TypeScript treats `import x from "cjs-lib"`:

- **`esModuleInterop`**: emits helper code so default and namespace imports of CJS modules behave like they do in Babel and Node's ESM loader. It also turns on `allowSyntheticDefaultImports`.
- **`allowSyntheticDefaultImports`**: type-checking only. It lets you write a default import for modules that have no declared default. It does not change emit.

```ts
// With esModuleInterop: true
import express from "express";           // works
import * as path from "node:path";       // works, but `import path from "node:path"` is also fine

// Without it, you would need:
import express = require("express");
```

Enable `esModuleInterop` in almost every project. Without it, `import * as x` of a callable CJS export has subtle bugs.

### `export =` and `import = require()`

Some CJS libraries use `module.exports = something`. Their type declarations use TypeScript's CommonJS-specific syntax:

```ts
// declaration side
export = createServer;

// consumer side, only needed when interop is off or module is "commonjs"
import createServer = require("./server");
```

These two forms cannot be used when emitting ES modules. If you author new code, use `export`/`import` and let the compiler emit CJS if you need it.

### ESM importing CJS, CJS importing ESM

- **ESM importing CJS** works. The CJS `module.exports` becomes the default import, and Node tries to detect named exports statically, which is not always reliable. If a named import fails at runtime, import the default and destructure it.
- **CJS importing ESM** was historically impossible with `require()` and required `await import()`. Newer Node versions can `require()` ESM that has no top-level `await`, and TypeScript added matching `nodenext` support in 5.8. Check your Node and TypeScript versions before relying on it. `await import()` always works.

```ts
// In a CJS file, load an ESM-only package:
const { default: chalk } = await import("chalk");
```

## Choosing for a new project

- **Node app or library, modern Node:** `"type": "module"`, `module: "nodenext"`, `.js` extensions in relative imports. This matches where the ecosystem is heading.
- **Bundled frontend:** `module: "esnext"` (or `preserve`), `moduleResolution: "bundler"`, `verbatimModuleSyntax: true`. The bundler handles resolution, so extensions are optional.
- **Publishing a library:** consumers may use either system. Decide whether to ship ESM only, CJS only, or both, and describe it in `package.json` (`exports`, `types`). See [publishing packages](../21-production-tooling/03-publishing-packages.md) for the dual-package trade-offs.

## Common mistakes

- **Using `moduleResolution: "bundler"` for code that Node runs directly.** It compiles, then fails at runtime because Node requires file extensions in ESM.
- **Setting `"type": "module"` but compiling with `module: "commonjs"`.** The emitted `require`/`exports` crash in an ESM context.
- **Writing `./math.ts` or an extensionless `./math` in Node ESM source.** Use `./math.js` (or enable `rewriteRelativeImportExtensions`).
- **Using `__dirname` in an ESM file.** It does not exist.
- **Turning off `esModuleInterop` and then using default imports from CJS.** You get `undefined is not a function` at runtime.
- **Assuming `import()` stays `import()`.** Under `module: "commonjs"` it is compiled to a promise wrapping `require`. Under `nodenext` for CJS files it is kept, so it can load ESM.

## Debugging

| Error | Likely cause |
|---|---|
| `SyntaxError: Cannot use import statement outside a module` | ESM output running in a CJS context. Fix `"type"`, or the `module` setting, or the file extension. |
| `ReferenceError: exports is not defined in ES module scope` (or `require is not defined`) | CJS output running in an ESM context. Same fix from the other side. |
| `ERR_REQUIRE_ESM` | `require()` of an ESM-only package on a Node version that does not support it. Use `await import()` or upgrade. |
| TS2835 | Missing file extension on a relative import under `nodenext`. Add `.js`. |
| TS1479 | A CJS file imports an ESM-only module. Use `import()`, or change the importing file to ESM. |
| `undefined` from a default import of a CJS lib | `esModuleInterop` is off, or the package exposes the function on `.default`. |

Useful commands:

```bash
tsc --showConfig           # the resolved tsconfig, including defaults
tsc --traceResolution      # how each specifier was resolved
tsc --module nodenext --moduleResolution nodenext --noEmit   # quick check of a setting
```

## Quick summary

- ESM and CJS are different loaders with different rules. TypeScript lets you write ESM syntax and emit either.
- `module` sets the output format, `moduleResolution` sets how specifiers are found. Use `nodenext` for Node, `bundler` for bundled apps.
- In Node ESM, relative imports need the `.js` extension, and `__dirname` becomes `import.meta`.
- Turn on `esModuleInterop` for sane default imports of CJS libraries.
- Most runtime errors in this area are a mismatch between `package.json` `"type"`, file extensions, and `module`.

**Next:** [Barrel files and module organization](./02-barrel-files-and-module-organization.md)