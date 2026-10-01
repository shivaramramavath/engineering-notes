# ES Modules

**ES modules (ESM)** are the official JavaScript module system (ES2015). They work in browsers, Node.js, Deno and Bun with the same syntax.

```js
// math.js
export const PI = 3.14159;
export function add(a, b) { return a + b; }
export default class Calculator {}

// main.js
import Calculator, { PI, add } from "./math.js";
```

## Enabling ESM

| Environment | How |
|-------------|-----|
| Browser | `<script type="module" src="./main.js"></script>` |
| Node.js | `.mjs` extension, or `"type": "module"` in the nearest `package.json` |
| Deno, Bun | default |
| Bundlers / TypeScript | default for source files |

```html
<script type="module">
  import { add } from "./math.js";
</script>
```

## Exporting

```js
// named exports (any number)
export const name = "Ada";
export function greet() {}
export class User {}
export { a, b as renamed };            // list form, with renaming

// default export (one per module)
export default function () {}          // anonymous allowed
export { helper as default };

// re-exports
export { x, y } from "./other.js";
export * from "./other.js";            // all named exports (not default)
export * as utils from "./utils.js";   // namespace re-export (ES2020)
export { default } from "./other.js";
export { default as Other } from "./other.js";
```

## Importing

```js
import def from "./mod.js";                       // default
import { a, b as c } from "./mod.js";             // named, with alias
import def, { a } from "./mod.js";                // both
import * as ns from "./mod.js";                   // namespace object: ns.a, ns.default
import "./side-effects.js";                       // run only, import nothing
import data from "./data.json" with { type: "json" };   // import attributes (JSON modules)
```

Rules:

- Import and export statements must be **top level** (not inside `if` or functions)
- Specifiers must be **string literals** (no variables); use dynamic `import()` for computed paths
- Imports are **hoisted**: they run before any other code in the file
- Names bound by `import` are **read-only** (assignment throws `TypeError`)

## Specifiers

| Form | Example | Notes |
|------|---------|-------|
| Relative | `"./utils.js"`, `"../lib/x.js"` | **extension required** in browsers and Node ESM |
| Absolute URL | `"https://cdn.example/lib.js"` | browsers, Deno |
| Bare | `"lodash-es"` | resolved by Node (`node_modules`), bundlers, or **import maps** in browsers |
| Node built-in | `"node:fs"` | `node:` prefix recommended |
| Alias | `"@/components/Button.js"` | bundler/TypeScript path mapping |

Browsers cannot resolve bare specifiers without an **import map**:

```html
<script type="importmap">
  { "imports": { "lodash-es": "https://cdn.jsdelivr.net/npm/lodash-es@4/lodash.js" } }
</script>
```

## Live bindings

Imports are **views** of the exporter's variables, not copies.

```js
// counter.js
export let count = 0;
export function inc() { count++; }

// main.js
import { count, inc } from "./counter.js";
console.log(count);   // 0
inc();
console.log(count);   // 1  (updated!)
count = 5;            // TypeError: Assignment to constant variable
```

Only the exporting module can change the value.

## Modules run once (singletons)

A module is evaluated **once**, on first import; every importer shares the same instance and state.

```js
// config.js
export const config = { debug: false };

// a.js:  import { config } from "./config.js"; config.debug = true;
// b.js:  import { config } from "./config.js"; console.log(config.debug);   // true
```

## Module scope and strictness

- Every module has its **own scope**: top-level `var`/`let`/`const`/functions are not global
- Modules are **always strict mode**
- Top-level `this` is `undefined`
- Loaded with **CORS** in browsers and are **deferred** by default (run after HTML parsing, like `defer`)
- Not available: `__dirname`, `__filename`, `require`, `module`, `exports`

```js
// ESM equivalents
console.log(import.meta.url);                       // file:///project/src/main.js
import { fileURLToPath } from "node:url";
import path from "node:path";
const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);
// Node 20.11+: import.meta.filename and import.meta.dirname
```

## `import.meta`

| Property | Meaning |
|----------|---------|
| `import.meta.url` | URL of the current module |
| `import.meta.dirname` / `filename` | Node 20.11+ and Deno/Bun |
| `import.meta.resolve("./x.js")` | resolve a specifier to a URL |
| `import.meta.env` | build-tool specific (Vite) |

```js
const worker = new Worker(new URL("./worker.js", import.meta.url), { type: "module" });
const data = await fetch(new URL("./data.json", import.meta.url));
```

## Dynamic `import()`

Loads a module **on demand** and returns a promise of the **module namespace object**. It works in any script (not just modules) and allows computed specifiers.

