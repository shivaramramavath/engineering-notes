# Dependency Security and Supply Chain Defense in TypeScript

> Hardening the TypeScript software supply chain: `@types/*` risks, type declaration poisoning, lockfile security, automated vulnerability auditing, and dependency isolation.

| Metadata | Details |
| :--- | :--- |
| **Category** | Security |
| **Difficulty** | Advanced |
| **Prerequisites** | [21-production-tooling/03-publishing-packages.md](../21-production-tooling/03-publishing-packages.md), [13-compiler-options/00-tsconfig-json.md](../13-compiler-options/00-tsconfig-json.md) |
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

Modern TypeScript applications rely on hundreds of third-party npm packages. However, the software supply chain presents severe security attack vectors:
- **Typo-squatting**: Attackers publish packages with names resembling popular libraries (`lodsh` instead of `lodash`).
- **Malicious Install Scripts**: Packages execute arbitrary shell commands during `npm install` via lifecycle scripts (`preinstall`, `postinstall`).
- **Ambient Type Declaration Poisoning**: A compromised `@types/*` package or internal `.d.ts` file can declare global augmentations (`declare global { ... }`) that silently weaken the compiler's strictness across the entire repository.
- **Dependency Drift**: Using open version ranges (`^1.2.0`) allows compromised minor or patch releases to deploy directly to production.

Securing the supply chain requires defensive package managers (pnpm), automated auditing tools (Socket.dev, npm audit), lockfile pinning, and ambient type sandboxing.

---

## 2. Core Concepts

### Ambient Declaration Poisoning
In TypeScript, any `.d.ts` file in your project or `node_modules/@types` can augment the global namespace. A rogue dependency can insert:
```typescript
declare global {
  interface Object {
    unsafeBackdoor(): void;
  }
}
```
Or override standard library types, masking security checks across your application.

```
+-------------------------------------------------------------------------+
|                  Supply Chain Attack Vectors & Defenses                 |
+-------------------------------------------------------------------------+
|                                                                         |
|  [ Attack Vectors ]                                                     |
|  1. Lifecycle scripts (postinstall scripts running curl / bash)         |
|  2. Ambient type poisoning (shadowing global interfaces)                 |
|  3. Transitive dependency hijacking (compromised maintainer token)      |
|                                                                         |
|  [ Multi-Layer Defense ]                                                |
|  +-- pnpm with ignore-scripts (blocks arbitrary install scripts)        |
|  +-- Socket.dev / Snyk (monitors network calls and filesystem access)   |
|  +-- typeRoots & types in tsconfig.json (sandboxes global type loading) |
|  +-- --frozen-lockfile in CI (guarantees immutable dependencies)        |
|                                                                         |
+-------------------------------------------------------------------------+
```

### Dependency Isolation via pnpm
Unlike npm and yarn, pnpm creates a content-addressable storage pool and uses hard links and symlinks to prevent **phantom dependencies**. Code cannot import a package unless it is explicitly declared in `package.json`.

---

## 3. How It Works Internally

1. **Type Discovery**: By default, `tsc` automatically includes all packages in `node_modules/@types` unless constrained by `"types": []` in `tsconfig.json`.
2. **Global Merge**: The TypeScript Binder merges all matching ambient interfaces into a single unified symbol.
3. **Lockfile Integrity**: Lockfiles (`pnpm-lock.yaml`, `package-lock.json`) store cryptographic SHA-512 integrity hashes for every downloaded tarball, preventing in-flight tampering.

---

## 4. Syntax & API Reference

### Sandboxing Global Types in `tsconfig.json`
```json
{
  "compilerOptions": {
    "types": ["node", "vitest"],
    "typeRoots": ["./types", "./node_modules/@types"]
  }
}
```

### Disabling Dangerous Install Scripts in `.npmrc`
```ini
# Prevent dependencies from executing arbitrary bash code on install
ignore-scripts=true
save-exact=true
```

---

## 5. Practical Examples

### Example 1: Isolating Allowed Ambient Types
```json
// tsconfig.json
{
  "compilerOptions": {
    // Only allow explicit ambient types; blocks rogue @types packages
    "types": [
      "node"
    ]
  }
}
```

### Example 2: CI Audit Gate with Socket.dev and npm audit
```yaml
# .github/workflows/security.yml
name: Supply Chain Audit
on: [push, pull_request]

jobs:
  audit:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: pnpm/action-setup@v3
        with: { version: 9 }
      - run: pnpm install --frozen-lockfile --ignore-scripts
      - run: pnpm audit --audit-level=high
      - name: Scan with Socket CLI
        run: npx @socketsecurity/cli cdxgen
```

