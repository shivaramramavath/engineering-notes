# Publishing TypeScript Packages: Exports, Publint, and AreTheTypesWrong

> Complete guide to publishing production-grade TypeScript packages to npm: `package.json` `exports` maps, dual ESM/CJS typings, validation with `publint` and `@arethetypeswrong/cli`, provenance, and modern release automation.

| Metadata | Details |
| :--- | :--- |
| **Category** | Production Tooling |
| **Difficulty** | Advanced |
| **Prerequisites** | [21-production-tooling/01-build-and-bundling.md](./01-build-and-bundling.md), [13-compiler-options/01-module-resolution.md](../13-compiler-options/01-module-resolution.md) |
| **TypeScript Baseline** | TS 5.0+, Node.js 20/22 LTS |
| **Tested tsconfig** | `"strict": true, "target": "ES2022", "module": "NodeNext"` |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & API Reference](#4-syntax--api-reference)
5. [Practical Examples](#5-practical-examples)
6. [Edge Cases, Pitfalls & Gotchas](#6-edge-cases-pitfalls--gotchas)
7. [Common Mistakes & Anti-Patterns](#7-common-mistakes--anti-patterns)
8. [Performance & Type-Checker Complexity](#8-performance--type-checker-complexity)
9. [Compiler Internals](#9-compiler-internals)
10. [Best Practices & Design Guidelines](#10-best-practices--design-guidelines)
11. [Real-World Case Study](#11-real-world-case-study)
12. [Interview Questions](#12-interview-questions)
13. [Exercises & Challenges](#13-exercises--challenges)

---

## 1. What & Why

Publishing an npm package written in TypeScript is notoriously error-prone. While a package may compile and test cleanly inside your own development environment, external consumers often experience frustrating failures:
- `Cannot find module 'pkg' or its corresponding type declarations` (TS2307).
- Incompatible CommonJS vs. ECMAScript Module (ESM) imports (`require` of ES Module not supported).
- Type resolution resolving to `any` because `package.json` conditional exports are misordered.

Modern package authors cannot rely on manual testing. Modern release engineering leverages automated verification tools:
1. **`publint`**: Verifies that your `package.json` fields conform to current Node.js and bundler packaging specifications.
2. **`@arethetypeswrong/cli` (attw)**: Analyzes your built package against every module resolution mode supported by TypeScript (`node10`, `node16/nodenext`, `bundler`), catching subtle typing bugs before release.
3. **npm Provenance**: Cryptographically signs published tarballs using GitHub Actions OIDC tokens to ensure supply chain integrity.

---

## 2. Core Concepts

### Modern `package.json` Field Hierarchy
- **`type`**: Set to `"module"` for modern native ESM packages.
- **`main`**: Fallback entry point for legacy CommonJS environments.
- **`module`**: Fallback entry point for legacy bundlers (Webpack 4, Rollup).
- **`types`**: Legacy root fallback declaration file.
- **`exports`**: The authoritative modern standard for defining public module entry points and subpaths in Node.js and TypeScript 5.0+.

```
+-------------------------------------------------------------------------+
|                  Modern Conditional Exports Resolution                  |
+-------------------------------------------------------------------------+
|                                                                         |
|                          import { fn } from "pkg"                       |
|                                    |                                    |
|                                    v                                    |
|                   Node.js / TS Environment Check                        |
|                                    |                                    |
|          +-------------------------+-------------------------+          |
|          | ESM (import)                                      | CJS (require)
|          v                                                   v          |
|  exports["."]["import"]                             exports["."]["require"]|
|   +-- types: dist/index.d.ts                         +-- types: dist/index.d.cts|
|   +-- default: dist/index.js                         +-- default: dist/index.cjs|
|                                                                         |
+-------------------------------------------------------------------------+
```

### The Extension Rule (`.d.ts` vs `.d.mts` vs `.d.cts`)
TypeScript declaration extensions must strictly match runtime JavaScript module formats:
- `.js` (ESM) $\rightarrow$ `.d.ts` (with `"type": "module"`) or `.d.mts`
- `.cjs` (CommonJS) $\rightarrow$ `.d.cts`

---

## 3. How It Works Internally

### How `@arethetypeswrong/cli` Audits Packages
1. **Tarball Unpacking**: Simulates `npm pack` to inspect only the files that will actually be published to the registry.
2. **Matrix Resolution**: Evaluates entry points across four distinct TypeScript compiler hosts:
   - Node10 (legacy resolution)
   - Node16 (Node.js ESM / CJS dual resolution)
   - NodeNext (modern Node.js standards)
   - Bundler (Vite, Webpack 5, esbuild)
3. **Problem Detection**:
   - `Untyped Resolution`: JavaScript found, but types are missing.
   - `False CJS / False ESM`: Declaring CommonJS in `exports` while emitting ES syntax (or vice versa).
   - `CJS / ESM Internal Conflict`: Missing types for specific conditions.

---

## 4. Syntax & API Reference

### Authoritative Dual `package.json` Template
```json
{
  "name": "@acme/math-utils",
  "version": "1.0.0",
  "type": "module",
  "main": "./dist/index.cjs",
  "module": "./dist/index.js",
  "types": "./dist/index.d.ts",
  "exports": {
    ".": {
      "types": {
        "import": "./dist/index.d.ts",
        "require": "./dist/index.d.cts"
      },
      "import": "./dist/index.js",
      "require": "./dist/index.cjs"
    },
    "./package.json": "./package.json"
  },
  "files": [
    "dist"
  ],
  "scripts": {
    "build": "tsup src/index.ts --format esm,cjs --dts",
    "check:exports": "publint",
    "check:types": "attw --pack ."
  }
}
```

---

## 5. Practical Examples

### Example 1: Validating Built Artifacts with ATTW
```bash
# Terminal execution validating published types
$ pnpm attw --pack .

@acme/math-utils v1.0.0
- Entering "."
  - (esm) node16: OK
  - (cjs) node16: OK
  - bundler: OK
  - node10: OK
All entry points resolved cleanly across all environments!
```

### Example 2: Subpath Exports with Dedicated Types
```json
{
  "exports": {
    ".": {
      "types": "./dist/index.d.ts",
      "import": "./dist/index.js",
      "require": "./dist/index.cjs"
    },
    "./testing": {
      "types": "./dist/testing.d.ts",
      "import": "./dist/testing.js",
      "require": "./dist/testing.cjs"
    }
  }
}
```

### Example 3: Edge Case - Masquerading as CJS
```typescript
// If dist/index.cjs contains:
// export const version = "1.0.0"; // SYNTAX ERROR in CommonJS!
// Node.js throws: SyntaxError: Unexpected token 'export'

// Correct CommonJS Output (emitted by bundler):
"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.version = "1.0.0";
```

### Example 4: Compiler Error & Types Condition Misplacement
```json
// BAD: "types" placed AFTER "import"
{
  "exports": {
    ".": {
      "import": "./dist/index.js",
      "types": "./dist/index.d.ts"
    }
  }
}
// Under TypeScript node16 resolution:
// TS2307: Cannot find module '@acme/pkg' or its corresponding type declarations.
// Because TS matches "import" condition first, finds .js, and immediately aborts type resolution!
```

### Example 5: GitHub Actions Automated Provenance Workflow
```yaml
# .github/workflows/release.yml
name: Release Package
on:
  push:
    tags:
      - 'v*'

jobs:
  publish:
    runs-on: ubuntu-latest
    permissions:
      contents: read
      id-token: write # Required for npm provenance
    steps:
      - uses: actions/checkout@v4
      - uses: pnpm/action-setup@v3
        with:
          version: 9
      - uses: actions/setup-node@v4
        with:
          node-version: 22
          registry-url: 'https://registry.npmjs.org'
      - run: pnpm install --frozen-lockfile
      - run: pnpm build
      - run: pnpm check:exports
      - run: pnpm check:types
      - run: pnpm publish --provenance --access public
        env:
          NODE_AUTH_TOKEN: ${{ secrets.NPM_TOKEN }}
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **Forgetting `./package.json` in `exports`**:
   Tools like modern bundlers, linters, and versioning scripts frequently read `require('pkg/package.json')`. If your `exports` map does not explicitly define `"./package.json": "./package.json"`, Node.js will block access with a `Package subpath './package.json' is not defined by "exports"` error.
2. **The Default Export Trap**:
   In ESM, `export default fn` creates `{ default: fn }`. In CommonJS, `module.exports = fn` sets the root export. If not handled carefully via named exports, consumers using `require('pkg')` get `{ default: [Function: fn] }` instead of the function itself. **Always prefer named exports for published libraries.**
3. **Leaking Private Dependencies into `.d.ts`**:
   If an exported interface in your library references a type from `devDependencies` (e.g. `@types/express`), consumers without that devDependency will get compiler errors when installing your library. Run `publint` to verify that all referenced types reside in `dependencies`.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Bad Practice | Good Practice | Problem / Diagnostic |
| :--- | :--- | :--- | :--- |
| **Misordered Types Condition** | Placing `"types"` after `"import"` | Placing `"types"` first in conditions | `TS2307: Cannot find module or its corresponding type declarations` |
| **Missing `.d.cts` for CJS** | Pointing CJS `types` to `.d.ts` (ESM) | Use `.d.cts` for CJS branch | ATTW: `False CJS` / `Masquerading as ESM` |
| **Unpublished `files` Array** | Omitting `files` field in package.json | Explicitly specify `"files": ["dist"]` | Accidental upload of src, tests, and tokens |
| **Missing `./package.json` Export** | Omitting `./package.json` from `exports` | Add `"./package.json": "./package.json"` | `ERR_PACKAGE_PATH_NOT_EXPORTED` |
| **Publishing Without Type Audit** | Running `npm publish` without `attw` | Run `attw --pack .` in CI pipeline | Broken consumer installations |
| **Default Export Ambiguity** | Exporting a single default function | Export explicit named entities | `require('pkg').default is not a function` |

---

## 8. Performance & Type-Checker Complexity

- **Consumer Impact**: When packages provide precompiled `.d.ts` declaration stubs, consumers' type-checkers do not evaluate internal function bodies or implementation ASTs. Type checking is fast ($O(1)$ symbol lookup).
- **Declaration Size Optimization**: Minimize `.d.ts` size by avoiding deeply inlined utility types (e.g., inlining massive Zod schemas or complex nested conditional types). Export clean, flattened interfaces.

---

## 9. Compiler Internals

When TypeScript resolves a package under `moduleResolution: "NodeNext"`:
1. `ts.resolveModuleName`: Reads `package.json` at the dependency root.
2. `ts.readPackageJsonExports`: Evaluates the `exports` object matching conditions (`types`, `import`, `require`).
3. `ts.loadModuleFromNodeModules`: Maps the matched path to disk.
4. If the target file ends in `.d.cts`, TypeScript sets its internal module kind to CommonJS; if `.d.mts` or `.d.ts` (with `"type": "module"`), it sets the module kind to ESNext.

---

## 10. Best Practices & Design Guidelines

1. **Always Run `attw` Before Publishing**: Integrate `@arethetypeswrong/cli` as a blocking CI gate.
2. **Publish Provenance**: Use `--provenance` on GitHub Actions to establish verifiable supply-chain security.
3. **Prefer Named Exports**: Eliminates interop confusion between CJS `require` and ESM `import`.
4. **Whitelist Published Files**: Always specify `"files": ["dist"]` in `package.json`.

---

## 11. Real-World Case Study

A popular open-source state management library released version 2.0 migrated entirely to pure ESM. Immediately after publishing, 400+ GitHub issues were opened:
- Next.js projects failed with `ERR_REQUIRE_ESM`.
- TypeScript projects using `moduleResolution: "node"` could no longer resolve type declarations.

**Remediation**:
1. Implemented a dual build pipeline using `tsup` outputting both `dist/index.js` (ESM) and `dist/index.cjs` (CJS).
2. Corrected the `exports` map order with `"types"` at the top.
3. Added `attw --pack .` to GitHub Actions.

Subsequent releases passed all matrix tests with zero regression issues.

---

## 12. Interview Questions

<details>
<summary>1. Why must the `"types"` key be the first entry in a conditional exports block?</summary>

In Node.js and TypeScript, conditional export keys are evaluated in object iteration order. If `"import"` or `"require"` appears before `"types"`, the resolver matches the JavaScript file first and immediately terminates resolution without checking for declaration files.
</details>

<details>
<summary>2. What is `@arethetypeswrong/cli` (attw) and what problems does it identify?</summary>

`attw` is a diagnostic tool that tests a published package against TypeScript's resolution modes (`node10`, `node16`, `nodenext`, `bundler`). It identifies mismatched module kinds (e.g. CJS types pointing to ESM code), missing types, and export map misconfigurations.
</details>

<details>
<summary>3. What is npm provenance and how is it generated?</summary>

npm provenance establishes a cryptographic audit trail linking an npm package to its exact GitHub Actions source commit and workflow run using OpenID Connect (OIDC). It is generated during `npm publish --provenance`.
</details>

<details>
<summary>4. What is the difference between `.d.ts`, `.d.mts`, and `.d.cts`?</summary>

- `.d.ts`: Standard declaration file; interpreted as ESM or CJS depending on the nearest `package.json` `"type"` field.
- `.d.mts`: Explicitly treated as an ECMAScript Module declaration file regardless of `package.json`.
- `.d.cts`: Explicitly treated as a CommonJS declaration file regardless of `package.json`.
</details>

<details>
<summary>5. Why should `./package.json` be included in the `exports` map?</summary>

Modern Node.js encapsulates all files not explicitly listed in `exports`. If `./package.json` is omitted, external tooling attempting to read `package.json` (such as linters, formatters, or bundler plugins) will throw an export encapsulation runtime error.
</details>

<details>
<summary>6. How do you prevent sensitive files (.env, test files) from being published to npm?</summary>

By specifying an explicit whitelist in the `"files"` property of `package.json` (e.g. `"files": ["dist"]`), or by configuring a `.npmignore` file.
</details>

<details>
<summary>7. What is `publint`?</summary>

`publint` is a linter that audits an npm package's `package.json` configuration against modern Node.js and bundler runtime specifications, catching malformed subpath patterns and invalid file paths.
</details>

<details>
<summary>8. Why are default exports discouraged in libraries supporting both CJS and ESM?</summary>

ESM default exports compile to `{ default: value }` in CommonJS. When CJS consumers execute `const pkg = require('pkg')`, they must access `pkg.default`, which frequently leads to confusion and runtime `TypeError: pkg is not a function` errors.
</details>

---

## 13. Exercises & Challenges

<details>
<summary>Challenge 1 (Easy): Create a minimal files whitelist in package.json</summary>

**Task**: Configure `package.json` to publish only the `dist/` directory and `README.md`.

**Solution**:
```json
{
  "files": [
    "dist",
    "README.md"
  ]
}
```
</details>

<details>
<summary>Challenge 2 (Easy): Fix export condition order</summary>

**Task**: Fix the conditional exports block below:
```json
{
  "exports": {
    ".": {
      "import": "./dist/index.js",
      "types": "./dist/index.d.ts"
    }
  }
}
```

**Solution**:
```json
{
  "exports": {
    ".": {
      "types": "./dist/index.d.ts",
      "import": "./dist/index.js"
    }
  }
}
```
</details>

<details>
<summary>Challenge 3 (Medium): Add ATTW and Publint verification scripts</summary>

**Task**: Write the `scripts` section for a library that runs build, publint, and attw prior to publishing.

**Solution**:
```json
{
  "scripts": {
    "build": "tsup src/index.ts --format esm,cjs --dts",
    "prepublishOnly": "npm run build && publint && attw --pack ."
  }
}
```
</details>

<details>
<summary>Challenge 4 (Medium): Configure dual CJS/ESM typings</summary>

**Task**: Configure `package.json` `exports` to point ESM imports to `.d.ts` and CJS requires to `.d.cts`.

**Solution**:
```json
{
  "exports": {
    ".": {
      "types": {
        "import": "./dist/index.d.ts",
        "require": "./dist/index.d.cts"
      },
      "import": "./dist/index.js",
      "require": "./dist/index.cjs"
    }
  }
}
```
</details>

<details>
<summary>Challenge 5 (Hard): Scripted pre-publish validation gate</summary>

**Task**: Write a Node.js pre-publish gate script that verifies git cleanliness, builds artifacts, runs attw, and ensures no private packages are being published.

**Solution**:
```typescript
import { execSync } from "node:child_process";
import fs from "node:fs";

function run(cmd: string) {
  console.log(`> ${cmd}`);
  execSync(cmd, { stdio: "inherit" });
}

try {
  const pkg = JSON.parse(fs.readFileSync("package.json", "utf-8"));
  if (pkg.private) {
    throw new Error("Cannot publish private package!");
  }

  const gitStatus = execSync("git status --porcelain").toString().trim();
  if (gitStatus.length > 0) {
    throw new Error("Git working directory has uncommitted changes!");
  }

  run("npm run build");
  run("npx publint");
  run("npx @arethetypeswrong/cli --pack .");
  console.log("All pre-publish checks passed!");
} catch (error) {
  console.error("Publish gate failed:", error);
  process.exit(1);
}
```
</details>

<details>
<summary>Challenge 6 (Hard): Multi-entrypoint library package structure</summary>

**Task**: Design the package structure and `package.json` for a library exporting `.` (core) and `./node` (server utilities) with independent bundles and typings.

**Solution**:
```json
{
  "name": "@acme/toolkit",
  "version": "1.0.0",
  "type": "module",
  "exports": {
    ".": {
      "types": "./dist/index.d.ts",
      "import": "./dist/index.js",
      "require": "./dist/index.cjs"
    },
    "./node": {
      "types": "./dist/node.d.ts",
      "import": "./dist/node.js",
      "require": "./dist/node.cjs"
    },
    "./package.json": "./package.json"
  },
  "files": [
    "dist"
  ]
}
```
</details>
