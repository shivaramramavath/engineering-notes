# Module Resolution and Paths

**Module resolution** is how the compiler turns an import specifier like `"./utils"` or `"lodash"` into an actual file. Get it wrong and you see "Cannot find module", imports that work in the editor but fail at runtime, or code that compiles and then breaks in Node. This note covers the resolution strategies, how bare package imports are found, and how `paths` aliases, `baseUrl`, and `package.json` `imports` fit in.

**Prerequisites:**
- [Target, module, and lib](./02-target-module-and-lib.md)
- [ES modules and CommonJS](../08-modules/01-es-modules-and-commonjs.md)
- [Imports and exports](../08-modules/00-imports-and-exports.md)

---

## The `moduleResolution` strategies

| Value | Mimics | Use for |
|---|---|---|
| `nodenext` (and `node16`) | Node.js, including ESM rules and `package.json` `exports` | Node apps and libraries |
| `bundler` | bundlers (Vite, webpack, esbuild) | code a bundler processes |
| `node10` (also written `node`) | old CommonJS-only Node resolution | legacy projects |
| `classic` | pre-Node TypeScript behavior | almost never |

It defaults from `module`, so set the two together:

| `module` | `moduleResolution` |
|---|---|
| `nodenext` | `nodenext` |
| `esnext` / `es2022` / `preserve` | `bundler` |
| `commonjs` | `node10` (legacy) or `bundler` |

### How they differ in practice

| | `node10` | `nodenext` | `bundler` |
|---|---|---|---|
| Reads `package.json` `exports` | no | yes | yes |
| Relative imports need extensions | no | **yes**, in ESM files (`./a.js`) | no |
| Directory imports (`./utils` to `utils/index.ts`) | yes | ESM: no. CJS: yes | yes |
| Honors `import` / `require` conditions | no | per file format | `import` |

- `nodenext` is strict because Node's real ESM loader is strict. TypeScript is telling you what will fail at runtime.
- `bundler` is permissive because bundlers are. It is **wrong for code run directly by Node**: it will happily accept extensionless imports that Node's ESM loader rejects.

## Relative imports

For `import { x } from "./utils"`, the compiler tries, in order (for non-ESM resolution): `./utils.ts`, `./utils.tsx`, `./utils.d.ts`, then a directory `./utils/package.json` (`types`), then `./utils/index.ts`, `index.tsx`, `index.d.ts`.

With `nodenext` in an ES module, you write the **output** extension and TypeScript maps it back:

```ts
import { x } from "./utils.js";   // resolves to utils.ts at compile time
```

From TS 5.7, `rewriteRelativeImportExtensions` lets you write `.ts` in source and rewrites to `.js` on emit. `allowImportingTsExtensions` permits `.ts` extensions when you do not emit JavaScript (`noEmit`), for bundler workflows.

## Bare specifiers (packages)

For `import _ from "lodash"`, the compiler walks up the directory tree looking in each `node_modules`:

1. `node_modules/lodash` and its own types (`package.json` `exports` with a `types` condition, or `types`/`typings`, or an adjacent `index.d.ts`).
2. `node_modules/@types/lodash`.
3. Parent directory's `node_modules`, and so on to the root.

With `nodenext`/`bundler`, a package's `exports` map controls which paths are importable and which condition (`types`, `import`, `require`, `default`) is used. Deep imports not listed in `exports` fail. More in [third-party types](../09-declaration-files/04-third-party-types.md).

## Path aliases: `paths`

`paths` maps import patterns to locations, so you can write `@/components/Button` instead of `../../../components/Button`.

```jsonc
{
  "compilerOptions": {
    "paths": {
      "@/*": ["./src/*"],
      "@shared/*": ["../shared/src/*"]
    }
  }
}
```

Rules:

- The pattern may contain **one** `*`, matched against the rest of the specifier and substituted into the target.
- Targets are lists, tried in order.
- Since TS 4.1, `paths` works **without `baseUrl`**, and targets are relative to the tsconfig that declares them.
- `paths` does **not** change how packages are resolved from `node_modules`.

### The critical caveat: types only

**`paths` affects type checking, not emitted code.** `tsc` does not rewrite `"@/utils"` into a relative path in the JavaScript output. The same alias must be configured in whatever runs the code:

| Runner | How to make aliases work at runtime |
|---|---|
| Vite | `resolve.alias`, or a plugin that reads tsconfig `paths` |
| webpack | `resolve.alias` / `tsconfig-paths-webpack-plugin` |
| esbuild / bundlers | alias options or a tsconfig-paths plugin |
| Jest / Vitest | `moduleNameMapper` / `alias` |
| Node via `tsc` output | a post-processing step (for example `tsc-alias`), or avoid aliases |
| Runners like `tsx` | support tsconfig `paths` out of the box |

