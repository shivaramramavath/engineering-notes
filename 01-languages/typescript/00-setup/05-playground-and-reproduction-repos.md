# TypeScript Playground & Minimal Reproduction Repositories

> A practical guide to isolating compiler bugs, sharing type proofs via the TypeScript Playground, configuring minimal reproduction repositories (MREs), and bisecting type regressions.

| Metadata | Value |
| :--- | :--- |
| **Level** | Intermediate |
| **Prerequisites** | [02-editor-and-playground.md](file:///e:/Learning/engineering-notes/01-languages/typescript/00-setup/02-editor-and-playground.md) |
| **Time to Read** | 20 minutes |
| **Status** | Complete |
| **TypeScript Baseline** | Since TS 5.0+ / TS 7.0 Native |
| **Tested tsconfig** | `strict: true`, `target: ES2022`, `moduleResolution: NodeNext` |

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

### Problem Statement
When reporting a type issue to library maintainers (e.g. Zod, TanStack, Prisma) or filing an issue on the official Microsoft TypeScript GitHub repository, submitting a complex multi-file codebase or vague bug description results in immediate closure. Maintainers require a **Minimal Reproducible Example (MRE)**: an isolated single-file demonstration or self-contained repository with zero superfluous dependencies.

### Historical Context
Historically, bug reports contained large zip archives or pasting huge snippets into issues. Microsoft introduced the modern TypeScript Playground with embedded compiler state in URL hashes, along with the `ts-error-translator` and automated bisecting tools.

### Real-World Relevance
Creating clean MREs is an essential senior engineering skill. It allows teams to prove whether a bug originates in their domain code, a third-party declaration file, or the TypeScript compiler itself.

---

## 2. Core Concepts

### Principles of a Minimal Reproducible Example (MRE)
1. **Minimal**: Remove every dependency, model property, and helper function not directly required to trigger the type error.
2. **Complete**: Include all necessary interfaces and declarations so the snippet compiles self-contained.
3. **Reproducible**: Fails consistently under identical compiler options.

```
       Monolithic Codebase (100,000 lines, 50 dependencies)
                              │
                              ▼ (Cut UI, Redux, Network, DB)
       Domain Business Logic (500 lines)
                              │
                              ▼ (Extract interfaces to single file)
       Isolated Type Proof (15 lines in TypeScript Playground)
```

---

## 3. How It Works Internally

### TypeScript Playground URL Encoding
The official playground (`typescriptlang.org/play`) encodes state using LZ-string compression in the URL fragment (`#code/...`):

```
+-------------------------------------------------------------+
|                  Playground State Encoding                  |
|                                                             |
|   { code, compilerOptions, tsVersion }                      |
|                     │                                       |
|                     ▼ LZ-String Compression                 |
|   "JYOwLgpgTgZghgYwgAgGIHt..."                              |
|                     │                                       |
|                     ▼                                       |
|   https://www.typescriptlang.org/play#code/JYOwLgpg...      |
+-------------------------------------------------------------+
```

When opened, the browser decompresses the string in a web worker, initializes the in-browser `tsserver` instance, and renders errors in Monaco Editor.

---

## 4. Syntax & API Reference

### Reproduction Repository Template Anatomy

A standard reproduction repo contains only 3 files:

```bash
# Minimal layout
.
├── package.json
├── tsconfig.json
└── index.ts
```

#### Minimal `package.json`:
```json
{
  "name": "ts-repro",
  "version": "1.0.0",
  "type": "module",
  "devDependencies": {
    "typescript": "5.7.3"
  },
  "scripts": {
    "repro": "tsc --noEmit"
  }
}
```

---

## 5. Practical Examples

### Example 1: Isolating an Inference Failure
- **Goal:** Strip a complex React form component down to a 10-line type inference bug.
- **Before (150 lines of JSX, hooks, state):**
```typescript
// Messy original code with React dependencies
```
- **After (MRE):**
```typescript
type Validator<T> = (value: unknown) => value is T;

function createField<T>(validator: Validator<T>, initial: T) {
  return { validator, initial };
}

// Reproduces type inference widening issue:
const isString: Validator<string> = (v: unknown): v is string => typeof v === 'string';
const field = createField(isString, 'default');
```
- **Explanation:** Demonstrates the bug with zero external libraries.

### Example 2: Reproducing a Type-Check Performance Regression
- **Goal:** Author a synthetic recursive type that triggers `TS2589: Type instantiation is excessively deep`.
- **Code:**
```typescript
// MRE for deep recursion failure
type InfiniteTuple<T, Acc extends any[] = []> = 
  Acc['length'] extends 1000 
    ? Acc 
    : InfiniteTuple<T, [...Acc, T]>;

// @ts-expect-error TS2589: Type instantiation is excessively deep and possibly infinite.
type Thousand = InfiniteTuple<string>;
```
- **Real Compiler Error:**
```
error TS2589: Type instantiation is excessively deep and possibly infinite.
```

### Example 3: Bisecting a Compiler Regression with Git
- **Goal:** Find the exact TypeScript version or commit where a type broke.
- **Script snippet (`bisect.sh`):**
```bash
#!/bin/bash
# Install specific version and test
npm install --no-save typescript@$1
npx tsc --noEmit
```

### Example 4: Negative Test — Unreproducible Local Issue
- **Goal:** Demonstrate an issue caused by local `node_modules` pollution rather than a TypeScript bug.
- **Symptom:** Code compiles in Playground, but fails on local machine.
- **Remediation CLI:**
```bash
# Clean local cache and re-test
rm -rf node_modules package-lock.json dist .tsbuildinfo
npm install
npx tsc --noEmit
```

### Example 5: Type Testing Assertion with `expect-type`
- **Goal:** Assert exact compile-time types inside an MRE.
- **Code:**
```typescript
import { expectTypeOf } from 'expect-type';

function getUser() {
  return { id: 1, name: 'Alice' };
}

// Statically asserts exact type matches
expectTypeOf(getUser).returns.toEqualTypeOf<{ id: number; name: string }>();
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **Ambient Typings in Repro**: Forgetting to include `@types/node` when reproducing a Node.js issue in the Playground causes misleading `Cannot find name 'process'` errors. Use the Playground's `"Plugins"` or `"Settings"` menu to add ambient packages.
2. **Compiler Options Mismatch**: A type error that reproduces with `"strictNullChecks": true` will vanish if the recipient tests it with default sloppy options. Always share the full compiler option set.
3. **Monaco Worker Caching**: The Playground occasionally caches older language service workers. Click `"Clear Cache"` if diagnostics behave erratically.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Why It Fails | Idiomatic Solution | Benefit |
| :--- | :--- | :--- | :--- |
| Linking a full GitHub application repository | Maintainers don't have time to clone and debug 10k lines | Reduce to a single Playground link or 3-file repo | Fast issue triage and bug resolution |
| Omitting `tsconfig.json` options in bug reports | Maintainer cannot reproduce error | Share exact compiler flags | Eliminates "works on my machine" pushback |
| Pasting code screenshots | Code cannot be copied, compiled, or tested | Provide raw text or Playground URL | Enables automated regression testing |
| Including unrelated npm dependencies | Adds noise and third-party variables | Replace third-party types with minimal interfaces | Pinpoints the exact compiler fault |

---

## 8. Performance & Type-Checker Complexity

- **Playground Worker Overhead**: The browser Monaco editor runs TypeScript in a Web Worker using WebAssembly or JS compilation. Complex recursive conditional types may freeze the worker thread while native `tsc` compiles it cleanly.

---

## 9. Compiler Internals

When Microsoft engineers triage an issue:
1. They take the provided Playground link.
2. They save the code into the TypeScript repository under `tests/cases/compiler/<issueNumber>.ts`.
3. They run the internal test runner `jake runtests-parallel` to generate a baseline `.errors.txt` diff.
4. Once fixed, that test remains in the official suite permanently to prevent regressions.

---

## 10. Best Practices & Design Guidelines

1. **Rule of 20 Lines**: If your reproduction exceeds 20 lines, try stripping it further.
2. **Replace External Libraries with Stubs**: If reproducing an issue with Zod, write a minimal 3-line type mimicking `z.ZodType<T>`.
3. **Verify in Nightly**: Test your MRE against `typescript@next` in the Playground to ensure the bug hasn't already been resolved.

---

## 11. Real-World Case Study

A senior engineer identified a subtle type regression in TypeScript 5.2 where mapped types over discriminated unions lost narrowing in certain generic contexts.

### Strategy:
1. Stripped 4,000 lines of Redux Toolkit slices down to 12 lines of pure types.
2. Verified the snippet failed in `5.2.2`, but passed in `5.1.6`.
3. Submitted a clean Playground link to Microsoft's GitHub issue tracker.

**Outcome**: The TypeScript team confirmed the regression within 3 hours, assigned a champion, and merged a patch into the next release.

---

## 12. Interview Questions

### Q1: What constitutes a Minimal Reproducible Example (MRE) in TypeScript?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

An MRE is the smallest possible self-contained snippet of code that reliably demonstrates a specific type behavior or bug. It removes all third-party libraries (replacing them with stub interfaces), eliminates all unrelated properties and functions, and includes the exact `tsconfig.json` compiler options required to trigger the compiler diagnostic.
</details>

### Q2: How does the TypeScript Playground preserve your compiler settings in links?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

The TypeScript Playground serializes the source code string and active compiler options into a JSON object, compresses it using LZ-string compression, and appends it to the URL hash fragment (`#code/<compressedData>`). When the URL is shared, the recipient's browser decompresses the hash, restoring the exact code, TypeScript version, and compiler flags without storing data on a database server.
</details>

---

## 13. Exercises & Challenges

### Exercise 1 (Easy): Create an MRE for a Union Narrowing Failure
- **Problem Statement**: Create a 6-line standalone code snippet proving that `typeof null === 'object'` requires a null check before property access.
- **Solution**:
```typescript
function getLength(val: string | null): number {
  // @ts-expect-error TS18047: 'val' is possibly 'null'.
  return val.length;
}
```

### Exercise 2 (Medium): Stub an External Library Type
- **Problem Statement**: Create a minimal stub for an API response wrapper `{ data: T; error: null } | { data: null; error: Error }` without importing external libraries.
- **Solution**:
```typescript
export type ApiResponse<T> =
  | { readonly data: T; readonly error: null }
  | { readonly data: null; readonly error: Error };
```

### Exercise 3 (Hard): Automated Playground URL Generator
- **Problem Statement**: Author a Node.js function using `lz-string` (or standard compression algorithm) that generates a valid TypeScript Playground URL from a source string.
- **Solution**:
```typescript
export function makePlaygroundUrl(code: string): string {
  // Encodes code into standard URL parameter
  const encoded = encodeURIComponent(code);
  return `https://www.typescriptlang.org/play?strict=true#code/${encoded}`;
}
```
