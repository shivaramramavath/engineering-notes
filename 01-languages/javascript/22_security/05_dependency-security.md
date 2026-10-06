# Dependency Security

A typical Node project pulls in hundreds or thousands of packages, most of them **transitive** (dependencies of your dependencies). Your app inherits their bugs, and their maintainers' mistakes or compromises. Supply-chain security is about knowing what you run and limiting what a bad package can do.

## Prerequisites

- [npm and package managers](../00_setup/03_npm-and-package-managers.md)
- [Modules](../13_modules/README.md)

---

## Threats

| Threat | What happens |
|---|---|
| **Known vulnerability** | A published CVE affects a version you use (e.g. a prototype pollution bug in a utility library) |
| **Malicious package** | An attacker publishes harmful code deliberately |
| **Typosquatting / dependency confusion** | A look-alike name (or a public package shadowing a private name) gets installed by mistake |
| **Account/maintainer compromise** | A legitimate package gets a malicious new release |
| **Install scripts** | `preinstall`/`postinstall` scripts run arbitrary code on install |
| **Abandoned packages** | No one fixes future vulnerabilities |

Most incidents are the first (outdated, known-vulnerable versions), so start there.

---

## Know What You Have

```bash
npm ls                    # dependency tree
npm ls <package>          # why is this package here? which versions?
npm outdated              # what has newer versions
npm audit                 # check installed tree against the advisory database
npm audit --omit=dev      # only production dependencies
npm audit fix             # apply compatible fixes (semver-safe)
```

Notes on `npm audit`:

- It reports **known** advisories only; a clean audit doesn't mean "secure."
- Severity is generic. Judge **reachability**: a vulnerability in a dev-only build tool or an unused code path matters less than one on your request path.
- `npm audit fix --force` can apply breaking upgrades; review the changes and run tests first.
- Advisories often live deep in the tree. Use `npm ls` to find which direct dependency pulls the bad version in, then upgrade that parent, or use [`overrides`](https://docs.npmjs.com/cli/configuring-npm/package-json#overrides) in `package.json` as a temporary patch.

```json
{
  "overrides": {
    "vulnerable-pkg": "^2.1.4"
  }
}
```

(Other package managers have equivalents, for example `resolutions` in Yarn, `pnpm.overrides` in pnpm. Check your tool's docs.)

---

## Lockfiles and Reproducible Installs

The lockfile (`package-lock.json`, `pnpm-lock.yaml`, `yarn.lock`) records the **exact** resolved versions and integrity hashes.

- **Commit the lockfile.** Without it, builds can silently pick up newer (possibly compromised) versions.
- In CI and production builds, use `npm ci`: it installs exactly what the lockfile says, fails if `package.json` and the lockfile disagree, and never rewrites the lockfile. Use `npm install` only when you intend to change dependencies.
- Review lockfile diffs in pull requests, not just `package.json`: a one-line change can add dozens of new packages.

```bash
npm ci
```

### Version ranges

`^1.2.3` accepts any newer `1.x.x`, so new releases enter automatically on `npm install`. The lockfile is what actually pins you. Pinning exact versions in `package.json` adds predictability but also means *you* own the update work. Pick a policy and automate updates.

---

## Install Scripts

Packages can run code at install time via lifecycle scripts. That code has your user's privileges and runs before you've ever imported the package.

```bash
npm ci --ignore-scripts          # skip lifecycle scripts
npm config set ignore-scripts true
```

Disabling scripts can break packages that legitimately need a build step (native addons, some tools), so you may need to allow specific ones. Some package managers let you control this per-package; check what yours supports.

---

## Automated Updates

Manual updates don't happen consistently. Use a bot:

- **Dependabot** (GitHub) or **Renovate** open pull requests for new versions and security fixes.
- Group low-risk updates, and require **CI to pass** (your [test suite](../21_testing/README.md) is what makes automatic updates safe).
- Prioritize security updates; schedule the rest.

---

## Choosing Dependencies Carefully

Before adding a package, ask:

- **Do I need it?** A 10-line helper often beats a dependency (and its whole subtree).
- **Is it maintained?** Recent releases, responsive issues, more than one maintainer.
- **Is it widely used and reviewed?** Download counts aren't proof of safety, but obscurity increases risk.
- **Is the name exactly right?** Check the spelling and the publisher/scope before `npm install`; typosquats rely on typos.
- **How big is its dependency tree?** `npm ls` after installing; fewer transitive packages means less exposure.
- **Does it need install scripts or native code?**

Prefer platform features (`fetch`, `structuredClone`, `node:crypto`, `node:test`, `Array.prototype.at`, etc.) where they cover your need.

---

## Hardening the Pipeline

| Practice | Why |
|---|---|
| Enable **2FA** on npm and GitHub accounts; use publish tokens with least privilege | Prevents your own packages being hijacked |
| Scope private packages (`@yourorg/pkg`) and configure the registry for that scope | Mitigates dependency confusion |
| Verify provenance/signatures where available (`npm audit signatures`) | Detects tampered registry packages that have published signatures/provenance |
| Don't expose secrets to install/build steps unnecessarily | A malicious install script could read environment variables |
| Run CI with minimal permissions; no long-lived credentials in the repo | Limits blast radius |
| Keep Node and your package manager updated | Fixes in the runtime and tooling matter too |

For frontend code loaded from CDNs, use **Subresource Integrity** so the browser rejects a modified file:

```html
<script src="https://cdn.example.com/lib.js"
        integrity="sha384-..." crossorigin="anonymous"></script>
```

---

## Common Mistakes

| Mistake | Fix |
|---|---|
| Not committing the lockfile | Commit it; use `npm ci` in CI |
| Ignoring `npm audit` entirely, or `--force` fixing blindly | Triage by reachability; test after updating |
| Treating "dev dependency" as harmless | Dev tools run on developer machines and CI with secrets; they're targets too |
| Installing a package from a blog snippet without checking the name | Verify name, publisher, and repository |
| Leaving unused packages | Remove them (`npm uninstall`; check with tools like `depcheck`) |
| Never updating | Small regular updates beat a risky big-bang upgrade |
| Pinning forever "for stability" | Pinned doesn't mean patched |

---

## Responding to a Vulnerability

1. Identify the affected package and version (`npm audit`, `npm ls <pkg>`).
2. Check whether the vulnerable code path is **reachable** in your app.
3. Upgrade the direct dependency that brings it in; use `overrides` as a stopgap.
4. Run tests, deploy, and add a regression test if it's behavior you rely on.
5. If a package was **compromised or malicious**: remove/pin to a known-good version, **rotate any secrets** that were present where it ran, and review logs.

---

## Quick Summary

- Most of your code is other people's code; manage it deliberately.
- Commit lockfiles, install with **`npm ci`**, review lockfile diffs.
- Run **`npm audit`** regularly, triage by reachability, and fix via parent upgrades or `overrides`.
- Limit **install scripts**, enable **2FA**, and prefer fewer, well-maintained dependencies.
- Automate updates with Dependabot/Renovate, backed by good tests.

**Next:** [Security Checklist](./06_security-checklist.md)
