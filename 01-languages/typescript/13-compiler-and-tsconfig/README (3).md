# 13 - Compiler and tsconfig

How to configure TypeScript: what goes in `tsconfig.json`, what `strict` really turns on, how `target`, `module`, and `lib` relate to your runtime, how imports are resolved, and how to keep large codebases fast with incremental builds and project references. Most "TypeScript is behaving strangely" problems turn out to be configuration problems, so this section is about knowing what the compiler is actually being told.

## Prerequisites

- [00 Setup](../00-setup/README.md): a working project with `tsc`
- [08 Modules](../08-modules/README.md): especially [ES modules and CommonJS](../08-modules/01-es-modules-and-commonjs.md)
- [09 Declaration Files](../09-declaration-files/README.md): useful for the `declaration`, `types`, and `typeRoots` parts

## Notes in this section

| # | Note | Covers |
|---|---|---|
| 00 | [Compiler options](./00-compiler-options.md) | tsconfig structure, `include`/`exclude`/`files`, `extends`, the options you use daily, CLI diagnostics |
| 01 | [Strict mode](./01-strict-mode.md) | What each `strict` flag does, flags beyond `strict`, adopting strictness incrementally |
| 02 | [Target, module, and lib](./02-target-module-and-lib.md) | Syntax vs APIs vs module format, choosing values, class field semantics, common mismatches |
| 03 | [Module resolution and paths](./03-module-resolution-and-paths.md) | `nodenext` vs `bundler`, bare specifiers, `paths`, `imports`, `--traceResolution` |
| 04 | [Project references and incremental builds](./04-project-references-and-incremental-builds.md) | `incremental`, `composite`, `tsc -b`, monorepo and test/source splits |
| 05 | [tsconfig recipes](./05-tsconfig-recipes.md) | Starting configs for Node, Vite, Next.js, NestJS, libraries, monorepos, and tests |

Read 00 to 03 in order. 04 matters once projects get big or split into packages. 05 is a lookup you return to when starting something new.

## Where is my problem?

| Symptom | Likely cause | Go to |
|---|---|---|
| `tsc file.ts` ignores my settings | `tsc` with file arguments skips tsconfig | [00](./00-compiler-options.md) |
| A file is not being checked, or one I excluded is | `include`/`exclude` rules, or it is imported | [00](./00-compiler-options.md) |
| Null and undefined errors everywhere after enabling `strict` | `strictNullChecks` | [01](./01-strict-mode.md) |
| `arr[0]` is possibly `undefined` | `noUncheckedIndexedAccess` | [01](./01-strict-mode.md) |
| "Do you need to change your target library?" (TS2550) | `lib` lacks the API | [02](./02-target-module-and-lib.md) |
| Compiles, then `SyntaxError` or "x is not a function" at runtime | `target` or `lib` ahead of the runtime | [02](./02-target-module-and-lib.md) |
| "Cannot use import statement outside a module" | `module` vs `package.json` `"type"` mismatch | [02](./02-target-module-and-lib.md) and [08/01](../08-modules/01-es-modules-and-commonjs.md) |
| TS2307 or TS2792, "Cannot find module" | resolution strategy, missing file, missing types | [03](./03-module-resolution-and-paths.md) |
| TS2835, relative import needs an extension | `nodenext` ESM requires `.js` | [03](./03-module-resolution-and-paths.md) |
| Alias imports work in the editor, fail at runtime | `paths` is types-only | [03](./03-module-resolution-and-paths.md) |
| Type checking is slow | `incremental`, references, expensive types | [04](./04-project-references-and-incremental-builds.md) |
| Cross-package imports fail until something is built | references not built with `tsc -b` | [04](./04-project-references-and-incremental-builds.md) |
| Starting a new project | pick a recipe | [05](./05-tsconfig-recipes.md) |

## Ideas that recur across the section

- **`target`, `lib`, and `module` are three separate decisions.** Syntax level, available APIs, and module format. None adds polyfills.
- **Types are claims about the runtime.** `lib` and `paths` make the checker accept things. Only the runtime or bundler makes them true.
- **`module` and `moduleResolution` come as a pair.** `nodenext`/`nodenext` for Node, `esnext`/`bundler` for bundled code.
- **`strict` is a floor, not a ceiling.** Several of the most useful flags are outside it.
- **Know which config is read.** The editor, `tsc`, the bundler, and the test runner may each read a different one.
- **Inspect, do not guess.** `--showConfig`, `--listFiles`, `--explainFiles`, and `--traceResolution` answer most questions in seconds.

## Useful commands

```bash
tsc --noEmit                  # type-check only
tsc --showConfig              # effective config after extends and defaults
tsc --listFiles               # every file in the program
tsc --explainFiles            # why each file is included
tsc --traceResolution         # how each import was resolved
tsc --extendedDiagnostics     # timing and counts
tsc -b --verbose              # build with references, explaining up-to-date checks
```

## Related sections

- [08 Modules](../08-modules/README.md): how module systems and extensions interact with `module` settings
- [09 Declaration Files](../09-declaration-files/README.md): `declaration`, `types`, `typeRoots`, `skipLibCheck`
- [14 Type System Internals](../14-type-system-internals/README.md): why strict flags change what is assignable
- [21 Production Tooling](../21-production-tooling/README.md): build, bundling, monorepos, publishing
- [22 Performance](../22-performance/README.md): type-checking and build performance
- [28 Cheatsheets: tsconfig](../28-cheatsheets/04-tsconfig.md)
- [27 Interview: compiler and tsconfig](../27-interview/06-compiler-and-tsconfig.md)

## Next

[14 Type System Internals](../14-type-system-internals/README.md)
