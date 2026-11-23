# DefinitelyTyped Workflow and Best Practices

> Architecture and contributor methodology for DefinitelyTyped (`@types/`): package structure, tests with `dtslint`, PR approval workflows, and automated npm publishing.

| Metadata | Value |
| :--- | :--- |
| **Level** | Advanced |
| **Prerequisites** | [04-third-party-types.md](file:///E:/Learning/engineering-notes/01-languages/typescript/09-declaration-files/04-third-party-types.md), [05-authoring-declarations-for-javascript-libraries.md](file:///E:/Learning/engineering-notes/01-languages/typescript/09-declaration-files/05-authoring-declarations-for-javascript-libraries.md) |
| **Time to Read** | 24 minutes |
| **Status** | Complete |
| **TypeScript Baseline** | Since TS 2.0 / TS 5.x / TS 7.0 Native |
| **Tested tsconfig** | `strict: true`, `noImplicitAny: true` |

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
**DefinitelyTyped** is one of the largest and most active open-source repositories on GitHub. It hosts ambient type declarations for thousands of JavaScript libraries that don't ship their own types.

When a pull request merges into DefinitelyTyped:
- Automated bots (`DefinitelyTyped-bot`) validate review approvals.
- Microsoft's automated publication pipeline (**types-publisher**) builds, tests, and publishes the package to npm under the `@types/` scope within hours.

Understanding DefinitelyTyped's strict directory structure, header conventions, test files, and review rules is essential for contributing and maintaining enterprise types.

---

## 2. Core Concepts

### Package Folder Structure
In DefinitelyTyped (`types/<package-name>/`):
```
types/my-library/
  index.d.ts           <-- The main declaration file
  my-library-tests.ts  <-- Type test file (verified by dtslint)
  tsconfig.json       <-- Standard tsconfig for the package
  package.json        <-- Metadata & contributor list
```

### The Header Convention
Every `index.d.ts` in DefinitelyTyped begins with a standardized comment header:
```typescript
// Type definitions for my-library 1.2
// Project: https://github.com/author/my-library
// Definitions by: Ada Lovelace <https://github.com/ada>
//                 Alan Turing <https://github.com/alan>
// Definitions: https://github.com/DefinitelyTyped/DefinitelyTyped
// TypeScript Version: 4.5
```

---

## 3. How It Works Internally

```
Contributor submits PR to DefinitelyTyped/DefinitelyTyped
                          |
                          v
        Automated CI runs dtslint & tsc
                          |
             Are all type tests passing?
                     /          \
                   Yes           No (CI Fails)
                   /
    PR Approved by Maintainers / DT Bot
                   |
                   v
           Merge into 'master'
                   |
                   v
   Microsoft types-publisher Service
  - Bumps patch version
  - Publishes to npm under @types/my-library
```

The publication pipeline is fully automated. Merging a PR into `master` triggers an automated build that creates an npm package with version matching the library header (e.g. `1.2.x`).

---

## 4. Syntax & API Reference

### Writing Tests in `*-tests.ts`
Tests in DefinitelyTyped are pure TypeScript files that exercise the API. They are compiled with `tsc --noEmit` to ensure that valid code compiles and invalid code is rejected:
```typescript
import { calculate, type Options } from "my-library";

// Valid usage test:
const opts: Options = { timeout: 1000 };
const result: number = calculate(10, 20, opts);

// Failure test (verified using $ExpectError or @ts-expect-error):
// @ts-expect-error
calculate("invalid", "args");
```

---

## 5. Practical Examples

### 5.1 Standard `package.json` in DefinitelyTyped
```json
{
  "private": true,
  "name": "@types/simple-slug",
  "version": "1.0.9999",
  "devDependencies": {
    "@types/simple-slug": "workspace:."
  },
  "owners": [
    {
      "name": "Ada Lovelace",
      "githubUsername": "ada"
    }
  ]
}
```

### 5.2 Real-World Application: Complete Package Submission
```typescript
// types/uuid-formatter/index.d.ts
// Type definitions for uuid-formatter 2.0
// Project: https://github.com/org/uuid-formatter
// Definitions by: Dev Team <https://github.com/devteam>
// Definitions: https://github.com/DefinitelyTyped/DefinitelyTyped

export interface FormatOptions {
  uppercase?: boolean;
  hyphens?: boolean;
}

export function formatUuid(raw: string, options?: FormatOptions): string;
```

### 5.3 Edge Case: Multi-Version Declarations (`tsX.Y` folders)
When supporting older TypeScript versions in DefinitelyTyped, sub-folders named `tsX.Y` (e.g. `ts4.5/`) host the downleveled declaration files.

### 5.4 Failure Case: Disallowed `any` under `dtslint`
```typescript
// dtslint rule: no-unnecessary-generics or no-explicit-any
// @ts-expect-error
export function run(arg: any): any; // dtslint fails! Must use unknown or generic!
```

### 5.5 Idiomatic Pattern: `tsconfig.json` in DefinitelyTyped
```json
{
  "compilerOptions": {
    "module": "commonjs",
    "lib": ["es6"],
    "noImplicitAny": true,
    "noImplicitThis": true,
    "strictNullChecks": true,
    "strictFunctionTypes": true,
    "baseUrl": "../",
    "typeRoots": ["../"],
    "types": [],
    "noEmit": true,
    "forceConsistentCasingInFileNames": true
  },
  "files": ["index.d.ts", "uuid-formatter-tests.ts"]
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas
1. **Strict Linter Rules**: DefinitelyTyped enforces strict ESLint rules (forbidding `Function`, `Object`, `any`, and unconstrained generics).
2. **Author Notifications**: When updating existing definitions, DefinitelyTyped bots automatically tag original authors for review approval.
3. **No External Dependencies**: Declarations cannot introduce arbitrary npm runtime dependencies; they can only depend on other `@types/` packages.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Problem | Solution | Compiler Error |
| :--- | :--- | :--- | :--- |
| Missing header comment | Types-publisher cannot determine version | Include standardized header block | DT linter error: Missing header |
| Using `any` where `unknown` suffices | Fails DT strict linting | Use `unknown` or generic parameter | `dtslint: Avoid the 'any' type` |
| Submitting PR for package that bundles types | Creates conflicting duplicate types | Submit PR to package's own repo instead | Rejected by DT maintainers |
| Forgetting test file (`*-tests.ts`) | PR cannot be verified by CI | Provide complete test file exercising API | Missing test file error |
| Using `export default` for CJS module | Breaks CJS interop | Use `export =` syntax | Incompatible export check |
| Using global types in modular declaration | Pollutes global scope for all consumers | Keep types encapsulated inside module | Global namespace pollution |

---

## 8. Performance & Type-Checker Complexity
- DefinitelyTyped contains millions of lines of code. Tests are sharded across CI runners.
- Local verification with `pnpm test <package-name>` tests only your specific package folder, executing in seconds.

---

## 9. Compiler Internals
- `dtslint` wraps `typescript-eslint` and the TypeScript compiler API.
- It parses type assertions like `// $ExpectType string` by evaluating type nodes at the corresponding text position in `checker.ts`.

---

## 10. Best Practices & Design Guidelines
- Run local tests before submitting PR: `pnpm test <package-name>`.
- Provide comprehensive tests in `*-tests.ts` covering edge cases and invalid invocations.
- Always prefer bundled types in your own packages over DefinitelyTyped when possible.

---

## 11. Real-World Case Study
### Contributing Type Definitions for a Legacy Redis Plugin
A developer needed types for an unmaintained Redis cache library `redis-sliding-cache`.

**Process:**
1. Forked `DefinitelyTyped/DefinitelyTyped`.
2. Created `types/redis-sliding-cache/` with `index.d.ts`, `tsconfig.json`, and `redis-sliding-cache-tests.ts`.
3. Ran `pnpm test redis-sliding-cache` locally.
4. Opened PR. The DT bot tagged community members, CI validated `dtslint` in 4 minutes, maintainers merged the PR, and `@types/redis-sliding-cache` was live on npm in under 2 hours.

---

## 12. Interview Questions

<details>
<summary>1. What is DefinitelyTyped and how does it relate to <code>@types/</code> on npm?</summary>

DefinitelyTyped is the central open-source monorepo where community members author declaration files for untyped JavaScript libraries. An automated Microsoft pipeline publishes every merged package to npm under the `@types/` scope.
</details>

<details>
<summary>2. What files are required for a new package in DefinitelyTyped?</summary>

1. `index.d.ts` (the declaration file with standard header).
2. `<package-name>-tests.ts` (type tests).
3. `tsconfig.json` (compiler configuration).
4. `package.json` (package metadata and owners list).
</details>

<details>
<summary>3. What is the header comment convention in DefinitelyTyped?</summary>

It specifies the package name, major.minor version, project URL, and list of GitHub contributor usernames. The types-publisher reads this header to determine which version to publish to npm.
</details>

<details>
<summary>4. What tool does DefinitelyTyped use to test type definitions?</summary>

`dtslint`, which runs ESLint with specialized TypeScript rules and type-checking tests.
</details>

<details>
<summary>5. When should a library author publish to DefinitelyTyped vs bundling types?</summary>

Package authors should **always bundle types directly** in their own package whenever possible. DefinitelyTyped is intended for third-party, legacy, or unmaintained libraries where the author does not want to bundle types.
</details>

<details>
<summary>6. How do you test that an expression triggers a compile error in DefinitelyTyped?</summary>

Using `// @ts-expect-error` directly above the invalid expression in the test file.
</details>

<details>
<summary>7. How does the DefinitelyTyped bot handle PR reviews?</summary>

It inspects the PR diff, identifies existing package owners from `package.json`, mentions them for review, and automatically merges the PR once owners or maintainers approve.
</details>

<details>
<summary>8. Can declaration files in DefinitelyTyped import other npm libraries?</summary>

They can only import other `@types/` packages (e.g. `@types/node`). They cannot depend on arbitrary runtime npm packages.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Standard DefinitelyTyped Header (Easy)
Write the standard header comment for `quick-math` version 2.1.
<details>
<summary>Solution</summary>

```typescript
// Type definitions for quick-math 2.1
// Project: https://github.com/example/quick-math
// Definitions by: Jane Doe <https://github.com/janedoe>
// Definitions: https://github.com/DefinitelyTyped/DefinitelyTyped
```
</details>

### Exercise 2: Test File with Expected Failure (Easy)
Write a test in `*-tests.ts` verifying that a function `multiply(a: number, b: number)` rejects strings.
<details>
<summary>Solution</summary>

```typescript
import { multiply } from "quick-math";

// Valid call:
const res: number = multiply(2, 4);

// Expected failure:
// @ts-expect-error
multiply("2", "4");
```
</details>

### Exercise 3: Package.json Configuration for DT (Medium)
Create a valid `package.json` for a DefinitelyTyped submission.
<details>
<summary>Solution</summary>

```json
{
  "private": true,
  "name": "@types/mini-calc",
  "version": "1.0.9999",
  "devDependencies": {
    "@types/mini-calc": "workspace:."
  },
  "owners": [
    {
      "name": "Jane Doe",
      "githubUsername": "janedoe"
    }
  ]
}
```
</details>

### Exercise 4: DT Compliant tsconfig.json (Medium)
Create a minimal valid `tsconfig.json` for a DefinitelyTyped package directory.
<details>
<summary>Solution</summary>

```json
{
  "compilerOptions": {
    "module": "commonjs",
    "lib": ["es6"],
    "noImplicitAny": true,
    "noImplicitThis": true,
    "strictNullChecks": true,
    "strictFunctionTypes": true,
    "baseUrl": "../",
    "typeRoots": ["../"],
    "types": [],
    "noEmit": true
  },
  "files": ["index.d.ts", "mini-calc-tests.ts"]
}
```
</details>

### Exercise 5: Replace Prohibited `Function` Type (Hard)
Refactor a legacy signature `export function addMiddleware(fn: Function): void` to pass strict `dtslint` rules.
<details>
<summary>Solution</summary>

```typescript
export type MiddlewareCallback = (...args: any[]) => void | Promise<void>;
export function addMiddleware(fn: MiddlewareCallback): void;
```
</details>

### Exercise 6: Complete DT Validation Simulation (Hard)
Write a node script that verifies a package folder in DefinitelyTyped contains all required files and a valid header.
<details>
<summary>Solution</summary>

```typescript
import fs from "fs";
import path from "path";

function validateDtPackage(dir: string): boolean {
  const indexDts = path.join(dir, "index.d.ts");
  const tsconfig = path.join(dir, "tsconfig.json");
  const pkgJson = path.join(dir, "package.json");

  if (!fs.existsSync(indexDts) || !fs.existsSync(tsconfig) || !fs.existsSync(pkgJson)) {
    console.error("Missing required DT files!");
    return false;
  }

  const content = fs.readFileSync(indexDts, "utf8");
  if (!content.startsWith("// Type definitions for ")) {
    console.error("Missing or invalid DefinitelyTyped header comment in index.d.ts");
    return false;
  }

  console.log("DT Package Structure Valid!");
  return true;
}
```
</details>
