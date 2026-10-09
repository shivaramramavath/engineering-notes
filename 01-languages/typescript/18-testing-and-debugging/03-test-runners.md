# Test Runners

A **test runner** finds your test files, executes them, and reports results. With TypeScript there is an extra question the plain-JavaScript world does not have: *how do `.ts` files get turned into something Node can run, and does anything check the types?* The answer differs by runner, and mixing it up leads to a classic trap: tests pass while the project has type errors. This note compares the main runners, explains the TypeScript-specific setup, and covers configuration.

**Prerequisites:**
- [Unit testing](./00-unit-testing.md)
- [Compiler options](../13-compiler-and-tsconfig/00-compiler-options.md)
- [Compiler architecture](../14-type-system-internals/05-compiler-architecture.md) (transpiling vs type-checking)

---

## The key idea: running vs type-checking

Running TypeScript requires removing the types. Checking TypeScript requires the full compiler. Most fast runners **only strip types** and never check them:

| Step | Done by |
|---|---|
| Strip types, turn `.ts` into `.js` | the runner, via esbuild, SWC, Babel, or the TypeScript transpiler |
| Check types and report errors | `tsc` (or a runner that explicitly runs the type checker) |

So a test suite can be **green while `tsc` reports errors**. Always run type-checking as its own step, in CI and locally:

```json
{
  "scripts": {
    "typecheck": "tsc --noEmit",
    "test": "vitest run",
    "check": "npm run typecheck && npm run test"
  }
}
```

This is also why per-file transpilation options matter: runners that transpile one file at a time need `isolatedModules`-style code ([compiler options](../13-compiler-and-tsconfig/00-compiler-options.md)).

## The main runners

### Vitest

A runner built on Vite, with a Jest-compatible API (`describe`, `it`, `expect`, `vi`). It runs TypeScript and ES modules out of the box through esbuild, with no extra transform setup.

```ts
// vitest.config.ts
import { defineConfig } from "vitest/config";

export default defineConfig({
  test: {
    environment: "node",                     // or "jsdom" / "happy-dom" for DOM code
    include: ["src/**/*.test.ts"],
    coverage: { provider: "v8", reporter: ["text", "html"] },
  },
});
```

```bash
npx vitest            # watch mode
npx vitest run        # single run (CI)
npx vitest run -t "gold discount"
```

Strengths: fast, native ESM and TypeScript, shares config with Vite projects, built-in coverage, mocking, watch mode, and optional type-testing support ([type testing](./04-type-testing.md)). A good default for new projects.

### Jest

The long-established runner with a very large ecosystem. It was designed around CommonJS and Babel, so TypeScript needs a transform:

- **`ts-jest`:** compiles with the TypeScript compiler and can **report type errors** as test failures. Slower, but one tool does both.
- **`@swc/jest`** or **Babel with `@babel/preset-typescript`:** fast transpile-only, no type checking.

```js
// jest.config.js (SWC transform, as an example)
module.exports = {
  testEnvironment: "node",
  transform: { "^.+\\.(t|j)sx?$": "@swc/jest" },
  moduleNameMapper: { "^@/(.*)$": "<rootDir>/src/$1" },     // path aliases
};
```

Strengths: mature, huge community, many guides and plugins, snapshot testing, common in React and NestJS projects. Friction: native ES modules need extra configuration, and CommonJS-era assumptions show up in module mocking. Check the current documentation for your Jest and Node versions.

### Node's built-in test runner

Node ships with `node:test` and `node:assert`:

```ts
import { describe, it } from "node:test";
import assert from "node:assert/strict";
import { slugify } from "./slugify.js";

describe("slugify", () => {
  it("lowercases", () => {
    assert.equal(slugify("Hello"), "hello");
  });
});
```

```bash
node --test
```

Strengths: no dependencies, fast startup. Considerations: TypeScript files need a loader or a runtime that strips types (recent Node versions can, with restrictions, see [type erasure and runtime](../14-type-system-internals/00-type-erasure-and-runtime.md)), and the feature set (mocking, coverage, matchers) is more basic than Vitest or Jest. Fine for libraries that want minimal tooling.

### End-to-end and browser runners

**Playwright** and **Cypress** drive real browsers for end-to-end tests. They have their own runners and TypeScript support, and are used in addition to a unit or integration runner, not instead of one ([integration testing](./01-integration-testing.md)).

### Others

Mocha (with Chai), AVA, and similar tools remain in use, mostly in older projects. They work with TypeScript through a loader or a transpiling register hook.

## Choosing

| Situation | Reasonable choice |
|---|---|
| New project, Vite or modern ESM tooling | **Vitest** |
| Existing Jest suite, React or NestJS conventions | stay with **Jest**, use SWC for speed |
| Want type errors surfaced by the test run itself | Jest with **ts-jest**, or Vitest's typecheck mode |
| Small library, zero dependencies | **`node:test`** |
| Browser end-to-end tests | **Playwright** or **Cypress** |

Migrating from Jest to Vitest is usually modest, because the APIs are similar (`jest.fn` becomes `vi.fn`, `jest.mock` becomes `vi.mock`), but module-mocking and timer details differ. Test the migration on a few files first.

## TypeScript setup details

### Globals vs imports

You can import test functions explicitly (`import { it, expect } from "vitest"`), or enable **globals** so `describe`, `it`, and `expect` are available everywhere. If you enable globals, TypeScript needs to know about them:

