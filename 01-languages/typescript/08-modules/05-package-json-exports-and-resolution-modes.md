# Package.json Exports and Resolution Modes

> Modern TypeScript module resolution (`bundler`, `node16`, `nodenext`) aligns compile-time type resolution directly with Node.js subpath exports, condition matching, and package entry point encapsulation.

| Metadata | Value |
| :--- | :--- |
| **Level** | Advanced |
| **Prerequisites** | [00-imports-and-exports.md](file:///E:/Learning/engineering-notes/01-languages/typescript/08-modules/00-imports-and-exports.md), [01-es-modules-and-commonjs.md](file:///E:/Learning/engineering-notes/01-languages/typescript/08-modules/01-es-modules-and-commonjs.md) |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **TypeScript Baseline** | Since TS 4.7 / TS 5.0 / TS 7.0 Native |
| **Tested tsconfig** | `moduleResolution: NodeNext` / `bundler` |

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
In legacy Node.js, any file in a package could be imported by reaching into its internal folder structure:
```typescript
import { internalHelper } from "some-pkg/dist/internal/helper.js";
```
This created severe coupling: package authors could not refactor private files without breaking consumers.

Node.js introduced **Conditional Subpath Exports** in `package.json` (`"exports"`), providing strict encapsulation:
- Only paths explicitly exposed in `"exports"` can be imported.
- Internal files are blocked from external access by runtime module loaders.
- Packages can declare distinct entry points for CommonJS vs. ESM, browsers vs. Node, and production vs. development.

Starting in TypeScript 4.7 and 5.0, TypeScript added dedicated resolution modes (**`node16`**, **`nodenext`**, and **`bundler`**) that read and enforce these `package.json` rules at compile time.

---

## 2. Core Concepts

### Resolution Modes
- **`node16` / `nodenext`**: Strictly mirrors Node.js runtime resolution. Requires explicit file extensions in relative imports (`./file.js`), enforces `package.json` exports, respects `"type": "module"`, and forbids extensionless imports.
- **`bundler`** (TS 5.0+): Designed for frontend apps built with Vite, Rollup, Webpack, or esbuild. Respects `package.json` exports and conditional flags, but permits extensionless imports and arbitrary asset imports (`.css`, `.vue`, `.svg`).
- **`node10`** (formerly `node`): Legacy CommonJS resolution; ignores modern `exports` fields.

### The `types` Condition Ordering Rule
In `package.json`, conditions are evaluated in **first-to-match** order. The `"types"` condition must **always be declared first**:
```json
{
  "exports": {
    ".": {
      "types": "./dist/index.d.ts",
      "import": "./dist/index.js",
      "require": "./dist/index.cjs"
    }
  }
}
```
If `"types"` is placed after `"import"`, tools may resolve the `.js` file first and fail to find the type definitions!

---

## 3. How It Works Internally

```
Consumer: import { Button } from "my-ui/button"
                           |
                           v
              Reads package.json of "my-ui"
                           |
      Does "exports" contain "./button"?
                 /             \
               Yes              No (Throws TS2307 / Cannot find module)
               /
      Check Conditions:
      1. "types"?  =======> Match "./dist/button.d.ts"
      2. "import"?
      3. "require"?
```

When the compiler resolves a package import:
1. It looks up the package directory in `node_modules`.
2. It parses `package.json`. If `"exports"` is defined, legacy `"main"` and `"types"` fields at the root are completely ignored.
3. It checks the import path against the subpath keys in `"exports"`.
4. It evaluates conditions matching the current tsconfig compiler options (`types`, `import` or `require`, `node`, `browser`).

---

## 4. Syntax & API Reference

### Complete Dual-Package `package.json` Example
```json
{
  "name": "@acme/core",
  "version": "2.0.0",
  "type": "module",
  "exports": {
    ".": {
      "types": "./dist/esm/index.d.ts",
      "import": "./dist/esm/index.js",
      "require": "./dist/cjs/index.cjs"
    },
    "./utils": {
      "types": "./dist/esm/utils.d.ts",
      "import": "./dist/esm/utils.js"
    }
  }
}
```

---

## 5. Practical Examples

### 5.1 Importing Valid Subpaths
```typescript
import { CoreEngine } from "@acme/core";
import { formatString } from "@acme/core/utils";
```

### 5.2 Real-World Application: Multi-Platform Library Configuration
```json
{
  "name": "isomorphic-fetcher",
  "exports": {
    ".": {
      "types": "./dist/index.d.ts",
      "browser": {
        "import": "./dist/browser.js"
      },
      "node": {
        "import": "./dist/node.js"
      },
      "default": "./dist/fallback.js"
    }
  }
}
```

### 5.3 Edge Case: Subpath Patterns with Wildcards
```json
{
  "exports": {
    "./features/*": {
      "types": "./dist/features/*.d.ts",
      "import": "./dist/features/*.js"
    }
  }
}
```
Allows importing `my-pkg/features/auth` resolving to `./dist/features/auth.d.ts`.

### 5.4 Failure Case: Accessing Private Internal Path
```typescript
// @ts-expect-error TS2307: Cannot find module '@acme/core/dist/internal/secret.js' or its corresponding type declarations.
import { secret } from "@acme/core/dist/internal/secret.js";
```

### 5.5 Idiomatic Pattern: Selecting Resolution Mode for Frontend
```json
// tsconfig.json for Next.js / Vite projects:
{
  "compilerOptions": {
    "target": "ES2022",
    "module": "ESNext",
    "moduleResolution": "bundler",
    "verbatimModuleSyntax": true
  }
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas
1. **Root `types` Ignored When `exports` Exists**: If `"exports"` is present in `package.json`, TypeScript completely ignores the top-level `"types": "index.d.ts"` field! You MUST declare `"types"` inside `"exports"`.
2. **Order of Conditions Matters**: JSON object keys have defined iteration order in modern specifications. Placing `"types"` after `"import"` causes resolution failures in some toolchains.
3. **No Deep Private Imports**: Attempting to bypass `"exports"` to import internal files triggers runtime error `ERR_PACKAGE_PATH_NOT_EXPORTED`.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Problem | Solution | Compiler Error |
| :--- | :--- | :--- | :--- |
| Placing `"types"` condition after `"import"` | Bundlers/compilers match `"import"` first; types are lost | Always list `"types"` as the very first condition | Missing type declarations |
| Omitting `"types"` inside `"exports"` | Top-level `"types"` is ignored when `exports` is present | Add `"types"` property to every exported subpath | `TS7016: Could not find declaration file...` |
| Using `moduleResolution: node10` with modern packages | Fails to resolve `package.json` exports | Upgrade to `NodeNext` or `bundler` | Module not found errors |
| Reaching into private package paths | Blocked at runtime by Node.js package loader | Only import paths exposed in `"exports"` | `TS2307: Cannot find module...` |
| Forgetting extension in `NodeNext` subpath | Relative subpaths must match exact target extensions | Ensure target paths in `exports` specify extensions | Resolution failure |
| Dual-package hazard without shared state isolation | Loading both CJS and ESM duplicates singletons | Put shared singleton state in separate CJS module | Split application state |

---

## 8. Performance & Type-Checker Complexity
- `package.json` exports resolution avoids filesystem directory scanning by directing the compiler straight to designated paths.
- Wildcard subpath patterns (`./* -> ./*.d.ts`) execute in $O(1)$ string replacement time.

---

## 9. Compiler Internals
- Handled by `processExports` in `src/compiler/moduleSpecifiers.ts`.
- The compiler parses the package's `exports` field using Node.js's official subpath pattern matching specification.
- Conditions active in the type checker depend on `moduleResolution`: `NodeNext` activates `["types", "node", "import"|"require"]`; `bundler` activates `["types", "import", "browser"]`.

---

## 10. Best Practices & Design Guidelines
- Always declare `"types"` as the very first key within each export condition block.
- For backend services on Node.js, use `moduleResolution: NodeNext`.
- For web applications built with Vite/Rollup/Next.js, use `moduleResolution: bundler`.
- Use `"exports"` to strictly encapsulate library internals.

---

## 11. Real-World Case Study
### Diagnosing Missing Types in Published Enterprise NPM Package
A component library author published `@org/design-system` with:
```json
{
  "types": "./dist/index.d.ts",
  "exports": {
    ".": "./dist/index.js",
    "./button": "./dist/button.js"
  }
}
```
**Symptom:** Consumers using `NodeNext` reported error `TS7016: Could not find declaration file for module '@org/design-system'`.

**Fix:**
Because `exports` was defined, the root `"types"` was ignored. The author updated `package.json`:
```json
{
  "exports": {
    ".": {
      "types": "./dist/index.d.ts",
      "import": "./dist/index.js"
    },
    "./button": {
      "types": "./dist/button.d.ts",
      "import": "./dist/button.js"
    }
  }
}
```
TypeScript immediately resolved all types.

---

## 12. Interview Questions

<details>
<summary>1. What is the difference between <code>moduleResolution: "NodeNext"</code> and <code>"bundler"</code>?</summary>

`NodeNext` strictly follows Node.js ECMAScript Modules rules: relative imports require explicit file extensions (`./file.js`), and `package.json` exports are strictly enforced. `bundler` (TS 5.0+) respects modern `package.json` exports and conditions, but allows extensionless imports and arbitrary non-JS assets typical in bundler setups like Vite or Webpack.
</details>

<details>
<summary>2. Why must the <code>"types"</code> condition be listed first in <code>package.json</code> exports?</summary>

Node.js and TypeScript evaluate conditional export keys in object iteration order. If `"import"` or `"require"` appears before `"types"`, the resolver may match the runtime JavaScript file first, bypassing the type declaration.
</details>

<details>
<summary>3. What happens to the root <code>"types"</code> field when <code>"exports"</code> is defined in <code>package.json</code>?</summary>

In modern TypeScript resolution modes (`Node16`, `NodeNext`, `bundler`), defining `"exports"` causes the compiler to completely ignore the root `"types"` and `"main"` fields. Types must be declared inside the `"exports"` structure.
</details>

<details>
<summary>4. What error does Node.js throw if an external consumer attempts to import a private subpath?</summary>

`ERR_PACKAGE_PATH_NOT_EXPORTED`. TypeScript reflects this by emitting compile error `TS2307: Cannot find module or its corresponding type declarations`.
</details>

<details>
<summary>5. How do wildcard subpath exports work?</summary>

Using the `*` wildcard:
```json
"./components/*": { "types": "./dist/*.d.ts", "import": "./dist/*.js" }
```
Importing `pkg/components/card` maps directly to `./dist/card.d.ts`.
</details>

<details>
<summary>6. What does <code>package.json</code>'s <code>"type": "module"</code> signify?</summary>

It tells Node.js and TypeScript that all `.js` files in the package should be treated as native ES Modules rather than CommonJS.
</details>

<details>
<summary>7. How do you declare different types for CommonJS and ESM consumers of the same package?</summary>

By nesting `"types"` under the respective condition:
```json
".": {
  "import": { "types": "./dist/esm/index.d.ts", "default": "./dist/esm/index.js" },
  "require": { "types": "./dist/cjs/index.d.cts", "default": "./dist/cjs/index.cjs" }
}
```
</details>

<details>
<summary>8. What compiler option enables checking that your package exports are valid?</summary>

Tools like `@arethetypeswrong/cli` (`attw`) inspect published packages and verify that all conditional exports and declaration files resolve correctly across all TypeScript resolution modes.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Correct Condition Ordering (Easy)
Fix the ordering bug in this `package.json` snippet:
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
<details>
<summary>Solution</summary>

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

### Exercise 2: Configure Bundler Resolution (Easy)
Configure `tsconfig.json` for a modern Vite + React frontend application.
<details>
<summary>Solution</summary>

```json
{
  "compilerOptions": {
    "target": "ES2022",
    "module": "ESNext",
    "moduleResolution": "bundler",
    "jsx": "react-jsx",
    "strict": true
  }
}
```
</details>

### Exercise 3: Multi-Subpath Library Package.json (Medium)
Create a `package.json` with three exported subpaths: `. `, `./auth`, and `./database`.
<details>
<summary>Solution</summary>

```json
{
  "name": "@my-corp/backend-kit",
  "type": "module",
  "exports": {
    ".": {
      "types": "./dist/index.d.ts",
      "import": "./dist/index.js"
    },
    "./auth": {
      "types": "./dist/auth/index.d.ts",
      "import": "./dist/auth/index.js"
    },
    "./database": {
      "types": "./dist/database/index.d.ts",
      "import": "./dist/database/index.js"
    }
  }
}
```
</details>

### Exercise 4: Testing Package Exports with `attw` (Medium)
Run the Are The Types Wrong CLI tool in an npm script to audit package exports before publishing.
<details>
<summary>Solution</summary>

```json
{
  "scripts": {
    "check-types": "attw --pack ."
  }
}
```
</details>

### Exercise 5: Dual CJS/ESM Type Mapping (Hard)
Author declaration paths for a dual-format package where CommonJS uses `.d.cts` and ESM uses `.d.mts`.
<details>
<summary>Solution</summary>

```json
{
  "name": "isomorphic-crypto",
  "exports": {
    ".": {
      "import": {
        "types": "./dist/esm/index.d.mts",
        "default": "./dist/esm/index.mjs"
      },
      "require": {
        "types": "./dist/cjs/index.d.cts",
        "default": "./dist/cjs/index.cjs"
      }
    }
  }
}
```
</details>

### Exercise 6: Internal Subpath Resolver Implementation (Hard)
Write a TypeScript function that resolves an import specifier against a mock `package.json` `exports` map according to Node.js subpath rules.
<details>
<summary>Solution</summary>

```typescript
interface ExportMap {
  [subpath: string]: { types?: string; import?: string; default?: string } | string;
}

function resolveSubpath(specifier: string, exports: ExportMap): string | null {
  const entry = exports[specifier];
  if (!entry) return null;
  if (typeof entry === "string") return entry;
  return entry.types || entry.import || entry.default || null;
}

const map: ExportMap = {
  ".": { types: "./dist/index.d.ts", import: "./dist/index.js" },
  "./utils": { types: "./dist/utils.d.ts", import: "./dist/utils.js" }
};

console.log(resolveSubpath("./utils", map)); // "./dist/utils.d.ts"
```
</details>
