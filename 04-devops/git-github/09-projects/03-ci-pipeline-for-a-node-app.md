<!--
================================================================================
Metadata:
  Title: Reference Project 03 - Complete CI Pipeline for Node.js & TypeScript
  Track: Git & GitHub / DevOps
  Difficulty: Intermediate
  Estimated Time: 60 minutes
  Prerequisites: 04-github-actions/02-workflow-syntax.md, 04-github-actions/10-matrix-and-concurrency.md
  Key Terminology: Matrix Build, Dependency Caching, Status Checks, Code Coverage, Concurrency Cancellation
================================================================================
-->

# Reference Project 03: Complete CI Pipeline for Node.js & TypeScript

[<- Back to Projects Overview](./README.md) | [Next Project: 04-docker-build-and-push-to-ghcr.md ->](./04-docker-build-and-push-to-ghcr.md)

---

## 1. Overview & Architecture

A production-grade Continuous Integration (CI) pipeline serves as the primary automated quality gate for all code before it merges into a protected trunk branch. An enterprise CI pipeline must balance three competing requirements: **rigorous quality verification** (formatting, linting, type safety, unit and integration tests), **fast execution speed** (via aggressive dependency caching and parallel execution), and **cost efficiency** (by auto-canceling stale superseded runs).

This project implements a complete, battle-tested GitHub Actions workflow for an Express / TypeScript backend API.

### Pipeline Architecture Diagram

```
+-------------------------------------------------------------------------+
| Pull Request Event (Opened / Synchronize to 'main')                     |
+-------------------------------------------------------------------------+
                                    |
            +-----------------------+-----------------------+
            | (Concurrency: cancel-in-progress = true)      |
            v                                               v
+------------------------+                     +------------------------+
| Job: Lint & Format     |                     | Job: TypeScript Typecheck|
| - npm run lint         |                     | - npx tsc --noEmit     |
| - Prettier check       |                     +------------------------+
+------------------------+                                  |
            \                                              /
             +----------------------+---------------------+
                                    |
                                    v
+-------------------------------------------------------------------------+
| Job: Unit Test Matrix (Node 18.x, 20.x, 22.x)                          |
| - Parallel runners on ubuntu-latest                                     |
| - Dependency caching via actions/setup-node (npm cache)                |
| - Code coverage reporting with lcov                                     |
+-------------------------------------------------------------------------+
                                    |
                                    v
+-------------------------------------------------------------------------+
| Job: Build & Package Verification                                       |
| - npm run build (Emits dist/)                                           |
| - Verifies zero bundle compilation errors                              |
+-------------------------------------------------------------------------+
                                    |
                                    v
+-------------------------------------------------------------------------+
| Required Status Check Gateway -> Pull Request Unblocked for Merge       |
+-------------------------------------------------------------------------+
```

---

## 2. Prerequisites & Setup

### Environment Requirements
- **Node.js**: Version 20.x LTS
- **npm**: Version 10.x
- **Git**: Version 2.30+

### Repository File Structure
```text
node-ci-project/
├── .github/
│   └── workflows/
│       └── ci.yml             # Complete CI workflow definition
├── src/
│   ├── index.ts               # Express server entry point
│   ├── calculator.ts          # Core business logic module
│   └── calculator.test.ts     # Jest / Vitest test file
├── .eslintrc.json             # ESLint configuration
├── .prettierrc                # Prettier formatting rules
├── package.json               # Node dependencies and scripts
└── tsconfig.json              # TypeScript compiler configuration
```

---

## 3. Step-by-Step Implementation Walkthrough

### Step 1: Initialize Project Directory & Package Manifest
```bash
mkdir -p ~/git-labs/node-ci-project && cd ~/git-labs/node-ci-project
git init
```

Create `package.json`:
```json
cat << 'EOF' > package.json
{
  "name": "node-typescript-ci-demo",
  "version": "1.0.0",
  "private": true,
  "scripts": {
    "build": "tsc",
    "lint": "eslint src/**/*.ts",
    "format:check": "prettier --check src/**/*.ts",
    "test": "jest --coverage",
    "typecheck": "tsc --noEmit"
  },
  "devDependencies": {
    "@types/jest": "^29.5.12",
    "@types/node": "^20.11.24",
    "@typescript-eslint/eslint-plugin": "^7.1.0",
    "@typescript-eslint/parser": "^7.1.0",
    "eslint": "^8.57.0",
    "jest": "^29.7.0",
    "prettier": "^3.2.5",
    "ts-jest": "^29.1.2",
    "typescript": "^5.3.3"
  }
}
EOF
```

### Step 2: TypeScript & Linter Configuration

`tsconfig.json`:
```json
cat << 'EOF' > tsconfig.json
{
  "compilerOptions": {
    "target": "ES2022",
    "module": "commonjs",
    "moduleResolution": "node",
    "strict": true,
    "esModuleInterop": true,
    "skipLibCheck": true,
    "forceConsistentCasingInFileNames": true,
    "outDir": "./dist",
    "rootDir": "./src"
  },
  "include": ["src/**/*"]
}
EOF
```