An import that type-checks perfectly and then fails with `Cannot find module '@/utils'` at runtime is the classic symptom.

### An alternative: `package.json` `imports`

Node supports its own subpath imports, which need no extra tooling and work at runtime:

```json
{
  "imports": {
    "#utils/*": "./dist/utils/*"
  }
}
```

```ts
import { x } from "#utils/format.js";
```

Names must start with `#`. TypeScript understands `imports` with `nodenext` and `bundler` resolution. Because the map points at runtime files, you often point it at `dist` (or use conditions so source and output both work). This is a good option for Node libraries and servers where you want aliases without a build plugin.

## `baseUrl`

`baseUrl` lets bare-looking imports resolve from a fixed directory (`import x from "utils/format"` finds `src/utils/format`). Since `paths` no longer needs it, `baseUrl` is mostly legacy. It has costs: it competes with package names (a local `utils` folder can shadow an npm package), and bundlers need separate configuration. Prefer `paths` with an explicit prefix like `@/`.

## Other resolution-related options

| Option | Purpose |
|---|---|
| `rootDirs` | treat several folders as one virtual root for relative imports |
| `typeRoots` / `types` | where global `@types` packages are found and which are auto-included |
| `resolveJsonModule` | allow importing `.json` files |
| `moduleSuffixes` | try suffixes like `.ios` before the extension (React Native style) |
| `customConditions` | extra `exports` conditions to match |
| `resolvePackageJsonExports` / `resolvePackageJsonImports` | toggle `exports` / `imports` support (on by default for `nodenext` and `bundler`) |
| `preserveSymlinks` | do not resolve symlinks to real paths |
| `paths` + `include` | files reached through `paths` must still be reachable by the program |

## Monorepos

Packages in a workspace are usually symlinked into `node_modules`, so they resolve like any package: through `package.json` `exports`/`types`. Two common approaches:

- Point `types`/`exports` at **built output** (`dist/index.d.ts`) and use [project references](./04-project-references-and-incremental-builds.md) to build in order.
- Use `paths` or `customConditions` to resolve workspace packages **straight to source** during development. Fast, but the runtime must resolve the same way.

See [monorepos](../21-production-tooling/02-monorepos.md).

## Common mistakes

- **Using `bundler` resolution for code that Node executes directly.** It type-checks code that Node's ESM loader rejects.
- **Setting `paths` and forgetting the runtime or bundler.** The editor is happy and production crashes.
- **Omitting `.js` in relative imports under `nodenext`.** You get TS2835, and Node would fail too.
- **Mixing `module` and `moduleResolution` values** that do not belong together.
- **Overusing `baseUrl`,** so local folders shadow packages.
- **Deep-importing a package path that is not in its `exports`.**
- **Case-mismatched imports** that work on macOS/Windows and break on Linux CI. Keep `forceConsistentCasingInFileNames` on.
- **Aliases that point outside `include`,** leaving files unchecked.

## Debugging

```bash
tsc --traceResolution | grep "my-module"    # every lookup, with the reason it succeeded or failed
tsc --explainFiles                          # why each file is in the program
tsc --showConfig                            # effective moduleResolution and paths
```

Error codes you will meet:

| Code | Meaning | Typical fix |
|---|---|---|
| TS2307 | Cannot find module 'x' | wrong path, missing file, missing `include`, missing `@types`, wrong resolution mode |
| TS2792 | Cannot find module. Did you mean to set `moduleResolution` to `nodenext`, or add `paths`? | resolution strategy does not match how the project is built |
| TS2834 / TS2835 | Relative imports need explicit file extensions in ESM | add `.js` |
| TS7016 | No declaration file for module | install `@types/x` or declare it |

If it works in the editor but not at runtime, compare how the editor/`tsc` resolves with how the runner resolves. They are separate implementations.

## Quick summary

- `moduleResolution` decides how imports map to files: `nodenext` for Node, `bundler` for bundled code, `node10` for legacy.
- Bare imports walk up `node_modules`, preferring a package's own types, then `@types`. `exports` maps are honored with `nodenext` and `bundler`.
- Under `nodenext` ESM, relative imports need the `.js` extension.
- `paths` aliases are **type-checking only**. Configure the same aliases in your bundler, test runner, or runtime, or use `package.json` `imports`.
- `--traceResolution` is the tool to answer "why can't it find this".

**Next:** [Project references and incremental builds](./04-project-references-and-incremental-builds.md)
