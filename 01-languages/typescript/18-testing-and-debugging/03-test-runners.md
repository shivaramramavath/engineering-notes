# Test Runners: Vitest, Jest, and Node.js Native Test Runner

> Comparing modern TypeScript test runners: Vitest, Jest (ts-jest vs @swc/jest), and the native Node.js test runner (`node:test`), configurations, ESM support, and coverage collection.

| Metadata | Details |
| :--- | :--- |
| **Category** | Testing and Debugging |
| **Difficulty** | Intermediate to Advanced |
| **Prerequisites** | [13-compiler-and-tsconfig/02-target-module-and-lib.md](../13-compiler-and-tsconfig/02-target-module-and-lib.md), [18-testing-and-debugging/00-unit-testing.md](./00-unit-testing.md) |
| **TypeScript Baseline** | TS 4.5+, TS 5.0+ |
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

Historically, running TypeScript tests required configuring complex Babel or `ts-jest` pipelines inside Jest. In large enterprise codebases, `ts-jest` suffered from agonizingly slow execution because it ran the full TypeScript type checker synchronously across thousands of test files on every run.

Today, developers choose between three primary runners:
1. **Vitest**: The industry leader for modern TypeScript/Vite projects. Sub-millisecond esbuild transpilation, native ESM, multi-threading, browser mode, and built-in type testing.
2. **Jest (with `@swc/jest` or `esbuild-jest`)**: The legacy standard, still common in established enterprise monoliths.
3. **Node.js Native Test Runner (`node:test`)**: Zero external dependencies, built directly into Node 18+ and Node 20/22 LTS, pairing with Node's native `--experimental-strip-types` to execute tests with zero configuration.

```
+--------------------+-----------------------+-----------------------+
| Feature            | Vitest                | Jest (@swc/jest)      | Node.js node:test     |
+--------------------+-----------------------+-----------------------+
| Dependencies       | Lightweight (Vite)    | Heavy (many packages) | Zero (Built-in)       |
| Native ESM         | First-class           | Experimental / Flaky  | First-class           |
| Execution Speed    | Ultra-fast (threads)  | Fast (with SWC)       | Ultra-fast (native C++)|
| Type Testing       | Built-in expectTypeOf | Requires tsd / tsdx   | None (use tsc)        |
+--------------------+-----------------------+-----------------------+
```

---

## 2. Core Concepts

### 1. Vitest Configuration (`vitest.config.ts`)
```typescript
import { defineConfig } from 'vitest/config';

export default defineConfig({
  test: {
    globals: true,
    environment: 'node',
    coverage: {
      provider: 'v8',
      reporter: ['text', 'json', 'html']
    }
  }
});
```

### 2. Node.js Native Runner (`node:test` and `node:assert`)
Starting in Node 18/20+, Node provides a built-in test module:
```typescript
import test from 'node:test';
import assert from 'node:assert/strict';

test('native addition test', () => {
  assert.equal(1 + 1, 2);
});
```
With Node 22+ (`node --experimental-strip-types --test src/*.test.ts`), this runs TypeScript tests directly with **zero npm dependencies**.

---

## 3. How It Works Internally

- **Vitest**: Spawns worker threads via `tinypool`. Each worker transforms files using the Vite/Rollup transform pipeline with cached esbuild transforms, executing tests in an isolated VM or worker context.
- **Node.js Native Runner**: Uses internal C++ bindings in libuv and V8 to schedule test microtasks, reporting results directly to standard output formatted in standard TAP (Test Anything Protocol) or spec format.

---

## 4. Syntax & API Reference

### Comparing Equivalent Test Assertions
```typescript
// Vitest / Jest:
expect(actual).toEqual({ a: 1 });
expect(actual).toBeCloseTo(0.3, 5);

// Node.js node:assert/strict:
assert.deepStrictEqual(actual, { a: 1 });
assert(Math.abs(actual - 0.3) < 0.00001);
```

---

## 5. Practical Examples

### Example 1: Basic Node.js `node:test` Suite with Subtests
```typescript
import test from 'node:test';
import assert from 'node:assert/strict';

test('Math Operations', async (t) => {
  await t.test('multiplication', () => {
    assert.equal(3 * 4, 12);
  });

  await t.test('division by zero throws', () => {
    assert.throws(() => {
      const divide = (a: number, b: number) => {
        if (b === 0) throw new RangeError('Division by zero');
        return a / b;
      };
      divide(10, 0);
    }, RangeError);
  });
});
```

### Example 2: Real-World Multi-Project Monorepo Vitest Config
```typescript
import { defineConfig } from 'vitest/config';

export default defineConfig({
  test: {
    workspace: [
      'packages/*/vitest.config.ts',
      'apps/*/vitest.config.ts'
    ],
    coverage: {
      provider: 'v8',
      thresholds: {
        lines: 85,
        functions: 85,
        branches: 80,
        statements: 85
      }
    }
  }
});
```

