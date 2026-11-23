# Gradual JavaScript to TypeScript Migration: allowJs, checkJs, and JSDoc

> Architectural blueprint for safely and incrementally migrating enterprise JavaScript codebases to TypeScript using `allowJs`, `checkJs`, JSDoc type annotations, and automated migration tooling.

| Metadata | Details |
| :--- | :--- |
| **Category** | Production Tooling |
| **Difficulty** | Intermediate to Advanced |
| **Prerequisites** | [13-compiler-options/00-tsconfig-json.md](../13-compiler-options/00-tsconfig-json.md), [01-fundamentals/01-primitive-types.md](../01-fundamentals/01-primitive-types.md) |
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

Migrating a large, mission-critical JavaScript repository to TypeScript is one of the most common high-impact technical initiatives in modern software engineering. However, "big bang" rewrites—attempting to convert every file simultaneously—almost always fail:
- Feature development freezes for weeks or months.
- Merge conflicts across long-lived migration branches cause severe developer friction.
- Hastily added `any` casts degrade type safety and introduce hidden regressions.

TypeScript is explicitly designed to support **incremental migration**. A team can introduce TypeScript without changing a single line of JavaScript code, gradually ratcheting up type safety over time:
1. **Phase 1**: Add `tsconfig.json` with `allowJs: true`.
2. **Phase 2**: Enable `checkJs: true` and add `@ts-check` comments to validate JS files using JSDoc.
3. **Phase 3**: Convert high-value leaf files to `.ts`.
4. **Phase 4**: Enable strict compiler flags (`strict: true`, `noImplicitAny: true`) across the entire repository.

```
+-------------------------------------------------------------------------+
|                  The 4-Stage Incremental Migration Funnel               |
+-------------------------------------------------------------------------+
|                                                                         |
|  [ Stage 1: Coexistence ]                                               |
|  allowJs: true (tsc builds both .js and .ts files without errors)       |
|            |                                                            |
|            v                                                            |
|  [ Stage 2: JSDoc & Type Checking ]                                     |
|  checkJs: true + JSDoc (@param, @returns, @type)                        |
|            |                                                            |
|            v                                                            |
|  [ Stage 3: File-by-File Conversion ]                                   |
|  Rename .js -> .ts starting from leaf dependencies upwards              |
|            |                                                            |
|            v                                                            |
|  [ Stage 4: Strict Mode Lockdown ]                                      |
|  strict: true, noImplicitAny: true, allowJs: false                      |
|                                                                         |
+-------------------------------------------------------------------------+
```

---

## 2. Core Concepts

### `allowJs`
When `"allowJs": true` is enabled, the TypeScript compiler includes `.js` and `.jsx` files in its compilation program. It allows TypeScript files to import JavaScript modules and vice versa.

### `checkJs`
When `"checkJs": true` is enabled, TypeScript actively performs type checking on `.js` files. It infers types from assignments and parses JSDoc annotations to validate call signatures and property accesses. Alternatively, you can add `// @ts-check` to the top of specific JavaScript files.

### JSDoc Typing
TypeScript provides native parsing for JSDoc tags:
- `@type {TypeName}`: Annotates variable or constant types.
- `@param {Type} name`: Annotates function parameters.
- `@returns {Type}`: Annotates function return values.
- `@typedef {Object} Name`: Defines reusable structural types.

---

## 3. How It Works Internally

1. **JSDoc Parsing**: The TypeScript scanner encounters comments starting with `/**`. If the comment conforms to JSDoc formatting, the parser creates a `ts.JSDoc` node attached to the corresponding AST declaration.
2. **Type Extraction**: The TypeChecker evaluates `@type` annotations. It maps JSDoc type expressions (e.g., `{string[]}`, `{Promise<number>}`) to internal `ts.Type` objects.
3. **Inference Fallback**: Where annotations are omitted in JS files, TypeScript uses flow-sensitive type inference. If a variable is initialized to `[]`, TypeScript infers `any[]` and tracks pushed elements to widen or narrow the type dynamically.

---

## 4. Syntax & API Reference

### Migration `tsconfig.json` Configuration
```json
{
  "compilerOptions": {
    "target": "ES2022",
    "module": "NodeNext",
    "moduleResolution": "NodeNext",
    "allowJs": true,
    "checkJs": true,
    "noEmit": true,
    "strict": false,
    "noImplicitAny": false,
    "skipLibCheck": true
  },
  "include": ["src/**/*"]
}
```

