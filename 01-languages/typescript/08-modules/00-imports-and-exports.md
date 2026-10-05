# Imports and Exports

A TypeScript file becomes a **module** the moment it has a top-level `import` or `export`. Modules have their own scope; everything inside is private unless exported. This note covers the syntax you use every day, the type-only variants that are specific to TypeScript, and the rules the compiler enforces around them.

**Prerequisites:**
- [Type aliases](../01-fundamentals/04-type-aliases.md)
- [Interfaces](../04-objects-and-interfaces/00-interfaces.md)

---

## Module vs script

```ts
// a.ts - no import/export: this is a SCRIPT, its declarations are global
const version = "1.0";

// b.ts - has an export: this is a MODULE, its declarations are local
export const name = "b";
```

If two script files both declare `const version`, TypeScript reports a duplicate declaration, because they share the global scope. If a file has no imports or exports but you want it treated as a module, add an empty export:

```ts
export {};
```

Use this when you see "Cannot redeclare block-scoped variable" across unrelated files.

## Named exports and imports

```ts
// math.ts
export const PI = 3.14159;
export function add(a: number, b: number): number {
  return a + b;
}
export interface Point {
  x: number;
  y: number;
}
```

```ts
// main.ts
import { PI, add, type Point } from "./math";
import { add as sum } from "./math";   // rename on import
import * as math from "./math";         // namespace import: math.PI, math.add

const p: Point = { x: 1, y: 2 };
```

You can also export after declaration, and rename on export:

```ts
function internalName() {}
const other = 1;
export { internalName as publicName, other };
```

## Default exports

```ts
// logger.ts
export default class Logger {}

// main.ts
import Logger from "./logger";       // any local name works
import MyLog from "./logger";        // same thing
```

A module has at most one default export. Named exports are usually the better default choice in TypeScript:

- The importer must use the exported name (or rename explicitly), so refactors and "find all references" stay reliable.
- Auto-import and rename tooling work better with names.
- Named exports make it harder to import the same thing under five different names across a codebase.

Default exports are still the convention for some frameworks (for example Next.js pages and many config files), so use them where the tool expects them.

## Type-only imports and exports

Types are erased at compile time, but the compiler needs to know whether an import is a type or a value to decide what to keep. `import type` says "this exists only for the type system and must not appear in the output":

```ts
import type { Point } from "./math";          // whole statement erased
import { add, type Point as P } from "./math"; // inline: only `P` is erased

export type { Point };                         // type-only export
export type { Point } from "./math";           // type-only re-export
```

Why it matters:

- Under `verbatimModuleSyntax` (TS 5.0+) the compiler emits imports exactly as written, except those marked `type`. Importing a type without the modifier is an error. This makes emitted code predictable and is the recommended setting for new projects. It replaces the older `importsNotUsedAsValues` and `preserveValueImports` flags.
- Under `isolatedModules` (required by Babel, esbuild, SWC and most bundlers, which transpile files one at a time), the compiler cannot look at another file to decide whether `export { Point } from "./math"` is a type. Re-exporting a type needs `export type`.
- Type-only imports also help break accidental runtime circular dependencies, since they vanish from the output.

Classes are both a value and a type, so `import type { Logger }` lets you annotate with it but not call `new Logger()`.

## Re-exports

```ts
export { add } from "./math";               // re-export a name
export { add as sum } from "./math";        // re-export under another name
export * from "./math";                     // re-export everything (not `default`)
export * as math from "./math";             // re-export as a namespace object
export type * from "./types";               // type-only star re-export (TS 5.0+)
```

`export *` does not re-export the default export. If two `export *` sources export the same name, that name becomes ambiguous and is silently excluded, so you get a confusing "has no exported member" error at the import site. Re-exports are the basis of barrel files (see [barrel files](./02-barrel-files-and-module-organization.md)).

## Side-effect imports

```ts
import "./polyfills";
import "reflect-metadata";
```

No bindings are imported. The module is just executed. TypeScript never removes these, even under `verbatimModuleSyntax`.

## Dynamic import

`import()` loads a module at runtime and returns a promise. Use it for lazy loading and code splitting.

```ts
async function loadMath() {
  const { add } = await import("./math");
  return add(1, 2);
}

type MathModule = typeof import("./math"); // the module's type, no runtime import
```

Whether `import()` is kept as is or turned into `require` depends on your `module` setting (see [ES modules and CommonJS](./01-es-modules-and-commonjs.md)).

## How imports behave at runtime

- **Imports are live bindings (ESM).** An imported name reflects later changes to the exported variable in its home module. Importers cannot assign to it.
- **Imports are hoisted.** All static imports are resolved and executed before the module body, no matter where they appear.
- **Modules run once.** The first import executes the module; later imports reuse the same instance, which is why module-level state works as a singleton.
- **Circular imports** work only if neither side uses the other's value during module initialization. Using it too early gives `undefined` or a `ReferenceError` at runtime, which TypeScript cannot catch.

## Common mistakes

- **Importing a type without `type` under `isolatedModules` or `verbatimModuleSyntax`.** Use `import type` or the inline `type` modifier.
- **Assuming `export *` includes the default export.** It does not.
- **Mixing default and named exports for the same thing**, then importing the wrong form. Pick one style per project.
- **Relying on side effects of an import that TypeScript elided.** A normal import used only as a type is removed; use `import "./x"` if you need the side effect.
- **Importing from a path alias that only exists in `tsconfig`.** The compiler resolves it but the runtime or bundler may not. See [module resolution and paths](../13-compiler-and-tsconfig/03-module-resolution-and-paths.md).

## Debugging

- "Module has no exported member 'X'": check the spelling, whether it is a default export, and whether `export *` conflicts hide it.
- "Cannot find module './x'": check the path, the file extension rules for your `moduleResolution`, and that the file is included by `tsconfig`.
- "'X' is a type and must be imported using a type-only import when 'verbatimModuleSyntax' is enabled": add `type`.
- `undefined` at runtime for something that type-checks: suspect a circular import and move the shared code to a third module.
- Run `tsc --traceResolution` to see exactly where the compiler looks for a module.

## Quick summary

- A file with a top-level `import` or `export` is a module; otherwise it is a global script.
- Prefer named exports. Use default exports only where a tool expects them.
- Use `import type` / `export type` for types. Enable `verbatimModuleSyntax` in new projects.
- `export *` skips the default export and drops conflicting names.
- Static imports are hoisted, live, and run once; dynamic `import()` loads lazily.

**Next:** [ES Modules and CommonJS](./01-es-modules-and-commonjs.md)