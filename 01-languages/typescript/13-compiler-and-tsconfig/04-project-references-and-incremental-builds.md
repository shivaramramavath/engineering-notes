# Project References and Incremental Builds

As a codebase grows, type-checking everything on every run gets slow. TypeScript has two features to cut that down: **incremental builds**, which cache the results of the last run so unchanged files are skipped, and **project references**, which split a large codebase into smaller projects that depend on each other and are built in order. Together they make large repos and monorepos practical, and they give editors faster, more accurate feedback.

**Prerequisites:**
- [Compiler options](./00-compiler-options.md)
- [Module resolution and paths](./03-module-resolution-and-paths.md)
- [Declaration files](../09-declaration-files/00-declaration-files.md)

---

## Incremental builds

```jsonc
{
  "compilerOptions": {
    "incremental": true,
    "tsBuildInfoFile": "./.cache/app.tsbuildinfo"   // optional
  }
}
```

With `incremental` on, `tsc` writes a `.tsbuildinfo` file recording the files, their hashes, and the dependency graph from the last run. On the next run, it re-checks and re-emits only files affected by your changes.

- Where the file goes: next to the output (in `outDir`) by default, or at `tsBuildInfoFile`.
- It works with `noEmit` too, so type-check-only runs are still incremental.
- Add the `.tsbuildinfo` file to `.gitignore`. It is a cache, not source.
- If the cache ever seems wrong, delete it and rebuild. It is safe to remove.
- It speeds up repeat runs. It does not help a cold CI run unless you cache the `.tsbuildinfo` and outputs between jobs.

## Project references

A **project reference** says: "this project depends on that project's *output*."

```text
repo/
├── tsconfig.json              <- solution file: references everything
├── tsconfig.base.json         <- shared options
├── packages/
│   ├── core/
│   │   ├── tsconfig.json      <- composite project
│   │   └── src/
│   ├── api/
│   │   ├── tsconfig.json      <- references core
│   │   └── src/
│   └── web/
│       ├── tsconfig.json      <- references core
│       └── src/
```

### The referenced project

A project that others reference must set `composite`:

```jsonc
// packages/core/tsconfig.json
{
  "extends": "../../tsconfig.base.json",
  "compilerOptions": {
    "composite": true,
    "outDir": "dist",
    "rootDir": "src"
  },
  "include": ["src"]
}
```

`composite: true` enforces the rules that make independent builds possible:

- `declaration` is turned on, so `.d.ts` files are emitted.
- `rootDir` defaults to the directory containing the tsconfig.
- **Every source file must be matched** by `include` or `files`. TypeScript needs the complete file list to know when the project is up to date.
- It implies `incremental`.

### The referencing project

```jsonc
// packages/api/tsconfig.json
{
  "extends": "../../tsconfig.base.json",
  "compilerOptions": {
    "composite": true,
    "outDir": "dist",
    "rootDir": "src"
  },
  "include": ["src"],
  "references": [{ "path": "../core" }]
}
```

When `api` imports from `core`, TypeScript reads `core`'s **emitted `.d.ts` files**, not its source. That is the point: `api` does not re-check `core`'s code, so changing `api` does not touch `core`, and changing `core` triggers a rebuild of `core` and then only of the projects that depend on it.

### The solution file

A root config that builds everything, with no files of its own:

```jsonc
// tsconfig.json (root)
{
  "files": [],
  "references": [
    { "path": "packages/core" },
    { "path": "packages/api" },
    { "path": "packages/web" }
  ]
}
```

`"files": []` stops the root from also compiling every file in the repo.

## Building with `--build`

```bash
tsc --build              # or: tsc -b
tsc -b --watch           # rebuild on change
tsc -b --verbose         # explain what is up to date and what is not
tsc -b --dry             # show what would be built, without building
tsc -b --clean           # delete outputs
tsc -b --force           # rebuild everything
```

`tsc -b` builds each referenced project **in dependency order** and skips those that are up to date. Plain `tsc -p packages/api` does **not** build references first, so if `core` has no output yet, `api` fails with "cannot find module" or "project not built" errors.

Referenced projects must not form a cycle.

## What editors do

In an editor, "go to definition" on an import from a referenced project would normally land in the `.d.ts`. Two options make it nicer:

