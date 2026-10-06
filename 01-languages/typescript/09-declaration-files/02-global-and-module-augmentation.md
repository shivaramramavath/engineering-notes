# Global and Module Augmentation

**Augmentation** adds to a type that is declared somewhere else: a global like `Window`, a built-in like `Array`, or an interface exported by a library. You do not edit the original. You write a declaration that *merges* into it. It is the standard fix for "Property 'user' does not exist on type 'Request'" and "Property 'myApp' does not exist on type 'Window'".

**Prerequisites:**
- [Ambient declarations](./01-ambient-declarations.md) (especially the module vs script gotcha)
- [Interfaces](../04-objects-and-interfaces/00-interfaces.md)
- [Declaration merging](./03-declaration-merging.md) (augmentation works because interfaces merge; read it right after this note if the mechanism is unfamiliar)

---

## Two forms

| | Global augmentation | Module augmentation |
|---|---|---|
| Target | the global scope (`Window`, `Array`, `process.env`, ...) | a specific module's exports |
| Syntax | `declare global { ... }` | `declare module "module-name" { ... }` |
| Required file type | a **module** (has top-level `import`/`export`) | a **module** (has top-level `import`/`export`) |

In both cases the file must be a module. In a script file, the same text means something different (see below).

## Global augmentation

```ts
// src/types/global.d.ts
export {};   // makes this file a module

declare global {
  interface Window {
    myApp: { version: string; debug: boolean };
  }
}
```

```ts
window.myApp.version;   // string
```

`export {}` is what makes this work. `declare global` is only allowed inside a module. In a script file you would just write `interface Window { ... }` at the top level, and it would merge with the global `Window` directly. But that stops working the moment someone adds an import to the file.

You are only describing the type. Something at runtime must set `window.myApp`, or reading it fails.

### Adding to built-in types

```ts
export {};

declare global {
  interface Array<T> {
    last(): T | undefined;
  }
}

// runtime implementation, elsewhere
Array.prototype.last = function () {
  return this[this.length - 1];
};
```

The declaration and the implementation are separate. If you forget the implementation, TypeScript is satisfied and the call throws at runtime. Extending built-in prototypes is also generally discouraged, since it can clash with other libraries and future language features. Prefer a standalone function.

### Environment variables

```ts
export {};

declare global {
  namespace NodeJS {
    interface ProcessEnv {
      DATABASE_URL: string;
      NODE_ENV: "development" | "production" | "test";
    }
  }
}
```

Now `process.env.DATABASE_URL` is `string` instead of `string | undefined`. This is a common pattern, but be aware that it **asserts** the variable exists. Nothing checks it at runtime. Validate the real environment at startup and treat the augmentation as documentation. See [config and environment](../20-nodejs-backend/01-config-and-environment.md) and [trust boundaries](../15-runtime-validation/00-trust-boundaries.md).

## Module augmentation

Use it to add to types exported by a library. The classic case is Express, where middleware attaches `req.user`:

```ts
// src/types/express.d.ts
import type { User } from "../models/user";

declare module "express-serve-static-core" {
  interface Request {
    user?: User;
  }
}
```

Points that decide whether this works:

- **Augment the module that actually declares the interface.** Express's `Request` interface is declared in `express-serve-static-core`, and `express` re-exports it. Augmenting `"express"` directly can fail to merge because that module only re-exports. When an augmentation seems to be ignored, look at where the interface is *defined*.
- The file has a top-level `import`, so it is a module and `declare module` means augmentation. Without it, you would be (re)declaring the module and replacing its types.
- The module specifier must resolve, or you get "Invalid module name in augmentation".

### Another example: a library with extensible interfaces

Many libraries expose interfaces designed to be augmented, for theming, plugins, or route maps:

```ts
import "some-ui-lib";

declare module "some-ui-lib" {
  interface Theme {
    brandColor: string;
  }
}
```

The `import "some-ui-lib"` form (no bindings) is a common way to make the file a module without importing anything. Check the library's documentation for the interface names it expects you to augment.

## What augmentation can and cannot do

- **Can** add new members to existing **interfaces**, **namespaces**, and classes (through the interface merge). Merging follows the [declaration merging](./03-declaration-merging.md) rules.
- **Can** add new overloads to existing function members.
- **Cannot** add new top-level declarations to the module. Only patch what exists.
- **Cannot** augment default exports. Only named exports can be patched.
- **Cannot** change or remove an existing member. Redeclaring a property with a *different type* is an error ("Subsequent property declarations must have the same type").
- **Cannot** merge with **type aliases**. Only interfaces merge. If the library declares a `type`, you cannot extend it this way.

## Where to put the file

- Name it `*.d.ts` and keep it in a folder that `tsconfig` includes (`include: ["src"]` or a dedicated `types/` folder).
- If a `.d.ts` is outside `include`, nothing happens.
- When building a library, remember that `tsc` does not copy hand-written `.d.ts` files into `dist`. Augmentations that consumers need must ship with the package.

```json
{
  "include": ["src", "types"]
}
```

## Common mistakes

- **Forgetting `export {}` or an import,** so `declare global` errors ("Augmentations for the global scope can only be directly nested in external modules or ambient module declarations") or the augmentation replaces rather than merges.
- **Augmenting the wrong module,** typically a re-exporting entry point instead of the declaring module.
- **Augmenting a `type` alias.** Only interfaces merge.
- **Assuming the runtime matches.** `req.user` is typed, but a middleware must set it. Make it optional (`user?: User`) unless you are certain.
- **Placing the file outside the compilation.** Nothing is wrong with the syntax, and nothing happens.
- **Using augmentation to silence errors,** such as marking everything optional or `any`, when a small wrapper type or helper would be more honest.

## Debugging

- Hover the symbol you augmented. If the new member is missing, the augmentation file is not loaded (`tsc --listFiles`), or you augmented the wrong module.
- "Duplicate identifier" or "Subsequent property declarations must have the same type": your new declaration conflicts with an existing member. Rename it or match the type.
- "Cannot augment module 'x' because it resolves to a non-module entity": the target is exported with `export =` (CommonJS style) as a non-module value. Augmenting it works differently, or not at all. Check how the library declares its exports.
- Works in the editor but not in the build (or the reverse): the build `tsconfig` may exclude the file. Check `tsc --showConfig`.
- Tests fail to see augmentations: test runners such as ts-jest or Vitest use their own `tsconfig`. Make sure it includes the augmentation file.

## Quick summary

- Augmentation adds to existing types without editing them, via interface merging.
- Use `declare global { ... }` for globals and `declare module "x" { ... }` for libraries. The file **must be a module** (`export {}` or any top-level import).
- Augment the module that *declares* the interface, not one that only re-exports it.
- You can add members to interfaces and namespaces. You cannot add new top-level exports, change existing members, augment default exports, or merge into `type` aliases.
- The augmentation is types only. Runtime behavior must be provided separately.

**Next:** [Declaration merging](./03-declaration-merging.md)