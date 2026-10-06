# 08 - Modules

How TypeScript organizes code across files: the `import`/`export` syntax, the two JavaScript module systems it has to target, how to structure modules in a growing codebase, and the older `namespace` feature. Most real-world module problems are not syntax errors. They are mismatches between `package.json`, file extensions, and `tsconfig`, so this section pays particular attention to configuration.

## Prerequisites

- [01 Fundamentals](../01-fundamentals/README.md)
- [04 Objects and Interfaces](../04-objects-and-interfaces/README.md)
- [00 Setup](../00-setup/README.md): a working project with `tsc`

## Notes in this section

| # | Note | Covers |
|---|---|---|
| 00 | [Imports and exports](./00-imports-and-exports.md) | Named and default exports, type-only imports, re-exports, dynamic `import()`, `verbatimModuleSyntax` |
| 01 | [ES modules and CommonJS](./01-es-modules-and-commonjs.md) | `module` vs `moduleResolution`, Node's per-file rules, `esModuleInterop`, `.js` extensions, common runtime errors |
| 02 | [Barrel files and module organization](./02-barrel-files-and-module-organization.md) | `index.ts` barrels, circular dependencies, feature vs layer layout, enforcing boundaries |
| 03 | [Namespaces](./03-namespaces.md) | `namespace`, `declare namespace`, merging, why modules replaced them |

Read 00 and 01 in order. 02 and 03 can be read independently afterwards.

## Quick decisions

| Question | Answer |
|---|---|
| Named or default exports? | Named, unless a framework expects default |
| Importing a type? | `import type { X }` or `import { type X }` |
| Node app, what do I set? | `"type": "module"`, `module` and `moduleResolution` both `nodenext`, `.js` in relative imports |
| Bundled frontend? | `module: esnext` (or `preserve`), `moduleResolution: bundler` |
| Default import of a CJS package gives `undefined`? | Enable `esModuleInterop` |
| Should I add a barrel here? | Only at a feature or package boundary, with explicit exports |
| Should I use a `namespace`? | Not in new code. Only for `.d.ts` typings and merging |

## Ideas that recur across the section

- **Types are erased; imports are not always.** Whether an import survives into the output depends on whether it is used as a value, and on `verbatimModuleSyntax` / `isolatedModules`. Marking type imports with `type` makes this explicit.
- **`module` is output, `moduleResolution` is lookup.** Set them as a matching pair.
- **Node decides per file.** The nearest `package.json` `"type"` field and the file extension determine ESM vs CommonJS.
- **Runtime failures are invisible to the type checker.** Cycles, missing extensions, and ESM/CJS mismatches all type-check and then crash.

## Related sections

- [09 Declaration Files](../09-declaration-files/README.md): `declare namespace`, `export =`, and module augmentation
- [13 Compiler and tsconfig](../13-compiler-and-tsconfig/README.md): `module`, `moduleResolution`, `paths`, `verbatimModuleSyntax`
- [14 Type System Internals: type erasure](../14-type-system-internals/00-type-erasure-and-runtime.md)
- [21 Production Tooling: publishing packages](../21-production-tooling/03-publishing-packages.md) and [monorepos](../21-production-tooling/02-monorepos.md)
- [24 Best Practices: project structure](../24-best-practices/02-project-structure.md)

## Next

[09 Declaration Files](../09-declaration-files/README.md)