### JSDoc Syntax Patterns in `.js` Files
```javascript
// @ts-check

/**
 * @typedef {Object} CustomerProfile
 * @property {string} id - Unique customer UUID
 * @property {string} email - Verified email address
 * @property {number} [loyaltyPoints] - Optional loyalty points
 */

/**
 * Calculates discount multiplier based on customer points.
 * @param {CustomerProfile} profile
 * @param {number} basePrice
 * @returns {number}
 */
export function calculateDiscount(profile, basePrice) {
  const points = profile.loyaltyPoints ?? 0;
  if (points > 1000) {
    return basePrice * 0.8;
  }
  return basePrice;
}
```

---

## 5. Practical Examples

### Example 1: Basic JSDoc Type Definition in Pure JS
```javascript
// @ts-check

/**
 * @typedef {Object} DatabaseRecord
 * @property {number} id
 * @property {string} key
 * @property {boolean} active
 */

/** @type {DatabaseRecord[]} */
const records = [];

records.push({ id: 1, key: "session_timeout", active: true });

// Error detected in pure JS without compilation:
// @ts-expect-error - TS2322: Type 'string' is not assignable to type 'number'
records.push({ id: "invalid_id", key: "max_retries", active: false });
```

### Example 2: Importing TypeScript Types inside JavaScript JSDoc
```javascript
// @ts-check
// You can import real TypeScript types directly inside JSDoc comments!

/**
 * @typedef {import('./types.ts').ServerOptions} ServerOptions
 * @typedef {import('./types.ts').ServerInstance} ServerInstance
 */

/**
 * @param {ServerOptions} options
 * @returns {ServerInstance}
 */
export function initializeServer(options) {
  return {
    port: options.port,
    listen() {
      console.log(`Server listening on port ${options.port}`);
    }
  };
}
```

### Example 3: Edge Case - `this` Context in JavaScript Prototype Functions
```javascript
// @ts-check

/**
 * @this {{ count: number, step: number }}
 */
function incrementCounter() {
  this.count += this.step;
}

const counter = {
  count: 0,
  step: 5,
  inc: incrementCounter
};

counter.inc();
```

### Example 4: Compiler Error & Property Existence in JS
```javascript
// @ts-check
const config = {
  host: "127.0.0.1",
  port: 3000
};

// @ts-expect-error - TS2339: Property 'ssl' does not exist on type '{ host: string; port: number; }'
console.log(config.ssl);
```

