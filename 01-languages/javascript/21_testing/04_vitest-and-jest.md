# Vitest and Jest

Vitest and Jest are the two dominant JavaScript test runners. They give you the same core experience: `describe`/`it`/`expect`, mocking, snapshots, watch mode, and coverage. The difference is mostly in how they handle modules and tooling.

## Prerequisites

- [Unit testing](./02_unit-testing.md)
- [ES modules vs CommonJS](../13_modules/03_esm-vs-commonjs.md): it explains most of the setup friction below

---

## Which One?

| | **Vitest** | **Jest** |
|---|---|---|
| Module system | Native ESM, TypeScript, JSX out of the box | CommonJS-first; ESM support requires opt-in; usually Babel/ts-jest for transforms |
| Built on | Vite (shares your Vite config) | Own runner + Babel |
| Speed | Fast startup, smart watch mode | Mature, can be slower to start |
| API | Jest-compatible (`vi` instead of `jest`) | The original |
| Globals (`describe`, `it`...) | Opt-in (`globals: true`) or import explicitly | On by default |
| Ecosystem | Growing quickly | Largest, with years of plugins and Stack Overflow answers |

**Rule of thumb:** new project using ESM/TypeScript/Vite → Vitest. Existing Jest codebase, React Native, or heavy reliance on Jest-only plugins → stay with Jest. Migrating between them is usually small because the APIs align.

Node also ships a built-in runner (`node:test`, run with `node --test`) that needs no dependencies. It's fine for small libraries and scripts, with fewer features around mocking, snapshots, and watch UX.

---

## Setup

```bash
# Vitest
npm install -D vitest
# package.json → "scripts": { "test": "vitest", "test:run": "vitest run" }

# Jest
npm install -D jest
# package.json → "scripts": { "test": "jest" }
```

`vitest` (no args) starts **watch mode** in a terminal and reruns only affected tests. `vitest run` runs once and exits, which is what CI should use. Jest runs once by default; use `jest --watch` for watching.

### Minimal Vitest config

```js
// vitest.config.js
import { defineConfig } from 'vitest/config';

export default defineConfig({
  test: {
    environment: 'node',          // or 'jsdom' / 'happy-dom' for browser-like code
    include: ['src/**/*.test.js'],
    setupFiles: ['./test/setup.js'],
    coverage: { provider: 'v8', reporter: ['text', 'html'] },
  },
});
```

### Minimal Jest config

```js
// jest.config.js
export default {
  testEnvironment: 'node',        // for DOM: install jest-environment-jsdom and use 'jsdom'
  testMatch: ['**/*.test.js'],
  setupFilesAfterEnv: ['./test/setup.js'],
};
```

### Jest and native ESM

Jest transforms code to CommonJS by default. For native ESM you must run Node with `--experimental-vm-modules` and set `"type": "module"` in `package.json`, and some mocking APIs differ (`jest.unstable_mockModule`). If that sounds painful, that's the main reason people pick Vitest. Check the Jest docs for your version's current ESM status.

---

## The Shared API

Both runners support the same building blocks:

```js
import { describe, it, test, expect, beforeAll, afterAll, beforeEach, afterEach } from 'vitest';
// In Jest with globals, no import is needed.

describe('group', () => {
  beforeEach(() => { /* before each test in this block */ });

  it('does something', () => { expect(1 + 1).toBe(2); });
  test.skip('not now', () => {});
  test.todo('write this later');
});
```

