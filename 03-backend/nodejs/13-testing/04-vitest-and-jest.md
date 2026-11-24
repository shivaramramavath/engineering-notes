# Vitest and Jest

**Jest** and **Vitest** are the two most widely used JavaScript test frameworks. They share nearly the same API (`describe`, `it`, `expect`, mocks), so skills transfer between them. This file covers setup, the common API, configuration, mocking basics, coverage, and how the two differ.

See also: [Unit Testing](./02_unit-testing.md), [Integration Testing](./03_integration-testing.md), [Mocking](./05_mocking.md).

## Which one?

| | **Vitest** | **Jest** |
|---|-----------|----------|
| Built on | Vite (esbuild, native ESM) | Its own runtime, Babel or SWC transforms |
| ESM and TypeScript | First-class, zero config | Needs configuration (Babel, `ts-jest`, or SWC; ESM support is experimental) |
| Speed | Very fast, smart watch mode that re-runs only affected tests | Fast, heavier startup, especially with transforms |
| API | Jest-compatible (`vi` instead of `jest`) | Original |
| Globals (`describe`, `expect`) | Opt in (`globals: true`) or import them | On by default |
| Ecosystem | Growing quickly | Largest: plugins, React Native support, many tutorials |
| Browser mode, UI | Built-in UI and browser-mode options | Via add-ons |
| Best for | New projects, Vite-based apps, ESM and TypeScript | Existing Jest codebases, React Native |

Both are good. For a new project that uses ES modules or Vite, pick Vitest. If your project already uses Jest, there is rarely a reason to migrate urgently.

## Vitest setup

```bash
npm install --save-dev vitest
```

```json
{
  "type": "module",
  "scripts": {
    "test": "vitest run",
    "test:watch": "vitest",
    "coverage": "vitest run --coverage"
  }
}
```

```js
// vitest.config.js
import { defineConfig } from 'vitest/config';

export default defineConfig({
  test: {
    environment: 'node',                 // 'jsdom' or 'happy-dom' for DOM code
    globals: false,                      // true to use describe/it/expect without imports
    include: ['src/**/*.test.js'],
    setupFiles: ['./test/setup.js'],     // runs before each test file
    restoreMocks: true,                  // restore spies after each test
    clearMocks: true,
    testTimeout: 5000,
    coverage: {
      provider: 'v8',
      reporter: ['text', 'html', 'lcov'],
      include: ['src/**'],
      thresholds: { lines: 80, branches: 75, functions: 80, statements: 80 },
    },
  },
});
```

Vitest reads your existing `vite.config.js` if you have one, so aliases and plugins apply to tests as well.

```js
import { describe, it, expect, vi, beforeEach } from 'vitest';
```

## Jest setup

```bash
npm install --save-dev jest
```

```json
{
  "scripts": {
    "test": "jest",
    "test:watch": "jest --watch",
    "coverage": "jest --coverage"
  }
}
```

```js
// jest.config.js (CommonJS project)
module.exports = {
  testEnvironment: 'node',                   // 'jsdom' for DOM code (install jest-environment-jsdom)
  testMatch: ['**/*.test.js'],
  setupFilesAfterEnv: ['./test/setup.js'],
  restoreMocks: true,
  clearMocks: true,
  collectCoverageFrom: ['src/**/*.js'],
  coverageThreshold: { global: { lines: 80, branches: 75 } },
};
```

Jest uses CommonJS by default. For ES modules run Node with the experimental flag:

```bash
NODE_OPTIONS=--experimental-vm-modules npx jest
```

and set `"type": "module"` in `package.json`. Many projects instead let Babel or SWC transform ESM to CommonJS (`babel-jest`, `@swc/jest`, or `ts-jest` for TypeScript). In ESM mode, `jest` is not a global; import it: `import { jest } from '@jest/globals'`.

## Core API (identical in both)

### Structure

```js
describe('group', () => {
  beforeAll(() => { /* once before all tests in this block */ });
  afterAll(() => { /* once after all tests */ });
  beforeEach(() => { /* before every test */ });
  afterEach(() => { /* after every test */ });

  it('does something', () => {});
  test('also does something', () => {});         // identical to it()
});
```

| Modifier | Effect |
|----------|--------|
| `it.only(...)`, `describe.only(...)` | Run only this (great while debugging; do not commit) |
| `it.skip(...)`, `describe.skip(...)` | Skip |
| `it.todo('write this test')` | Placeholder shown in the report |
| `it.each(table)(name, fn)` | Parameterized tests |
| `it.fails(...)` (Vitest), `it.failing(...)` (Jest) | Expected to fail |
| `it('...', fn, timeoutMs)` | Custom timeout |
| `it.concurrent(...)` | Run in parallel within a file |

Hooks run in order: outer `beforeAll`, outer `beforeEach`, inner `beforeEach`, test, inner `afterEach`, outer `afterEach`.

