# Ambient Declarations

An **ambient declaration** tells the compiler that something exists at runtime, without defining it. You use the `declare` keyword for variables, functions, classes, enums, namespaces, and whole modules. It is how you describe globals injected by a script tag or bundler, untyped packages, and non-code imports like images and CSS.

**Prerequisites:**
- [Declaration files](./00-declaration-files.md)
- [Imports and exports](../08-modules/00-imports-and-exports.md) (the module vs script distinction matters a lot here)

---

## The `declare` forms

```ts
declare const APP_VERSION: string;
declare let counter: number;
declare var legacyGlobal: unknown;

declare function track(event: string, data?: object): void;

declare class Analytics {
  constructor(key: string);
  send(event: string): void;
}

declare enum LogLevel { Debug, Info, Error }

declare namespace Telemetry {
  function start(): void;
  const version: string;
}
```

Each one says "this is there at runtime." The compiler emits nothing for them. If the thing does not actually exist, you get a `ReferenceError` at runtime, and the type checker cannot warn you.

Declarations of this kind live in `.d.ts` files, or in a `.ts` file when it is a script (no top-level import/export). Putting them in `.d.ts` files keeps intent clear.

## Typing globals

### Values injected by a bundler or build step

```ts
// src/env.d.ts
declare const __APP_VERSION__: string;
declare const __DEV__: boolean;
```

This is the usual way to type constants replaced at build time (for example through a bundler's `define` option). The file must be included by `tsconfig` (`include: ["src"]` covers it) and must stay a **script**: no top-level `import` or `export`. Adding one turns it into a module, and `__DEV__` is then no longer global (see the next section for the right way to do that).

### Values from a `<script>` tag

```ts
// global.d.ts
declare const google: {
  maps: { Map: new (el: HTMLElement, opts: object) => unknown };
};
```

Prefer a typed `window` property if the value is attached there (see [global and module augmentation](./02-global-and-module-augmentation.md)).

### Framework client types

Tools such as Vite ship their global types as a reference you add once:

```ts
// src/vite-env.d.ts
/// <reference types="vite/client" />
```

This brings in typings for `import.meta.env` and asset imports. The same mechanism is how `@types/node` globals reach your code.

## Typing modules

### `declare module "name"`

Describes an untyped package, so imports from it are checked:

```ts
// types/legacy-chart.d.ts
declare module "legacy-chart" {
  export interface ChartOptions {
    width: number;
    height: number;
  }
  export function draw(el: HTMLElement, options: ChartOptions): void;
}
```

```ts
import { draw } from "legacy-chart";   // typed
```

If you only need the import to stop failing, a **shorthand ambient module** types everything as `any`:

```ts
declare module "legacy-chart";
```

Everything imported from it is `any`. That compiles, but you give up all checking for that package. Treat it as a temporary stopgap, and see [third-party types](./04-third-party-types.md) for better options.

### Wildcard modules for non-code files

Bundlers let you `import` images, CSS, and other assets. Tell TypeScript what the import produces:

```ts
// src/assets.d.ts
declare module "*.svg" {
  const url: string;
  export default url;
}

declare module "*.module.css" {
  const classes: Record<string, string>;
  export default classes;
}
```

```ts
import logo from "./logo.svg";            // string
import styles from "./app.module.css";    // Record<string, string>
```

Only one `*` is allowed in the pattern. This only describes the type: you still need a bundler or loader that actually handles those files at runtime.

## The big gotcha: module vs script

`declare module "x" { ... }` behaves differently depending on whether the **file itself** is a module.

```ts
// types.d.ts - a SCRIPT (no top-level import/export)
declare module "legacy-chart" {
  export function draw(): void;
}
// -> this DECLARES the module "legacy-chart"
```

```ts
// types.d.ts - now a MODULE (has an import)
import type { Something } from "./something";

declare module "legacy-chart" {
  export function draw(): void;
}
// -> this AUGMENTS an existing "legacy-chart" module
//    and errors if the module can't be resolved
```

Adding a single import changes the meaning. If you need to declare an untyped module *and* import types, put the `declare module` in its own script file, or use `import("...")` types inside the declaration instead of a top-level import:

```ts
declare module "legacy-chart" {
  export function draw(el: import("./dom-types").Target): void;
}
```

`import()` types are allowed inside declarations without turning the file into a module.

## Making sure the compiler sees them

Ambient declarations only work if the file is part of the program:

- Files matched by `include` / `files` in `tsconfig.json` are loaded.
- `typeRoots` and `types` control which **packages** under `node_modules/@types` (or custom folders) are auto-included as global declarations. Setting `"types": []` disables the automatic inclusion of every `@types/*` package, which can make `process` or `describe` suddenly disappear.
- A `.d.ts` outside `include` is silently ignored, which looks like "my declaration does nothing."

```json
{
  "compilerOptions": {
    "typeRoots": ["./node_modules/@types", "./types"],
    "types": ["node"]
  },
  "include": ["src", "types"]
}
```

## `declare enum` and `const enum`

`declare enum` describes an enum that exists at runtime. `declare const enum` describes an enum whose values were meant to be inlined at build time, and it is risky across package boundaries and with per-file transpilers. Under `isolatedModules`, ambient `const enum` use is restricted. Avoid both in new declarations, and prefer union types or `as const` objects ([enums and const objects](../01-fundamentals/05-enums-and-const-objects.md)).

## Common mistakes

- **Adding an `import` to a global declaration file,** which silently turns it into a module and un-globals everything in it.
- **Declaring something that is not actually there.** The compiler cannot check; you get a runtime error.
- **Using shorthand `declare module "x";` and forgetting about it.** The package is `any` forever, and typos go unnoticed.
- **Declaring a wildcard module but not configuring the bundler.** The types pass, then the build fails.
- **Putting the `.d.ts` outside `include`.** Nothing happens.
- **Using `var` vs `let/const` carelessly.** Only `var` and function declarations create properties on `globalThis`. A `declare const` does not appear as `window.X` in the type system.
- **Duplicating a declaration that a package already provides** (for example redeclaring `process`). Install the `@types` package instead.

## Debugging

- Add a deliberate error to the `.d.ts` (for example a stray token). If no error appears, the file is not in the program. Fix `include`/`files`.
- `tsc --listFiles | grep my-declaration` confirms the file is loaded.
- "Cannot find name 'X'": the global declaration is not loaded, or the file became a module.
- "Invalid module name in augmentation, module 'x' cannot be found": a `declare module` in a *module* file is treated as augmentation of a module that does not resolve. Move it to a script file.
- Editor works but `tsc` fails (or vice versa): check that both use the same `tsconfig` and TypeScript version.

## Quick summary

- `declare` states that something exists at runtime without defining it. It emits no code, and nothing verifies it.
- Use `declare const/function/class/namespace` for globals, `declare module "x"` for untyped packages, and `declare module "*.ext"` for assets.
- A file with a top-level `import`/`export` is a module. In it, `declare module "x"` means augmentation, not declaration.
- The declaration must be inside the compilation (`include`, `typeRoots`, `types`) to take effect.
- Shorthand `declare module "x";` is `any` and is a stopgap.

**Next:** [Global and module augmentation](./02-global-and-module-augmentation.md)