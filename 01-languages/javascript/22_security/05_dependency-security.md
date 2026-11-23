# Dependency and Supply Chain Security in JavaScript

> Software supply chain security, npm ecosystem threats, malicious lifecycle scripts, lockfile poisoning, and dependency auditing.

| Metadata | Details |
| :--- | :--- |
| **Level** | Advanced |
| **Prerequisites** | [Node.js Runtime](file:///e:/Learning/engineering-notes/01-languages/javascript/16_nodejs/01_node-runtime.md), [Security Fundamentals](file:///e:/Learning/engineering-notes/01-languages/javascript/22_security/01_xss.md) |
| **Time to read** | 24 minutes |
| **Status** | Complete |
| **Standard / Runtime** | npm v9+, OpenSSF, Node.js Permissions Model |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & API Reference](#4-syntax--api-reference)
5. [Practical Examples](#5-practical-examples)
6. [Edge Cases, Pitfalls & Gotchas](#6-edge-cases-pitfalls--gotchas)
7. [Common Mistakes & Anti-Patterns](#7-common-mistakes--anti-patterns)
8. [Performance Characteristics](#8-performance-characteristics)
9. [Engine Internals](#9-engine-internals)
10. [Best Practices & Design Guidelines](#10-best-practices--design-guidelines)
11. [Real-World Case Study](#11-real-world-case-study)
12. [Interview Questions](#12-interview-questions)
13. [Exercises & Challenges](#13-exercises--challenges)

---

## 1. What & Why

The JavaScript npm registry is the largest open-source package ecosystem in history, hosting over 3 million packages with billions of daily downloads. Modern web applications rarely write all functionality from scratch; a typical Node.js or React application depends on hundreds of direct and transitive third-party dependencies, comprising over 80% to 90% of the actual shipping code.

This massive reliance on open source has transformed the **Software Supply Chain** into the primary attack vector for threat actors. Attackers no longer need to find zero-day vulnerabilities in your application code; they simply compromise a single maintainer account of a deeply nested dependency, publish a malicious minor version update, and inherit full execution privileges inside your continuous integration (CI) servers, developer workstations, and production containers.

Defending against supply chain attacks requires understanding threats like **Typosquatting**, **Dependency Confusion**, **Malicious Lifecycle Scripts**, **Lockfile Poisoning**, and automating defenses using **`npm ci --ignore-scripts`**, **Software Bill of Materials (SBOM)**, and automated vulnerability scanners.

---

## 2. Core Concepts

### The Taxonomy of Supply Chain Attacks
1. **Typosquatting**: Registering packages with names phonetically or visually similar to popular packages (e.g. `cross-env` vs `crossenv`, `colors` vs `colour`), hoping developers mistype package names during installation.
2. **Account Takeover / Credential Stuffing**: Hijacking the npm account of a trusted open-source maintainer (via leaked passwords or missing 2FA) and publishing a malicious release under the legitimate package name (e.g. the *event-stream* or *ua-parser-js* incidents).
3. **Dependency Confusion (Namespace Confusion)**: Registering public packages on npm with identical names to a company's internal, proprietary private packages. If the company's package manager is misconfigured, it prioritizes the higher version from the public registry over the private registry.
4. **Malicious Lifecycle Scripts (`postinstall`)**: Packages that execute arbitrary shell scripts upon installation (`npm install`) to exfiltrate SSH keys, AWS credentials, and environment variables from developer machines.
5. **Lockfile Poisoning**: Malicious pull requests that secretly alter registry URLs or dependency tarball hashes in `package-lock.json`.

---

## 3. How It Works Internally

```
+--------------------------------------------------------------------------+
| DEVELOPER MACHINE / CI PIPELINE                                          |
| Executes: npm install malicious-utility                                  |
+------------------------------------+-------------------------------------+
                                     |
                                     v
+--------------------------------------------------------------------------+
| NPM PACKAGE EXTRACTION & LIFECYCLE HOOKS                                 |
| 1. npm unpacks tarball into node_modules                                 |
| 2. npm detects "scripts": { "postinstall": "node setup.js" }             |
| 3. Executes `setup.js` with FULL SYSTEM PRIVILEGES!                      |
+------------------------------------+-------------------------------------+
                                     |
                                     v
+--------------------------------------------------------------------------+
| MALICIOUS `setup.js` EXECUTION (Zero Sandbox Isolation)                  |
| - Reads ~/.ssh/id_rsa                                                    |
| - Reads process.env (AWS_SECRET_ACCESS_KEY, NPM_TOKEN, GITHUB_TOKEN)     |
| - Issues HTTPS POST to https://attacker.com/exfiltrate                   |
| - Spawns persistent reverse shell or crypto miner                       |
+--------------------------------------------------------------------------+
```

---

## 4. Syntax & API Reference

### Auditing and Securing the CLI
```bash
# 1. Run automated security vulnerability scan
npm audit

# 2. Automatically fix vulnerable dependencies (updates semver-compatible versions)
npm audit fix

# 3. Deterministic installation without running arbitrary lifecycle scripts
npm ci --ignore-scripts

# 4. Generate Software Bill of Materials (SBOM) in CycloneDX format
npm sbom --format cyclonedx

# 5. Enforce 2FA on npm publishing
npm profile enable-2fa auth-and-writes
```

### Strict `.npmrc` Security Configuration
```ini
# Enforce strict engine versions
engine-strict=true

# Block execution of postinstall scripts during global/local installs
ignore-scripts=true

# Ensure lockfile is strictly respected
save-exact=true
```

---

## 5. Practical Examples

### Example 1: Disabling Lifecycle Scripts in CI/CD Pipelines
```yaml
# .github/workflows/ci.yml
name: Secure CI Pipeline

on: [push, pull_request]

jobs:
  build:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4

      - name: Setup Node.js
        uses: actions/setup-node@v4
        with:
          node-version: 20
          cache: 'npm'

      # CRITICAL: Install dependencies WITHOUT executing arbitrary lifecycle scripts
      - name: Install Dependencies Safely
        run: npm ci --ignore-scripts

      # Run automated security vulnerability audit
      - name: Audit Dependencies
        run: npm audit --audit-level=high

      # Run tests
      - name: Run Test Suite
        run: npm test
```

### Example 2: Node.js Permissions Model (Runtime Sandboxing)
```bash
# Since Node.js v20+, Node provides an experimental Permissions Model.
# Restrict filesystem and child_process execution even if a dependency is malicious!

node --experimental-permission \
     --allow-fs-read=/app/public \
     --allow-fs-write=/app/logs \
     server.js

# If any third-party dependency attempts to read ~/.ssh or spawn a sub-process,
# Node terminates the operation immediately with `ERR_ACCESS_DENIED`!
```

### Example 3: Auditing Lockfile Tarball URLs for Lockfile Poisoning
```javascript
// Script to verify all package-lock.json dependencies originate from the official npm registry
import fs from 'node:fs';

function auditLockfileIntegrity(lockfilePath = 'package-lock.json') {
  const content = JSON.parse(fs.readFileSync(lockfilePath, 'utf8'));
  const packages = content.packages || {};
  const violations = [];

  for (const [pkgPath, meta] of Object.entries(packages)) {
    if (!pkgPath) continue; // Skip root project
    const resolvedUrl = meta.resolved;

    // Check if package resolved to an unofficial registry or arbitrary URL
    if (resolvedUrl && !resolvedUrl.startsWith('https://registry.npmjs.org/')) {
      violations.push({ package: pkgPath, url: resolvedUrl });
    }
  }

  if (violations.length > 0) {
    console.error('LOCKFILE POISONING DETECTED! Packages resolving to untrusted sources:');
    console.table(violations);
    process.exit(1);
  } else {
    console.log('✔ All lockfile packages resolve to official registry.npmjs.org');
  }
}

auditLockfileIntegrity();
```

### Example 4: Pinning Exact Dependency Versions (`save-exact`)
```json
// In package.json:
// DANGEROUS: Caret (^) and Tilde (~) automatically accept new minor/patch releases:
// "dependencies": { "express": "^4.19.2" } // Can pull in malicious 4.20.0!

// SECURE: Exact version pinning requires explicit, human review of upgrades:
{
  "dependencies": {
    "express": "4.19.2",
    "zod": "3.23.8"
  }
}
```

### Example 5: Automated License & Vulnerability Scanning via Script
```javascript
import { execFile } from 'node:child_process';
import { promisify } from 'node:util';

const execFileAsync = promisify(execFile);

async function runSecurityAudit() {
  try {
    const { stdout } = await execFileAsync('npm', ['audit', '--json']);
    const auditData = JSON.parse(stdout);

    const vulnerabilities = auditData.metadata?.vulnerabilities;
    console.log(`Vulnerabilities: Low: ${vulnerabilities.low}, High: ${vulnerabilities.high}, Critical: ${vulnerabilities.critical}`);

    if (vulnerabilities.critical > 0 || vulnerabilities.high > 0) {
      console.error('Critical or High vulnerabilities detected! Halting build.');
      process.exit(1);
    }
  } catch (err) {
    // npm audit returns exit code 1 if vulnerabilities exist
    console.error('Audit failed with vulnerabilities:', err.stdout);
    process.exit(1);
  }
}

runSecurityAudit();
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **`npm ci --ignore-scripts` and Native Addons**: Some legitimate packages (like `bcrypt`, `sharp`, or `canvas`) use `postinstall` to download pre-compiled native binaries or invoke `node-gyp`. Running `--ignore-scripts` will break these specific packages. Workaround: Use `@lavamoat/allow-scripts` to selectively whitelist only the specific packages permitted to run lifecycle scripts.
2. **False Sense of Security from `npm audit`**: `npm audit` only checks against *known, reported* CVEs. If an attacker publishes a brand-new zero-day malicious update, `npm audit` reports 0 vulnerabilities until the package is analyzed and reported hours or days later.
3. **Peer Dependency Traps**: Installing packages with `--legacy-peer-deps` or `--force` can bypass security constraint checks and pull in mismatched, vulnerable transitive dependencies.
4. **Environment Secret Leakage in Dev Dependencies**: Development tools (test runners, bundlers, linters) have identical file system and network access as production dependencies. A malicious dev dependency installed on a laptop can steal your SSH keys.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Why It Breaks | Correct Pattern |
| :--- | :--- | :--- |
| Running `npm install` in CI/CD without `--ignore-scripts` | Executes arbitrary code from compromised dependencies | Use `npm ci --ignore-scripts` |
| Using `^` and `~` in package dependencies | Automatically pulls in unvetted minor updates | Pin exact versions or enforce strict `package-lock.json` |
| Deleting `package-lock.json` to "fix" install errors | Destroys cryptographic checksums; pulls in unverified trees | Keep lockfile versioned in git; resolve conflicts cleanly |
| Ignoring `npm audit` warnings | Known exploits remain unpatched in production | Audit and fix dependencies regularly |
| Publishing packages without 2-Factor Authentication (2FA) | Makes maintainer accounts vulnerable to credential stuffing | Enforce 2FA on npm accounts |
| Storing production API secrets in `.env` files committed to git | Immediately harvested by public scanners | Use secret managers (AWS Secrets Manager, Doppler) |

---

## 8. Performance Characteristics

- **`npm ci` vs `npm install`**: `npm ci` skips dependency resolution algorithms and directly unpacks the exact tree defined in `package-lock.json`, running up to 2x to 3x faster than `npm install`.
- **`--ignore-scripts` Speedup**: Skipping compilation and execution of postinstall scripts speeds up CI installation times by 30% to 50%.

---

## 9. Engine Internals

1. **Lockfile Cryptographic Integrity (`integrity: sha512-...`)**:
   - Every entry in `package-lock.json` contains a Base64-encoded Subresource Integrity (SRI) SHA-512 hash of the package tarball.
   - When downloading a package, npm computes the SHA-512 hash of the downloaded bytes. If the hash does not match the lockfile hash, npm halts installation immediately, preventing tarball tampering or Man-in-the-Middle attacks.

---

## 10. Best Practices & Design Guidelines

- **Always Commit `package-lock.json`**: The lockfile is your single source of truth for deterministic, verified dependency trees.
- **Audit Dependency Additions**: Before running `npm install <new-package>`, check its maintenance health, download counts, and open issues on npm and GitHub. Avoid packages that have no maintainers or single-digit downloads.
- **Use Automated Dependency Renovators**: Tools like Dependabot or Renovate automatically create PRs for dependency updates, allowing tests to run and verify stability before merging.

---

## 11. Real-World Case Study

### The `event-stream` Supply Chain Attack (2018)
```javascript
// WHAT HAPPENED:
// A popular npm package `event-stream` (millions of weekly downloads) was transferred
// by its original author to a new volunteer maintainer.
// The new maintainer added a dependency `flatmap-stream` containing heavily obfuscated malicious code.

// THE TARGET:
// The malicious code specifically targeted the BitPay cryptocurrency wallet application.
// When bundled inside the Copay mobile wallet app, it harvested private keys and seed phrases
// and exfiltrated them to an attacker-controlled server in Malaysia.

// THE LESSON:
// 1. Never trust transitive dependencies blindly.
// 2. Lock dependencies strictly with `package-lock.json`.
// 3. Monitor package maintainer ownership transfers with tools like Socket.dev.
// 4. Run CI installations with `--ignore-scripts`.
```

---

## 12. Interview Questions

<details>
<summary>1. What is a Software Supply Chain attack and why is the npm ecosystem particularly vulnerable?</summary>
A supply chain attack targets the third-party dependencies, libraries, and build tools an application relies upon rather than attacking the application directly. The npm ecosystem is uniquely vulnerable because JavaScript applications typically rely on hundreds of deeply nested transitive dependencies, packages execute lifecycle scripts with full system privileges upon installation, and many popular packages are maintained by single unvetted volunteers without mandatory 2FA.
</details>

<details>
<summary>2. What does npm ci --ignore-scripts do and why should it be used in CI/CD?</summary>
<code>npm ci</code> ensures clean, deterministic installation based strictly on <code>package-lock.json</code>, throwing an error if the lockfile is out of sync with <code>package.json</code>. The <code>--ignore-scripts</code> flag disables the execution of arbitrary lifecycle hooks (like <code>preinstall</code> and <code>postinstall</code>), preventing malicious packages from running arbitrary shell scripts on build servers.
</details>

<details>
<summary>3. What is Dependency Confusion?</summary>
Dependency Confusion occurs when an attacker discovers the name of an organization's internal private package (e.g. <code>@company/auth</code>) and publishes a package with the exact same name on the public npm registry with a very high version number (e.g. <code>99.9.9</code>). If the company's package manager is not strictly configured with private registry scoping, it pulls the malicious public package instead of the internal one.
</details>

<details>
<summary>4. What is Typosquatting in the context of package managers?</summary>
Typosquatting is the practice of publishing malicious packages under names that mimic popular libraries (e.g. <code>lodsh</code> instead of <code>lodash</code>, or <code>crossenv</code> instead of <code>cross-env</code>). When a developer accidentally makes a typing mistake during <code>npm install</code>, the malicious package is installed, executing malicious code on their machine.
</details>

<details>
<summary>5. How does package-lock.json ensure cryptographic integrity?</summary>
Each package entry in <code>package-lock.json</code> contains an <code>integrity</code> field with a SHA-512 hash of the package tarball. During installation, npm verifies the downloaded tarball against this hash; if the package has been altered or tampered with on the registry, installation is aborted immediately.
</details>

<details>
<summary>6. What is a Software Bill of Materials (SBOM)?</summary>
An SBOM is a formal, structured inventory of all software components, direct dependencies, transitive dependencies, and licensing metadata included in an application. It provides complete visibility into what software is running, allowing organizations to immediately identify whether a newly discovered CVE affects their systems.
</details>

<details>
<summary>7. What is the limitation of relying solely on npm audit?</summary>
<code>npm audit</code> is reactive: it only detects known, cataloged vulnerabilities that have been assigned a CVE and reported to the advisory database. It cannot detect zero-day malicious code, un-reported hijacked packages, or newly published malicious typosquatting packages.
</details>

<details>
<summary>8. What is the Node.js Permissions Model?</summary>
Introduced in Node.js v20 (experimental), the Permissions Model allows developers to restrict what system resources the Node process can access (e.g. <code>--allow-fs-read</code>, <code>--allow-child-process=false</code>). Even if a malicious dependency is loaded, it cannot read unauthorized directories or execute shell binaries.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Dependency Version Pinning Linter
Write a script `lintPackageDependencies()` that reads `package.json` and verifies that all dependencies are strictly pinned to exact versions (no `^`, `~`, or `*` prefixes).

<details>
<summary>View Solution</summary>

```javascript
import fs from 'node:fs';

export function lintPackageDependencies(packageJsonPath = 'package.json') {
  const pkg = JSON.parse(fs.readFileSync(packageJsonPath, 'utf8'));
  const violations = [];

  const checkDeps = (deps, type) => {
    if (!deps) return;
    for (const [name, version] of Object.entries(deps)) {
      if (version.startsWith('^') || version.startsWith('~') || version === '*' || version.startsWith('>')) {
        violations.push({ package: name, version, type });
      }
    }
  };

  checkDeps(pkg.dependencies, 'dependencies');
  checkDeps(pkg.devDependencies, 'devDependencies');

  if (violations.length > 0) {
    console.error('UNPINNED DEPENDENCIES DETECTED:');
    console.table(violations);
    return false;
  }

  console.log('✔ All dependencies are strictly pinned.');
  return true;
}

lintPackageDependencies();
```
</details>

### Exercise 2: Postinstall Script Detector
Write a Node.js utility `scanForLifecycleScripts()` that scans `node_modules` and flags any installed package that defines a `preinstall`, `install`, or `postinstall` script in its `package.json`.

<details>
<summary>View Solution</summary>

```javascript
import fs from 'node:fs';
import path from 'node:path';

export function scanForLifecycleScripts(nodeModulesDir = './node_modules') {
  const flagged = [];

  function crawl(dir) {
    const entries = fs.readdirSync(dir, { withFileTypes: true });

    for (const entry of entries) {
      if (!entry.isDirectory()) continue;

      if (entry.name.startsWith('@')) {
        // Scoped package directory
        crawl(path.join(dir, entry.name));
        continue;
      }

      const pkgJsonPath = path.join(dir, entry.name, 'package.json');
      if (fs.existsSync(pkgJsonPath)) {
        try {
          const content = JSON.parse(fs.readFileSync(pkgJsonPath, 'utf8'));
          const scripts = content.scripts || {};
          const hooks = ['preinstall', 'install', 'postinstall'].filter(h => scripts[h]);

          if (hooks.length > 0) {
            flagged.push({
              package: content.name || entry.name,
              hooks: hooks.join(', '),
              command: hooks.map(h => `${h}: ${scripts[h]}`).join(' | '),
            });
          }
        } catch (e) {
          // Skip invalid JSON
        }
      }
    }
  }

  crawl(nodeModulesDir);
  console.log(`Scanned packages with lifecycle scripts: ${flagged.length}`);
  console.table(flagged);
  return flagged;
}
```
</details>