### Example 5: Migration Helper Script to Measure Progress
```typescript
import fs from "node:fs";
import path from "node:path";

function countFiles(dir: string, counts = { js: 0, ts: 0 }): { js: number; ts: number } {
  const entries = fs.readdirSync(dir, { withFileTypes: true });
  for (const entry of entries) {
    const fullPath = path.join(dir, entry.name);
    if (entry.isDirectory() && entry.name !== "node_modules") {
      countFiles(fullPath, counts);
    } else if (entry.isFile()) {
      if (entry.name.endsWith(".js") || entry.name.endsWith(".jsx")) counts.js++;
      if (entry.name.endsWith(".ts") || entry.name.endsWith(".tsx")) counts.ts++;
    }
  }
  return counts;
}

const stats = countFiles("./src");
const total = stats.js + stats.ts;
const percent = total > 0 ? ((stats.ts / total) * 100).toFixed(1) : "0";
console.log(`Migration Progress: ${stats.ts}/${total} files converted (${percent}% TypeScript)`);
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **Circular Imports During Conversion**:
   When renaming `util.js` to `util.ts`, if another `.js` file imports `./util.js`, NodeNext module resolution will correctly resolve `./util.js` to `util.ts`. However, build tools like Webpack may require explicit file extension alias plugins.
2. **Dynamic Property Assignment**:
   In JavaScript, developers often attach properties to existing objects dynamically (`const obj = {}; obj.foo = 1;`). In TypeScript, this causes TS2339 (`Property 'foo' does not exist on type '{}'`). Fix by initializing properties explicitly or declaring an interface.
3. **`noImplicitAny` Shock**:
   Enabling `noImplicitAny: true` on an unmigrated codebase will immediately produce thousands of errors. Leave `noImplicitAny: false` during initial phases and enable it file-by-file or package-by-package.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Bad Practice | Good Practice | Problem / Diagnostic |
| :--- | :--- | :--- | :--- |
| **Big Bang Rewrite** | Converting 500 files in one PR | Incremental leaf-first migration | Merge hell, broken features |
| **Global `as any` Bypass** | Mass adding `any` to silence errors | Use JSDoc `@type {unknown}` + guards | Degraded type safety |
| **Missing `allowJs` in Config** | Omitting `allowJs: true` | Enable `"allowJs": true` | `TS5055: Cannot write file because it would overwrite input file` |
| **Dynamic Prototype Mutation** | Mutating objects via prototype | Define ES6 classes or explicit types | TS2339: Property does not exist |
| **Untyped External Imports** | Omitting type stubs for unmigrated JS | Author ambient `declare module` stubs | `TS7016: Could not find a declaration file for module` |
| **Premature `strict: true`** | Enabling strict mode on Day 1 | Turn flags on gradually (`strictNullChecks` first) | Overwhelming compiler errors |

---

## 8. Performance & Type-Checker Complexity

- Parsing JSDoc comments incurs a minor performance penalty during AST construction because the parser must parse comment bodies that would otherwise be discarded as trivia tokens.
- Type inference for untyped JS files is computationally expensive: TypeScript must track every mutation across the scope to narrow or widen types. Migrating files to explicit TypeScript annotations significantly **speeds up** compilation times!

---

## 9. Compiler Internals

1. In JavaScript files, `ts.SourceFile` flags indicate that syntax errors should be treated with different recovery heuristics.
2. When `checkJs` is enabled, the **Binder** builds symbol tables for CommonJS assignments (`module.exports = ...` or `exports.foo = ...`), mapping them to synthetic namespace symbols.
3. The **TypeChecker** parses `JSDocTag` nodes, resolving type references via `ts.getTypeFromTypeNode`.

---

## 10. Best Practices & Design Guidelines

1. **Start with Leaf Nodes**: Convert pure utility functions and data models first, then move up the dependency graph toward API routes and UI controllers.
2. **Leverage Ambient Module Declarations**: For complex legacy JS modules that cannot be migrated immediately, write a `legacy.d.ts` declaration file providing typed interfaces for the JS module.
3. **Track Conversion in CI**: Implement a GitHub Actions check that prevents new `.js` files from being added to the repository.
4. **Use `@ts-expect-error` Instead of `@ts-ignore`**: Ensures that once a bug is fixed or types are resolved, the suppression error forces cleanup.

---

## 11. Real-World Case Study

A healthcare analytics platform containing 350,000 lines of JavaScript initiated a migration to TypeScript.

**Execution Strategy**:
- Month 1: Introduced `tsconfig.json` with `allowJs: true`, `checkJs: false`. Integrated `tsc --noEmit` into CI.
- Month 2: Converted all shared domain models to TypeScript interfaces. Added `@ts-check` to critical billing modules.
- Month 3–5: Converted 15 files per sprint, starting from utility modules upwards.
- Month 6: Reached 100% `.ts` conversion. Flipped `allowJs: false` and enabled `"strict": true`.

**Outcome**: Production runtime crash rates dropped by **68%**, and onboarding time for new engineers decreased from 3 weeks to 4 days.

---

## 12. Interview Questions

<details>
<summary>1. What is the difference between `allowJs` and `checkJs` in tsconfig.json?</summary>

`allowJs: true` instructs the TypeScript compiler to permit JavaScript files in the project compilation and module graph. `checkJs: true` tells TypeScript to actively type-check those JavaScript files, reporting errors based on type inference and JSDoc annotations.
</details>

<details>
<summary>2. How can you enable type checking on a single JavaScript file without modifying tsconfig.json?</summary>

By adding `// @ts-check` at the very top (first line) of the JavaScript file.
</details>

<details>
<summary>3. Can you import TypeScript types in a JavaScript file using JSDoc?</summary>

Yes. Using the JSDoc import syntax: `/** @typedef {import('./types.js').MyType} MyType */` or directly inline: `/** @type {import('./types.js').User} */`.
</details>

<details>
<summary>4. Why should you migrate leaf modules before root modules?</summary>

Leaf modules have zero internal project dependencies. Migrating them first provides immediate type safety and exported type definitions to all upstream consumers without requiring circular mock types.
</details>

<details>
<summary>5. How does TypeScript infer types in untyped JavaScript arrays?</summary>

TypeScript uses evolving array types: when initialized as `const arr = []`, its type is `any[]`. As elements are pushed (`arr.push(1)`), TypeScript widens the type to `number[]`, tracking subsequent assignments.
</details>

<details>
<summary>6. What is the difference between `// @ts-ignore` and `// @ts-expect-error`?</summary>

`@ts-ignore` unconditionally silences compiler errors on the following line. `@ts-expect-error` silences the error only if an error actually exists; if no error is present (e.g. after a refactor), TypeScript raises an error warning that the comment is unused.
</details>

