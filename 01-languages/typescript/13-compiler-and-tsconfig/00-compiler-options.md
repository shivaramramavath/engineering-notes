# Compiler Options

`tsconfig.json` tells the TypeScript compiler **which files are in the program** and **how to check and emit them**. Editors read it too, so it also controls what you see in your IDE. This note covers the file's structure, how files get selected, how `extends` works, the options you meet most often, and the CLI commands for inspecting what the compiler is actually doing.

**Prerequisites:**
- [Toolchain](../00-setup/00-toolchain.md)
- [First project](../00-setup/01-first-project.md)

---

## Structure of a tsconfig

```jsonc
{
  "extends": "./tsconfig.base.json",      // inherit from another config
  "compilerOptions": {                     // how to check and emit
    "target": "ES2022",
    "strict": true,
    "outDir": "dist"
  },
  "include": ["src"],                      // which files to load
  "exclude": ["src/**/*.test.ts"],         // subtract from include
  "files": [],                             // explicit file list (rarely needed)
  "references": []                         // other projects (see project references note)
}
```

`tsconfig.json` is parsed as JSONC: comments and trailing commas are allowed.

Generate a starter file with:

```bash
npx tsc --init
```

The generated file is heavily commented. Trim it down to the options you actually use.

## Which files are included

- If neither `files` nor `include` is given, **all** `.ts`, `.tsx`, and `.d.ts` files under the config's directory are included (`.js` too with `allowJs`).
- `include` and `exclude` take glob patterns: `*` (any characters except `/`), `**/` (any directory depth), and `?` (one character).
- `exclude` defaults to `node_modules`, `bower_components`, `jspm_packages`, and your `outDir`. If you **set** `exclude`, the defaults no longer apply, so add `node_modules` back yourself if needed.
- `exclude` only filters what `include` found. A file excluded here can still be pulled in by an `import` from an included file.
- `files` lists exact paths and fails if one is missing. It suits tiny projects. Use `include` otherwise.

Files **imported** by included files are part of the program, even if `include` did not match them. That is why `tsc` may check files outside `src` when an import reaches them.

## `extends`

```jsonc
// tsconfig.json
{
  "extends": "./tsconfig.base.json",
  "compilerOptions": { "outDir": "dist" },
  "include": ["src"]
}
```

Merging rules:

- `compilerOptions` are **merged**: the child overrides individual options.
- `files`, `include`, and `exclude` are **replaced** (not merged) if the child specifies them.
- Relative paths inside an inherited config (such as `outDir` or `paths`) resolve **relative to the config file where they were written**, not the child.
- `extends` can be an array in TS 5.0+, with later entries overriding earlier ones.
- You can extend from packages: community bases such as `@tsconfig/node20` or `@tsconfig/strictest` give you vetted starting points.
- TS 5.5 added the `${configDir}` template variable, which makes shared base configs able to refer to the *consuming* project's directory (for example `"${configDir}/src"`).

## Options you will use constantly

| Option | What it does |
|---|---|
| `strict` | enables the strict family ([strict mode](./01-strict-mode.md)) |
| `target` | JavaScript version to emit ([target, module, lib](./02-target-module-and-lib.md)) |
| `module` | output module format |
| `moduleResolution` | how import specifiers are looked up ([module resolution](./03-module-resolution-and-paths.md)) |
| `lib` | which built-in type declarations exist |
| `outDir` / `rootDir` | where emitted files go, and the root of the source tree |
| `noEmit` | type-check only, write no files (common when a bundler builds) |
| `declaration` / `declarationMap` | emit `.d.ts` files and maps ([declaration files](../09-declaration-files/00-declaration-files.md)) |
| `sourceMap` | emit `.js.map` for debuggers |
| `skipLibCheck` | skip type-checking of all `.d.ts` files |
| `esModuleInterop` | sane default imports from CommonJS |
| `isolatedModules` | require that every file can be transpiled alone (needed by Babel, esbuild, SWC) |
| `verbatimModuleSyntax` | emit imports/exports as written, except `import type` |
| `resolveJsonModule` | allow `import data from "./data.json"` |
| `jsx` | how JSX is emitted (`react-jsx`, `preserve`, ...) |
| `allowJs` / `checkJs` | include and optionally type-check `.js` files |
| `incremental` | cache results between runs ([incremental builds](./04-project-references-and-incremental-builds.md)) |
| `noEmitOnError` | do not emit if there are type errors |
| `forceConsistentCasingInFileNames` | error when the same file is imported with different casing (a real problem across macOS/Windows and Linux) |