```jsonc
// tsconfig.json
{
  "compilerOptions": {
    "types": ["vitest/globals"]      // or "jest" / "node" for the relevant runner
  }
}
```

Explicit imports avoid this configuration and make files self-describing, and they keep test globals out of production code. If you use globals, put the `types` entry in a **separate test tsconfig** so `describe` is not visible in `src/` ([tsconfig recipes](../13-compiler-and-tsconfig/05-tsconfig-recipes.md)).

### A tsconfig for tests

```jsonc
// tsconfig.test.json
{
  "extends": "./tsconfig.json",
  "compilerOptions": { "noEmit": true, "types": ["vitest/globals", "node"] },
  "include": ["src", "test"]
}
```

Point the editor and `tsc --noEmit` at the config that includes your test files, so tests are type-checked too.

### Path aliases

`paths` in `tsconfig` only affect type-checking. The test runner resolves imports itself, so aliases must be configured there too: Vitest `resolve.alias` (or a plugin that reads tsconfig paths), Jest `moduleNameMapper` ([module resolution and paths](../13-compiler-and-tsconfig/03-module-resolution-and-paths.md)).

### ES modules and `.js` extensions

If your project uses Node ESM with `nodenext`, relative imports carry `.js` extensions even in `.ts` files. Vitest handles this. Other runners may need a resolver option or a loader. If imports like `./thing.js` fail to resolve to `thing.ts`, check the runner's module resolution settings.

## Environments

| Environment | Use for |
|---|---|
| `node` | backend code, pure logic |
| `jsdom` or `happy-dom` | simulated browser DOM for front-end unit tests (React components with Testing Library) |
| a real browser | browser-only APIs, rendering fidelity (Playwright component or e2e tests) |

Simulated DOMs are fast but incomplete (layout, some APIs). Use the lightest environment that supports what you are testing. For front-end specifics, see [React and frontend](../19-react-and-frontend/README.md).

## Coverage

```bash
npx vitest run --coverage
```

Coverage providers are typically **v8** (fast, uses the engine's built-in data) or **Istanbul** (instruments code, more configurable). Pick one, set thresholds if your team wants a floor, and exclude generated code and config files. Remember that coverage finds untested code and does not prove correctness ([unit testing](./00-unit-testing.md)).

## Running tests well

- **Watch mode** re-runs affected tests on change, which keeps feedback loops short.
- **Filtering:** `-t "name"` by test name, a path argument by file, `.only` temporarily.
- **Parallelism:** runners run test files in parallel workers. Tests that share external state (a database) need isolation, or a limit on concurrency ([integration testing](./01-integration-testing.md)).
- **Separate suites:** use naming patterns or separate configs for unit, integration, and end-to-end tests, so the fast set runs constantly.
- **CI:** run `typecheck`, lint, and tests as separate steps so failures are easy to read, and use sharding to split large suites across machines ([CI and deployment](../21-production-tooling/05-ci-and-deployment.md)).
- **Reporters:** the default reporter is fine locally. CI often wants JUnit or JSON output for dashboards.

## Important rules and misconceptions

- **A passing test run does not mean the types are correct.** Most runners never type-check.
- **Runner configs are separate from `tsconfig`.** Aliases, module formats, and environments are configured in both places.
- **Vitest and Jest are similar, not identical.** Mocking, timers, and ESM details differ.
- **Globals are a convenience, not a requirement.** Explicit imports are clearer and avoid type leakage.
- **A faster runner does not fix slow tests.** Slow tests usually come from I/O and setup, not the runner.

## Common mistakes

- Assuming the test run catches type errors.
- Enabling test globals in the main `tsconfig`, so `describe` and `it` are visible in production code.
- Forgetting to configure path aliases in the runner.
- Mixing CommonJS and ESM assumptions, then fighting module mocking.
- Running everything in one slow suite, so developers stop running it.
- Letting tests share a database while running in parallel.
- Pinning no versions, so runner or TypeScript upgrades change behavior unexpectedly.
- Leaving `.only` in committed tests (use a lint rule or CI check to block it).

## Debugging

- If a test file is not picked up, check the `include` pattern and the file naming (`.test.ts` vs `.spec.ts`).
- If imports fail in tests but work in the app, compare module resolution: aliases, extensions, ESM versus CommonJS, and the test environment.
- If the editor shows type errors in tests that the runner ignores (or the reverse), check which tsconfig each uses.
- If tests behave differently in CI, compare Node versions, environment variables, and parallelism settings.
- Run one file in isolation and with `--no-threads` or a single worker (flag names vary by runner) to rule out cross-test interference.
- For deeper investigation, attach a debugger to the runner ([debugging](./05-debugging.md)).

## Quick summary

- Runners execute tests, but TypeScript adds a split: **running** (strip types) versus **type-checking** (`tsc`). Run both.
- Vitest is a strong default for new projects (native TypeScript and ESM, Jest-like API). Jest is the established option (use SWC or ts-jest). `node:test` suits minimal setups.
- Configure the runner for aliases, environment (`node` or a DOM), and coverage, and keep test globals out of production type scope.
- Separate fast unit tests from slow integration and end-to-end suites.
- Keep a `typecheck` script in CI. Green tests alone do not mean type-correct code.

**Next:** [Type testing](./04-type-testing.md)
