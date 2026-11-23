# TypesVersions and Declaration Downleveling

> Managing multi-version TypeScript support: `typesVersions` in `package.json`, declaration downleveling via `downlevel-dts`, and backwards compatibility strategies for library authors.

| Metadata | Value |
| :--- | :--- |
| **Level** | Advanced |
| **Prerequisites** | [00-declaration-files.md](file:///E:/Learning/engineering-notes/01-languages/typescript/09-declaration-files/00-declaration-files.md), [05-package-json-exports-and-resolution-modes.md](file:///E:/Learning/engineering-notes/01-languages/typescript/08-modules/05-package-json-exports-and-resolution-modes.md) |
| **Time to Read** | 22 minutes |
| **Status** | Complete |
| **TypeScript Baseline** | Since TS 3.1 / TS 5.x / TS 7.0 Native |
| **Tested tsconfig** | `declaration: true` |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & API Reference](#4-syntax--api-reference)
5. [Practical Examples](#5-practical-examples)
6. [Edge Cases, Pitfalls & Gotchas](#6-edge-cases-pitfalls--gotchas)
7. [Common Mistakes & Anti-Patterns](#7-Common-mistakes--anti-patterns)
8. [Performance & Type-Checker Complexity](#8-performance--type-checker-complexity)
9. [Compiler Internals](#9-compiler-internals)
10. [Best Practices & Design Guidelines](#10-best-practices--design-guidelines)
11. [Real-World Case Study](#11-real-world-case-study)
12. [Interview Questions](#12-interview-questions)
13. [Exercises & Challenges](#13-exercises--challenges)

---

## 1. What & Why
TypeScript evolves rapidly, regularly introducing new syntax and type features:
- TS 4.1 introduced template literal types.
- TS 4.7 introduced variance annotations (`in` / `out`).
- TS 5.0 introduced const type parameters (`<const T>`).

When a library author adopts these modern features in their published `.d.ts` files, consumers running older versions of TypeScript (e.g. TS 4.5) will encounter syntax errors during type checking:
```
error TS1005: '=' expected (on <const T>)
```

To allow libraries to use cutting-edge TypeScript features while remaining compatible with older compiler versions, TypeScript introduced **`typesVersions`** in `package.json`, and the community standard tool **`downlevel-dts`**.

---

## 2. Core Concepts

### What is `typesVersions`?
`typesVersions` is a configuration field in `package.json` that directs the TypeScript compiler to different declaration files based on the consumer's TypeScript version:
```json
{
  "name": "my-library",
  "types": "./dist/index.d.ts",
  "typesVersions": {
    "<5.0": {
      "*": ["./ts4.9/*"]
    }
  }
}
```
If a consumer runs TypeScript 4.9, the compiler reads declarations from `./ts4.9/`. If the consumer runs TypeScript 5.0+, it reads modern declarations from `./dist/`.

### Declaration Downleveling
**`downlevel-dts`** is an automated tool that transpiles modern `.d.ts` syntax into older, backwards-compatible equivalent syntax:
- Rewrites `Awaited<T>` to fallback conditional types.
- Rewrites `<const T>` to standard generics with `readonly` constraints.
- Strips `in` / `out` variance annotations.

---

## 3. How It Works Internally

```
Consumer runs 'tsc' with TS version 4.8
                      |
                      v
          Reads package.json of dependency
                      |
Does package.json have "typesVersions"?
          /                       \
        Yes                        No
        /                           \
Matches range "<5.0"?           Use default "types"
        /
Redirects import path:
index.d.ts ===> ./ts4.8/index.d.ts (Downleveled types)
```

During module resolution, TypeScript checks its own internal version (`ts.version`) against the semver ranges defined in `typesVersions`. The first matching range redirects all declaration lookups to the specified target directory.

---

## 4. Syntax & API Reference

### Complete `package.json` Configuration
```json
{
  "name": "@org/resilient-lib",
  "version": "2.0.0",
  "main": "./dist/index.js",
  "types": "./dist/index.d.ts",
  "typesVersions": {
    "<4.8": {
      "*": ["./dist/ts3.4/*"]
    },
    "<5.0": {
      "*": ["./dist/ts4.8/*"]
    }
  }
}
```

---

## 5. Practical Examples

### 5.1 Downleveling `const` Type Parameters
Modern TypeScript (TS 5.0+):
```typescript
export declare function defineRoutes<const T extends readonly string[]>(routes: T): T;
```

Downleveled for TS < 5.0:
```typescript
export declare function defineRoutes<T extends readonly string[]>(routes: T): T;
```

### 5.2 Real-World Application: Automated Build Script
```json
{
  "scripts": {
    "build": "tsc && npm run downlevel",
    "downlevel": "downlevel-dts dist/ dist/ts4.8/ --to=4.8"
  }
}
```

### 5.3 Edge Case: Interaction with `package.json` `"exports"`
When `"exports"` is present in `package.json`, modern TypeScript uses conditional exports. `typesVersions` can still be used as a fallback for tools that do not support modern export conditions.

### 5.4 Failure Case: Unsorted Semver Ranges
```json
// WRONG: Upper range matches first!
"typesVersions": {
  "<5.0": { "*": ["ts4.9/*"] },
  "<4.5": { "*": ["ts4.0/*"] } // Never reached because <5.0 matches TS 4.4!
}
```

### 5.5 Idiomatic Pattern: Downleveling Variance Annotations
Modern (TS 4.7+):
```typescript
export interface ReadStream<out T> {
  read(): T;
}
```
Downleveled for TS < 4.7:
```typescript
export interface ReadStream<T> {
  read(): T;
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas
1. **Range Ordering**: Ranges must be listed in descending order (highest version to lowest version) or most specific to least specific.
2. **Path Wildcards**: The key `"*"` acts as a catch-all wildcard for subpath imports.
3. **Dual Downleveling Overhead**: Publishing downleveled types increases package tarball size on npm. Only downlevel when targeting enterprise consumers on older LTS compilers.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Problem | Solution | Compiler Error |
| :--- | :--- | :--- | :--- |
| Inverting version range order | More general range intercepts older versions | List ranges in proper priority order | Consumers get incompatible types |
| Manually writing downleveled types | Labor-intensive and prone to drift | Use `downlevel-dts` in build pipeline | Signature desynchronization |
| Omitting fallback `types` field | Older tools that don't know `typesVersions` fail | Always include top-level `"types"` | Module not found |
| Supporting ancient versions indefinitely | Bloats library maintenance burden | Support current TS + last 2 minor LTS releases | High maintenance drag |
| Breaking subpath resolution | Wildcard `"*"` misconfigured in mapping | Use `"*": ["./dist/ts3.4/*"]` | `TS2307: Cannot find module` |

---

## 8. Performance & Type-Checker Complexity
- Resolving `typesVersions` involves a quick semver check during initial package inspection ($O(1)$).
- Downleveled types are often slightly simpler syntactically, reducing type-checker evaluation overhead on older compilers.

---

## 9. Compiler Internals
- In `src/compiler/moduleSpecifiers.ts`, `getPackageJsonTypesVersionsPaths` parses the `typesVersions` field.
- `ts.VersionRange` compares the running compiler version against declared range keys.

---

## 10. Best Practices & Design Guidelines
- Adopt `downlevel-dts` if your library targets enterprise consumers bound to older TypeScript versions.
- Cleanly deprecate versions of TypeScript that have reached end-of-life according to the official TypeScript release schedule.
- Test your published package against older TypeScript versions in CI using a matrix build.

---

## 11. Real-World Case Study
### DefinitelyTyped Multi-Version Support for `@types/node`
DefinitelyTyped uses `typesVersions` extensively:
```json
{
  "types": "index.d.ts",
  "typesVersions": {
    "<=4.5": {
      "*": ["ts4.5/*"]
    }
  }
}
```
This architecture allows millions of Node.js developers on different TypeScript versions to consume official Node typings without syntax errors.

---

## 12. Interview Questions

<details>
<summary>1. What problem does <code>typesVersions</code> solve in <code>package.json</code>?</summary>

It allows a library to provide different `.d.ts` declaration files tailored to the specific version of TypeScript running on the consumer's machine, preventing syntax errors when consumers use older compilers that don't understand newer syntax features.
</details>

<details>
<summary>2. What is <code>downlevel-dts</code>?</summary>

`downlevel-dts` is a build-time CLI tool that converts modern TypeScript declaration files into backwards-compatible declaration files targeting older versions of TypeScript (e.g. TS 3.4, 4.0, 4.5).
</details>

<details>
<summary>3. What happens if a consumer's TypeScript version does not match any range in <code>typesVersions</code>?</summary>

The compiler falls back to the default `"types"` (or `"main"`) declaration file specified in `package.json`.
</details>

<details>
<summary>4. How does <code>typesVersions</code> handle wildcard subpaths?</summary>

By specifying `"*": ["./tsX.Y/*"]`, any subpath imported by the consumer (e.g. `pkg/utils`) is mapped to the corresponding file in the downleveled folder (`pkg/tsX.Y/utils.d.ts`).
</details>

<details>
<summary>5. Name three TypeScript syntax features that commonly require downleveling for older compilers.</summary>

1. Template literal types (``prefixed_${string}``, TS 4.1).
2. Variance annotations (`in` and `out`, TS 4.7).
3. Const type parameters (`<const T>`, TS 5.0).
</details>

<details>
<summary>6. How should version ranges be ordered in <code>typesVersions</code>?</summary>

They should be ordered from most specific to least specific, or descending semver order, because TypeScript checks ranges sequentially and selects the first match.
</details>

<details>
<summary>7. Does <code>typesVersions</code> affect JavaScript runtime execution?</summary>

No. `typesVersions` is exclusively a TypeScript compiler feature and is completely ignored by Node.js, browsers, and JavaScript bundlers.
</details>

<details>
<summary>8. How can you verify that downleveled types work across multiple TypeScript versions?</summary>

By running automated test scripts in CI across a matrix of TypeScript versions (`npm install typescript@4.8`, `npm install typescript@5.0`, etc.).
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Configure TypesVersions for TS < 5.0 (Easy)
Add a `typesVersions` field to `package.json` routing TS < 5.0 to `./ts4.9/*`.
<details>
<summary>Solution</summary>

```json
{
  "types": "./dist/index.d.ts",
  "typesVersions": {
    "<5.0": {
      "*": ["./ts4.9/*"]
    }
  }
}
```
</details>

### Exercise 2: Downlevel Variance Annotation (Easy)
Manually downlevel `interface Producer<out T> { get(): T; }` for TS 4.5.
<details>
<summary>Solution</summary>

```typescript
interface Producer<T> {
  get(): T;
}
```
</details>

### Exercise 3: Downlevel-dts Build Script (Medium)
Add npm build scripts to compile types and generate downleveled versions for TS 4.5.
<details>
<summary>Solution</summary>

```json
{
  "scripts": {
    "build:types": "tsc --emitDeclarationOnly",
    "downlevel": "downlevel-dts dist/ dist/ts4.5/ --to=4.5",
    "build": "npm run build:types && npm run downlevel"
  }
}
```
</details>

### Exercise 4: Multi-Range Mapping (Medium)
Configure `typesVersions` supporting `<4.5` and `<5.0` distinct targets.
<details>
<summary>Solution</summary>

```json
{
  "typesVersions": {
    "<4.5": {
      "*": ["./dist/ts4.0/*"]
    },
    "<5.0": {
      "*": ["./dist/ts4.5/*"]
    }
  }
}
```
</details>

### Exercise 5: Automated Semver Range Matcher (Hard)
Write a TypeScript function that takes a TS version string (e.g. `"4.9.5"`) and matches it against a mock `typesVersions` mapping.
<details>
<summary>Solution</summary>

```typescript
function matchTypesVersion(version: string, mappings: Record<string, string[]>): string | null {
  const [major, minor] = version.split(".").map(Number);
  const current = major * 10 + minor;

  for (const range of Object.keys(mappings)) {
    if (range.startsWith("<")) {
      const target = parseFloat(range.slice(1)) * 10;
      if (current < target) {
        return mappings[range][0];
      }
    }
  }
  return null;
}

const map = { "<5.0": ["./ts4.9/*"], "<4.5": ["./ts4.0/*"] };
console.log(matchTypesVersion("4.8.2", map)); // "./ts4.9/*"
```
</details>

### Exercise 6: Complete Downleveling CI Matrix Configuration (Hard)
Write a GitHub Actions CI workflow snippet testing a package against TypeScript 4.8, 5.0, and 5.4.
<details>
<summary>Solution</summary>

```yaml
jobs:
  test-types:
    runs-on: ubuntu-latest
    strategy:
      matrix:
        ts-version: ["4.8.4", "5.0.4", "5.4.5"]
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-node@v4
        with:
          node-version: 20
      - run: npm ci
      - run: npm install -D typescript@${{ matrix.ts-version }}
      - run: npx tsc --noEmit
```
</details>