Rough categories, useful for finding things in the [official option reference](https://www.typescriptlang.org/tsconfig): *Type Checking*, *Modules*, *Emit*, *JavaScript Support*, *Interop Constraints*, *Language and Environment*, *Projects*.

## Defaults depend on other options

Many defaults are computed. For example:

- `lib` defaults based on `target`.
- `moduleResolution` defaults based on `module`.
- `useDefineForClassFields` defaults to `true` when `target` is ES2022 or higher.
- `strict` defaults to `false`. A fresh `tsc --init` turns it on, but a hand-written tsconfig without it is not strict.

When behavior surprises you, print the effective configuration instead of guessing:

```bash
npx tsc --showConfig
```

## Which config does the compiler use?

- `tsc` with no file arguments searches for `tsconfig.json` in the current directory, then upward.
- `tsc -p path/to/tsconfig.json` (or `--project`) uses a specific file or folder.
- **`tsc somefile.ts` ignores `tsconfig.json` entirely** and uses defaults plus command-line flags. This surprises people who wonder why their strict settings do not apply.
- Editors use the **nearest** `tsconfig.json` above the file. A file not covered by any `include` gets an "inferred project" with default settings. If a file shows odd errors in the editor only, check that a tsconfig includes it.

Large projects often have several configs: a base, one for the app, one for tests, one for build scripts. See [recipes](./05-tsconfig-recipes.md).

## Useful CLI commands

```bash
tsc --noEmit                 # type-check only
tsc --watch                  # re-check on change
tsc --build                  # build a project and its references
tsc --showConfig             # effective tsconfig after extends and defaults
tsc --listFiles              # every file in the program
tsc --explainFiles           # why each file is included
tsc --traceResolution        # how each import was resolved
tsc --extendedDiagnostics    # timing and counts, for performance work
```

## Common mistakes

- **Running `tsc file.ts` and expecting tsconfig settings to apply.**
- **Setting `exclude` and accidentally including `node_modules`.** The default is replaced.
- **Expecting `exclude` to remove a file that is imported.** It is still part of the program.
- **Expecting a child config's `include` to add to the parent's.** It replaces.
- **Putting `outDir` inside `include`** so that emitted `.js` and `.d.ts` files get re-read. Keep output out of the input globs.
- **Copying a huge generated tsconfig** with dozens of commented options you do not understand.
- **Different tools reading different configs** (editor, test runner, bundler), giving inconsistent errors.
- **Relying on defaults that change between TypeScript versions.** Set important options explicitly.

## Debugging

- **File missing from the program:** `tsc --listFiles | grep name`, then `--explainFiles` to see why it is or is not included.
- **Option not taking effect:** `tsc --showConfig` shows the final merged value. Check for an overriding config earlier in the `extends` chain, or a different tsconfig being used.
- **Editor disagrees with CLI:** check which tsconfig the editor loaded, and that it uses the workspace TypeScript version.
- **Slow checks:** `--extendedDiagnostics` shows time spent and number of files and types ([type-checking performance](../22-performance/00-type-checking-performance.md)).

## Quick summary

- `tsconfig.json` defines the file set (`include`, `exclude`, `files`) and the rules (`compilerOptions`).
- Imported files join the program regardless of `exclude`. Setting `exclude` or `include` replaces the defaults and the parent's values.
- `extends` merges `compilerOptions` but replaces file lists, and resolves paths relative to the file they were written in.
- `tsc file.ts` ignores tsconfig. Use `tsc -p` or plain `tsc`.
- `tsc --showConfig`, `--listFiles`, and `--traceResolution` answer most "why is it doing that" questions.

**Next:** [Strict mode](./01-strict-mode.md)
