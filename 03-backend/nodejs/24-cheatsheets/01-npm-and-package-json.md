# Cheatsheet 01: npm, package.json & Workspaces Reference

> Production reference card for `package.json` manifest configurations, subpath exports/imports, semver ranges, and monorepo workspace workflows.

---

### 1. Modern `package.json` Structure
```json
{
  "name": "@org/backend-service",
  "version": "1.2.0",
  "type": "module",
  "main": "./dist/index.js",
  "types": "./dist/index.d.ts",
  "engines": {
    "node": ">=22.0.0",
    "npm": ">=10.0.0"
  },
  "exports": {
    ".": {
      "import": "./dist/index.js",
      "types": "./dist/index.d.ts"
    },
    "./utils": "./dist/utils/index.js"
  },
  "imports": {
    "#config": "./src/config/index.js",
    "#db/*": "./src/infrastructure/database/*.js"
  },
  "scripts": {
    "dev": "node --watch --env-file=.env src/index.js",
    "start": "node --env-file=.env.production src/index.js",
    "build": "tsc",
    "test": "node --test"
  }
}
```

---

### 2. Subpath Imports (`#prefix`)
Enables absolute project imports without relative `../../` path hell:
```javascript
// In src/services/user.service.js:
import { env } from '#config';        // Resolves directly to ./src/config/index.js
import { pool } from '#db/connection'; // Resolves to ./src/infrastructure/database/connection.js
```

---

### 3. Semantic Versioning Ranges

| Symbol | Example | Matches | Rule |
| :---: | :--- | :--- | :--- |
| **`^` (Caret)** | `^1.2.3` | `1.2.3` up to `<2.0.0` | Minor and patch updates allowed. Breaks on major. |
| **`~` (Tilde)** | `~1.2.3` | `1.2.3` up to `<1.3.0` | Only patch updates allowed. |
| **Exact** | `1.2.3` | Exactly `1.2.3` | Locks exact version. |
| **`*`** | `*` | Any version | **Never use in production**. |

---

### 4. Essential npm CLI Commands

```bash
# Clean install for CI/CD (strictly respects package-lock.json, deletes existing node_modules)
npm ci

# Production prune (removes devDependencies inside Docker containers)
npm prune --omit=dev

# Dependency auditing and automated CVE fixing
npm audit
npm audit fix --omit=dev

# List outdated packages compared to npm registry
npm outdated

# Link local package for development across folders
npm link
```

---

### 5. Monorepo Workspaces (`npm workspaces`)
```json
// Root package.json:
{
  "name": "enterprise-monorepo",
  "workspaces": [
    "packages/*",
    "apps/*"
  ]
}
```

```bash
# Run command across all workspaces:
npm run build --workspaces

# Run command inside specific workspace:
npm run test --workspace=@org/auth-service

# Add dependency to specific workspace:
npm install zod --workspace=@org/auth-service
```