### Example 3: Edge Case - Pinning Exact Dependency Versions
```json
// package.json
// Notice NO carets (^) or tildes (~); pinned exact versions
{
  "dependencies": {
    "express": "4.19.2",
    "pino": "9.1.0",
    "zod": "3.23.8"
  }
}
```

### Example 4: Compiler Error & Phantom Dependency Block
```typescript
// If a project uses pnpm without declaring 'lodash' in its package.json:
// Even if 'lodash' exists transitively in node_modules:
// @ts-expect-error - TS2307: Cannot find module 'lodash' or its corresponding type declarations.
import _ from "lodash";
```

### Example 5: Automated Lockfile Integrity Verifier
```typescript
import fs from "node:fs";

export function verifyLockfileIntegrity(lockfilePath = "pnpm-lock.yaml"): boolean {
  if (!fs.existsSync(lockfilePath)) {
    console.error("Lockfile missing!");
    return false;
  }
  const content = fs.readFileSync(lockfilePath, "utf-8");
  // Check that all tarballs contain SHA-512 integrity hashes
  return content.includes("integrity:");
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **`ignore-scripts` Breaking Legitimate Packages**:
   Some native C++ bindings (like `sharp` or `bcrypt`) rely on install scripts. Whitelist them explicitly using pnpm's `pnpm.onlyBuiltDependencies` array in `package.json`.
2. **Ambient Pollution via `skipLibCheck`**:
   `skipLibCheck: true` skips type checking *inside* `.d.ts` files, but it does NOT disable global declarations from being loaded into your project scope.
3. **Overriding `types: []`**:
   Configuring `"types": []` in `tsconfig.json` disables all automatic `@types` loading. You must explicitly list all necessary types (e.g. `["node"]`).

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Bad Practice | Good Practice | Problem / Diagnostic |
| :--- | :--- | :--- | :--- |
| **Permissive Version Ranges** | Using `^` or `~` in package.json | Pin exact versions (`1.2.3`) | Unintended breaking/malicious updates |
| **Unrestricted Install Scripts** | Running `pnpm install` without flags | Set `ignore-scripts=true` in `.npmrc` | RCE via malicious postinstall script |
| **Unfrozen CI Lockfile** | `pnpm install` in CI | `pnpm install --frozen-lockfile` | Dependency drift in production |
| **Loading All Ambient Types** | Omitting `types` array in tsconfig | Explicitly whitelist `"types": ["node"]` | Ambient type poisoning |
| **Phantom Dependencies** | Using hoisted flat node_modules | Use pnpm isolated virtual store | Runtime missing module errors |
| **Ignoring Audit Failures** | `--audit-level=none` in CI | Fail CI on high/critical findings | Known CVEs deployed to production |

---

## 8. Performance & Type-Checker Complexity

- Constraining the `types` array in `tsconfig.json` reduces the number of declaration files parsed by `ts.createProgram`, speeding up compiler initialization by **15%–30%**.
- pnpm's content-addressable storage prevents duplicate disk allocations across multiple monorepo packages.

---

## 9. Compiler Internals

During program creation:
1. `ts.resolveTypeReferenceDirective`: Navigates `typeRoots` and locates candidate `.d.ts` packages.
2. If `options.types` is specified, the compiler restricts type reference resolution exclusively to the named packages.
3. Symbols from included declaration files are bound to the global scope (`ts.SymbolFlags.Transient`).

---

## 10. Best Practices & Design Guidelines

1. **Set `ignore-scripts=true` in `.npmrc`**: Block execution of untrusted scripts during installation.
2. **Pin Exact Versions**: Remove `^` and `~` for critical dependencies.
3. **Use pnpm**: Eliminates phantom dependencies and isolates module boundaries.
4. **Enforce `--frozen-lockfile` in CI**: Guarantees bit-for-bit identical dependency installations.

---

## 11. Real-World Case Study

In 2021, the popular `ua-parser-js` package was hijacked after a maintainer's npm account was compromised. A malicious version containing a cryptominer and password stealer in its `postinstall` script was published to npm.

**Impact Analysis**:
- Organizations running standard `npm install` automatically downloaded and executed the cryptominer.
- Teams utilizing `ignore-scripts=true` and exact lockfile pinning were completely **unaffected**, as the malicious install script was prevented from executing.

---

## 12. Interview Questions

<details>
<summary>1. How can a malicious dependency execute code during `npm install`?</summary>

Through npm lifecycle scripts such as `preinstall`, `install`, and `postinstall`. These scripts execute arbitrary shell commands with the privileges of the user running the installation.
</details>

<details>
<summary>2. How does `ignore-scripts=true` protect a TypeScript project?</summary>

It instructs the package manager to download and extract dependencies without executing any defined lifecycle scripts, neutralizing malware attempting to run remote shell commands upon installation.
</details>

<details>
<summary>3. What is "ambient type declaration poisoning"?</summary>

It occurs when a third-party `.d.ts` file modifies the global namespace (e.g. augmenting `globalThis` or `Object`), silently weakening the type checker's constraints or masking security flaws across the entire codebase.
</details>

<details>
<summary>4. What are "phantom dependencies" and why are they dangerous?</summary>

Phantom dependencies occur when code imports a package that exists in `node_modules` due to flat hoisting, even though it is not listed in `package.json`. If an unreferenced package is removed or upgraded elsewhere, the code crashes at runtime.
</details>

<details>
<summary>5. What is the role of `--frozen-lockfile` in CI?</summary>

It forces the package manager to fail immediately if `package.json` and the lockfile do not match, preventing accidental or unauthorized dependency upgrades in deployment pipelines.
</details>

<details>
<summary>6. How does setting `"types": ["node"]` in `tsconfig.json` improve security?</summary>

It prevents the compiler from automatically reading and binding every `@types/*` package located in `node_modules`, restricting ambient types strictly to verified packages.
</details>

<details>
<summary>7. What is npm provenance?</summary>

A mechanism that cryptographically certifies that an npm package was built from an authenticated source commit on a public GitHub Actions workflow using OpenID Connect (OIDC).
</details>

<details>
<summary>8. How do you safely install packages that legitimately require native build scripts when `ignore-scripts=true` is set?</summary>

Using pnpm's `pnpm.onlyBuiltDependencies` setting in `package.json`, which permits specific whitelisted packages (like `esbuild` or `sharp`) to run build scripts while keeping all others blocked.
</details>

---

## 13. Exercises & Challenges

<details>
<summary>Challenge 1 (Easy): Author a secure .npmrc file</summary>

**Task**: Write an `.npmrc` file that disables install scripts and forces exact version saves.

**Solution**:
```ini
ignore-scripts=true
save-exact=true
engine-strict=true
```
</details>

<details>
<summary>Challenge 2 (Easy): Restrict global types in tsconfig.json</summary>

**Task**: Configure `tsconfig.json` to load only `@types/node` and `@types/express`.

**Solution**:
```json
{
  "compilerOptions": {
    "types": ["node", "express"]
  }
}
```
</details>

<details>
<summary>Challenge 3 (Medium): Configure pnpm onlyBuiltDependencies</summary>

**Task**: Configure `package.json` to permit only `esbuild` to run install scripts while `ignore-scripts` is active.

**Solution**:
```json
{
  "pnpm": {
    "onlyBuiltDependencies": [
      "esbuild"
    ]
  }
}
```
</details>

<details>
<summary>Challenge 4 (Medium): CI Dependency Security Workflow</summary>

**Task**: Author a GitHub Actions job that runs `pnpm audit` and blocks PRs with High or Critical vulnerabilities.

**Solution**:
```yaml
name: Security Audit
on: [pull_request]

jobs:
  audit:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: pnpm/action-setup@v3
        with: { version: 9 }
      - run: pnpm audit --audit-level=high
```
</details>

<details>
<summary>Challenge 5 (Hard): Lockfile Tampering Detection Script</summary>

**Task**: Write a Node.js script that parses `pnpm-lock.yaml` to ensure every package is fetched from the official npm registry (`registry.npmjs.org`) and not an untrusted mirror.

**Solution**:
```typescript
import fs from "node:fs";

export function checkRegistryUrls(lockfilePath = "pnpm-lock.yaml"): boolean {
  const content = fs.readFileSync(lockfilePath, "utf-8");
  const urlMatches = content.match(/resolution:\s*\{tarball:\s*([^}]+)\}/g) ?? [];

  for (const match of urlMatches) {
    if (!match.includes("registry.npmjs.org")) {
      console.error(`Suspicious non-official registry source detected: ${match}`);
      return false;
    }
  }
  console.log("All package tarballs sourced from registry.npmjs.org.");
  return true;
}
```
</details>

<details>
<summary>Challenge 6 (Hard): Ambient Declaration Sandbox Audit</summary>

**Task**: Write a script using the TypeScript compiler API to inspect all source files in a project and alert if any file contains a `declare global` statement.

**Solution**:
```typescript
import ts from "typescript";
import fs from "node:fs";

export function auditAmbientDeclarations(filePaths: string[]) {
  for (const file of filePaths) {
    const code = fs.readFileSync(file, "utf-8");
    const sourceFile = ts.createSourceFile(file, code, ts.ScriptTarget.Latest, true);

    ts.forEachChild(sourceFile, (node) => {
      if (ts.isModuleDeclaration(node) && (node.flags & ts.NodeFlags.GlobalAugmentation)) {
        console.warn(`WARNING: Global augmentation detected in ${file}!`);
      }
    });
  }
}
```
</details>
