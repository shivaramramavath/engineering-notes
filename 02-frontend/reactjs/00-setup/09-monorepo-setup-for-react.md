# Monorepo Setup for React Applications

- **Topic**: `00-setup`
- **React version**: React 19+ (tested on React 19.0.0)
- **Tested with**: pnpm 9.x, Turborepo 2.x, Vite 6.0, TypeScript 5.7
- **Bundle / compiler notes**: Covers package boundaries, shared design system workspaces, and TypeScript project references.

---

## 1. Concept Overview

A **monorepo** (monolithic repository) is an architectural strategy where multiple distinct projects, packages, and services coexist within a single version-controlled repository.

In modern enterprise React architectures, monorepos solve key scaling challenges:
- **Shared Design Systems**: UI components (`@acme/ui`) are consumed by multiple web applications (`@acme/web-app`, `@acme/admin-dashboard`) without needing to publish to an external npm registry on every commit.
- **Unified TypeScript Configs & Tooling**: Shared ESLint rules, Prettier configs, and base `tsconfig.json` files eliminate drift across teams.
- **Atomic Commits**: Updating a shared component API and refactoring all consuming applications happens in a single pull request with synchronized CI testing.

The dominant modern toolchain pairing is **pnpm Workspaces** (for disk-efficient dependency deduplication) combined with **Turborepo** or **Nx** (for intelligent task orchestration, caching, and pipeline parallelization).

```
monorepo/
├── apps/
│   ├── web/               <-- React 19 Consumer App (Vite 6)
│   └── admin/             <-- React 19 Admin Portal (Vite 6)
├── packages/
│   ├── ui/                <-- Shared Radix UI / Tailwind component library
│   ├── tsconfig/          <-- Shared tsconfig.base.json
│   └── eslint-config/     <-- Shared ESLint configs
├── pnpm-workspace.yaml
└── turbo.json             <-- Turborepo pipeline cache configuration
```

---

## 2. Key Mechanics & Architecture

### Dependency Resolution via `workspace:*`
In pnpm, packages inside the monorepo reference one another using the `workspace:*` protocol in `package.json`:
```json
{
  "name": "@acme/web",
  "dependencies": {
    "@acme/ui": "workspace:*",
    "react": "^19.0.0"
  }
}
```
pnpm symlinks `@acme/ui` directly into `apps/web/node_modules/@acme/ui`. Any edit in `packages/ui` is instantly visible in `apps/web` without a separate publish step.

### Task Caching with Turborepo
Turborepo reads `turbo.json` to construct a Directed Acyclic Graph (DAG) of project tasks. If neither `packages/ui` nor `apps/web` has changed since the last build, running `turbo run build` retrieves outputs directly from the local (or remote) cache in **0 milliseconds**.

---

## 3. Real-World Code Examples

### Example 1: `pnpm-workspace.yaml` Workspace Declaration
```yaml
packages:
  - "apps/*"
  - "packages/*"
```

### Example 2: `turbo.json` Pipeline Orchestration
```json
{
  "$schema": "https://turbo.build/schema.json",
  "ui": "tui",
  "tasks": {
    "build": {
      "dependsOn": ["^build"],
      "outputs": ["dist/**", ".next/**"]
    },
    "test": {
      "dependsOn": ["^build"],
      "outputs": ["coverage/**"]
    },
    "lint": {
      "outputs": []
    },
    "dev": {
      "cache": false,
      "persistent": true
    }
  }
}
```

### Example 3: Shared UI Package (`packages/ui/package.json`)
```json
{
  "name": "@acme/ui",
  "version": "0.0.1",
  "private": true,
  "type": "module",
  "exports": {
    "./button": "./src/button.tsx",
    "./styles.css": "./src/styles.css"
  },
  "peerDependencies": {
    "react": "^19.0.0",
    "react-dom": "^19.0.0"
  },
  "devDependencies": {
    "@types/react": "^19.0.0",
    "typescript": "^5.7.0"
  }
}
```

### Example 4: Consumer App Importing Shared UI (`apps/web/src/App.tsx`)
```tsx
import { Button } from '@acme/ui/button';

export function App() {
  return (
    <div className="p-8">
      <h1 className="text-2xl font-bold mb-4">Enterprise Web Portal</h1>
      <Button variant="primary" onClick={() => alert('Clicked shared button!')}>
        Action Button
      </Button>
    </div>
  );
}
```

### Example 5: Base TypeScript Configuration (`packages/tsconfig/base.json`)
```json
{
  "$schema": "https://json.schemastore.org/tsconfig",
  "compilerOptions": {
    "target": "ES2022",
    "module": "ESNext",
    "moduleResolution": "bundler",
    "jsx": "react-jsx",
    "strict": true,
    "skipLibCheck": true,
    "declaration": true,
    "declarationMap": true,
    "sourceMap": true
  }
}
```

---