### Matchers (`expect`)

```js
// Equality
expect(a).toBe(b);                        // Object.is
expect(a).toEqual(b);                     // deep equality, ignores undefined properties
expect(a).toStrictEqual(b);               // deep equality including undefined properties and class types

// Truthiness
expect(x).toBeTruthy(); expect(x).toBeFalsy();
expect(x).toBeNull(); expect(x).toBeUndefined(); expect(x).toBeDefined();
expect(x).toBeNaN();

// Numbers
expect(n).toBeGreaterThan(3);   expect(n).toBeGreaterThanOrEqual(3);
expect(n).toBeLessThan(5);      expect(n).toBeLessThanOrEqual(5);
expect(0.1 + 0.2).toBeCloseTo(0.3, 5);

// Strings and collections
expect(str).toMatch(/regex/);   expect(str).toContain('sub');
expect(arr).toContain(item);    expect(arr).toContainEqual({ id: 1 });
expect(arr).toHaveLength(3);
expect(obj).toHaveProperty('a.b.c', 42);
expect(obj).toMatchObject({ a: 1 });
expect(arr).toEqual(expect.arrayContaining([1, 2]));

// Types
expect(x).toBeInstanceOf(Date);
expect(typeof x).toBe('string');

// Errors
expect(() => fn()).toThrow();
expect(() => fn()).toThrow(TypeError);
expect(() => fn()).toThrow(/message/);

// Promises
await expect(promise).resolves.toBe(1);
await expect(promise).rejects.toThrow('failed');

// Negation
expect(x).not.toBe(y);
```

### Asymmetric matchers (flexible partial matching)

```js
expect(user).toEqual({
  id: expect.any(Number),
  name: 'Ada',
  createdAt: expect.any(Date),
  email: expect.stringContaining('@'),
  roles: expect.arrayContaining(['admin']),
  meta: expect.objectContaining({ source: 'api' }),
});

expect(spy).toHaveBeenCalledWith(expect.anything(), expect.stringMatching(/^user-/));
```

Great for fields you cannot predict (IDs, timestamps) without giving up on checking the rest.

### Counting assertions

```js
it('calls the callback', async () => {
  expect.assertions(2);                   // fails if exactly 2 assertions did not run
  await run((v) => { expect(v).toBeDefined(); expect(v).toBe(1); });
});
```

## Mock functions (spies)

```js
// Vitest: vi    Jest: jest
const fn = vi.fn();                        // a recording function
fn('a', 1);
fn('b');

expect(fn).toHaveBeenCalled();
expect(fn).toHaveBeenCalledTimes(2);
expect(fn).toHaveBeenCalledWith('a', 1);
expect(fn).toHaveBeenLastCalledWith('b');
expect(fn).toHaveBeenNthCalledWith(1, 'a', 1);
fn.mock.calls;                             // [['a', 1], ['b']]
fn.mock.results;                           // [{ type: 'return', value: undefined }, ...]
```

Control what mocks return:

```js
const getUser = vi.fn()
  .mockReturnValueOnce({ id: 1 })         // first call
  .mockReturnValue({ id: 99 });           // all later calls

const fetchData = vi.fn().mockResolvedValue({ ok: true });            // async success
const failing = vi.fn().mockRejectedValue(new Error('network'));      // async failure
const custom = vi.fn((a, b) => a + b);                                // custom implementation
custom.mockImplementationOnce(() => 0);
```

Spy on an existing method while keeping or replacing its behavior:

```js
const spy = vi.spyOn(console, 'error').mockImplementation(() => {});   // silence and record
doSomethingThatLogs();
expect(spy).toHaveBeenCalledWith(expect.stringContaining('failed'));
spy.mockRestore();                                                       // put the original back
```

Resetting:

| Method | Effect |
|--------|--------|
| `mockClear()` | Clears recorded calls and results (keeps implementation) |
| `mockReset()` | Clears calls and resets implementation (Vitest: to the original or undefined, per version; check the docs) |
| `mockRestore()` | Restores the original method of a spied object |
| `vi.clearAllMocks()` / `vi.resetAllMocks()` / `vi.restoreAllMocks()` | The same across all mocks |
| Config `clearMocks`, `resetMocks`, `restoreMocks` | Apply automatically before or after each test |

Turn on `restoreMocks` (or call `vi.restoreAllMocks()` in `afterEach`) so spies never leak between tests. Details of mocking strategy are in [Mocking](./05_mocking.md).

## Module mocking

```js
// Vitest
import { vi } from 'vitest';
import { sendEmail } from './mailer.js';

vi.mock('./mailer.js', () => ({
  sendEmail: vi.fn().mockResolvedValue({ ok: true }),
}));

it('sends a welcome email', async () => {
  await registerUser('ada@example.com');
  expect(sendEmail).toHaveBeenCalledWith('ada@example.com', expect.any(String));
});
```

