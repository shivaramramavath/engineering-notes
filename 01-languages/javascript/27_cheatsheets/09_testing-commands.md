# Testing Commands & CLI Flags Cheatsheet

> Quick-reference guide covering execution commands, CLI flags, coverage options, and watch modes across Node.js native test runner, Vitest, and Jest.

| Metadata | Details |
| :--- | :--- |
| **Track** | Reference Cheatsheets (`27_cheatsheets`) |
| **Baseline** | Node.js 22 LTS / Vitest 2+ / Jest 29+ |
| **Level** | Reference / Quick Lookup |

---

## 1. Node.js Native Test Runner (`node --test`)

| Command | Purpose |
| :--- | :--- |
| `node --test` | Executes all test files matching standard glob patterns |
| `node --test tests/app.test.js` | Runs a specific test file |
| `node --test --watch` | Watches files and re-runs tests on change |
| `node --test --test-name-pattern="Auth"` | Runs tests whose description matches regex `"Auth"` |
| `node --test --experimental-test-coverage`| Generates code coverage metrics in terminal |
| `node --test --test-reporter=spec` | Formats output with spec reporter |
| `node --test --test-reporter=tap` | Emits machine-readable Test Anything Protocol (TAP) |
| `node --test --test-timeout=5000` | Fails tests that take longer than 5,000ms |

---

## 2. Vitest CLI Reference

| Command | Purpose |
| :--- | :--- |
| `npx vitest` | Starts Vitest in interactive watch mode |
| `npx vitest run` | Runs test suite once for CI/CD |
| `npx vitest run -t "calculator"` | Runs tests matching test name pattern |
| `npx vitest run --coverage` | Generates coverage report using c8 / v8 |
| `npx vitest --ui` | Opens browser visual dashboard |
| `npx vitest run --update` (`-u`) | Updates outdated inline/file snapshots |

---

## 3. Jest CLI Reference

| Command | Purpose |
| :--- | :--- |
| `npx jest` | Runs test suite |
| `npx jest --watch` | Starts interactive watch mode |
| `npx jest --watchAll` | Watches all files without git repository filter |
| `npx jest -t "auth flow"` | Filters tests by description |
| `npx jest --coverage` | Collects coverage statistics |
| `npx jest -u` | Updates snapshot files |
| `npx jest --bail` | Stops test run immediately upon first failure |
| `npx jest --runInBand` | Runs all tests serially in single thread (debugging) |

---

## 4. Package.json Test Script Recipes

```json
{
  "scripts": {
    "test": "node --test tests/**/*.test.js",
    "test:watch": "node --test --watch tests/**/*.test.js",
    "test:coverage": "node --test --experimental-test-coverage",
    "test:vitest": "vitest run",
    "test:vitest:watch": "vitest"
  }
}
```