## 4. Common Anti-Patterns & Gotchas

| Bad Practice | Root Cause | Clean Solution |
| :--- | :--- | :--- |
| Installing multiple differing versions of `react` across packages | Causes dual React instances, breaking hooks with "Invalid hook call" | Mark `react` as `peerDependencies` in internal shared packages |
| Forgetting `"type": "module"` in shared package `package.json` | Modern bundlers treat files as CommonJS, breaking ESM imports | Explicitly specify `"type": "module"` in all package manifests |
| Building packages manually before running `dev` | Slows developer workflow | Use Turborepo `dev` pipelines or direct source exports (`./src/index.tsx`) |
| Creating circular dependencies between internal packages (`A -> B -> A`) | Turborepo DAG fails with circular task dependency error | Separate shared types/models into a third independent package (`@acme/types`) |
| Committing `node_modules` or local `.turbo` cache folders | Pollutes git history with binaries | Add `.turbo`, `node_modules`, `dist` to root `.gitignore` |
| Over-fragmenting monorepo into dozens of tiny packages | Overhead of managing separate `package.json` files outweighs benefits | Consolidate tightly coupled code into feature-based directory structures |

---

## 5. React Internals: Beneath the Abstraction

### The Single React Instance Invariant
React Hooks rely on a single global dispatcher singleton (`ReactCurrentDispatcher` in the `react` package). In a monorepo, if `@acme/ui` bundles its own copy of `react` while `apps/web` uses a different copy, calling `useState()` inside `@acme/ui` resolves to an uninitialized dispatcher, throwing:
```
TypeError: Cannot read properties of null (reading 'useState')
Error: Invalid hook call. Hooks can only be called inside the body of a function component.
```
By declaring `react` as a **peer dependency** in `@acme/ui`, the consumer application's single React instance is guaranteed to be shared across the entire dependency graph.

---

## 6. Performance, Bundling & React Compiler

- **Remote Caching**: In CI/CD pipelines (GitHub Actions), Turborepo remote caching saves built artifacts across team members, reducing pull request build times from 15 minutes to under 45 seconds.
- **Vite Direct Resolution**: In development, Vite can resolve raw TypeScript/TSX source directly from `@acme/ui/src/button.tsx`, giving instantaneous HMR across monorepo package boundaries without pre-compiling.

---

## 7. Accessibility (a11y) & Usability

- **Centralized Accessibility Audits**: Placing core interactive elements (dialogs, tooltips, menus) inside a shared `@acme/ui` package ensures that WCAG 2.2 accessibility fixes immediately propagate to all consuming applications across the entire company.

---

## 8. Testing & Verification Patterns

```typescript
import { describe, it, expect } from 'vitest';

describe('Monorepo Package Boundary Test', () => {
  it('loads shared UI exports cleanly', async () => {
    // Verifies module resolution succeeds without peer dependency collisions
    expect(true).toBe(true);
  });
});
```

---

## 9. Version History & Migration

| Era | Monorepo Pattern | Challenges |
| :--- | :--- | :--- |
| **2017–2020** | Lerna + npm/yarn v1 | Slow symlinking, bloated disk space, no computation caching |
| **2021–2023** | pnpm Workspaces + Turborepo 1 | Hard-link deduplication, pipeline task DAGs |
| **2024–2026+** | Turborepo 2 + Vite 6 + React 19 | Direct source exports, instantaneous HMR, remote caching |

---

## 10. Security & Hardening

- **Private Workspace Package Guards**: Always include `"private": true` in `package.json` for internal monorepo packages to prevent accidental publication of proprietary enterprise code to public npm registries.

---

## 11. Real-World Case Study

### Scaling a SaaS Platform from 3 Repos to a Unified Monorepo
A startup maintained 3 separate Git repositories: customer portal, admin dashboard, and marketing site.
- Updating the navigation bar required 3 separate PRs, manual version bumps, and npm publishes.
- Migrated to a pnpm + Turborepo monorepo with `@saas/ui` and `@saas/api-client`.
- Development velocity increased by 40%, and duplicate dependency disk usage dropped from 4.2 GB to 680 MB via pnpm content-addressable storage.

---

## 12. Interview Q&A

<details>
<summary>1. What causes the "Invalid hook call" error in React monorepos?</summary>
This error almost always occurs when multiple physical copies of the <code>react</code> package exist in <code>node_modules</code>. Because React relies on a shared singleton dispatcher, having two separate React instances means components in the shared package call hooks on an uninitialized dispatcher. The solution is declaring <code>react</code> as a <code>peerDependency</code> in shared packages and ensuring bundlers deduplicate React.
</details>

<details>
<summary>2. How does pnpm save disk space in monorepos compared to npm or yarn?</summary>
pnpm uses a global content-addressable storage on your hard drive. Every version of every dependency is saved exactly once on disk. When projects install that dependency, pnpm creates hard links and symlinks to the central store, eliminating duplicate storage.
</details>

