# CI and Deployment for TypeScript: GitHub Actions, Caching, and Zero-Emission

> Blueprint for high-speed, zero-defect TypeScript CI/CD pipelines: GitHub Actions workflows, distributed `.tsbuildinfo` caching, parallelized type-checking (`tsc --noEmit`), and zero-emission containerization.

| Metadata | Details |
| :--- | :--- |
| **Category** | Production Tooling |
| **Difficulty** | Advanced |
| **Prerequisites** | [21-production-tooling/01-build-and-bundling.md](./01-build-and-bundling.md), [13-compiler-options/00-tsconfig-json.md](../13-compiler-options/00-tsconfig-json.md) |
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

Continuous Integration (CI) is the final barrier protecting production environments from broken types, runtime exceptions, and regressions. In enterprise software development, however, naive TypeScript CI pipelines frequently suffer from two severe failure modes:
1. **Excessive Duration**: Running full compilation (`tsc`), linting, and testing sequentially in a single job causes CI times to balloon to 15–30 minutes, drastically slowing down pull request velocity.
2. **False Confidence**: Running builds with tools that strip types (Babel, Vite, esbuild) without a dedicated `tsc --noEmit` validation step allows severe type violations to slip undetected into production Docker containers.

Modern CI engineering employs a **parallel, cached, multi-stage architecture**:
- **Matrix Parallelism**: Concurrently run type-checking, linting, unit testing, and packaging across independent runners.
- **Incremental State Caching**: Persist `.tsbuildinfo` files across CI runs to limit compiler validation only to changed files.
- **Multi-Stage Docker Builds**: Transpile and bundle in ephemeral build containers, copying only zero-emission JavaScript outputs and production dependencies into hardened runtime images.

```
+-------------------------------------------------------------------------+
|                  Modern High-Velocity CI Pipeline                       |
+-------------------------------------------------------------------------+
|                                                                         |
|                          git push origin feat/api                       |
|                                     |                                   |
|                                     v                                   |
|                        [ GitHub Actions Matrix ]                        |
|                                     |                                   |
|         +-----------------+---------+---------+-----------------+       |
|         |                 |                   |                 |       |
|         v                 v                   v                 v       |
|  [ Job: Lint ]    [ Job: Typecheck ]   [ Job: Tests ]    [ Job: Build ] |
|  Biome / ESLint   tsc --noEmit         Vitest            tsup / Vite    |
|  (30s)            (Cached .tsbuildinfo) (Parallel nodes) (Artifacts)    |
|                   (45s)                (60s)             (50s)          |
|         |                 |                   |                 |       |
|         +-----------------+---------+---------+-----------------+       |
|                                     |                                   |
|                                     v                                   |
|                     [ Job: Docker Containerize ]                        |
|                     Multi-Stage Minimal Runtime Image                   |
|                                                                         |
+-------------------------------------------------------------------------+
```

---

## 2. Core Concepts

### Zero-Emission Type Checking (`tsc --noEmit`)
In modern workflows, type checking is decoupled from artifact generation. `tsc --noEmit` validates the entire type system graph without writing `.js` or `.d.ts` files to disk. It returns exit code `0` on success and `1` on type error, making it the ideal CI quality gate.

### Build Info Caching (`incremental: true`)
When `"incremental": true` is enabled in `tsconfig.json`, TypeScript generates a `.tsbuildinfo` file storing file hashes, dependency trees, and resolved symbols. In CI, saving and restoring this file via `@actions/cache` reduces type check times by 60%–80%.

### Multi-Stage Docker Builds
Production Docker images should never contain TypeScript compilers, source code, or `devDependencies`. A multi-stage Dockerfile builds the application in a temporary Node.js container and copies only transpiled JavaScript and production `node_modules` into a lean Alpine/Distroless container.

---

## 3. How It Works Internally

1. **GitHub Actions Cache Key**: CI generates a cache key based on the lockfile and source hashes:
   `key: tsbuildinfo-${{ runner.os }}-${{ hashFiles('pnpm-lock.yaml') }}-${{ hashFiles('src/**/*.ts') }}`