```js
// Jest (CommonJS or Babel-transformed ESM)
jest.mock('./mailer.js', () => ({
  sendEmail: jest.fn().mockResolvedValue({ ok: true }),
}));
```

Both frameworks **hoist** `vi.mock` / `jest.mock` calls to the top of the file, before imports. In Vitest, use `vi.hoisted()` when the factory needs variables that must exist before hoisting:

```js
const { sendEmail } = vi.hoisted(() => ({ sendEmail: vi.fn() }));
vi.mock('./mailer.js', () => ({ sendEmail }));
```

Use `vi.importActual` / `jest.requireActual` to mock only part of a module:

```js
vi.mock('./utils.js', async () => ({
  ...(await vi.importActual('./utils.js')),
  now: vi.fn(() => 1_700_000_000_000),
}));
```

## Fake timers

```js
beforeEach(() => { vi.useFakeTimers(); });
afterEach(() => { vi.useRealTimers(); });

it('runs the callback after 1 second', () => {
  const cb = vi.fn();
  setTimeout(cb, 1000);

  vi.advanceTimersByTime(999);
  expect(cb).not.toHaveBeenCalled();

  vi.advanceTimersByTime(1);
  expect(cb).toHaveBeenCalledTimes(1);
});

it('controls the current date', () => {
  vi.setSystemTime(new Date('2026-01-01T00:00:00Z'));
  expect(new Date().getUTCFullYear()).toBe(2026);
});
```

Other helpers: `vi.runAllTimers()`, `vi.runOnlyPendingTimers()`, `await vi.advanceTimersByTimeAsync(ms)` (for promises mixed with timers). Jest uses the same names under `jest.`.

## Snapshots

A snapshot stores a value's serialized form and fails when it later changes.

```js
expect(renderReport(data)).toMatchSnapshot();                // saved in __snapshots__/ next to the test
expect(config).toMatchInlineSnapshot(`
  {
    "debug": false,
    "port": 3000,
  }
`);
```

```bash
npx vitest -u          # update snapshots after an intentional change   (jest -u)
```

Review snapshot diffs in code review like any other change. Use them for stable, meaningful output (serialized data, generated code, error messages), not huge or volatile objects. See [Testing Patterns](./06_testing-patterns.md).

## Testing the DOM and components

Install a DOM environment and Testing Library:

```bash
npm install --save-dev jsdom @testing-library/dom @testing-library/user-event
# React: @testing-library/react   Vue: @testing-library/vue
```

```js
// vitest.config.js → test: { environment: 'jsdom' }
import { screen } from '@testing-library/dom';
import userEvent from '@testing-library/user-event';

it('increments the counter on click', async () => {
  document.body.innerHTML = '<button id="b">Count: 0</button>';
  mountCounter(document.getElementById('b'));

  await userEvent.click(screen.getByRole('button'));

  expect(screen.getByRole('button')).toHaveTextContent('Count: 1');
});
```

Per-file environment override:

```js
// @vitest-environment jsdom
```

Query by **role, label, and visible text** (what users perceive), not CSS classes or test IDs where you can avoid it.

## Running tests

```bash
npx vitest                          # watch mode: re-runs affected tests on save
npx vitest run                      # single run (CI)
npx vitest run user.test.js         # files matching a filter
npx vitest -t "rejects invalid"     # tests whose name matches   (jest -t)
npx vitest --coverage
npx vitest --reporter=verbose
npx vitest --ui                     # browser UI for results (needs @vitest/ui)
npx vitest --shuffle                # randomize order to detect inter-test dependencies

npx jest --watch                    # watch changed files (needs git)
npx jest --runInBand                # run serially (-i)
npx jest --detectOpenHandles        # find things keeping the process alive
npx jest --onlyChanged              # (-o) tests related to changed files
```

## Coverage

```bash
npm install --save-dev @vitest/coverage-v8          # Vitest
npx vitest run --coverage

npx jest --coverage                                  # Jest (built in, Babel/V8 providers)
```

| Provider | Notes |
|----------|-------|
| **v8** | Uses V8's built-in coverage: fast, no instrumentation (Vitest default; Jest: `coverageProvider: 'v8'`) |
| **istanbul** | Instruments code: slightly slower, very mature, consistent across runtimes |

Reports: `text` (terminal), `html` (browsable), `lcov` (for services such as Codecov and Coveralls), `json-summary`. Use thresholds in CI so coverage cannot silently slide. Interpretation advice is in [Testing Fundamentals](./01_testing-fundamentals.md).

## Setup files and shared helpers

```js
// test/setup.js
import { afterEach, vi } from 'vitest';

process.env.TZ = 'UTC';                    // note: set TZ before Node starts for reliable effect (see below)

afterEach(() => {
  vi.restoreAllMocks();
  vi.useRealTimers();
});
```

