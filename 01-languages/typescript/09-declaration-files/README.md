# 09 - Declaration Files

How TypeScript learns about code it cannot see the source of: JavaScript libraries, globals injected at runtime, build-time constants, asset imports, and the typed API that your own library exposes to others. This section covers writing, consuming, extending, and shipping `.d.ts` files.

The recurring theme: a declaration file is a **promise about runtime**. The compiler does not verify it, so every technique here is only as good as its match to what actually exists at runtime.

## Prerequisites

- [08 Modules](./../08-modules/README.md): especially [imports and exports](../08-modules/00-imports-and-exports.md) and [namespaces](../08-modules/03-namespaces.md)
- [04 Objects and Interfaces](../04-objects-and-interfaces/README.md): interfaces, since merging is central here
- [14 Type System Internals: type erasure](../14-type-system-internals/00-type-erasure-and-runtime.md)

## Notes in this section

| # | Note | Covers |
|---|---|---|
| 00 | [Declaration files](./00-declaration-files.md) | What `.d.ts` files are, generating them, `types` and `exports` in `package.json`, hand-written declarations |
| 01 | [Ambient declarations](./01-ambient-declarations.md) | `declare` for globals, modules and assets, wildcard modules, the module-vs-script gotcha |
| 02 | [Global and module augmentation](./02-global-and-module-augmentation.md) | `declare global`, `declare module` augmentation, Express `Request`, `Window`, `ProcessEnv` |
| 03 | [Declaration merging](./03-declaration-merging.md) | What merges with what, overload ordering, namespace merging, interface vs type |
| 04 | [Third-party types](./04-third-party-types.md) | How types are found, `@types`, `types`/`typeRoots`, missing and wrong types, shipping types |

Read 00, 01, and 02 in order. 03 explains the mechanism behind 02 and can be read alongside it. 04 is the practical guide you will return to most often.

## What do I need?

| Situation | Go to |
|---|---|
| "Could not find a declaration file for module 'x'" | [04](./04-third-party-types.md) |
| `import logo from "./logo.svg"` fails to type-check | [01](./01-ambient-declarations.md) (wildcard modules) |
| A build-time constant like `__DEV__` is undefined to TypeScript | [01](./01-ambient-declarations.md) |
| `Property 'user' does not exist on type 'Request'` | [02](./02-global-and-module-augmentation.md) |
| `Property 'myApp' does not exist on type 'Window'` | [02](./02-global-and-module-augmentation.md) |
| `process.env.X` is `string \| undefined` and I want it typed | [02](./02-global-and-module-augmentation.md), with the runtime caveat |
| "Duplicate identifier" or "Subsequent property declarations must have the same type" | [03](./03-declaration-merging.md) |
| A function that also has properties | [03](./03-declaration-merging.md) (namespace + function) |
| I am publishing a library and want consumers to get types | [00](./00-declaration-files.md) |
| My `declare module` or augmentation does nothing | [01](./01-ambient-declarations.md) and [02](./02-global-and-module-augmentation.md) (file is a module vs script, or not in `include`) |

## Ideas that recur across the section

- **Module vs script decides everything.** A `.d.ts` with a top-level `import`/`export` is a module. Without one, it is global. In a script, `declare module "x"` *declares* a module. In a module, it *augments* one.
- **Declarations are unchecked promises.** They emit no code, and a wrong one fails at runtime, not at compile time.
- **The file must be in the program.** `include`, `files`, `typeRoots`, and `types` decide whether a declaration is ever seen. A declaration that silently does nothing is usually not loaded.
- **Interfaces are open, type aliases are closed.** Augmentation works only because interfaces merge.
- **Prefer a small accurate declaration over a broad inaccurate one.** Shorthand `declare module "x";` makes a package `any`.

## Useful commands

```bash
tsc --listFiles                      # every file in the program, including .d.ts
tsc --traceResolution                # how each import was resolved
tsc --showConfig                     # the effective tsconfig
tsc --declaration --emitDeclarationOnly --outDir /tmp/types   # preview generated declarations
```

## Related sections

- [13 Compiler and tsconfig](../13-compiler-and-tsconfig/README.md): `declaration`, `types`, `typeRoots`, `skipLibCheck`, `moduleResolution`
- [15 Runtime Validation](../15-runtime-validation/README.md): checking data that types alone only assert
- [20 Node.js Backend: config and environment](../20-nodejs-backend/01-config-and-environment.md)
- [21 Production Tooling: publishing packages](../21-production-tooling/03-publishing-packages.md)
- [23 Security: unsafe types and assertions](../23-security/00-unsafe-types-and-assertions.md)

## Next

[10 Advanced Types](../10-advanced-types/README.md)