2. **Incremental Validation**: `tsc` reads `.tsbuildinfo`. It compares the timestamp and SHA-256 hashes of every source file against the stored table. Files whose hashes match are assumed valid; only modified files and their transitive importers are re-evaluated.
3. **Exit Code Propagation**: If any diagnostic with category `Error` is generated, `tsc` exits with status `2` (or `1`), immediately terminating the CI step.

---

## 4. Syntax & API Reference

### Production GitHub Actions Workflow (`.github/workflows/ci.yml`)
```yaml
name: Continuous Integration

on:
  push:
    branches: [main]
  pull_request:
    branches: [main]

jobs:
  typecheck:
    name: TypeScript Quality Gate
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4

      - uses: pnpm/action-setup@v3
        with:
          version: 9

      - uses: actions/setup-node@v4
        with:
          node-version: 22
          cache: 'pnpm'

      - name: Restore tsbuildinfo cache
        uses: actions/cache@v4
        with:
          path: '**/*.tsbuildinfo'
          key: ${{ runner.os }}-tsbuildinfo-${{ hashFiles('pnpm-lock.yaml') }}-${{ github.sha }}
          restore-keys: |
            ${{ runner.os }}-tsbuildinfo-${{ hashFiles('pnpm-lock.yaml') }}-
            ${{ runner.os }}-tsbuildinfo-

      - name: Install dependencies
        run: pnpm install --frozen-lockfile

      - name: Run typecheck
        run: pnpm tsc --noEmit
```

### Multi-Stage Dockerfile (`Dockerfile`)
```dockerfile
# Stage 1: Build & Transpile
FROM node:22-alpine AS builder
WORKDIR /app

RUN corepack enable && corepack prepare pnpm@latest --activate

COPY pnpm-lock.yaml package.json ./
RUN pnpm install --frozen-lockfile

COPY . .
RUN pnpm build

# Stage 2: Minimal Production Runtime
FROM node:22-alpine AS runner
WORKDIR /app
ENV NODE_ENV=production

RUN corepack enable && corepack prepare pnpm@latest --activate

COPY pnpm-lock.yaml package.json ./
RUN pnpm install --prod --frozen-lockfile

# Copy only compiled outputs from builder
COPY --from=builder /app/dist ./dist

USER node
EXPOSE 3000

CMD ["node", "dist/index.js"]
```

---

## 5. Practical Examples

### Example 1: Enforcing Strict Clean CI Flags
```json
// package.json scripts
{
  "scripts": {
    "typecheck": "tsc --noEmit --pretty false",
    "typecheck:ci": "tsc --build --noEmit",
    "lint:ci": "eslint . --max-warnings 0"
  }
}
```

### Example 2: Problem Matcher for GitHub Actions Annotations
```yaml
# Adding TypeScript problem matcher allows errors to display inline on GitHub PR diffs
- name: Register TypeScript Problem Matcher
  run: echo "::add-matcher::.github/tsc-problem-matcher.json"

- name: Typecheck
  run: pnpm tsc --noEmit
```

### Example 3: Edge Case - Docker Ignore Optimization
```
# .dockerignore
node_modules
dist
.git
.github
*.tsbuildinfo
coverage
*.md
```

### Example 4: Compiler Error & CI Pipeline Termination
```typescript
// If an engineer submits code with a type error:
export function formatCurrency(amount: number, currency: string): string {
  return `${currency} ${amount.toFixed(2)}`;
}

// In another file:
// @ts-expect-error - TS2345: Argument of type 'string' is not assignable to parameter of type 'number'.
formatCurrency("100.50", "USD");

// Running `tsc --noEmit` in CI returns exit code 1, automatically
// blocking merge into the main branch.
```

### Example 5: Health Check Script for Container Deployments
```typescript
import http from "node:http";

export function performHealthCheck(port: number): Promise<boolean> {
  return new Promise((resolve) => {
    const req = http.get(`http://localhost:${port}/health`, (res) => {
      resolve(res.statusCode === 200);
    });
    req.on("error", () => resolve(false));
    req.setTimeout(2000, () => {
      req.destroy();
      resolve(false);
    });
  });
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **Stale Cache Poisoning**:
   If a cache key does not include lockfile or tsconfig changes, a corrupted `.tsbuildinfo` can falsely pass CI despite real type errors. Always include lockfile hashes in your cache keys.