`.eslintrc.json`:
```json
cat << 'EOF' > .eslintrc.json
{
  "parser": "@typescript-eslint/parser",
  "plugins": ["@typescript-eslint"],
  "extends": [
    "eslint:recommended",
    "plugin:@typescript-eslint/recommended"
  ],
  "rules": {
    "@typescript-eslint/no-explicit-any": "error",
    "@typescript-eslint/explicit-function-return-type": "warn"
  }
}
EOF
```

`.prettierrc`:
```json
cat << 'EOF' > .prettierrc
{
  "semi": true,
  "trailingComma": "all",
  "singleQuote": true,
  "printWidth": 100,
  "tabWidth": 2
}
EOF
```

### Step 3: Application Code & Unit Tests

`src/calculator.ts`:
```typescript
cat << 'EOF' > src/calculator.ts
export class Calculator {
  public add(a: number, b: number): number {
    return a + b;
  }

  public divide(a: number, b: number): number {
    if (b === 0) {
      throw new Error('Division by zero is not permitted');
    }
    return a / b;
  }
}
EOF
```

`src/calculator.test.ts`:
```typescript
cat << 'EOF' > src/calculator.test.ts
import { Calculator } from './calculator';

describe('Calculator Service', () => {
  let calc: Calculator;

  beforeEach(() => {
    calc = new Calculator();
  });

  test('adds positive numbers correctly', () => {
    expect(calc.add(10, 20)).toBe(30);
  });

  test('throws error on division by zero', () => {
    expect(() => calc.divide(10, 0)).toThrow('Division by zero is not permitted');
  });
});
EOF
```

### Step 4: Author GitHub Actions Workflow (`.github/workflows/ci.yml`)

```yaml
mkdir -p .github/workflows
cat << 'EOF' > .github/workflows/ci.yml
name: "Continuous Integration"

on:
  push:
    branches: ["main"]
  pull_request:
    branches: ["main"]

# Automatically cancel redundant in-progress builds when new commits are pushed
concurrency:
  group: ${{ github.workflow }}-${{ github.ref }}
  cancel-in-progress: true

permissions:
  contents: read

jobs:
  lint-and-typecheck:
    name: "Code Quality & Typecheck"
    runs-on: ubuntu-latest
    steps:
      - name: Checkout Code
        uses: actions/checkout@v4

      - name: Setup Node.js 20
        uses: actions/setup-node@v4
        with:
          node-version: 20
          cache: "npm"

      - name: Install Dependencies
        run: npm ci

      - name: Verify Formatting (Prettier)
        run: npm run format:check

      - name: Run ESLint
        run: npm run lint

      - name: Run TypeScript Typecheck
        run: npm run typecheck

  test-matrix:
    name: "Test on Node ${{ matrix.node-version }}"
    runs-on: ubuntu-latest
    needs: [lint-and-typecheck]
    strategy:
      fail-fast: false
      matrix:
        node-version: [18.x, 20.x, 22.x]

    steps:
      - name: Checkout Code
        uses: actions/checkout@v4

      - name: Setup Node.js ${{ matrix.node-version }}
        uses: actions/setup-node@v4
        with:
          node-version: ${{ matrix.node-version }}
          cache: "npm"

      - name: Install Dependencies
        run: npm ci

      - name: Execute Tests with Coverage
        run: npm test

  build:
    name: "Build Verification"
    runs-on: ubuntu-latest
    needs: [test-matrix]
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-node@v4
        with:
          node-version: 20
          cache: "npm"
      - run: npm ci
      - run: npm run build
      - name: Verify Build Artifact Output
        run: |
          test -d dist || { echo "dist directory missing!"; exit 1; }
          echo "Build verification successful."
EOF
```

---

## 4. Verification & Testing

### Step 1: Local Verification
Run commands locally before pushing:
```bash
npm ci
npm run format:check
npm run lint
npm run typecheck
npm test
npm run build
```

### Step 2: Push and Monitor Workflow via `gh` CLI
```bash
git add .
git commit -m "feat(ci): implement complete node CI pipeline"
git push -u origin main

# Watch workflow run directly in terminal
gh run watch
```

Expected output:
```text
✓ Code Quality & Typecheck in 24s
✓ Test on Node 18.x in 32s
✓ Test on Node 20.x in 29s
✓ Test on Node 22.x in 31s
✓ Build Verification in 18s
Completed successfully in 1m 15s.
```

---

## 5. Production Readiness & Troubleshooting

### Production Readiness Checklist
- [x] Use `npm ci` rather than `npm install` for reproducible dependency graphs.
- [x] Enable dependency caching (`cache: 'npm'` in `actions/setup-node`).
- [x] Configure `concurrency.cancel-in-progress: true` to save runner minutes.
- [x] Enforce branch protection rules requiring all matrix checks to pass before merging.

### Troubleshooting Guide

| Failure Mode | Diagnosis | Solution |
|:---|:---|:---|
| `npm ci` fails with lockfile error | `package.json` and `package-lock.json` are out of sync | Run `npm install` locally and commit updated lockfile |
| Lint step fails on formatting | Developer didn't run prettier | Run `npx prettier --write .` and commit |
| Out of memory error during build | Node default memory limit reached | Set `NODE_OPTIONS: "--max-old-space-size=4096"` |

---

[<- Back to Projects Overview](./README.md) | [Next Project: 04-docker-build-and-push-to-ghcr.md ->](./04-docker-build-and-push-to-ghcr.md)