For a reliable time zone, set it when launching: `TZ=UTC vitest run` or in the script: `"test": "TZ=UTC vitest run"`. Changing `process.env.TZ` after startup works in some Node versions but is not guaranteed.

Custom matchers:

```js
expect.extend({
  toBeValidEmail(received) {
    const pass = /^[^@\s]+@[^@\s]+\.[^@\s]+$/.test(received);
    return { pass, message: () => `expected ${received} ${pass ? 'not ' : ''}to be a valid email` };
  },
});

expect('ada@example.com').toBeValidEmail();
```

## Projects, workspaces, and multiple environments

Run different groups with different settings (unit vs integration, node vs jsdom):

```js
// vitest.config.js (Vitest 3+ uses "projects"; older versions used vitest.workspace.js)
import { defineConfig } from 'vitest/config';

export default defineConfig({
  test: {
    projects: [
      { test: { name: 'unit', include: ['src/**/*.test.js'], environment: 'node' } },
      { test: { name: 'integration', include: ['test/**/*.int.test.js'], testTimeout: 30_000 } },
      { test: { name: 'dom', include: ['src/ui/**/*.test.js'], environment: 'jsdom' } },
    ],
  },
});
```

Check the docs for your installed version: configuration keys for workspaces and projects have changed between releases.

## Migrating Jest to Vitest

Most tests work unchanged. Typical edits:

| Jest | Vitest |
|------|--------|
| `jest.fn()`, `jest.spyOn()`, `jest.mock()` | `vi.fn()`, `vi.spyOn()`, `vi.mock()` |
| Globals available | Add `globals: true` or import from `'vitest'` |
| `jest.requireActual` | `await vi.importActual` |
| `jest.config.js` | `vitest.config.js` (or `test` in `vite.config.js`) |
| `jest-environment-jsdom` | `jsdom` or `happy-dom` |
| `__mocks__` folders | Supported, but prefer explicit `vi.mock` factories |
| `done` callbacks | Use `async`/`await` (Vitest does not support `done`) |
| Timers/`jest.useFakeTimers('modern')` | `vi.useFakeTimers()` |

## Continuous integration

```yaml
# .github/workflows/test.yml
name: test
on: [push, pull_request]
jobs:
  test:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-node@v4
        with: { node-version: 22, cache: npm }
      - run: npm ci
      - run: npx vitest run --coverage
```

- Use `npm ci` for reproducible installs
- Run in single-run mode (never watch mode) in CI
- Upload coverage and fail the build on threshold violations
- Cache dependencies; shard large suites (`--shard=1/4`) across machines

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| Committing `.only` | Silently skips the rest of the suite in CI | Lint rule (`no-only-tests`) or a CI check |
| Not restoring spies and timers | State leaks to other tests | `restoreMocks: true`, `vi.useRealTimers()` in `afterEach` |
| Using `vi.mock` variables before initialization | Hoisting error | `vi.hoisted()` |
| `toBe` for objects, `toEqual` where types matter | False failures or false passes | `toEqual`/`toStrictEqual` deliberately |
| Giant snapshots | Nobody reviews them; they get blindly updated | Small, focused snapshots; explicit assertions |
| Snapshot updates (`-u`) without reading diffs | Bugs get enshrined | Review snapshot changes in PRs |
| Mixing ESM and CommonJS under Jest | Confusing errors | Choose one; configure transforms or use Vitest |
| Real timers in tests | Slow, flaky | Fake timers |
| Tests relying on file order | Break under `--shuffle` or parallel runs | Isolate state |
| `done` callbacks with promises | Double completion, timeouts | `async`/`await` |
| Setting `TZ` inside the test process | May not take effect | Set it in the command or script |
| Coverage thresholds set once and forgotten | Either too low or blocking | Review them periodically |
| Different config for local and CI | "Works on my machine" | One config, same Node version, `npm ci` |

## Key takeaways

- Vitest and Jest have nearly identical APIs: `describe`, `it`, `expect`, hooks, `vi`/`jest` mocks, fake timers, snapshots, and coverage
- Prefer Vitest for new ESM, TypeScript, or Vite projects; Jest remains excellent for established codebases and React Native
- Know the matchers: `toBe` vs `toEqual` vs `toStrictEqual`, `toBeCloseTo`, `rejects`/`resolves`, and asymmetric matchers like `expect.any`
- Use mock functions, `spyOn`, `vi.mock`/`jest.mock` for boundaries, and restore them after each test
- Use fake timers and `setSystemTime` for time-dependent code
- Run in watch mode locally and single-run mode with coverage and thresholds in CI
- Keep configuration in one place and separate unit and integration projects

**Next:** [Mocking](./05_mocking.md)