2. **Memory Limit Crashes (Out of Memory)**:
   In large monorepos, `tsc` can exceed Node's default memory limit (2GB). Set `NODE_OPTIONS="--max-old-space-size=4096"` in GitHub Actions job environment variables.
3. **CI vs. Local Environment Discrepancies**:
   Differences between developer operating systems (case-insensitive macOS/Windows vs. case-sensitive Linux CI runners) cause TS2307 import errors. Always enable `"forceConsistentCasingInFileNames": true` in `tsconfig.json`.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Bad Practice | Good Practice | Problem / Diagnostic |
| :--- | :--- | :--- | :--- |
| **Omitting Typecheck in CI** | Only running `vite build` | Add explicit `tsc --noEmit` step | Broken types pass silently to production |
| **Shipping TS to Docker** | Running `ts-node` in production container | Multi-stage build emitting raw JS | High memory usage, slow startup |
| **Unfrozen Lockfiles** | Running `pnpm install` in CI | Use `pnpm install --frozen-lockfile` | Unintended dependency version drift |
| **Ignoring Case Sensitivity** | `import from './MyFile'` on Linux | `forceConsistentCasingInFileNames: true` | `TS1149: File name differs only in casing` |
| **Missing `skipLibCheck`** | Re-checking all `@types/*` in CI | Enable `"skipLibCheck": true` | CI duration doubles unnecessarily |
| **Serial CI Execution** | Running lint $\rightarrow$ test $\rightarrow$ typecheck | Run parallel GitHub Actions matrix jobs | Long developer feedback loop |

---

## 8. Performance & Type-Checker Complexity

- `tsc --noEmit` duration scales with AST size and type inference depth.
- **Cache Hit vs. Miss**:
  - Cold CI run (no cache): 75 seconds.
  - Cached CI run (`.tsbuildinfo` restored): 8 seconds (90% reduction).
- Enable `"skipLibCheck": true` to skip re-checking third-party declarations inside `node_modules`.

---

## 9. Compiler Internals

Under `tsc --noEmit`:
1. The **Parser** and **Binder** process all project files.
2. The **TypeChecker** performs full diagnostics across all statements.
3. The **Emitter** is completely bypassed (`emitWorker` does not write files to the file system).
4. If `"incremental": true` is enabled, the compiler serializes the diagnostic cache graph into the `.tsbuildinfo` binary/JSON format before process exit.

---

## 10. Best Practices & Design Guidelines

1. **Enforce `tsc --noEmit` as a Required Status Check**: Never merge PRs without successful type check status.
2. **Use Multi-Stage Dockerfiles**: Keep production container images lean and hardened.
3. **Enable `forceConsistentCasingInFileNames`**: Prevents cross-OS import failures.
4. **Cache `.tsbuildinfo` in CI**: Dramatically accelerates pull request review cycles.

---

## 11. Real-World Case Study

An e-commerce backend built with NestJS suffered from 22-minute GitHub Actions build times, blocking critical hotfix releases.

**Optimization Steps**:
1. Split monolithic pipeline into three parallel GitHub Actions jobs: `typecheck`, `lint`, and `test`.
2. Restored `.tsbuildinfo` from GitHub Actions cache.
3. Configured multi-stage Alpine Docker build with `--frozen-lockfile`.

**Results**:
- Total CI duration dropped from **22 minutes** to **2 minutes 15 seconds**.
- Image size shrank from 1.2GB to **86MB**.

---

## 12. Interview Questions

<details>
<summary>1. Why should `tsc --noEmit` be executed in CI if bundlers like Vite or esbuild already build the project?</summary>

Bundlers like esbuild or Vite transpile code by stripping types using syntax parsers; they do not perform semantic type checking. Without `tsc --noEmit`, type errors and invalid property accesses will deploy to production undetected.
</details>

<details>
<summary>2. How does `.tsbuildinfo` accelerate CI pipelines?</summary>

It stores file modification hashes and dependency graphs from previous compilations. On subsequent runs, `tsc` only checks files that have changed since the last cached run.
</details>

<details>
<summary>3. Why is `forceConsistentCasingInFileNames: true` essential for CI?</summary>

macOS and Windows file systems are case-insensitive, meaning `import './User'` works even if the file is `user.ts`. Linux CI runners are case-sensitive and will crash with module not found errors unless casing matches exactly.
</details>

