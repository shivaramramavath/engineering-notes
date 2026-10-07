# Target, Module, and Lib

Three options decide how your TypeScript relates to the JavaScript runtime it will run on:

- **`target`**: which JavaScript *syntax* the compiler emits (and which gets rewritten for older engines).
- **`module`**: which *module format* the output uses.
- **`lib`**: which built-in *APIs* (types for `Promise`, `Array.prototype.at`, `document`, ...) the type checker knows about.

They are independent, and mismatches cause two kinds of bugs: code the runtime cannot parse, and APIs the runtime does not have. This note explains each, how to choose, and how to diagnose the mismatches.

**Prerequisites:**
- [Compiler options](./00-compiler-options.md)
- [ES modules and CommonJS](../08-modules/01-es-modules-and-commonjs.md)

---

## `target`: syntax, not APIs

`target` controls which language features the compiler leaves alone and which it rewrites.

```ts
// source
const label = user?.profile?.name ?? "anonymous";
class Counter { count = 0; }
```

```js
// target: ES2022 -> emitted essentially as written
const label = user?.profile?.name ?? "anonymous";
class Counter { count = 0; }

// target: ES2019 -> optional chaining and ?? are downleveled
const label = (_b = (_a = user === null || user === void 0 ? void 0 : user.profile) === null ... ) !== null && ... ? ... : "anonymous";
```

Key points:

- Common values: `ES2015` (ES6), `ES2016` through `ES2023` (the exact list depends on your TypeScript version), and `ESNext` ("whatever this TypeScript version supports").
- **`target` only rewrites syntax.** It does **not** add polyfills. Targeting ES5 does not make `Promise` or `Array.prototype.includes` exist on an old engine.
- A higher target produces smaller, faster, more readable output, because fewer features need helper code. Choose the **highest target your runtime fully supports**.
- Rewriting some syntax (async/await below ES2017, spread and iteration below ES2015) needs helper functions, sometimes provided by `tslib` via `importHelpers`.

### Choosing a target

| Runtime | Reasonable target |
|---|---|
| Node.js (any maintained version) | match the version, or use a community base such as `@tsconfig/node20` which sets target, lib, and module for you |
| Modern evergreen browsers | `ES2020` to `ES2022`, depending on your support policy |
| Output processed by a bundler or Babel | often `ESNext`, letting the bundler downlevel for your browserslist |
| Legacy browsers | handled by a bundler/transpiler, not by `tsc` |

When a bundler (esbuild, SWC, Babel, Vite) produces the final JavaScript and `tsc` only type-checks (`noEmit`), `target` mostly affects the default `lib` and which syntax the checker accepts.

### Class fields: `useDefineForClassFields`

For `target` ES2022 or `ESNext`, `useDefineForClassFields` defaults to `true`. Class fields are emitted using JavaScript's native *define* semantics instead of the older *assign* semantics. The difference shows up in two places:

- A field declared without an initializer (`name: string;`) is emitted as `name;`, which sets it to `undefined`. Under the old behavior no code was emitted.
- Field initializers run **before** the constructor body but in a different order relative to parameter properties and base-class constructors.

If a library (some ORMs, DI frameworks, decorators) relies on assign semantics, set `useDefineForClassFields: false`, or declare fields with `declare name: string;` when they are provided elsewhere.

## `lib`: which APIs exist in the types

`lib` selects built-in declaration files. These decide whether `Array.prototype.at`, `Object.fromEntries`, `Promise.allSettled`, or `document` type-check.

```jsonc
{
  "compilerOptions": {
    "target": "ES2022",
    "lib": ["ES2022", "DOM", "DOM.Iterable"]
  }
}
```

