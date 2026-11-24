# Package Managers & Monorepos: npm, pnpm, Yarn, Bun

> Comparative analysis of modern package managers, hard-link content-addressable storage, phantom dependencies, and monorepo workspace setups.

| Metadata | Details |
| :--- | :--- |
| **Node.js Baseline** | Node.js 22 LTS / 24+ |
| **Package Managers** | `pnpm` v9+, `npm` v10+, `yarn` Berry, `bun` |
| **Monorepo Tools** | pnpm workspaces, Turborepo |

---

## 1. Package Manager Architectural Comparison

| Dimension | `npm` | `pnpm` (Industry Standard) | `Yarn` (Berry) | `Bun` |
| :--- | :--- | :--- | :--- | :--- |
| **Storage Model** | Flat `node_modules` | Content-addressable global hard-link store | PnP (Plug'n'Play) or node_modules | Flat binary store |
| **Disk Space Efficiency**| High duplication across projects | Minimal (files stored once globally) | Minimal | Moderate |
| **Phantom Dependencies**| Susceptible (flat hoisting leaks deps)| **Immune** (strict symlink isolation) | Immune in PnP mode | Susceptible |
| **Installation Speed** | Moderate | Fast | Fast | Extremely Fast |

---

## 2. What Is a "Phantom Dependency" and How Does `pnpm` Solve It?

- **The Problem in npm/yarn**: When package `A` depends on package `B`, npm flattens `node_modules` and hoists `B` to the top-level `node_modules/B`. Your application code can now execute `import 'B'` without declaring `B` in `package.json`.
- If `A` updates its dependencies and removes `B`, your code crashes in production with `MODULE_NOT_FOUND`.
- **How `pnpm` Solves It**: `pnpm` creates a nested symlink tree inside `node_modules/.pnpm/`. Top-level `node_modules` only contains symlinks to packages **explicitly declared in your `package.json`**. Undeclared transitive dependencies cannot be imported.

---

## 3. Monorepo Workspaces Setup (`pnpm-workspace.yaml`)

```yaml
# pnpm-workspace.yaml
packages:
  - 'apps/*'
  - 'packages/*'
```

Referencing internal packages in `apps/api/package.json`:
```json
{
  "name": "@company/api",
  "dependencies": {
    "@company/database": "workspace:*",
    "@company/shared-types": "workspace:*"
  }
}
```
`pnpm install` automatically links internal packages directly to local source without publishing to npm.
