# Barrel Files and Module Organization

A **barrel file** is a module (usually `index.ts`) that re-exports things from other modules so consumers can import from one place. Barrels are easy to add and easy to regret. This note covers when they help, what they cost, and how to organize modules so a codebase stays navigable as it grows.

**Prerequisites:**
- [Imports and exports](./00-imports-and-exports.md) (especially re-exports)
- [ES modules and CommonJS](./01-es-modules-and-commonjs.md)

---

## What a barrel looks like

```text
src/users/
├── index.ts          <- the barrel
├── user.model.ts
├── user.service.ts
└── user.repository.ts
```

```ts
// src/users/index.ts
export * from "./user.model";
export { UserService } from "./user.service";
export type { UserRepository } from "./user.repository";
```

```ts
// elsewhere
import { UserService, type User } from "../users";   // instead of three deep imports
```

Under Node ESM resolution, a directory import like `"../users"` does not resolve to `index.ts` automatically; you must write `"../users/index.js"`. Directory imports work with `moduleResolution: "bundler"` and in CommonJS. See [ES modules and CommonJS](./01-es-modules-and-commonjs.md).

## Why people use them

- **A single, short import path** for a feature or package.
- **A public API boundary:** only what the barrel exports is "public"; everything else in the folder is an implementation detail you can change freely.
- **Easier refactoring:** moving a file inside the folder does not change importers.

The boundary is the real benefit. Shorter import lines alone are not worth the costs below.

## What they cost

**1. Circular dependencies.** The most common real-world problem. A file inside a feature imports from its own barrel, the barrel imports that file, and now there is a cycle:

```ts
// users/user.service.ts
import { formatName } from "./index";   // goes through the barrel: cycle
```

Inside a folder, import siblings directly (`"./format"`), never through the barrel. Only code outside the folder should use the barrel. Cycles can produce `undefined` values at runtime that the type checker does not catch (see [imports and exports](./00-imports-and-exports.md)).

**2. Tree-shaking and bundle size.** Bundlers can usually drop unused re-exports from ES modules, but it depends on the bundler, the output format, and whether modules have side effects. A barrel over modules with top-level side effects forces the bundler to keep everything. If a package should be shaken, mark it in `package.json`:

```json
{
  "sideEffects": false
}
```

Only set this if it is true. Modules that register things or patch globals on import will break.

**3. Slower tooling.** Importing one symbol through a large barrel makes the compiler, test runner, and bundler load the whole graph behind it. Jest and Vitest suites often slow down noticeably because every test that touches the barrel loads all its modules. A barrel at the root of a large `src/` is the worst case.

**4. Name collisions.** With `export *` from multiple files, a duplicate name makes that name ambiguous and it is dropped, producing a confusing "has no exported member" error at the import site.

**5. Hidden coupling.** Autocomplete suggests the barrel import everywhere, so unrelated code starts depending on the whole feature instead of the one function it needs.

## Practical guidance

- **Use a barrel at feature or package boundaries.** One `index.ts` per feature folder, curated to the public API. Not one per directory.
- **Prefer explicit exports to `export *`** for the public surface. Explicit lists are reviewable and cannot leak new internals by accident:

```ts
// index.ts - deliberate public API
export { UserService } from "./user.service";
export type { User, CreateUserInput } from "./user.model";
// user.repository.ts is internal: not exported
```

- **Inside a feature, import files directly.** Barrels are for consumers.
- **Use `export type` for type-only re-exports** so `isolatedModules` and `verbatimModuleSyntax` are satisfied and the barrel stays cheap at runtime.
- **Do not put logic in barrels.** An `index.ts` that only re-exports is easy to reason about.
- **Avoid barrels in application root code that bundlers cannot shake,** or in test-heavy packages where load time matters. Direct imports cost nothing.

## Organizing modules

Two common shapes, which can be combined:

```text
By layer (type-based)            By feature (domain-based)
src/                             src/
├── controllers/                 ├── users/
├── services/                    │   ├── user.controller.ts
├── repositories/                │   ├── user.service.ts
└── models/                      │   └── index.ts
                                 └── orders/
                                     └── ...
```

- **By layer** is simple at the start. As the app grows, one change touches files in every folder.
- **By feature** keeps related code together and gives natural barrel boundaries. Most medium and large projects end up here, often with a small shared folder for cross-cutting code.

Rules that keep either shape healthy:

- **Dependencies point one way.** Features may import from `shared/`, but `shared/` must not import from a feature. Features should not reach into each other's internals.
- **One module, one reason to change.** Split a file when it holds unrelated concerns, not just because it is long.
- **Co-locate** tests and types with the code they describe.
- **Avoid deep relative paths** like `../../../shared/utils`. Path aliases (`@/shared/utils`) fix this, but they must be configured in the compiler **and** the runtime or bundler. See [module resolution and paths](../13-compiler-and-tsconfig/03-module-resolution-and-paths.md).
- **For packages, control the public API with `exports`** in `package.json` so consumers cannot deep-import internals. See [publishing packages](../21-production-tooling/03-publishing-packages.md).

More on layout in [project structure](../24-best-practices/02-project-structure.md), and on splitting a repo into packages in [monorepos](../21-production-tooling/02-monorepos.md).

## Detecting and preventing problems

```bash
# find circular dependencies (madge)
npx madge --circular --extensions ts src/
```

- ESLint: the `import/no-cycle` rule (eslint-plugin-import) flags cycles. `no-restricted-imports` can forbid importing a feature's internals or forbid imports through a particular barrel.
- Some projects enforce boundaries with a dependency tool such as dependency-cruiser or Nx module boundary rules.

## Common mistakes

- **A barrel in every folder.** It adds indirection with no boundary benefit.
- **Importing a sibling through its own barrel.** This creates cycles.
- **`export *` everywhere.** Internals leak, and name collisions are silent.
- **Re-exporting a type without `export type`** under `isolatedModules`.
- **Assuming a barrel is free at runtime.** It is free only if the bundler and test runner can skip the unused parts.
- **Adding `sideEffects: false` to a package that has side effects.** Code is dropped and things silently stop working.

## Debugging

- A value is `undefined` at runtime but typed correctly: look for a cycle through a barrel. Run `madge --circular`.
- Slow type-checking or tests after adding a barrel: switch hot-path imports to the direct file path and measure again. See [type-checking performance](../22-performance/00-type-checking-performance.md).
- Bundle larger than expected: check the bundler's analyzer output for modules pulled in through a barrel, and check `sideEffects` flags.
- "Module has no exported member 'X'" after adding a new `export *`: look for a duplicate name in the re-exported modules.

## Quick summary

- A barrel re-exports a module's public API from one `index.ts`. Its real value is the boundary, not the shorter import path.
- Costs: circular dependencies, bundle and test-load overhead, silent name collisions, hidden coupling.
- Use barrels at feature or package boundaries, with explicit exports. Inside a feature, import files directly.
- Organize by feature when the project grows, keep dependencies pointing one way, and automate cycle detection.

**Next:** [Namespaces](./03-namespaces.md)