### Example 3: Edge Case - Code Coverage with Native V8 Profiler
```typescript
// Vitest uses Node's native V8 coverage collection:
// Command: npx vitest run --coverage
// Collects byte-accurate source mapped line and branch coverage without Babel instrumenting code.
```

### Example 4: Failure Case - CommonJS vs ESM Interop Crash in Jest
```typescript
// In Jest with ESM packages (like chalk v5, node-fetch v3):
// SyntaxError: Cannot use import statement outside a module
// Vitest handles pure ESM dependencies out of the box without transformIgnorePatterns!
```

### Example 5: Idiomatic Standalone CLI Test Runner Script
```typescript
import { run } from 'node:test';
import { spec } from 'node:test/reporters';
import { glob } from 'node:fs/promises';

export async function runAllTests() {
  const files: string[] = [];
  for await (const file of glob('**/*.test.ts')) {
    files.push(file);
  }

  run({ files })
    .compose(spec)
    .pipe(process.stdout);
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **Jest ESM Fragility**: Jest was architected before ES Modules existed. Running Jest with native ESM requires experimental flags (`NODE_OPTIONS=--experimental-vm-modules`) and complex `transformIgnorePatterns` regexes.
2. **Type Checking in CI**: Vitest and Node's test runner strip types for speed. If you do not have `tsc --noEmit` in your CI workflow, syntactic type errors in test files will go unnoticed.
3. **Environment Isolation**: In Vitest, `environment: 'jsdom'` or `'happy-dom'` mocks the browser `window` object for React/Vue component tests, whereas `environment: 'node'` is 3x faster for pure backend services.
4. **Coverage Calculation on Unused Files**: By default, coverage tools only measure files imported by tests. Set `all: true` or `include: ['src/**/*.ts']` to identify completely untested source files.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Bad Code | Good Code | Compiler / Runtime Impact |
| :--- | :--- | :--- | :--- |
| **Using Slow ts-jest in 2026** | `ts-jest` type-checking 5,000 files | Vitest or `@swc/jest` + `tsc --noEmit` | 20x slower CI build times |
| **Global Jest Namespace Polluting** | Setting `types: ["jest"]` globally | Local imports `import { it } from 'vitest'` | Type collision between Jest and Vitest |
| **Missing Coverage Thresholds** | Coverage report without failure rules | Enforce 80%+ thresholds in config | Regressions slip into codebase silently |
| **Hardcoded Windows Paths in Globs** | `testMatch: ['src\\**\\*.test.ts']` | `['src/**/*.test.ts']` | Tests fail when executed on Linux CI |
| **Ignoring Monorepo Workspaces** | Running individual Vitest per package | Vitest workspace configuration | High CPU usage; redundant startup time |

---

## 8. Performance & Type-Checker Complexity

- **Transpilation Benchmarks**:
  - `ts-jest`: ~15 seconds for 500 test files.
  - `Vitest` (esbuild): ~0.8 seconds for 500 test files (18x faster).
  - `node:test` (Node 22 native type-stripping): ~0.6 seconds.
- **Coverage Overhead**: V8 native coverage overhead is under 5% compared to Babel Istanbul instrumentation (30%+).

---

## 9. Compiler Internals

When using Vitest globals (`describe`, `it`, `expect` without imports):
Add `"types": ["vitest/globals"]` to your `tsconfig.json` `"compilerOptions"`. The TypeScript Binder reads the global declarations file and binds those symbols into the global scope.

---

## 10. Best Practices & Design Guidelines

1. **Standardize on Vitest**: For 90% of TypeScript projects, Vitest provides the best balance of speed, features, and ESM compatibility.
2. **Use `node:test` for Zero-Dependency CLI Tools**: If building lightweight libraries, avoid third-party test dependencies.
3. **Always Run `tsc --noEmit` Alongside Tests**: Catch type regressions in CI.
4. **Use V8 Coverage**: Faster and more accurate than Istanbul instrumentation.

---

## 11. Real-World Case Study

### Enterprise Monorepo Migration from Jest to Vitest
A large SaaS company with 3,500 tests across 12 packages migrated from Jest to Vitest:
- **CI Test Execution Time**: Dropped from 6 minutes 45 seconds to 28 seconds.
- **Configuration Lines**: Reduced from 350 lines of Babel/Jest regex transforms to a clean 30-line `vitest.workspace.ts`.
- **Developer Experience**: Instant watch-mode HMR updates when editing TypeScript files.

---

## 12. Interview Questions

<details>
<summary>1. Why is `ts-jest` significantly slower than Vitest or `@swc/jest`?</summary>

`ts-jest` invokes the full TypeScript compiler (`tsc`) during test execution to perform both type checking and code emit synchronously. Vitest (using esbuild) and `@swc/jest` simply strip type annotations using fast Rust/Go native compilers, deferring type checking to a dedicated `tsc --noEmit` process.
</details>

<details>
<summary>2. What is the Node.js native test runner (`node:test`) and what are its pros and cons?</summary>

`node:test` is a built-in test runner introduced in Node 18+.
**Pros**: Zero external dependencies, ultra-fast startup, native ESM support, built into the Node runtime.
**Cons**: Lacks advanced mocking utilities (MSW must be used), lacks built-in snapshot formatting, and has no native browser DOM environment (`happy-dom`).
</details>

<details>
<summary>3. What is the difference between V8 code coverage and Istanbul (Babel) code coverage?</summary>

Istanbul instruments source code by injecting counting statements before every statement and branch, which slows down execution and alters AST line mappings. V8 coverage uses the JavaScript engine's internal execution counter bytes directly from memory, providing sub-millisecond, highly accurate coverage without modifying code.
</details>

<details>
<summary>4. How does Vitest integrate with Vite?</summary>

Vitest shares Vite's dev server configuration, plugins (React, Vue, Svelte), aliases (`@/`), and asset loaders, guaranteeing that test execution matches the exact build configuration of the application.
</details>

<details>
<summary>5. How do you handle type checking in test files when using Vitest?</summary>

Either run `tsc --noEmit` in CI or use Vitest's built-in type check command: `vitest --typecheck`.
</details>

<details>
<summary>6. How does Vitest isolate test files?</summary>

Vitest isolates test files by running them in separate worker threads using `tinypool` (or child processes), resetting global variables and module caches between test suites.
</details>

<details>
<summary>7. What is TAP (Test Anything Protocol)?</summary>

TAP is a standardized plain-text format for recording test results. The Node.js native test runner outputs TAP by default, allowing test results to be piped to any external reporter or CI visualizer.
</details>

<details>
<summary>8. How do you configure Vitest for both frontend React components and backend Node.js services in one repo?</summary>

Use Vitest Workspaces (`vitest.workspace.ts`). Configure packages targeting the browser with `environment: 'happy-dom'`, and backend packages with `environment: 'node'`.
</details>

---

## 13. Exercises & Challenges

<details>
<summary>Exercise 1 (Easy): Basic `node:test` Assertion</summary>

**Problem:** Write a standalone test script using `node:test` and `node:assert/strict` that verifies array sorting.

**Solution:**
```typescript
import test from 'node:test';
import assert from 'node:assert/strict';