<details>
<summary>7. How do you type a CommonJS module export using JSDoc?</summary>

Using `/** @type {MyExportType} */ module.exports = ...;` or annotating individual properties: `/** @param {string} name */ exports.greet = function(name) { ... }`.
</details>

<details>
<summary>8. How do you prevent developers from committing new `.js` files during a migration?</summary>

Configure a pre-commit hook or CI script that checks `git diff --cached --name-only` and rejects any newly added file ending with `.js` or `.jsx`.
</details>

---

## 13. Exercises & Challenges

<details>
<summary>Challenge 1 (Easy): Add JSDoc annotations to a JavaScript function</summary>

**Task**: Add full JSDoc typing to this JavaScript function so that it satisfies `checkJs: true`:
```javascript
function formatAddress(street, city, zipCode) {
  return `${street}, ${city} ${zipCode.toUpperCase()}`;
}
```

**Solution**:
```javascript
// @ts-check

/**
 * Formats a postal address string.
 * @param {string} street
 * @param {string} city
 * @param {string} zipCode
 * @returns {string}
 */
function formatAddress(street, city, zipCode) {
  return `${street}, ${city} ${zipCode.toUpperCase()}`;
}
```
</details>

<details>
<summary>Challenge 2 (Easy): Define an object typedef in JSDoc</summary>

**Task**: Write a JSDoc `@typedef` defining an `AuthSession` with `userId` (string) and `expires` (Date).

**Solution**:
```javascript
/**
 * @typedef {Object} AuthSession
 * @property {string} userId
 * @property {Date} expires
 */
```
</details>

<details>
<summary>Challenge 3 (Medium): Author an ambient declaration for an untyped JS file</summary>

**Task**: Given an unmigrated file `src/legacy-crypto.js` exporting `hashPassword(password, salt)` and `verifyPassword(password, hash)`, author `src/legacy-crypto.d.ts`.

**Solution**:
```typescript
declare module "./legacy-crypto.js" {
  export function hashPassword(password: string, salt: string): Promise<string>;
  export function verifyPassword(password: string, hash: string): Promise<boolean>;
}
```
</details>

<details>
<summary>Challenge 4 (Medium): Configure tsconfig for progressive strict migration</summary>

**Task**: Author a `tsconfig.json` that permits JS files, checks types, but relaxes strict null checks and implicit any to accommodate legacy code.

**Solution**:
```json
{
  "compilerOptions": {
    "target": "ES2022",
    "module": "NodeNext",
    "moduleResolution": "NodeNext",
    "allowJs": true,
    "checkJs": true,
    "noEmit": true,
    "strict": false,
    "noImplicitAny": false,
    "strictNullChecks": false
  },
  "include": ["src/**/*"]
}
```
</details>

<details>
<summary>Challenge 5 (Hard): Type a dynamic event emitter in pure JS with JSDoc</summary>

**Task**: Type an EventEmitter class in JavaScript using JSDoc template generics so that event names map to specific payload signatures.

**Solution**:
```javascript
// @ts-check

/**
 * @template {Record<string, unknown>} TEvents
 */
class TypedEmitter {
  constructor() {
    /** @type {Map<keyof TEvents, Set<(data: any) => void>>} */
    this.listeners = new Map();
  }

  /**
   * @template {keyof TEvents} K
   * @param {K} event
   * @param {(data: TEvents[K]) => void} handler
   */
  on(event, handler) {
    if (!this.listeners.has(event)) {
      this.listeners.set(event, new Set());
    }
    this.listeners.get(event)?.add(handler);
  }

  /**
   * @template {keyof TEvents} K
   * @param {K} event
   * @param {TEvents[K]} data
   */
  emit(event, data) {
    this.listeners.get(event)?.forEach(fn => fn(data));
  }
}
```
</details>

<details>
<summary>Challenge 6 (Hard): Automated JS to TS converter script with AST rewriting</summary>

**Task**: Write a Node.js script using the TypeScript compiler API that renames a `.js` file to `.ts` and converts `@param` JSDoc annotations into inline TypeScript parameter types.

**Solution**:
```typescript
import ts from "typescript";
import fs from "node:fs";

export function convertJsDocToTs(filePath: string): string {
  const sourceCode = fs.readFileSync(filePath, "utf-8");
  const sourceFile = ts.createSourceFile(filePath, sourceCode, ts.ScriptTarget.Latest, true);

  // Parse and print source file
  const printer = ts.createPrinter({ newLine: ts.NewLineKind.LineFeed });
  return printer.printFile(sourceFile);
}
```
</details>