- If `lib` is **omitted**, a default set is derived from `target` and includes DOM types.
- If you **set** `lib`, it **replaces** the default entirely. Forget `"DOM"` and `document` and `fetch` vanish (in Node projects that is exactly what you want, and `@types/node` supplies Node's globals instead).
- Names are per year (`ES2020`) or per feature (`ES2022.Array`, `ESNext.Disposable`).
- Typical sets:

| Project | `lib` |
|---|---|
| Browser app | `["ES2022", "DOM", "DOM.Iterable"]` |
| Node program | `["ES2022"]` plus `@types/node` |
| Shared code (both) | careful: do not rely on `DOM` or Node-only globals |

**`lib` is a promise about the runtime.** Setting `lib: ["ESNext"]` on a project that runs on an older Node makes `tsc` happily accept an API that does not exist there. The failure arrives at runtime as "x is not a function". Match `lib` to what the runtime actually provides, or add a polyfill.

### The TS2550 error

```text
Property 'at' does not exist on type 'string[]'.
Do you need to change your target library?
Try changing the 'lib' compiler option to 'es2022' or later.
```

Only change `lib` if your runtime has the API (or you polyfill it). Raising `lib` removes the error without making the code work on an older engine.

## `module`: output format

`module` picks the module system emitted into your JavaScript:

| Value | Output |
|---|---|
| `commonjs` | `require` / `module.exports` |
| `esnext`, `es2015`, `es2020`, `es2022` | ES modules (`import` / `export`) |
| `node16`, `node18`, `nodenext` | CommonJS **or** ESM, chosen per file the way Node does it |
| `preserve` | keep `import`/`export` exactly as written (TS 5.4+), for bundlers |
| `amd`, `umd`, `system` | older formats for specific loaders |

Choosing between them is the topic of [ES modules and CommonJS](../08-modules/01-es-modules-and-commonjs.md), with the short version:

- Node program or library: `module: "nodenext"` (with `moduleResolution: "nodenext"`).
- App compiled by a bundler: `module: "esnext"` or `"preserve"` with `moduleResolution: "bundler"`.
- CommonJS-only legacy project: `module: "commonjs"`.

`module` also changes the **default** `moduleResolution`, which is why changing only one of them can break imports. The two should be a matching pair ([module resolution and paths](./03-module-resolution-and-paths.md)).

Some features depend on `module` and `target` together: top-level `await` needs an ES-module output and `target` ES2017 or higher, and `import.meta` needs an ES-module output.

## Related emit options

| Option | Purpose |
|---|---|
| `downlevelIteration` | with `target` ES5, iterate `Set`, `Map`, generators, and strings correctly (adds helper code) |
| `importHelpers` | import shared helpers from `tslib` instead of inlining them in each file |
| `noEmitHelpers` | omit helpers entirely (you must provide them) |
| `esModuleInterop` | emit interop helpers so default imports of CommonJS modules work |
| `isolatedModules` | require that each file can be transpiled alone, as per-file tools do |

## Putting them together

Node 20+ ESM service:

```jsonc
{
  "compilerOptions": {
    "target": "ES2022",
    "lib": ["ES2022"],
    "module": "NodeNext",
    "moduleResolution": "NodeNext"
  }
}
```

Vite or other bundled front end:

```jsonc
{
  "compilerOptions": {
    "target": "ES2022",
    "lib": ["ES2022", "DOM", "DOM.Iterable"],
    "module": "ESNext",
    "moduleResolution": "bundler",
    "noEmit": true
  }
}
```

More complete files are in [tsconfig recipes](./05-tsconfig-recipes.md).

## Common mistakes

- **Treating `target` as a polyfill switch.** It rewrites syntax only.
- **Raising `lib` to silence an error** without checking that the runtime has the API.
- **Setting `lib` and losing `DOM`** (or keeping `DOM` in a Node project and getting browser globals like `window` type-checking by accident).
- **Targeting ES5 by habit.** It forces large helper output, `downlevelIteration`, and slower code, for engines almost nobody runs.
- **Mismatched `module` and `moduleResolution`.** For example `module: "commonjs"` with `moduleResolution: "bundler"` is rejected, and `nodenext` resolution with emitted CommonJS `require` of an ESM-only package fails at runtime.
- **Emitting ESM into a package without `"type": "module"`** (or the reverse), producing "Cannot use import statement outside a module" errors.
- **Forgetting that `useDefineForClassFields` changes emitted class behavior** when `target` goes from ES2021 to ES2022.

## Debugging

- `tsc --showConfig` shows the effective `target`, `module`, `moduleResolution`, and `lib` after defaults.
- `tsc --listFiles | grep "lib\."` shows exactly which built-in declaration files were loaded.
- If something compiles but crashes at runtime with "is not a function", compare `lib` against the runtime's actual support (for example with the MDN compatibility tables or `node -p "process.versions"`).
- If it crashes with a `SyntaxError`, the emitted syntax is newer than the runtime: lower `target`, or let a transpiler handle it.
- Look at the emitted `.js` once. Seeing what your `target` produces removes a lot of guesswork.

## Quick summary

- `target` sets the emitted **syntax** level, `lib` sets which **APIs** type-check, `module` sets the **module format**. None of them polyfills anything.
- Choose the highest `target` your runtime supports, or use a `@tsconfig/*` base for your Node version.
- Setting `lib` replaces the defaults, so include `DOM` yourself in browser projects and keep it out of Node ones.
- `lib` is a claim about the runtime: do not raise it to silence errors unless the runtime has the API.
- Keep `module` and `moduleResolution` as a matching pair, and align them with `package.json` `"type"`.

**Next:** [Module resolution and paths](./03-module-resolution-and-paths.md)