| Modifier | Use |
|---|---|
| `.only` | Run just this test/suite (don't commit it!) |
| `.skip` | Skip it |
| `.todo` | Placeholder shown in the report |
| `.each` | Parameterize |
| `.concurrent` | Run tests in the block in parallel (Vitest/Jest) |

### Lifecycle order

```text
beforeAll
  beforeEach → test 1 → afterEach
  beforeEach → test 2 → afterEach
afterAll
```

Hooks in outer `describe` blocks run before inner ones.

---

## Matchers Worth Knowing

```js
expect(x).toBe(y)                    // Object.is
expect(x).toEqual(y)                 // deep
expect(x).toStrictEqual(y)           // deep + strict about undefined/types
expect(x).toBeTruthy() / toBeFalsy() / toBeNull() / toBeUndefined() / toBeDefined()
expect(n).toBeGreaterThan(3)
expect(n).toBeCloseTo(0.3)
expect(arr).toHaveLength(3)
expect(arr).toContainEqual({ id: 1 })
expect(obj).toHaveProperty('a.b', 5)
expect(obj).toMatchObject({ a: 1 })
expect(fn).toThrow(/pattern/)
await expect(promise).resolves.toBe(1)
await expect(promise).rejects.toThrow('boom')

// Asymmetric matchers: fuzzy parts of a bigger structure
expect(user).toEqual({ id: expect.any(String), name: 'Asha', tags: expect.arrayContaining(['a']) });
```

---

## Snapshots

A snapshot saves a serialized value on first run and compares against it afterward.

```js
expect(renderReport(data)).toMatchSnapshot();                // stored in __snapshots__/
expect(config).toMatchInlineSnapshot(`{ "retries": 3 }`);    // stored in the test file
```

Update intentionally with `vitest -u` / `jest -u`.

Use them for **small, stable, human-reviewable output** (error messages, serialized config, generated text). Avoid huge snapshots: nobody reads them, and people just press `-u` to make failures go away. If a snapshot test fails, review the diff as carefully as a code change.

---

## Running Tests

```bash
npx vitest run                          # all tests once
npx vitest run src/pricing.test.js      # one file
npx vitest run -t "applies discount"    # by test name
npx vitest run --coverage               # coverage report

npx jest path/to/file.test.js
npx jest -t "applies discount"
npx jest --coverage
```

Coverage in Vitest needs a provider package (e.g. `@vitest/coverage-v8`), installed separately. Jest bundles coverage support.

---

## Vitest vs Jest: Differences That Bite

| Topic | Jest | Vitest |
|---|---|---|
| Mock helper object | `jest` | `vi` |
| Module mock hoisting | Babel plugin hoists `jest.mock` | `vi.mock` is hoisted automatically; use `vi.hoisted()` for values the factory needs |
| Globals | On by default | Off unless `globals: true` (also add types for TS) |
| `import` vs `require` mocking | Easy with CJS | ESM works, but you mock via `vi.mock('path')` before imports are evaluated |
| Behavior of `mockReset` | Removes the implementation | Semantics changed across versions; check the docs for yours |
| Timers config | `jest.useFakeTimers()` | `vi.useFakeTimers()` |

Because details shift between major versions, verify mocking and reset semantics in the documentation for the version you're on instead of assuming.

---

## Debugging Setup Problems

| Symptom | Likely cause |
|---|---|
| `SyntaxError: Cannot use import statement outside a module` (Jest) | No transform for ESM; configure Babel/`ts-jest`, or switch to native ESM mode or Vitest |
| `describe is not defined` (Vitest) | Globals not enabled; import from `'vitest'` or set `globals: true` |
| `document is not defined` | Wrong environment; use `jsdom`/`happy-dom` |
| Tests hang after finishing | Open handle (timer, DB, server); close it in `afterAll` |
| Mock not applied | Mock declared after the import was evaluated, or the wrong module path/specifier |
| TypeScript errors on `expect` extras | Missing types for globals or custom matchers |

---

## Quick Summary

- Vitest and Jest share nearly the same API; **Vitest** is the smoother choice for ESM/TS/Vite projects, **Jest** for existing/legacy setups.
- Use `vitest run` (or `jest`) in CI, watch mode locally.
- Know `toBe` vs `toEqual` vs `toStrictEqual`, and use asymmetric matchers for fuzzy parts.
- Keep snapshots small and reviewed.
- Most setup pain is module-system related.

**Next:** [Mocking](./05_mocking.md)