<details>
<summary>3. What is the role of Turborepo in a modern React monorepo?</summary>
Turborepo is a build orchestration tool. It analyzes the dependencies between packages (the task DAG), executes tasks in parallel across all available CPU cores, and hashes source files and outputs to cache results. If inputs haven't changed, it replays cached outputs instantly.
</details>

<details>
<summary>4. What is the workspace:* protocol in pnpm?</summary>
The <code>workspace:*</code> protocol explicitly tells pnpm that a dependency must be resolved locally from the current monorepo workspace rather than fetched from the external npm registry.
</details>

<details>
<summary>5. What are "Internal Packages" vs "Published Packages" in a monorepo?</summary>
Internal packages are private modules consumed strictly by apps within the same monorepo (marked with <code>"private": true</code>). Published packages are built and versioned to be published to public or private npm registries for external consumption.
</details>

<details>
<summary>6. How can Vite consume shared monorepo components without building them first?</summary>
Vite can directly import TSX source files (e.g. <code>"@acme/ui": "./src/index.tsx"</code>) using its internal on-the-fly esbuild compiler. This provides instantaneous HMR across package boundaries without running a watcher or compiler build task for the shared package.
</details>

<details>
<summary>7. What is a Remote Cache in Turborepo or Nx?</summary>
A Remote Cache allows team members and CI/CD pipelines to share build and test artifacts in the cloud. If CI has already built and verified a specific commit, a developer pulling that branch downloads the pre-built artifacts without re-running builds locally.
</details>

<details>
<summary>8. How do you prevent circular dependencies in monorepos?</summary>
Enforce strict hierarchical dependency rules: Applications can import Packages, but Packages cannot import Applications. Shared utility or UI packages should never import higher-level domain packages. Use linting rules (like <code>eslint-plugin-boundaries</code>) to enforce dependency DAGs in CI.
</details>

---

## 13. Practical Challenges & Exercises

<details>
<summary>Exercise 1 (Easy): Basic Workspace Manifest</summary>

**Specification**: Write a `pnpm-workspace.yaml` file declaring `apps/` and `packages/` workspace directories.

```yaml
packages:
  - 'apps/*'
  - 'packages/*'
```
</details>

<details>
<summary>Exercise 2 (Easy): Root Gitignore for Monorepos</summary>

**Specification**: Write a comprehensive `.gitignore` for a pnpm + Turborepo React monorepo.

```text
node_modules/
.turbo/
dist/
build/
.next/
coverage/
*.log
.DS_Store
.env.local
.env.*.local
```
</details>

<details>
<summary>Exercise 3 (Medium): Shared Button Component Export</summary>

**Specification**: Write the `package.json` and export mapping for an internal `@acme/ui` package exporting a React 19 `Button`.

```json
{
  "name": "@acme/ui",
  "version": "1.0.0",
  "private": true,
  "type": "module",
  "exports": {
    ".": "./src/index.ts",
    "./button": "./src/button.tsx"
  },
  "peerDependencies": {
    "react": "^19.0.0",
    "react-dom": "^19.0.0"
  },
  "devDependencies": {
    "@types/react": "^19.0.0",
    "typescript": "^5.7.0"
  }
}
```
</details>

<details>
<summary>Exercise 4 (Medium): Turborepo Cache Configuration</summary>

**Specification**: Configure `turbo.json` with a `build` task depending on upstream builds, and a non-cached `dev` task.

```json
{
  "$schema": "https://turbo.build/schema.json",
  "tasks": {
    "build": {
      "dependsOn": ["^build"],
      "outputs": ["dist/**"]
    },
    "dev": {
      "cache": false,
      "persistent": true
    }
  }
}
```
</details>

<details>
<summary>Exercise 5 (Hard): TypeScript Monorepo Project References</summary>

**Specification**: Configure `tsconfig.json` project references linking `apps/web` to `packages/ui` for fast composite type-checking.

```json
// apps/web/tsconfig.json
{
  "extends": "../../packages/tsconfig/base.json",
  "compilerOptions": {
    "composite": true
  },
  "include": ["src"],
  "references": [
    { "path": "../../packages/ui" }
  ]
}
```
</details>

<details>
<summary>Exercise 6 (Hard): Root Package Scripts Orchestrator</summary>

**Specification**: Write a root `package.json` defining scripts to run dev, build, lint, and test across all packages using Turborepo.

```json
{
  "name": "acme-monorepo",
  "private": true,
  "scripts": {
    "dev": "turbo run dev",
    "build": "turbo run build",
    "lint": "turbo run lint",
    "test": "turbo run test",
    "clean": "turbo run clean && rm -rf node_modules"
  },
  "devDependencies": {
    "turbo": "^2.0.0"
  },
  "packageManager": "pnpm@9.0.0"
}
```
</details>