```js
const { add } = await import("./math.js");
const mod = await import(`./locales/${lang}.js`);
button.addEventListener("click", async () => {
  const { openEditor } = await import("./editor.js");        // code splitting point
  openEditor();
});

mod.default;                                                  // default export lives on .default
```

Use for:

- Lazy loading heavy features (charts, editors)
- Conditional or environment-specific code
- Loading modules whose path is computed
- Using ESM from CommonJS

## Top-level `await`

Modules can `await` at the top level (ES2022).

```js
// config.js
const res = await fetch("/config.json");
export const config = await res.json();

// main.js
import { config } from "./config.js";    // waits until config.js finishes
```

- Importers **wait** for the awaited module to finish evaluating (siblings can still load in parallel)
- A rejection **fails the importing graph**
- Slow top-level `await` delays startup: keep it fast, or lazy-load with `import()`
- Not available in CommonJS or classic scripts

## Namespace objects

```js
import * as math from "./math.js";
math.add(1, 2);
Object.keys(math);                    // ["PI", "add", "default"]
// math.x = 1;                        // TypeError: namespaces are read-only (frozen-like, sealed)
```

Namespaces help organize and enable tree shaking when you access known properties.

## Circular dependencies

```js
// a.js
import { b } from "./b.js";
export const a = "A";
console.log(b);

// b.js
import { a } from "./a.js";
export const b = "B";
console.log(a);                       // ReferenceError: a is in the TDZ (a.js has not finished)
```

ESM handles cycles through live bindings, but accessing a binding **before it is initialized** throws a `ReferenceError` (`let`/`const`/`class`) or yields `undefined` (`var`). Functions declared with `function` are hoisted and usable. Prefer to **break cycles** (see the patterns file).

## Module loading phases

1. **Construction**: find, fetch and parse every module in the graph, build the dependency tree
2. **Instantiation**: allocate memory for exports, link importer and exporter bindings
3. **Evaluation**: run module code in dependency order (post-order), once each

Because linking happens before evaluation, ESM imports can be statically analyzed (tree shaking, early errors for missing exports).

## Browser specifics

```html
<script type="module" src="main.js"></script>
<script nomodule src="legacy.js"></script>          <!-- fallback for ancient browsers -->
<link rel="modulepreload" href="/chunks/vendor.js" />
```

- `async` attribute works on module scripts (run as soon as ready)
- Same module URL is fetched/evaluated once per page
- Module workers: `new Worker(url, { type: "module" })`
- Dynamic import and `import.meta.url` work

## Node specifics

```json
{ "type": "module" }
```

| Rule | Detail |
|------|--------|
| Extensions are **mandatory** | `import "./x.js"`, no implicit `.js` or `/index.js` |
| JSON | `import data from "./d.json" with { type: "json" }` |
| Built-ins | `import fs from "node:fs"`; `import { readFile } from "node:fs/promises"` |
| Importing CommonJS | allowed: `module.exports` becomes the default export |
| Requiring ESM | recent Node versions can `require()` synchronous ESM (check your version) |
| Loading `.cjs` / `.mjs` | extension overrides `"type"` |

## Named vs default exports

| | Named | Default |
|---|-------|---------|
| Count per module | many | one |
| Import name | fixed (rename with `as`) | chosen by importer |
| Refactoring/search | easy (consistent names) | names drift between files |
| Tree shaking | excellent | fine |
| Auto-import tooling | reliable | less reliable |
| Dynamic import | `mod.name` | `mod.default` |

Many style guides prefer **named exports** and reserve `default` for components/pages or single-purpose modules that frameworks expect.

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| Omitting the file extension | Fails in browsers and Node ESM | Always write `./x.js` |
| Using `import` inside `if` or functions | `SyntaxError` | Dynamic `import()` |
| Assigning to an imported binding | `TypeError` | Export a setter function or mutate an object |
| Expecting copies of exported values | Live bindings reflect changes | Know that imports are live |
| Circular imports reading uninitialized values | `ReferenceError` | Restructure shared code |
| `__dirname` / `require` in ESM | `ReferenceError` | `import.meta.dirname`, `createRequire` |
| Slow top-level `await` in widely imported modules | Delays startup | Lazy-load or make it a function |
| Forgetting `type="module"` | `Cannot use import statement outside a module` | Set it in HTML or `package.json` |
| Importing a default from a module with only named exports | `undefined` or error | Check the export list |
| Opening module pages via `file://` | CORS blocks module loading | Use a local dev server |

## Key takeaways

- ESM uses static `import`/`export`, live read-only bindings, strict mode and per-file scope
- Modules evaluate once and are shared (singleton state)
- Use dynamic `import()` for lazy or conditional loading and top-level `await` sparingly
- Include file extensions, and prefer named exports

**Next:** [CommonJS](./02_commonjs.md)
