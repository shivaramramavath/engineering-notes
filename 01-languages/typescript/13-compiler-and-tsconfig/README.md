# Module 13: Compiler Internals, tsconfig, and Tooling in TypeScript

Welcome to **Module 13: Compiler Internals, tsconfig, and Tooling in TypeScript**. This module provides a complete reference for configuring, optimizing, profiling, and scaling the TypeScript compiler from single projects to multi-package monorepos and next-generation native compilers.

---

## Module Overview

| Note | Title | Level | Core Topics |
| :--- | :--- | :--- | :--- |
| **00** | [Compiler Options and tsconfig Reference](./00-compiler-options.md) | Intermediate | Anatomy of `tsconfig.json`, `include`/`exclude`, `extends`, emit flags, sourcemaps, JSX modes |
| **01** | [Strict Mode and Pedantic Flags](./01-strict-mode.md) | Intermediate | Complete `"strict": true` breakdown, `noUncheckedIndexedAccess`, `exactOptionalPropertyTypes` |
| **02** | [Target, Module, and Lib](./02-target-module-and-lib.md) | Intermediate | Downleveling targets (`ES2022`), module formats (`NodeNext`, `bundler`, `preserve`), declaration libraries |
| **03** | [Module Resolution and Paths](./03-module-resolution-and-paths.md) | Intermediate | `NodeNext` vs `bundler`, package.json `exports`, subpath imports (`#`), `paths` alias mapping |
| **04** | [Project References and Incremental Builds](./04-project-references-and-incremental-builds.md) | Advanced | `composite: true`, `references: [{ path }]`, `tsc --build`, `.tsbuildinfo`, monorepos |
| **05** | [Production tsconfig Recipes](./05-tsconfig-recipes.md) | Advanced | Recipes for Node.js 22 ESM, Vite React, Next.js App Router, npm dual libraries, monorepos |
| **06** | [Extended Diagnostics and Generate Trace](./06-extended-diagnostics-and-generate-trace.md) | Advanced | `--extendedDiagnostics`, `--generateTrace`, Perfetto traces, `@typescript/analyze-trace`, union explosions |
| **07** | [TypeScript 7.0 Native Go Compiler (tsgo)](./07-typescript-7-native-go-compiler-tsgo.md) | Advanced | Native Go port (`tsgo`), multi-threaded parallel checking, 10x speedup, `isolatedDeclarations` |

---

## Learning Path

```
                    +------------------------------------+
                    | 00-compiler-options                |
                    | (tsconfig schema, emit controls)   |
                    +-----------------+------------------+
                                      |
                                      v
                    +------------------------------------+
                    | 01-strict-mode                     |
                    | (Strict flags, pedantic checks)    |
                    +-----------------+------------------+
                                      |
                                      v
                    +------------------------------------+
                    | 02-target-module-and-lib           |
                    | (Targets, module systems, lib sets)|
                    +-----------------+------------------+
                                      |
                                      v
                    +------------------------------------+
                    | 03-module-resolution-and-paths     |
                    | (NodeNext, bundler, subpaths)      |
                    +-----------------+------------------+
                                      |
                 +--------------------+--------------------+
                 |                                         |
                 v                                         v
+------------------------------------+   +------------------------------------+
| 04-project-references              |   | 05-tsconfig-recipes                |
| (composite, tsc -b, monorepos)     |   | (Node 22, Vite, Next.js, dual-pkg) |
+----------------+-------------------+   +-----------------+------------------+
                 |                                         |
                 +-------------------+---------------------+
                                     |
                                     v
                   +----------------------------------+
                   | 06-extended-diagnostics-trace    |
                   | (Profiling, analyze-trace)       |
                   +-----------------+----------------+
                                     |
                                     v
                   +----------------------------------+
                   | 07-typescript-7-native-compiler  |
                   | (tsgo architecture, 10x speedup) |
                   +----------------------------------+
```

---

## Key Best Practices
1. **Always Set `"strict": true`**: Prevent null pointer crashes and implicit anys across all code.
2. **Never Put `"DOM"` in Backend Projects**: Specify `"lib": ["ES2022"]` in pure Node.js services to prevent browser global leakage.
3. **Use Native Subpath Imports**: Prefer `package.json` `"imports"` (`#utils`) over `tsconfig.json` `paths` for aliases that resolve at runtime.
4. **Leverage Project References for Monorepos**: Split large codebases into composite projects built with `tsc --build` and `.tsbuildinfo` caching.
5. **Prepare for Native `tsgo`**: Enable `"isolatedDeclarations": true` and `"verbatimModuleSyntax": true` to future-proof for TypeScript 7 native multi-threaded compilation.