- **`declarationMap: true`** in the referenced project, so navigation jumps to the original `.ts`.
- The editor's language service by default loads referenced projects' **source** instead of requiring a build, so you see fresh types as you edit across projects. (Control with `disableSourceOfProjectReferenceRedirect`.) The command-line `tsc -b` still goes through the outputs.

## When to use references

Good fits:

- **Monorepos** with several packages that depend on each other ([monorepos](../21-production-tooling/02-monorepos.md)).
- **Separating app code from tests or tooling,** each with different `lib`, `types`, or strictness. For example, tests that need `vitest/globals` should not make those globals visible in `src`.
- **Large single packages** where `tsc` takes tens of seconds, and logical layers (core, server, client) can be split.
- Enforcing a **dependency direction**: a project can only import from projects it references.

Not worth it:

- Small projects. `incremental` alone is enough.
- When a bundler does the building and type-checking is already fast.

## Splitting source and tests

A common, small use of references:

```jsonc
// tsconfig.json (solution)
{
  "files": [],
  "references": [{ "path": "./tsconfig.src.json" }, { "path": "./tsconfig.test.json" }]
}
```

```jsonc
// tsconfig.test.json
{
  "extends": "./tsconfig.src.json",
  "compilerOptions": { "types": ["vitest/globals"], "noEmit": false, "outDir": "dist-test" },
  "include": ["test"],
  "references": [{ "path": "./tsconfig.src.json" }]
}
```

The test project sees the source project's declarations plus test globals, while the source project stays free of them. See [test runners](../18-testing-and-debugging/03-test-runners.md).

## Performance notes

- References reduce work per project and let `tsc` skip up-to-date ones. They add overhead per project, so very many tiny projects can be slower than a few mid-sized ones.
- Keep `skipLibCheck: true` unless you have a reason not to.
- Avoid heavy type-level code in widely imported modules, since every dependent project pays for it.
- Use `--extendedDiagnostics` to see where time goes. See [build performance](../22-performance/01-build-performance.md) and [type-checking performance](../22-performance/00-type-checking-performance.md).

## Important rules and misconceptions

- **`composite` is required** on every referenced project. Omitting it gives an error that the referenced project must have `composite: true`.
- **References are about `tsc` structure, not runtime.** Your packages still need correct `package.json` `main`/`exports`/`types` for the runtime and for consumers.
- **Outputs matter.** Because dependents read `.d.ts` files, a referenced project's `outDir` and `declaration` output must exist and be current. `tsc -b` ensures this.
- **Do not share one `outDir` between projects.** Their outputs would collide.
- **`prepend` (concatenating outputs)** is a legacy feature, rarely useful.
- **A tsbuildinfo is not a source of truth.** Delete it if the incremental state looks stale.

## Common mistakes

- **Running `tsc -p` and expecting references to build.** Use `tsc -b`.
- **Forgetting `composite: true`** on a referenced project.
- **A `composite` project whose `include` misses files,** producing "file is not listed within the file list of project" errors.
- **Circular references** between projects.
- **Committing `.tsbuildinfo` or `dist`** to source control.
- **Two projects emitting into the same directory.**
- **Importing from a sibling project that is not listed in `references`,** so it resolves to source (or fails) instead of output.
- **Expecting references to speed up a bundler build.** They help `tsc` and editors.

## Debugging

- `tsc -b --verbose` prints, per project, whether it is up to date and which input file or missing output caused a rebuild.
- `tsc --explainFiles` shows why a file ended up in a project, which finds accidental cross-project includes.
- "Output file ... has not been built from source file ...": the referenced project is stale. Run `tsc -b`.
- "File ... is not listed within the file list of project ... Projects must list all files or use an 'include' pattern": fix `include` in the composite project.
- When the editor shows stale types across projects, restart the TypeScript server and run `tsc -b`.

## Quick summary

- `incremental` caches results in a `.tsbuildinfo` file so repeat runs only recheck what changed.
- Project references split a repo into `composite` projects that depend on each other's `.d.ts` output. They enforce dependency direction and rebuild only what changed.
- Build with `tsc -b`, not `tsc -p`. Use `--verbose`, `--clean`, and `--force` to inspect or reset.
- A root "solution" tsconfig with `"files": []` and `references` builds everything.
- Use references for monorepos, source-vs-test separation, and large codebases. Skip them for small projects.

**Next:** [tsconfig recipes](./05-tsconfig-recipes.md)
