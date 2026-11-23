# Package Managers Compared: npm, pnpm, Yarn, and Bun

> An in-depth technical analysis of JavaScript package managers, exploring dependency resolution graphs, symlinked content-addressable storage, hoisting hazards, and monorepo workspace architectures.

| Metadata | Value |
| :--- | :--- |
| **Level** | Foundational / Tooling Architecture |
| **Prerequisites** | [02_runtimes-browser-and-node.md](./02_runtimes-browser-and-node.md) |
| **Time to Read** | 40 minutes |
| **Status** | Complete |
| **Specification** | npm v10+ / pnpm v9+ / Yarn v4+ / Bun v1.1+ |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Usage](#4-syntax--usage)
5. [Practical Examples](#5-practical-examples)
6. [Best Practices](#6-best-practices)
7. [Common Mistakes & Pitfalls](#7-common-mistakes--pitfalls)
8. [Performance & Security Notes](#8-performance--security-notes)
9. [Comparison / When to Use What](#9-comparison--when-to-use-what)
10. [Interview Questions](#10-interview-questions)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary / Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

### Problem Statement
Modern enterprise web and Node.js applications depend on hundreds of open-source libraries. A package manager is responsible for resolving complex, multi-tiered dependency trees, preventing version collisions, enforcing cryptographic checksum integrity, managing semantic versioning (SemVer), and orchestrating multi-package monorepo builds.

### Historical Evolution & The `node_modules` Dilemma
In early npm versions (v1 and v2), dependencies were nested recursively: package A depending on B created `node_modules/A/node_modules/B`. In deep dependency trees, this triggered catastrophic Windows `MAX_PATH` (260-character) filesystem errors and massive disk duplication.

npm v3 introduced **Dependency Hoisting**, flattening sub-dependencies to the top-level `node_modules`. While this solved path length issues, it introduced severe architectural flaws:
- **Phantom Dependencies:** Application code can import a transitive package that was never explicitly declared in `package.json` simply because it was hoisted to the root. If the direct parent package removes or updates that dependency, the application crashes silently in production.
- **Doppelgangers:** Conflicting versions of the same library duplicated across nested folders.
- **Disk Bloat:** A developer with 20 projects often stores 20 identical copies of React, TypeScript, and Babel on their machine.

Modern solutions (pnpm's hard-linked content-addressable store, Yarn's Plug'n'Play, and Bun's native installer) systematically eliminate these structural failure modes.

---

## 2. Core Concepts

### 1. The Package Managers
- **npm (Node Package Manager):** Ships default with Node.js. Uses flat `node_modules` hoisting and `package-lock.json`. Supports modern workspaces, `npm audit`, and `packageManager` corepack integration.
- **pnpm (Performant npm):** Uses a single global Content-Addressable Store (CAS) on disk. Project `node_modules` contains hard links and symbolic links, guaranteeing zero duplicate disk usage and strict prevention of phantom dependencies.
- **Yarn (v1 Classic vs v4 Berry):**
  - **Yarn Classic (v1):** Deprecated flat-hoisting manager.
  - **Yarn Berry (v4):** Modern TypeScript-first manager supporting **PnP (Plug'n'Play)** which eliminates `node_modules` entirely by generating a single `.pnp.cjs` dependency resolution map.
- **Bun:** Built-in Zig package manager that operates against the standard npm registry, utilizing kernel zero-copy system calls and hard links for extreme installation speeds.

### 2. Dependency Taxonomy in `package.json`
- `dependencies`: Packages required at runtime in production (e.g., `express`, `zod`).
- `devDependencies`: Packages required strictly for build, compilation, linting, and testing (e.g., `typescript`, `eslint`, `vitest`).
- `peerDependencies`: Expresses compatibility expectations; requires the consumer/host application to provide the dependency (e.g., `react` required by a component library).
- `optionalDependencies`: Packages that can fail installation without terminating the overall install build (e.g., OS-specific native binaries like `@rollup/rollup-linux-x64-gnu`).
- `overrides` (npm) / `resolutions` (yarn/pnpm): Force-pins transitive sub-dependency versions across the entire graph to remediate critical CVE security vulnerabilities.

---

## 3. How It Works Internally

### Directory Topologies Compared: Flat Hoisting vs pnpm Symlinks

```
-----------------------------------------------------------------------------
TRADITIONAL FLAT HOISTING (npm / Yarn v1)
-----------------------------------------------------------------------------
project/
└── node_modules/
    ├── bar/                  <-- Transitive sub-dep of foo HOISTED to root!
    │   └── package.json      <-- PHANTOM DEPENDENCY: Your app can require('bar')
    └── foo/                      without declaring it in package.json!
        └── package.json

-----------------------------------------------------------------------------
CONTENT-ADDRESSABLE SYMLINK TOPOLOGY (pnpm)
-----------------------------------------------------------------------------
Global Store: ~/.pnpm-store/v3/files/... (Indexed by SHA-512 content hash)

project/
└── node_modules/
    ├── .pnpm/                <-- Virtual store holding all real hard links
    │   ├── bar@1.0.0/node_modules/bar/ -> hardlink to ~/.pnpm-store
    │   └── foo@1.0.0/node_modules/
    │       ├── foo/          -> hardlink to ~/.pnpm-store
    │       └── node_modules/
    │           └── bar       -> symlink to ../../bar@1.0.0/node_modules/bar
    └── foo                   -> symlink to .pnpm/foo@1.0.0/node_modules/foo

*Result: App CANNOT require('bar') because only 'foo' is symlinked in root node_modules!
```

---

## 4. Syntax & Usage

### Core CLI Command Mapping

| Task | npm | pnpm | Yarn (Berry) | Bun |
| :--- | :--- | :--- | :--- | :--- |
| **Install all deps** | `npm install` | `pnpm install` | `yarn install` | `bun install` |
| **Add production dep** | `npm i <pkg>` | `pnpm add <pkg>` | `yarn add <pkg>` | `bun add <pkg>` |
| **Add dev dep** | `npm i -D <pkg>` | `pnpm add -D <pkg>` | `yarn add -D <pkg>` | `bun add -d <pkg>` |
| **Run script** | `npm run build` | `pnpm build` | `yarn build` | `bun run build` |
| **Execute CLI package** | `npx <pkg>` | `pnpm dlx <pkg>` | `yarn dlx <pkg>` | `bunx <pkg>` |
| **Security audit** | `npm audit` | `pnpm audit` | `yarn npm audit` | `bun pm audit` |
| **Clean cache** | `npm cache clean --force` | `pnpm store prune` | `yarn cache clean` | `bun pm cache rm` |

---

## 5. Practical Examples

### Example 1: Parsing and Validating SemVer Ranges
- **Goal:** Understand how `^` (caret), `~` (tilde), and exact versions resolve dependencies according to SemVer.
- **Code:**
```javascript
function evaluateSemverMatch(declaredRange, availableVersion) {
  const [decMajor, decMinor, decPatch] = declaredRange.replace(/[\^~]/, '').split('.').map(Number);
  const [availMajor, availMinor, availPatch] = availableVersion.split('.').map(Number);

  if (declaredRange.startsWith('^')) {
    // Caret (^): Allows changes that do not modify the left-most non-zero digit
    if (decMajor !== 0) return availMajor === decMajor && availMinor >= decMinor;
    return availMajor === 0 && availMinor === decMinor && availPatch >= decPatch;
  }
  if (declaredRange.startsWith('~')) {
    // Tilde (~): Allows patch updates if minor is specified
    return availMajor === decMajor && availMinor === decMinor && availPatch >= decPatch;
  }
  // Exact version match
  return declaredRange === availableVersion;
}

console.log('^1.2.3 accepts 1.9.0:', evaluateSemverMatch('^1.2.3', '1.9.0'));
console.log('^1.2.3 accepts 2.0.0:', evaluateSemverMatch('^1.2.3', '2.0.0'));
console.log('~1.2.3 accepts 1.2.8:', evaluateSemverMatch('~1.2.3', '1.2.8'));
console.log('~1.2.3 accepts 1.3.0:', evaluateSemverMatch('~1.2.3', '1.3.0'));
```
- **Output:**
```
^1.2.3 accepts 1.9.0: true
^1.2.3 accepts 2.0.0: false
~1.2.3 accepts 1.2.8: true
~1.2.3 accepts 1.3.0: false
```
- **Explanation:** Demonstrates the programmatic contract of `^` (caret allows minor/patch bumps) versus `~` (tilde allows patch only).

### Example 2: Inspecting Dependency Resolution Trees via Node API
- **Goal:** Programmatically inspect resolved package locations to detect whether a package is hoisted or isolated.
- **Code:**
```javascript
import { createRequire } from 'node:module';
const require = createRequire(import.meta.url);

function inspectPackageLocation(pkgName) {
  try {
    const resolvedPath = require.resolve(pkgName);
    return {
      status: 'RESOLVED',
      path: resolvedPath,
      isSymlink: resolvedPath.includes('.pnpm') || resolvedPath.includes('.store')
    };
  } catch (err) {
    return { status: 'NOT_FOUND', error: err.message };
  }
}

console.log('Inspecting built-in module:', inspectPackageLocation('node:path').status);
console.log('Inspecting undeclared package:', inspectPackageLocation('non-existent-pkg-xyz').status);
```
- **Output:**
```
Inspecting built-in module: RESOLVED
Inspecting undeclared package: NOT_FOUND
```
- **Explanation:** `require.resolve()` traverses parent `node_modules` directories in compliance with the Node.js module resolution algorithm.

### Example 3: Edge Case of Security Overrides in `package.json`
- **Goal:** Demonstrate how `overrides` enforce a secure transitive dependency version across all sub-dependencies.
- **Code:**
```javascript
// Simulated package.json dependency resolution tree
const manifest = {
  name: "enterprise-service",
  dependencies: {
    "vulnerable-parent-lib": "1.0.0" // Transitively depends on lodash@4.17.15 (CVE-2020-8203)
  },
  overrides: {
    "lodash": "^4.17.21" // Force-pins lodash everywhere in the resolution tree
  }
};

function resolveTransitiveVersion(depName, graph, overrides) {
  if (overrides[depName]) {
    return { version: overrides[depName], source: 'FORCED_OVERRIDE' };
  }
  return { version: graph[depName] || 'unknown', source: 'DIRECT_GRAPH' };
}

console.log('Lodash resolved to:', resolveTransitiveVersion('lodash', {}, manifest.overrides));
```
- **Output:**
```
Lodash resolved to: { version: '^4.17.21', source: 'FORCED_OVERRIDE' }
```
- **Explanation:** Modern package managers evaluate the `overrides` field (npm/pnpm) or `resolutions` (yarn) to override nested transitive dependency specifications during graph construction.

### Example 4: Failure Mode of Phantom Dependencies
- **Goal:** Show how code accessing an undeclared transitive package creates fragile runtime assumptions.
- **Code:**
```javascript
// Suppose package A depends on 'chalk', but our app only listed package A.
// In a flat npm structure, 'chalk' resides at root node_modules.
function simulatePhantomImport(declaredDependencies, attemptedImport) {
  if (!declaredDependencies.includes(attemptedImport)) {
    return {
      success: false,
      warning: `PHANTOM DEPENDENCY: '${attemptedImport}' is imported but not in package.json! Will fail in strict managers (pnpm/Yarn PnP).`
    };
  }
  return { success: true };
}

const directDeps = ['express', 'cors'];
console.log(simulatePhantomImport(directDeps, 'express'));
console.log(simulatePhantomImport(directDeps, 'body-parser'));
```
- **Output:**
```
{ success: true }
{
  success: false,
  warning: "PHANTOM DEPENDENCY: 'body-parser' is imported but not in package.json! Will fail in strict managers (pnpm/Yarn PnP)."
}
```
- **Explanation:** A flat `node_modules` allows `body-parser` to be imported even when undeclared. Strict managers like `pnpm` prevent this by isolating dependencies in nested symlink trees.

### Example 5: Idiomatic Monorepo Workspace Dependency Linking
- **Goal:** Model workspace protocol (`workspace:*`) resolution in multi-package repositories.
- **Code:**
```javascript
const workspacePackages = new Map([
  ['@corp/core', { version: '2.1.0' }],
  ['@corp/ui', { version: '1.4.0' }]
]);

function resolveDependency(pkgName, requestedRange) {
  if (requestedRange.startsWith('workspace:')) {
    if (!workspacePackages.has(pkgName)) {
      throw new Error(`Workspace package not found: ${pkgName}`);
    }
    return {
      type: 'LOCAL_WORKSPACE_SYMLINK',
      target: pkgName,
      activeVersion: workspacePackages.get(pkgName).version
    };
  }
  return { type: 'REMOTE_REGISTRY', target: pkgName, range: requestedRange };
}

console.log(resolveDependency('@corp/core', 'workspace:*'));
console.log(resolveDependency('lodash', '^4.17.21'));
```
- **Output:**
```
{
  type: 'LOCAL_WORKSPACE_SYMLINK',
  target: '@corp/core',
  activeVersion: '2.1.0'
}
{ type: 'REMOTE_REGISTRY', target: 'lodash', range: '^4.17.21' }
```
- **Explanation:** Monorepo package managers recognize the `workspace:` protocol to wire local packages together via symlinks without round-tripping through an external registry.

---

## 6. Best Practices

1. **Always Commit Lockfiles to Version Control:** Commit `package-lock.json`, `pnpm-lock.yaml`, or `yarn.lock` to ensure deterministic, bit-for-bit reproducible builds across CI/CD and developer workstations.
2. **Use `npm ci` / `pnpm install --frozen-lockfile` in CI Pipelines:** Standard `npm install` can mutate lockfiles if `package.json` changed. CI commands enforce that the lockfile matches `package.json` exactly.
3. **Pin Package Manager Versions with `corepack`:** Specify `"packageManager": "pnpm@9.12.0"` in `package.json` and enable Corepack (`corepack enable`) to prevent developers on different package manager versions from thrashing lockfiles.
4. **Prefer `pnpm` for Monorepos & Large Teams:** Saves gigabytes of disk space via content-addressable storage and prevents phantom dependency bugs.
5. **Regularly Audit Dependencies with `npm audit` or Socket.dev:** Automated vulnerability scanning catches compromised upstream packages before deployment.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Fix |
| :--- | :--- | :--- |
| **Running `npm install` in CI/CD** | `npm install` can update transitive dependencies and alter `package-lock.json`. | Always use `npm ci` or `pnpm install --frozen-lockfile`. |
| **Relying on Phantom Dependencies** | If a parent library updates and drops its dependency, your undeclared imports immediately throw `MODULE_NOT_FOUND`. | Explicitly list all directly imported libraries in `package.json`. |
| **Mixing Package Managers in One Repo** | Committing both `yarn.lock` and `package-lock.json` leads to conflicting dependency resolutions and build nondeterminism. | Standardize on one manager and enforce via `"packageManager"` in `package.json`. |
| **Manual Lockfile Editing** | Manually resolving git merge conflicts in lockfiles easily corrupts checksums and graph trees. | Regenerate lockfiles using `npm install` or `pnpm install`. |
| **Using `*` or `latest` for version ranges** | Unpinned ranges allow unexpected breaking changes to break production builds on minor deploys. | Use pinned versions or caret ranges (`^`) with locked lockfiles. |
| **Installing devDependencies in production Docker containers** | Bundling test runners and linters inflates container images by hundreds of megabytes. | Run `npm prune --production` or use multi-stage Docker builds. |

---

## 8. Performance & Security Notes

### Performance Notes
- **Hard Links & Content-Addressable Storage:** `pnpm` writes package files to disk once in a central store. Projects link to the store via hard links, reducing install times from 60 seconds to 2 seconds.
- **Cache Warming in CI:** Persist the package manager cache directory (`~/.npm`, `~/.local/share/pnpm/store`) across CI runs using GitHub Actions `actions/cache`.

### Security Notes
- **Typosquatting & Malicious Packages:** Threat actors publish packages with names mimicking popular libraries (e.g., `cross-env` vs `crossenv`).
- **Post-Install Script Hazards:** npm packages can execute arbitrary shell commands upon installation via `preinstall` / `postinstall` hooks. Use `--ignore-scripts` for untrusted dependencies.
- **npm Provenance:** Modern npm supports cryptographic provenance attestations linking packages directly to their source GitHub Actions build.

---

## 9. Comparison / When to Use What

| Feature | npm (v10+) | pnpm (v9+) | Yarn Berry (v4+) | Bun (v1.1+) |
| :--- | :--- | :--- | :--- | :--- |
| **Node_modules Layout** | Flat (Hoisted) | Isolated Symlink Tree | PnP (or node-modules) | Flat / Hardlinked |
| **Disk Efficiency** | Low (Duplicated) | **Maximum** (CAS Store) | High (Zip/PnP) | High (Cache Links) |
| **Phantom Dep Defense**| No | **Yes (Strict)** | **Yes (Strict PnP)** | No |
| **Speed (Cold Cache)** | Moderate | Fast | Fast | **Fastest** |
| **Monorepo Workspaces** | Supported | **Industry Standard** | Excellent | Supported |
| **Out-of-the-Box** | Ships with Node | Requires Corepack/npm | Requires Corepack | Standalone Binary |

---

## 10. Interview Questions

### Q1: What is a "phantom dependency" and how does `pnpm` solve it?
<details>
<summary>Answer</summary>
A phantom dependency occurs when a project's source code imports a package that is not declared in its `package.json`, but the import succeeds at runtime because a traditional package manager (npm or Yarn v1) flattened and hoisted that package from a parent dependency into the root `node_modules`. If the parent dependency ever removes or upgrades that sub-dependency, the application breaks immediately. `pnpm` solves this by using a nested symlink structure where root `node_modules` contains symlinks *only* for packages explicitly listed in `package.json`.
</details>

### Q2: What is the exact difference between `npm install` and `npm ci`?
<details>
<summary>Answer</summary>
- **`npm install`:** Reads `package.json` and updates dependencies according to SemVer ranges. If the dependencies in `package.json` don't match `package-lock.json`, it resolves new versions and overwrites `package-lock.json`.
- **`npm ci` (Clean Install):** Designed strictly for automated environments. It deletes the existing `node_modules` folder and installs directly from `package-lock.json`. It never mutates the lockfile; if `package.json` and `package-lock.json` are out of sync, `npm ci` exits immediately with a fatal error.
</details>

### Q3: How does SemVer handle `^1.2.3` vs `~1.2.3` vs `0.2.3`?
<details>
<summary>Answer</summary>
- `^1.2.3` (Caret): Allows updates that do not modify the leftmost non-zero digit. For `1.2.3`, it allows $<2.0.0$ and $\ge 1.2.3$ (minor and patch updates).
- `~1.2.3` (Tilde): Allows patch-level updates only. It allows $<1.3.0$ and $\ge 1.2.3$.
- `^0.2.3`: Because the major version is `0`, the software is considered unstable. The leftmost non-zero digit is `2` (minor), so `^0.2.3` allows $<0.3.0$ and $\ge 0.2.3$ (patch updates only).
</details>

### Q4: What is the purpose of `peerDependencies` in `package.json`?
<details>
<summary>Answer</summary>
`peerDependencies` are used by plugin and component libraries (e.g. `react-router` or an ESLint plugin) to declare that they require a specific dependency (e.g. `react >= 18`), but expect the consuming application to provide and manage that single instance. This guarantees that multiple plugins in the application share a single singleton instance of the peer dependency rather than each pulling in duplicate conflicting instances.
</details>

### Q5: What is Content-Addressable Storage (CAS) and how does pnpm leverage it?
<details>
<summary>Answer</summary>
Content-Addressable Storage is a data storage architecture where files are indexed and retrieved using a cryptographic hash of their content (e.g. SHA-512) rather than their file path. `pnpm` maintains a single global CAS store on disk. When a package is downloaded, each file is stored once in the CAS. Projects link to files via filesystem hard links. If 50 projects require `lodash@4.17.21`, the 100+ files of lodash are stored on disk exactly once.
</details>

### Q6: What is Corepack in modern Node.js?
<details>
<summary>Answer</summary>
Corepack is an official Node.js utility bridging the Node runtime and alternative package managers (pnpm and Yarn). When enabled (`corepack enable`), developers do not need to globally install pnpm or Yarn. Corepack intercepts calls to `pnpm` and `yarn`, reads the `"packageManager": "pnpm@9.12.0"` field in `package.json`, downloads the exact specified binary on demand, and executes it transparently.
</details>

### Q7: What are the security risks associated with `npm install` postinstall scripts?
<details>
<summary>Answer</summary>
Packages can specify lifecycle scripts (`preinstall`, `install`, `postinstall`) in `package.json` that execute arbitrary shell commands with the user's OS permissions upon download. If an attacker compromises an open-source package, they can inject malicious postinstall scripts that exfiltrate environment variables (AWS credentials, SSH keys, npm tokens) or install backdoors. This risk can be mitigated using `--ignore-scripts`.
</details>

### Q8: What is Yarn Plug'n'Play (PnP) and how does it eliminate `node_modules`?
<details>
<summary>Answer</summary>
Yarn PnP completely eliminates the generation of `node_modules`. Instead, Yarn caches all packages as compressed `.zip` files in a global directory and creates a single `.pnp.cjs` file in the project. `.pnp.cjs` intercepts Node's native module resolution system (`Module._load` / `require.resolve`) and directly maps import specifiers to the precise file offsets within the cached zip archives.
</details>

---

## 11. Practice Exercises

### Exercise 1: SemVer Range Satisfaction Checker
**Task:** Write a function `isCompatible(requestedRange, candidateVersion)` that correctly evaluates compatibility for exact, caret (`^`), and tilde (`~`) ranges across SemVer strings.
<details>
<summary>Verified Solution</summary>

```javascript
function isCompatible(range, candidate) {
  const parse = v => v.replace(/[\^~]/, '').split('.').map(Number);
  const [rMaj, rMin, rPat] = parse(range);
  const [cMaj, cMin, cPat] = parse(candidate);

  if (range.startsWith('^')) {
    if (rMaj > 0) return cMaj === rMaj && (cMin > rMin || (cMin === rMin && cPat >= rPat));
    if (rMin > 0) return cMaj === 0 && cMin === rMin && cPat >= rPat;
    return cMaj === 0 && cMin === 0 && cPat === rPat;
  }
  if (range.startsWith('~')) {
    return cMaj === rMaj && cMin === rMin && cPat >= rPat;
  }
  return range === candidate;
}

console.log('^1.4.0 with 1.5.2:', isCompatible('^1.4.0', '1.5.2')); // true
console.log('~1.4.0 with 1.5.0:', isCompatible('~1.4.0', '1.5.0')); // false
console.log('^0.1.2 with 0.1.5:', isCompatible('^0.1.2', '0.1.5')); // true
```
</details>

### Exercise 2: Detecting Circular Dependency Cycles in Package Graphs
**Task:** Write a function `detectCircularDependencies(graph)` that takes an adjacency map of dependencies and returns an array of detected circular dependency cycles.
<details>
<summary>Verified Solution</summary>

```javascript
function detectCircularDependencies(graph) {
  const visited = new Set();
  const recStack = new Set();
  const cycles = [];

  function dfs(node, path) {
    visited.add(node);
    recStack.add(node);
    path.push(node);

    const neighbors = graph[node] || [];
    for (const neighbor of neighbors) {
      if (!visited.has(neighbor)) {
        dfs(neighbor, path);
      } else if (recStack.has(neighbor)) {
        const cycleStartIndex = path.indexOf(neighbor);
        cycles.push(path.slice(cycleStartIndex).concat(neighbor));
      }
    }

    path.pop();
    recStack.delete(node);
  }

  for (const node of Object.keys(graph)) {
    if (!visited.has(node)) {
      dfs(node, []);
    }
  }

  return cycles;
}

const depGraph = {
  'A': ['B'],
  'B': ['C'],
  'C': ['A', 'D'],
  'D': []
};
console.log('Detected cycles:', detectCircularDependencies(depGraph));
// [ [ 'A', 'B', 'C', 'A' ] ]
```
</details>

### Exercise 3: Package.json Security Override Validator
**Task:** Write a utility `validateOverrides(packageJson)` that inspects whether any known vulnerable packages (e.g., `lodash < 4.17.21`) have appropriate overrides configured.
<details>
<summary>Verified Solution</summary>

```javascript
const VULNERABLE_SPECS = {
  'lodash': '^4.17.21',
  'axios': '^1.7.4'
};

function validateOverrides(pkg) {
  const overrides = pkg.overrides || pkg.resolutions || {};
  const issues = [];

  for (const [lib, secureVersion] of Object.entries(VULNERABLE_SPECS)) {
    if (!overrides[lib]) {
      issues.push(`Missing security override for '${lib}'. Recommended: ${secureVersion}`);
    }
  }

  return { secure: issues.length === 0, issues };
}

const samplePkg = { name: "demo", dependencies: { "lodash": "4.17.15" } };
console.log(validateOverrides(samplePkg));
```
</details>

### Exercise 4: Monorepo Topology Sorter (Topological Build Order)
**Task:** Write a function `getMonorepoBuildOrder(packages)` that returns the correct build execution sequence so upstream libraries build before downstream consumers.
<details>
<summary>Verified Solution</summary>

```javascript
function getMonorepoBuildOrder(packages) {
  const inDegree = {};
  const adj = {};

  for (const pkg of Object.keys(packages)) {
    inDegree[pkg] = 0;
    adj[pkg] = [];
  }

  for (const [pkg, deps] of Object.entries(packages)) {
    for (const dep of deps) {
      if (adj[dep]) {
        adj[dep].push(pkg);
        inDegree[pkg]++;
      }
    }
  }

  const queue = Object.keys(inDegree).filter(k => inDegree[k] === 0);
  const order = [];

  while (queue.length > 0) {
    const current = queue.shift();
    order.push(current);
    for (const dependent of adj[current]) {
      inDegree[dependent]--;
      if (inDegree[dependent] === 0) {
        queue.push(dependent);
      }
    }
  }

  if (order.length !== Object.keys(packages).length) {
    throw new Error('Cyclic dependency in monorepo packages!');
  }
  return order;
}

const monorepo = {
  'ui-components': ['core-utils'],
  'core-utils': [],
  'web-app': ['ui-components', 'core-utils']
};
console.log('Build order:', getMonorepoBuildOrder(monorepo));
// [ 'core-utils', 'ui-components', 'web-app' ]
```
</details>

### Exercise 5: Lockfile Integrity Hash Verifier
**Task:** Write a function `verifyPackageChecksum(fileBuffer, expectedSha512Integrity)` that verifies whether a downloaded `.tgz` tarball matches its integrity hash from `package-lock.json`.
<details>
<summary>Verified Solution</summary>

```javascript
import crypto from 'node:crypto';

function verifyPackageChecksum(content, expectedIntegrity) {
  // Integrity format: "sha512-<base64Hash>"
  const [algo, expectedBase64] = expectedIntegrity.split('-');
  const computedBase64 = crypto.createHash(algo).update(content).digest('base64');
  return computedBase64 === expectedBase64;
}

const dummyPayload = Buffer.from('package tarball content payload');
const computedIntegrity = 'sha512-' + crypto.createHash('sha512').update(dummyPayload).digest('base64');

console.log('Integrity matches:', verifyPackageChecksum(dummyPayload, computedIntegrity));
```
</details>

### Exercise 6: Dependency Hoisting Conflict Simulator
**Task:** Simulate what happens when Package A requires `lib@1.0.0` and Package B requires `lib@2.0.0` in a flat-hoisted structure.
<details>
<summary>Verified Solution</summary>

```javascript
function simulateHoisting(packageA, packageB) {
  const rootNodeModules = {};
  const nestedNodeModules = {};

  // First package encountered gets hoisted to root
  rootNodeModules['lib'] = packageA.deps['lib'];

  // Second package conflicts with root -> must remain nested under Package B
  if (packageB.deps['lib'] !== rootNodeModules['lib']) {
    nestedNodeModules['packageB/lib'] = packageB.deps['lib'];
  }

  return { rootNodeModules, nestedNodeModules };
}

const result = simulateHoisting(
  { name: 'pkgA', deps: { 'lib': '1.0.0' } },
  { name: 'pkgB', deps: { 'lib': '2.0.0' } }
);

console.log('Hoisting Result:', result);
```
</details>

---

## 12. Summary / Cheat Sheet

- **Deterministic Builds:** Always commit lockfiles (`package-lock.json`, `pnpm-lock.yaml`, `yarn.lock`).
- **CI Pipelines:** Always run `npm ci` or `pnpm install --frozen-lockfile` in CI/CD. Never `npm install`.
- **Phantom Dependencies:** Use `pnpm` to guarantee that code can only import packages explicitly declared in `package.json`.
- **Version Pinning:** Use Corepack (`"packageManager": "pnpm@9.12.0"`) to guarantee consistent package manager versions across engineering teams.
- **Vulnerability Overrides:** Use the `overrides` field in `package.json` to force-patch transitive security vulnerabilities.

---

## 13. Further Reading
- [npm CLI Documentation](https://docs.npmjs.com/)
- [pnpm Documentation & Architecture Guide](https://pnpm.io/motivation)
- [Yarn Plug'n'Play Specification](https://yarnpkg.com/features/pnp)
- [Node.js Corepack Documentation](https://nodejs.org/api/corepack.html)