test('sorts array in ascending order', () => {
  const numbers = [3, 1, 2];
  numbers.sort((a, b) => a - b);
  assert.deepStrictEqual(numbers, [1, 2, 3]);
});
```
</details>

<details>
<summary>Exercise 2 (Easy): Vitest Timeout Configuration</summary>

**Problem:** Configure an individual slow test in Vitest to have a custom timeout of 10,000 milliseconds.

**Solution:**
```typescript
import { it } from 'vitest';

it('slow network operation', async () => {
  await new Promise((r) => setTimeout(r, 6000));
}, 10000);
```
</details>

<details>
<summary>Exercise 3 (Medium): Vitest Environment Per-File Override</summary>

**Problem:** In a backend project using `environment: 'node'`, override a single test file to run in `'jsdom'`.

**Solution:**
```typescript
// @vitest-environment jsdom
import { it, expect } from 'vitest';

it('has access to window', () => {
  expect(window).toBeDefined();
});
```
</details>

<details>
<summary>Exercise 4 (Medium): Custom Coverage Exclusions</summary>

**Problem:** Write a `vitest.config.ts` snippet that excludes all `*.d.ts` files, test directories, and barrel `index.ts` files from coverage reports.

**Solution:**
```typescript
import { defineConfig } from 'vitest/config';

export default defineConfig({
  test: {
    coverage: {
      exclude: ['**/*.d.ts', '**/*.test.ts', '**/index.ts', '**/node_modules/**']
    }
  }
});
```
</details>

<details>
<summary>Exercise 5 (Hard): Standalone Test Reporter</summary>

**Problem:** Build a custom reporter function for `node:test` that logs passing tests in green and failing tests in red.

**Solution:**
```typescript
export async function customReporter(source: AsyncIterable<any>) {
  for await (const event of source) {
    if (event.type === 'test:pass') {
      console.log(`\x1b[32m✔ PASS: ${event.data.name}\x1b[0m`);
    } else if (event.type === 'test:fail') {
      console.log(`\x1b[31m✖ FAIL: ${event.data.name}\x1b[0m`);
    }
  }
}
```
</details>

<details>
<summary>Exercise 6 (Hard): Multi-Target Vitest Workspace</summary>

**Problem:** Write a `vitest.workspace.ts` definition that runs unit tests with `'node'`, UI tests with `'happy-dom'`, and integration tests sequentially with a single thread.

**Solution:**
```typescript
import { defineWorkspace } from 'vitest/config';

export default defineWorkspace([
  {
    test: {
      name: 'unit',
      include: ['src/**/*.unit.test.ts'],
      environment: 'node'
    }
  },
  {
    test: {
      name: 'ui',
      include: ['src/**/*.ui.test.ts'],
      environment: 'happy-dom'
    }
  },
  {
    test: {
      name: 'integration',
      include: ['src/**/*.integration.test.ts'],
      environment: 'node',
      pool: 'forks',
      poolOptions: { forks: { singleFork: true } }
    }
  }
]);
```
</details>