<details>
<summary>4. What is the benefit of multi-stage Docker builds for TypeScript?</summary>

They prevent development tools (`tsc`, `tsup`, `eslint`, devDependencies) and source files from being included in the final runtime image, reducing image size, memory footprint, and attack surface.
</details>

<details>
<summary>5. What does the `--frozen-lockfile` flag do in CI package managers?</summary>

It ensures that the package manager throws an immediate error if `package.json` and the lockfile (`pnpm-lock.yaml`, `package-lock.json`) are out of sync, preventing unexpected dependency upgrades.
</details>

<details>
<summary>6. How can you prevent `tsc` from running out of memory in large CI builds?</summary>

By passing `NODE_OPTIONS="--max-old-space-size=4096"` (or higher) to increase Node's V8 heap memory ceiling.
</details>

<details>
<summary>7. What is the purpose of `skipLibCheck: true`?</summary>

It instructs TypeScript to skip type-checking `.d.ts` declaration files inside `node_modules`, saving significant compilation time while trusting that published library types are internally valid.
</details>

<details>
<summary>8. How do GitHub Actions problem matchers improve developer experience?</summary>

They parse terminal output from tools like `tsc` and render clickable inline error annotations directly on pull request file diffs in the GitHub web UI.
</details>

---

## 13. Exercises & Challenges

<details>
<summary>Challenge 1 (Easy): Create a minimal Dockerignore for TypeScript</summary>

**Task**: Write a `.dockerignore` file excluding non-essential files from Docker build context.

**Solution**:
```
node_modules
dist
.git
.github
*.tsbuildinfo
coverage
```
</details>

<details>
<summary>Challenge 2 (Easy): Add incremental build option to tsconfig.json</summary>

**Task**: Configure `tsconfig.json` to generate build information for caching.

**Solution**:
```json
{
  "compilerOptions": {
    "incremental": true,
    "tsBuildInfoFile": "./.tsbuildinfo"
  }
}
```
</details>

<details>
<summary>Challenge 3 (Medium): Author a parallel GitHub Actions CI workflow</summary>

**Task**: Write a GitHub Actions YAML workflow that runs `lint` and `typecheck` jobs in parallel.

**Solution**:
```yaml
name: CI
on: [push, pull_request]

jobs:
  lint:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-node@v4
        with: { node-version: 22 }
      - run: npm ci
      - run: npm run lint

  typecheck:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-node@v4
        with: { node-version: 22 }
      - run: npm ci
      - run: npx tsc --noEmit
```
</details>

<details>
<summary>Challenge 4 (Medium): Secure runtime user in Dockerfile</summary>

**Task**: Modify a Dockerfile runner stage to run as non-root user `node`.

**Solution**:
```dockerfile
FROM node:22-alpine AS runner
WORKDIR /app
COPY --chown=node:node dist/ ./dist
COPY --chown=node:node package.json ./
USER node
CMD ["node", "dist/index.js"]
```
</details>

<details>
<summary>Challenge 5 (Hard): GitHub Actions workflow with cache eviction fallback</summary>

**Task**: Author a GitHub Actions workflow step restoring `.tsbuildinfo` with primary and fallback keys.

**Solution**:
```yaml
- name: Restore TypeScript Cache
  uses: actions/cache@v4
  with:
    path: .tsbuildinfo
    key: ${{ runner.os }}-ts-${{ hashFiles('pnpm-lock.yaml') }}-${{ github.sha }}
    restore-keys: |
      ${{ runner.os }}-ts-${{ hashFiles('pnpm-lock.yaml') }}-
      ${{ runner.os }}-ts-
```
</details>

<details>
<summary>Challenge 6 (Hard): Pre-push Git hook enforcing type checking</summary>

**Task**: Write a shell script for `.husky/pre-push` that verifies git branch status and runs `tsc --noEmit` before allowing pushes.

**Solution**:
```bash
#!/bin/sh
echo "Validating TypeScript types before push..."

pnpm tsc --noEmit
if [ $? -ne 0 ]; then
  echo "TypeScript typecheck failed! Aborting push."
  exit 1
fi

echo "Typecheck passed."
exit 0
```
</details>
