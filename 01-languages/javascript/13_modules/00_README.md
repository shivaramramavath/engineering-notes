# 13 · Modules

A **module** is a file with its own scope that explicitly shares what it wants (`export`) and explicitly uses what it needs (`import`). Modules replace global variables and script-tag ordering with a dependency graph the language and tools can understand.

```
main.js ──imports──► api.js ──imports──► http.js
   └─────imports──► utils.js
```

## Reading order

| # | File | You will learn |
|---|------|----------------|
| 1 | [ES Modules](./01_es-modules.md) | `import`/`export`, live bindings, dynamic `import()`, top-level `await`, `import.meta` |
| 2 | [CommonJS](./02_commonjs.md) | `require`, `module.exports`, the module wrapper, caching, cycles |
| 3 | [ESM vs CommonJS](./03_esm-vs-commonjs.md) | Differences, interop, `package.json` fields, dual packages |
| 4 | [Bundlers and Tree Shaking](./04_bundlers-and-tree-shaking.md) | Vite, Rollup, webpack, esbuild, code splitting, dead-code elimination |
| 5 | [Module Patterns](./05_module-patterns.md) | Singletons, barrels, facades, lazy loading, avoiding cycles |

## Two module systems at a glance

| | ES Modules (ESM) | CommonJS (CJS) |
|---|------------------|----------------|
| Syntax | `import` / `export` | `require()` / `module.exports` |
| Standard | ECMAScript | Node.js convention |
| Loading | static analysis, async capable | synchronous at runtime |
| Bindings | live, read-only | copies of exported values |
| Works in | browsers, Node, Deno, Bun | Node (and bundlers) |
| Top-level `await` | yes | no |
| Default for new code | **yes** | legacy and tooling configs |

## Goal

By the end you can structure code into modules, choose between ESM and CommonJS, configure `package.json` correctly, and understand what bundlers do to your imports.

## Prerequisites

- [Scope](../04_scope-and-execution/01_scope.md) and [Strict Mode](../04_scope-and-execution/05_strict-mode.md)
- [npm and Package Managers](../00_setup/03_npm-and-package-managers.md)

**Next:** [ES Modules](./01_es-modules.md)
