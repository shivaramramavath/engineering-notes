# Installation

Before you can create a NestJS project you need three things: a supported Node.js runtime, a package manager, and the Nest CLI. Version mismatches here cause most first-day failures, and NestJS 12 has two different Node.js minimums (one for running an app, a higher one for the CLI's generators). This file covers installing everything correctly, verifying it, and fixing the common problems.

---

## Overview

**What you are installing**

| Component | Role | Required? |
|---|---|---|
| **Node.js** | Runtime that executes your application and the CLI | Yes |
| **Package manager** (npm, Yarn, pnpm, or Bun) | Installs dependencies, runs scripts | Yes (npm ships with Node.js) |
| **`@nestjs/cli`** | The `nest` command: scaffolding, build, run, upgrade | Strongly recommended |
| **TypeScript** | Compiler. Installed as a project dependency by `nest new` | Installed for you |
| **Git** | Version control. `nest new` initializes a repository unless skipped | Recommended |

**Why it matters.** The CLI does real work (it generates code, wires modules, compiles, and watches files). The wrong Node.js version produces errors that look like framework bugs but are environment problems.

---

## Mental Model

```text
 Your machine
 ┌──────────────────────────────────────────────────────────────┐
 │  Version manager (nvm / fnm / Volta)  ← picks the Node version│
 │          │                                                    │
 │          ▼                                                    │
 │  Node.js  ──►  npm / pnpm / yarn / bun   ← install packages   │
 │          │                                                    │
 │          ▼                                                    │
 │  @nestjs/cli  (global)   ← the `nest` command                 │
 └──────────────────────────────────────────────────────────────┘
            │  nest new my-app
            ▼
 ┌──────────────────────────────────────────────────────────────┐
 │  my-app/                                                      │
 │    node_modules/  ← @nestjs/core, common, platform-express,   │
 │                     typescript, test runner, linter ...       │
 │    package.json   ← scripts call the local `nest` binary      │
 └──────────────────────────────────────────────────────────────┘
```

Two copies of the CLI exist in practice: the **global** one (used for `nest new` and `nest upgrade`) and the **project-local** one in `devDependencies` (used by `npm run start`, `npm run build`). They can have different versions.

---

## Core Concepts

### Node.js Version Requirements (NestJS 12)

NestJS 12 distinguishes between running an application and using the CLI's generators.

| What you are doing | Minimum Node.js |
|---|---|
| **Running** a Nest 12 application | **v20.19+**, or **v22.12+** on the 22.x line |
| `nest new`, `nest generate`, `nest upgrade` (the schematics) | **v22.22.3+**, **v24.15+**, or **v26+** |

Notes:

- The v12 packages are **ESM-only**. CommonJS applications consume them through `require(esm)`, which is available unflagged only from Node.js 20.19 and 22.12. The 21.x line never received it and is not supported.
- The CLI's schematics inherit a higher floor from the Angular devkit they build on. Node.js 20.19 can **run** your application but cannot **generate** code. The 23.x and 25.x lines and earlier 22.x/24.x releases are excluded for the CLI.
- **Recommendation: use the latest active LTS release.** It satisfies both rows.
- If you stay on **Jest**, it can load the ESM-only v12 packages only on Node.js **v24.9+** (older versions fail with `ERR_REQUIRE_ASYNC_MODULE`). Vitest (the ESM default) does not have this restriction.
- `nest upgrade` refuses to run on Node.js releases that lack `require(esm)`.
- AWS Lambda disables `require(esm)` by default on its Node.js runtimes. A CommonJS Nest 12 app there needs `NODE_OPTIONS=--experimental-require-module`. *Verify against current AWS Lambda documentation.*

> The exact patch numbers above come from the official NestJS 12 migration guide and will move as Node.js releases ship. Run `nest info` and read the CLI error messages. They state the requirement for your installed version.

### Version Managers

Install Node.js through a version manager rather than a system package or the website installer. You will work on projects that need different Node.js versions, and version managers avoid permission problems with global installs.

| Tool | Platforms | Notes |
|---|---|---|
| **nvm** | macOS, Linux (WSL on Windows) | Most common. Shell function, slower startup |
| **nvm-windows** | Windows | Separate project from nvm, different commands |
| **fnm** | macOS, Linux, Windows | Fast, Rust-based. Reads `.nvmrc` / `.node-version` |
| **Volta** | macOS, Linux, Windows | Pins Node and tool versions per project via `package.json` |
| **mise / asdf** | macOS, Linux | Multi-language version managers |

### Package Managers

`nest new` supports `npm`, `yarn`, `pnpm`, and `bun`. The package manager must be installed globally. Pick one per project and keep to it.

| Manager | Install | Characteristics |
|---|---|---|
| **npm** | Ships with Node.js | Default, universal, `npm ci` for reproducible installs |
| **pnpm** | `corepack enable` or `npm i -g pnpm` | Fast, disk-efficient, strict dependency isolation |
| **Yarn** | `corepack enable` | Classic or Berry, workspaces |
| **Bun** | Installed separately | Supported by the CLI as a package manager. Running Nest on Bun as a runtime is a separate matter. *Verify support level before using it in production* |

Corepack (bundled with many Node.js releases) pins the exact package-manager version through the `packageManager` field in `package.json`. *Verify Corepack availability on your Node.js version.*

### Installing the Nest CLI

Three ways, from most to least common:

| Approach | Command | Use when |
|---|---|---|
| **Global install** | `npm i -g @nestjs/cli` | You scaffold and upgrade projects regularly |
| **One-off with npx** | `npx @nestjs/cli new my-app` | You do not want a global install |
| **Project-local** | `npm i -D @nestjs/cli` | Always done by `nest new`. Scripts use this copy |

The global CLI is required for `nest upgrade` as a first step because the upgrade command ships with the CLI. The docs also recommend updating `@nestjs/schematics` alongside it.

---

## How It Works

What happens when you run `nest new my-app` (detail in [Nest CLI](./02-nest-cli.md)):

```text
nest new my-app
   │
   ├─ 1. CLI checks Node.js version and loads @nestjs/schematics
   ├─ 2. Prompts: package manager, module system (ESM/CJS), Observe (yes/no)
   ├─ 3. Schematic generates files in ./my-app
   ├─ 4. Package manager installs dependencies
   │      (@nestjs/*, typescript, test runner, linter, formatter)
   └─ 5. Git repository is initialized (unless --skip-git)
```

The local `node_modules/.bin/nest` binary then runs every `npm run ...` script.

---

## Basic Example

A clean install from scratch on macOS or Linux using `fnm` and npm:

```bash
# 1. Install a version manager (see its docs for your platform)
curl -fsSL https://fnm.vercel.app/install | bash
# restart your shell, then:

# 2. Install and select the latest LTS Node.js
fnm install --lts
fnm use lts-latest
node --version          # confirm it meets the requirements table above
npm --version

# 3. Install the Nest CLI globally
npm i -g @nestjs/cli

# 4. Verify
nest --version
nest info
```

What each step does:

1. `fnm install --lts` downloads the latest LTS release into a managed directory (no `sudo` needed).
2. `fnm use lts-latest` puts that version on your `PATH` for the current shell.
3. `npm i -g @nestjs/cli` installs the `nest` binary into the active Node.js version's global directory. **Global packages are per Node.js version** when using a version manager. Switching versions means reinstalling.
4. `nest info` prints OS, Node.js, npm, and Nest package versions. It is the first command to run when something is wrong, and the first output to paste into a bug report.

Example `nest info` output (from the official docs, values vary):

```text
[System Information]
OS Version     : macOS Sequoia 24.6.0
NodeJS Version : v22.12.0
NPM Version    : 10.9.0

[Nest CLI]
Nest CLI Version : 12.0.3

[Nest Platform Information]
platform-express version : 12.0.4
schematics version       : 12.0.4
testing version          : 12.0.4
common version           : 12.0.4
core version             : 12.0.4
cli version              : 12.0.3
```

---

## Practical Examples

### 1. Basic: Pin the Node.js Version for a Project

```bash
node --version > .nvmrc          # e.g. v22.14.0
# Anyone on the team (or CI) can now run:
fnm use          # or: nvm use
```

Also declare it in `package.json` so tooling can warn about mismatches:

```json
{
  "engines": { "node": ">=20.19.0" }
}
```

### 2. Common: Non-Interactive Project Creation (CI, scripts)

```bash
npx @nestjs/cli new my-app --package-manager npm --skip-git
```

In non-interactive environments, pass the project name and `--package-manager`. The module system then defaults to ESM, and Observe is set up only if you pass `--observe`.

### 3. Common: Use pnpm Instead of npm

```bash
corepack enable
nest new my-app --package-manager pnpm
```

### 4. Real-World: Install a Specific CLI Version

```bash
npm i -g @nestjs/cli@12          # latest 12.x
npm i -g @nestjs/cli@latest      # newest release
npm ls -g @nestjs/cli            # what is installed globally
```

Use this when you must scaffold a project matching an older framework major (for example a v11 project for a legacy codebase).

### 5. Real-World: Upgrade the CLI and an Existing Project

```bash
npm i -g @nestjs/cli@latest @nestjs/schematics@latest   # global first
cd my-existing-app
nest upgrade --dry-run            # report only, change nothing
nest upgrade                      # apply
```

`nest upgrade` (alias `nest update`) moves every recognized `@nestjs/*` package to the same major, bumps TypeScript to v6, and applies the mechanical parts of the migration. It does **not** migrate your project to ESM, Vitest, or oxlint. It only bumps the **local** CLI dependency, which is why you update the global binary first.

### 6. Edge Case: Using a Different Node.js Version per Project

```bash
cd project-a && fnm use       # reads .nvmrc → Node 22
cd project-b && fnm use       # reads .nvmrc → Node 24
```

Global npm packages are tied to each Node.js version, so install `@nestjs/cli` under each one you use for scaffolding, or rely on `npx`.

### 7. Edge Case: Corporate Proxy or Private Registry

```bash
npm config set proxy http://proxy.example.com:8080
npm config set https-proxy http://proxy.example.com:8080
npm config set registry https://registry.example.com/
npm config list
```

Registry and proxy settings live in `.npmrc` (project or user level). Keep tokens out of committed files.

---

## Syntax / API / Commands

| Command | Purpose |
|---|---|
| `node --version` / `node -v` | Installed Node.js version |
| `npm --version` | npm version |
| `npm i -g @nestjs/cli` | Install the CLI globally |
| `npm i -g @nestjs/cli@latest` | Update the global CLI |
| `npm ls -g --depth=0` | List global packages |
| `npm uninstall -g @nestjs/cli` | Remove the global CLI |
| `nest --version` | CLI version |
| `nest info` | System and Nest package versions |
| `npx @nestjs/cli new <name>` | Create a project without a global install |
| `fnm install --lts` / `nvm install --lts` | Install the latest LTS Node.js |
| `fnm use` / `nvm use` | Switch to the version in `.nvmrc` |
| `corepack enable` | Enable pinned pnpm/Yarn versions |
| `which nest` (macOS/Linux) / `where nest` (Windows) | Locate the binary |
| `npm config get prefix` | Where global packages are installed |

---

## Important Rules

1. **Install Node.js through a version manager**, not with `sudo` and not with a system package that you cannot switch.
2. **Never use `sudo npm i -g`.** It creates root-owned files and later permission errors. Fix the Node.js installation instead.
3. **Running and scaffolding have different Node.js floors.** An app that runs on Node.js 20.19 cannot be generated by it.
4. **Use the latest active LTS** unless you have a specific reason not to. Avoid odd-numbered (non-LTS) Node.js releases for production.
5. **Update the global CLI before `nest upgrade`.** The command is part of the CLI.
6. **Global packages are per Node.js version** when using a version manager.
7. **Commit your lockfile** (`package-lock.json`, `pnpm-lock.yaml`, `yarn.lock`, `bun.lock`) and use `npm ci` (or the equivalent) in CI.
8. **One package manager per project.** Mixing lockfiles causes drift.
9. **All `@nestjs/*` packages must share the same major version.** Mixed majors cause confusing runtime errors.
10. **Match local and production Node.js versions** (Docker image, CI, `.nvmrc`).

---

## Under the Hood

### What the CLI Package Contains

`@nestjs/cli` provides the `nest` binary. It delegates scaffolding to `@nestjs/schematics` (a collection built on the Angular devkit's schematics engine), compilation to a chosen builder (TypeScript compiler, SWC, or Rspack), and process management for `nest start`. This is why the schematics package has its own Node.js floor.

### Global vs Local Resolution

```text
$ nest start                        ← global binary on PATH
$ npm run start → "nest start"      ← local binary: node_modules/.bin/nest (npm adds it to PATH)
```

Scripts always use the project-local CLI. If the local and global versions differ, `nest new`/`nest upgrade` behave per the global one, and `start`/`build` per the local one.

### Why ESM-Only Packages Matter at Install Time

NestJS 12 packages are ESM. A CommonJS project can still `require()` them because modern Node.js supports `require(esm)`. This is why the **runtime** floor is tied to specific Node.js releases rather than a general "Node 20". Tools that bundle or transform your code (Jest, Lambda runtimes, custom bundlers) may impose extra constraints, which is why those warnings appear in the version notes.

---

## Common Patterns

### Per-Project Toolchain Pinning

`.nvmrc` + `engines` in `package.json` + the same Node.js major in your `Dockerfile` and CI image.

### No-Global-Install Workflow

Use `npx @nestjs/cli ...` for scaffolding and rely on the local CLI through `npm run` scripts. Suitable for ephemeral machines and CI.

### Team Onboarding Script

```bash
#!/usr/bin/env bash
set -euo pipefail
fnm install                # reads .nvmrc
fnm use
npm ci
npm run build
npm test
```

---

## Common Mistakes

| Mistake | Symptom | Why It Happens | Fix |
|---|---|---|---|
| Node.js too old for the CLI | `nest new` or `nest generate` fails with a Node.js version error | Schematics need v22.22.3+, v24.15+, or v26+ | Upgrade Node.js with your version manager |
| Node.js 21, 23, or 25 (odd lines) | Unsupported-version errors or `require(esm)` problems | These lines are excluded or never got `require(esm)` | Use the latest LTS |
| `nest: command not found` | Shell cannot find the binary | Global npm bin directory not on `PATH`, or Node.js version changed | `npm config get prefix`, add its `bin` to `PATH`, reinstall under the active version |
| `EACCES: permission denied` on global install | Install aborts | Global directory owned by root | Use a version manager. Do not use `sudo` |
| Running scripts with an old local CLI | Behavior differs from docs | Project pinned to an old `@nestjs/cli` | `npm ls @nestjs/cli`, update the local dev dependency |
| Mixed `@nestjs/*` majors | `Cannot read properties of undefined`, odd DI errors | Partial manual upgrade | Run `nest upgrade` or align all packages to one major |
| Mixed lockfiles (`package-lock.json` and `pnpm-lock.yaml`) | Different dependency trees per developer | Two package managers used | Delete one lockfile, standardize on one manager |
| Jest `ERR_REQUIRE_ASYNC_MODULE` after upgrade | Tests fail on import of `@nestjs/*` | Jest can load ESM-only packages only on Node.js 24.9+ | Run tests on Node.js 24.9+, or migrate to Vitest |
| Windows PowerShell blocks `nest` | "running scripts is disabled on this system" | PowerShell execution policy blocks `.ps1` shims | `Set-ExecutionPolicy -Scope CurrentUser RemoteSigned`, or use `cmd`/Git Bash/WSL |
| Installing under one Node.js version and running under another | CLI missing after `nvm use` | Global packages are per version | Reinstall, or use `npx` |
| Using the Bun/Yarn flag without that tool installed | `nest new` fails at install step | Package manager must be installed globally | Install it first, or pick another |

---

## Debugging

### Common Errors

| Message (approximate) | Meaning | Fix |
|---|---|---|
| `command not found: nest` | Binary not on `PATH` | Reinstall globally under the active Node.js, or use `npx` |
| `EACCES: permission denied, mkdir '/usr/local/lib/node_modules'` | Global dir not writable | Use a version manager |
| `engine "node" is incompatible with this module` (Yarn) / `EBADENGINE` (npm warning) | A package declares a Node.js range you do not satisfy | Upgrade Node.js |
| `ERR_REQUIRE_ASYNC_MODULE` | Jest loading ESM-only Nest packages on Node.js < 24.9 | Node.js 24.9+ or Vitest |
| `ERR_REQUIRE_ESM` | Old Node.js trying to `require` an ES module | Upgrade Node.js to a release with `require(esm)` |
| `ETIMEDOUT` / `ECONNRESET` during install | Network or proxy problem | Check proxy/registry settings, retry |

### Commands

```bash
nest info                         # versions of everything relevant
node --version && npm --version
npm ls @nestjs/core @nestjs/common @nestjs/cli     # confirm one major across packages
npm ls -g --depth=0                                # global packages for the active Node.js
npm config get prefix                              # where global installs go
echo $PATH | tr ':' '\n' | grep -i node            # is the Node.js bin on PATH? (macOS/Linux)
npm cache verify                                   # repair a corrupted cache
```

### Techniques

1. **Start with `nest info`.** It shows the Node.js and package versions in one place.
2. **Check which binary runs:** `which nest` / `where nest`, then `nest --version`.
3. **Check Node.js per shell:** version managers are per-shell. A new terminal may default to a different version.
4. **Delete and reinstall as a last resort:** `rm -rf node_modules package-lock.json && npm install` (only when you accept regenerating the lockfile).
5. **Read the whole error.** Version errors from the CLI state the required range for your situation.

---

## Performance

- **Installs:** `pnpm` and a warm cache are typically fastest. `npm ci` is faster and stricter than `npm install` in CI.
- **CI caching:** cache the package manager's store (not `node_modules`) keyed by the lockfile hash.
- **Disk:** `pnpm` shares one content-addressable store across projects.
- **Cold start of the CLI:** `npx` re-downloads when uncached. A global or local install avoids the download.

---

## Security

- **Verify package names.** Install `@nestjs/cli` (scoped under `@nestjs`). Typosquatted packages with similar names exist in the npm ecosystem.
- **Install scripts run code.** `npm i -g` and dependency installs may execute lifecycle scripts. Consider `npm install --ignore-scripts` for untrusted projects, then run needed scripts deliberately. *Understand the trade-off: some packages need their install scripts.*
- **Lockfiles are a security control.** They pin exact versions and integrity hashes. Use `npm ci` in CI.
- **Audit dependencies:** `npm audit`. See [Dependency Security](../07-production/01-security/07-dependency-security.md).
- **Do not commit registry tokens.** Keep them in user-level `.npmrc` or CI secrets.
- **Never use `sudo` for npm.** It runs lifecycle scripts as root.

---

## Production Considerations

- **Pin the runtime:** the Docker base image's Node.js major should match `.nvmrc` and CI.
- **Use an LTS release** in production, and track its end-of-life date.
- **Reproducible installs:** `npm ci --omit=dev` (or the equivalent) in production images.
- **CLI is a dev dependency.** Production images normally run compiled output with `node dist/...`, not the CLI. See [Docker](../07-production/04-deployment/03-docker.md).
- **Upgrade cadence:** upgrade Node.js and Nest on a schedule. `nest upgrade --dry-run` in a branch makes major upgrades routine.
- **Lambda and bundlers:** verify `require(esm)` behavior on your runtime before deploying a CommonJS app on Nest 12.

---

## Best Practices

### Recommended

```bash
fnm install --lts && fnm use lts-latest
npm i -g @nestjs/cli
node --version > .nvmrc
npm ci                       # in CI
```

### Avoid

```bash
sudo npm i -g @nestjs/cli    # root-owned globals, permission problems
npm install                  # in CI: may modify the lockfile
nvm use 21                   # an unsupported (non-LTS, no require(esm)) line
```

Why: version managers isolate runtimes, `npm ci` makes builds reproducible, and unsupported Node.js lines break the ESM-only packages.

Additional guidance:

- Keep the global CLI current. Keep each project's local CLI aligned with its Nest major.
- Document the supported Node.js range in the project README.
- Add `engines` to `package.json` and (optionally) `engine-strict=true` in `.npmrc` to enforce it.

---

## Version / Compatibility Notes

| Item | NestJS 12 | Earlier |
|---|---|---|
| Run an app | Node.js 20.19+ or 22.12+ | v11 required Node.js 20+ |
| CLI generators | Node.js 22.22.3+, 24.15+, or 26+ | Lower floor |
| TypeScript | v6 (bumped by `nest upgrade`, required by v12 CLI and schematics) | 5.x |
| Package format | ESM-only core packages (consumable from CJS via `require(esm)`) | CommonJS |
| Upgrade command | `nest upgrade` (alias `nest update`) | `npm-check-updates` recommended manually |
| Jest | v30, works with the ESM-only packages only on Node.js 24.9+ | Any supported |
| Package managers supported by `nest new` | npm, yarn, pnpm, bun | npm, yarn, pnpm |

*All figures come from the official NestJS 12 migration guide and CLI documentation as of this writing. Verify against `nest info` output and the current docs, since patch-level Node.js requirements change as Node.js releases ship.*

---

## Real-World Use Cases

- **Team onboarding:** `.nvmrc` + lockfile + an onboarding script gives every developer an identical toolchain.
- **CI pipelines:** `actions/setup-node` (or equivalent) with the pinned version, `npm ci`, then lint, build, and test.
- **Framework upgrades:** `nest upgrade --dry-run` to review the report before changing anything.
- **Multi-project machines:** `fnm`/Volta switching Node.js versions automatically per directory.
- **Ephemeral environments (Codespaces, dev containers):** `npx @nestjs/cli new` with no global state.

---

## Interview Questions

### Beginner

1. What do you need installed to start a NestJS project?
   - Node.js (a supported version), a package manager (npm ships with Node.js), and ideally the Nest CLI (`@nestjs/cli`).
2. What command creates a new project?
   - `nest new <name>`, or `npx @nestjs/cli new <name>` without a global install.
3. How do you check which versions you have?
   - `node --version`, `nest --version`, and `nest info` for the full picture.

### Intermediate

1. Why do you install Node.js through a version manager?
   - To switch versions per project, avoid `sudo`, and avoid permission problems with global packages.
2. What is the difference between the global and the project-local CLI?
   - Global: used for `nest new`/`nest upgrade`. Local: in `devDependencies`, used by `npm run` scripts. They can be different versions.
3. Why does NestJS 12 have two different Node.js minimums?
   - Running the app needs `require(esm)` (20.19+/22.12+). The CLI's schematics inherit a higher floor from the Angular devkit.
4. Why use `npm ci` in CI?
   - It installs exactly what the lockfile says, fails if the lockfile is out of sync, and is faster and more reproducible.

### Advanced

1. How would you upgrade a v11 project to v12 safely?
   - Update the global CLI and schematics, create a branch, run `nest upgrade --dry-run`, review the report, run `nest upgrade`, then build, lint, and run tests. Check lifecycle hook ordering, `@Optional()` inheritance, and test-runner constraints by hand.
2. What does `nest upgrade` deliberately not do?
   - Migrate your project to ESM, Vitest, or oxlint. Those are defaults for new projects only.
3. Why can a CommonJS app consume ESM-only packages?
   - Through `require(esm)`, available unflagged on Node.js 20.19+/22.12+. Constraints appear in tools that do not support it (older Node.js, Jest below Node.js 24.9, some Lambda runtimes by default).
4. How do you make sure local, CI, and production runtimes match?
   - `.nvmrc` + `engines` + the same Node.js major in the Dockerfile and CI image, plus a committed lockfile.

---

## Quick Reference

```text
Run a Nest 12 app        Node.js 20.19+ or 22.12+
Use nest new/generate    Node.js 22.22.3+ / 24.15+ / 26+
Recommended              latest active LTS
Install CLI              npm i -g @nestjs/cli          (never sudo)
No global install        npx @nestjs/cli new <name>
Verify                   node --version · nest --version · nest info
Upgrade project          npm i -g @nestjs/cli@latest → nest upgrade --dry-run → nest upgrade
Pin runtime              .nvmrc + "engines" + Docker/CI image
Reproducible install     npm ci  (commit the lockfile)
Package managers         npm · yarn · pnpm · bun   (one per project)
Jest + Nest 12           needs Node.js 24.9+ (else ERR_REQUIRE_ASYNC_MODULE)
```

---

## Key Takeaways

- NestJS 12 needs Node.js 20.19+/22.12+ to **run** and a higher floor (22.22.3+/24.15+/26+) for the CLI's generators. Use the latest active LTS and you satisfy both.
- Install Node.js with a version manager, the CLI globally, and never use `sudo` for npm.
- The global CLI scaffolds and upgrades. The local CLI builds and runs. Keep both in mind when versions disagree.
- `nest info` is the first diagnostic command for any environment problem.
- Pin the toolchain (`.nvmrc`, `engines`, lockfile, Docker image) so every machine behaves the same.
- `nest upgrade` handles the mechanical parts of a major upgrade but does not change your module format or test runner.

---

## Related Topics

```text
00-prerequisites
      ↓
[01 Installation]
      ↓
02 Nest CLI  →  03 First Project
```

- [Getting Started Overview](./README.md)
- [Nest CLI](./02-nest-cli.md)
- [First Project](./03-first-project.md)
- [Development Environment](./05-development-environment.md)
- [Dependency Security](../07-production/01-security/07-dependency-security.md)
- [Docker](../07-production/04-deployment/03-docker.md)
