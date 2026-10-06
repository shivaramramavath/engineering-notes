# Namespaces

A `namespace` groups related declarations under one name. They predate ES modules and were TypeScript's original answer to code organization. Today ES modules replace them for application code. You still need to read them, because they appear in older codebases, in `.d.ts` files, and in declaration merging.

**Prerequisites:**
- [Imports and exports](./00-imports-and-exports.md)
- [Interfaces](../04-objects-and-interfaces/00-interfaces.md)

---

## Syntax

```ts
namespace Validation {
  export interface StringValidator {
    isValid(s: string): boolean;
  }

  const lettersOnly = /^[A-Za-z]+$/;   // private: not exported

  export class LettersValidator implements StringValidator {
    isValid(s: string) {
      return lettersOnly.test(s);
    }
  }
}

const v = new Validation.LettersValidator();
v.isValid("abc"); // true
```

- Only members marked `export` are visible outside the namespace.
- Access members with dot notation: `Validation.LettersValidator`.
- The older spelling `module Validation { ... }` means the same thing. Always write `namespace`.

## What it compiles to

A namespace with runtime values becomes an object built by an immediately-invoked function:

```js
var Validation;
(function (Validation) {
  const lettersOnly = /^[A-Za-z]+$/;
  class LettersValidator { isValid(s) { return lettersOnly.test(s); } }
  Validation.LettersValidator = LettersValidator;
})(Validation || (Validation = {}));
```

A namespace that contains **only types** (interfaces, type aliases) emits nothing. This is called a non-instantiated namespace.

## Nesting, merging, and aliases

Namespaces can nest, and multiple blocks with the same name **merge** into one. This is how a namespace can be spread across files:

```ts
namespace Shapes {
  export namespace Polygons {
    export class Triangle {}
  }
}

namespace Shapes {                 // merges with the block above
  export class Circle {}
}

import Tri = Shapes.Polygons.Triangle;   // alias; unrelated to ES `import`
```

## Where namespaces still matter

### 1. Declaration files and global typings

In `.d.ts` files, `declare namespace` describes a global object or a library that attaches itself to the global scope:

```ts
// global.d.ts
declare namespace MyLib {
  function init(options: { debug: boolean }): void;
  const version: string;
}
```

UMD libraries combine this with `export as namespace`:

```ts
export as namespace MyLib;   // makes MyLib available as a global in scripts
export function init(): void;
```

See [ambient declarations](../09-declaration-files/01-ambient-declarations.md) and [global and module augmentation](../09-declaration-files/02-global-and-module-augmentation.md).

### 2. Declaration merging

A namespace can merge with a function, class, or enum of the same name to add static-like members:

```ts
function buildLabel(name: string) {
  return buildLabel.prefix + name;
}
namespace buildLabel {
  export let prefix = "Hello, ";
}

buildLabel("Asha"); // "Hello, Asha"
```

This is the typical way to type a function that also has properties, and it is common in library typings. See [declaration merging](../09-declaration-files/03-declaration-merging.md).

### 3. Legacy code

Older projects used `/// <reference path="..." />` plus `namespace` blocks across many files, often concatenated with `outFile`. If you maintain one, migrating to ES modules is usually worth it.

## Namespaces vs modules

| | Namespace | ES module |
|---|---|---|
| Scope | global object (or nested object) | per file |
| Dependencies | implicit, via load order or `/// <reference>` | explicit `import` |
| Tree-shaking | poor (one object) | good |
| Works with Babel, esbuild, SWC per-file transpilation | restricted | yes |
| Recommended for new code | no | yes |

The key difference: a module is its own scope and declares its dependencies, which tools can analyze. A namespace relies on being loaded in the right order and cannot be analyzed as well.

## Tooling caveats

Namespaces with runtime code are one of the few TypeScript features that generate code in a way that is not a plain type-stripping step. As a result:

- **Per-file transpilers** cannot always handle them. A namespace that merges across files, or `const enum` style cross-file usage, may fail under `isolatedModules`. Check your transpiler's documentation.
- **Node's built-in TypeScript type-stripping** and the compiler flag `erasableSyntaxOnly` (TS 5.8+) reject namespaces that contain runtime code, along with enums and parameter properties. Type-only namespaces and `declare namespace` are fine.
- **ESLint:** `@typescript-eslint/no-namespace` flags them by default. It allows `declare namespace` in declaration files.

If you want namespace-like grouping without these limits, use a module and a namespace import:

```ts
// validation.ts
export interface StringValidator { isValid(s: string): boolean }
export class LettersValidator implements StringValidator { /* ... */ }

// main.ts
import * as Validation from "./validation";
new Validation.LettersValidator();
```

Same call-site syntax, but real modules underneath.

## Common mistakes

- **Using namespaces for new application code.** Use modules.
- **Forgetting `export` on a member** and getting "Property 'X' does not exist on type 'typeof Ns'".
- **Mixing namespaces and top-level `import`/`export` in the same file** and expecting a global namespace. A file with a top-level `import` or `export` is a module, so its namespaces are local to it. For globals, use a script file (no top-level imports/exports) or `declare global`.
- **Confusing `import X = Ns.Y` (an alias) with an ES import.** It only creates a local name for an existing namespace member.
- **Expecting `export namespace` in a module to be tree-shakable.** It compiles to an object and is kept as a whole.

## Debugging

- If a merged namespace member is missing, check that the merged declarations are in the same scope and that the function or class is declared **before** the namespace that merges with it.
- If a type-only namespace unexpectedly appears in output, it contains a value (a `const`, class, or function).
- If a transpiler or `erasableSyntaxOnly` rejects a namespace, the fix is to convert it to a module, or to `declare namespace` if it is only a type description.

## Quick summary

- A `namespace` groups declarations into an object (or into nothing, if it only holds types). Only `export`ed members are visible outside it.
- ES modules replace namespaces for all new code: explicit dependencies, per-file scope, better tooling.
- Namespaces remain relevant in `.d.ts` files (`declare namespace`, `export as namespace`), in merging with functions, classes, and enums, and in legacy code.
- Namespaces with runtime code do not work with some per-file or type-stripping toolchains.

**Next:** [Declaration files](../09-declaration-files/README.md)