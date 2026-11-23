# Module 00: Environment Setup, Tooling & Workflow

> Foundational setup guide for professional JavaScript development: runtime engines (V8/SpiderMonkey/JavaScriptCore), host environments (Node.js/browsers), package management, debugging, code quality tooling, and zero-build static type checking.

| Metadata | Details |
| :--- | :--- |
| **Track** | JavaScript Language Reference & Engineering Notes |
| **Module** | `00_setup` |
| **Level** | Beginner to Intermediate |
| **Target Runtime** | Node.js v20+ / v22+ LTS (Verified on Node v24.11.1), Modern Evergreen Browsers |
| **Status** | Complete (6 of 6 notes) |

---

## Module Overview

Before engineering robust software in JavaScript, you must understand where the language executes, how engines compile and optimize code, how dependencies are resolved, how to isolate defects with interactive debuggers, and how to automate code quality.

This module establishes your local development environment and developer workflow using modern ECMAScript standards and Node.js LTS.

```
00_setup/
├── 01_what-is-javascript.md          # ECMAScript spec, TC39, V8 pipeline, runtime vs engine
├── 02_runtimes-browser-and-node.md   # Event loops, Web APIs, libuv, globalThis, isomorphic code
├── 03_npm-and-package-managers.md    # package.json, lockfiles, semver, pnpm/npm/yarn, security
├── 04_devtools-and-debugging.md      # Breakpoints, heap profiling, memory leaks, node --inspect
├── 05_tooling.md                     # ESLint flat config, Prettier, Biome, Husky, lint-staged
├── 06_jsdoc-and-type-checking.md     # JSDoc type checking, checkJs, tsserver, zero-build safety
└── README.md                         # Module roadmap and environment verification
```

---

## Curriculum Roadmap

| # | Note | Description | Key Topics |
| :-: | :--- | :--- | :--- |
| **01** | [What is JavaScript](./01_what-is-javascript.md) | The language specification, governance, and engine architecture. | TC39 stage process, ES2015-ES2025 evolution, Ignition & TurboFan, bytecode vs machine code. |
| **02** | [Runtimes: Browser and Node](./02_runtimes-browser-and-node.md) | Host environments and their surrounding platform APIs. | Browser Web APIs vs Node.js C++ bindings, `libuv`, event loop differences, `globalThis`. |
| **03** | [npm and Package Managers](./03_npm-and-package-managers.md) | Dependency management and package distribution. | `package.json`, semantic versioning, lockfile mechanics, `pnpm` hard links, `npm audit`. |
| **04** | [DevTools and Debugging](./04_devtools-and-debugging.md) | Professional diagnostic tools and debugging workflows. | Chrome DevTools, conditional breakpoints, `debugger;`, Node `--inspect`, memory snapshots. |
| **05** | [Modern Tooling](./05_tooling.md) | Static analysis, formatters, and git automation. | ESLint 9+ flat config, Prettier, Biome Rust linter/formatter, Husky pre-commit, `lint-staged`. |
| **06** | [JSDoc & Type Checking](./06_jsdoc-and-type-checking.md) | Type safety without compilation or transpilation steps. | `checkJs: true`, `@type`, `@typedef`, `@template`, `tsc --noEmit`, IDE autocomplete. |

---

## Environment Verification

Verify your local toolchain before proceeding to language fundamentals:

```bash
# 1. Verify Node.js version (v20+ or v22+ LTS required)
node -v

# 2. Verify npm package manager
npm -v

# 3. Verify Git installation
git --version

# 4. Quick smoke test: execute native ECMAScript module
node -e "console.log('JavaScript runtime ready: ' + process.version)"
```

---

## Next Steps

Once your environment is verified, proceed to [Module 01: Language Fundamentals](../01_fundamentals/README.md) to explore lexical structure, dynamic typing, type coercion, and operators.
