# Development Environment

A good development environment makes the right thing easy: code is formatted and linted automatically, environment variables load predictably, secrets stay out of Git, the Node.js version is pinned, and the same checks that run on your machine run in CI. This file sets up that environment for a NestJS 12 project: editor, linting with oxlint, formatting with Prettier, environment files, Git hygiene, local services, API clients, and a minimal CI pipeline.

---

## Overview

**What it is.** The tools and conventions around your code: editor and extensions, linter and formatter, environment-variable handling, version pinning, Git hooks, and local infrastructure (database, cache) for development.

**Why it exists.** Unreproducible environments cause "works on my machine" bugs, inconsistent style produces noisy diffs, and committed secrets cause breaches. A one-time setup prevents all three.

**Where it is used.** Every project and every team member, from first commit to CI.

**Why you should understand it.**

- NestJS 12 changed the defaults (oxlint instead of ESLint, Vitest for ESM projects), so older setup guides are partly out of date.
- Configuration management starts here. `.env` handling you adopt now carries into [03-configuration](../03-core-concepts/03-configuration/README.md).
- CI should run the same commands you run locally.

---

## Mental Model

```text
 You type code
      │
      ▼
 ┌───────────── Editor ─────────────┐
 │ TypeScript server  → type errors │   instant feedback
 │ oxlint extension   → lint hints  │
 │ Prettier on save   → formatting  │
 └───────────────┬──────────────────┘
                 │ git commit
                 ▼
 ┌──────── (optional) Git hooks ────┐
 │ lint-staged: format + lint       │   fast checks on changed files
 └───────────────┬──────────────────┘
                 │ git push
                 ▼
 ┌────────────── CI ────────────────┐
 │ npm ci → lint → typecheck →      │   authoritative checks
 │ build → unit tests → e2e tests   │
 └──────────────────────────────────┘

 Runtime configuration:   .env (local, ignored) ──► process.env ──► ConfigModule (validated)
```

Principle: **fast feedback locally, authoritative enforcement in CI, identical commands in both.**

---

## Core Concepts

### Editor Setup (VS Code as the Reference)

Any editor with a TypeScript language server works. VS Code is the common choice.

| Need | Extension / setting | Notes |
|---|---|---|
| TypeScript | Built in | Uses the workspace TypeScript if you select it (see below) |
| Formatting | Prettier extension | Format on save |
| Linting | The Oxc (oxlint) extension | *Verify the current extension ID in the marketplace* |
| Spell checking (optional) | Code Spell Checker | Catches typos in identifiers and comments |
| REST calls (optional) | REST Client, Thunder Client, or use Bruno/Postman | See below |
| EditorConfig | EditorConfig extension | Cross-editor whitespace rules |

Use the **workspace TypeScript version**, not the editor's bundled one, so editor diagnostics match `tsc`:

```jsonc
// .vscode/settings.json
{
  "typescript.tsdk": "node_modules/typescript/lib",
  "editor.formatOnSave": true,
  "editor.defaultFormatter": "esbenp.prettier-vscode",
  "[typescript]": { "editor.defaultFormatter": "esbenp.prettier-vscode" }
}
```

Commit `.vscode/settings.json` and `.vscode/extensions.json` (recommended extensions) so the team shares them. Do **not** commit personal settings or secrets.

### Linting With oxlint

NestJS 12 scaffolds **oxlint** for every generated project (ESM and CommonJS). It is a fast, Rust-based linter.

```bash
npm run lint
```

Linting finds likely bugs and rule violations (unused variables, suspicious patterns). Formatting does not. They are different tools with different jobs.

| Tool | Job | Command |
|---|---|---|
| **oxlint** | Find problems | `npm run lint` |
| **Prettier** | Normalize layout (quotes, wrapping, trailing commas) | `npm run format` |
| **TypeScript** | Check types | `npx tsc --noEmit` |

Important: oxlint does **not** replace the type checker. Run `tsc --noEmit` (or `nest build`) for type errors. Older projects still use ESLint, and the `nest upgrade` command deliberately does not migrate you to oxlint. The linter config file name and rule set come from your generated project. *Open it to see what is enabled.*

> Some typed lint rules that older Nest projects relied on through `typescript-eslint` (for example detecting floating promises) require type information. Check what your oxlint configuration covers, and keep a type-check step in CI. *Verify rule availability for your oxlint version.*

### Formatting With Prettier

Prettier formats code deterministically. Options live in `.prettierrc`.

```json
{
  "singleQuote": true,
  "trailingComma": "all"
}
```

The generated `format` script rewrites files in `src/` and `test/`. Commit `.prettierrc` so everyone formats identically. Let the editor format on save so the script is a safety net.

### Environment Variables and `.env` Files

Configuration that differs between environments (ports, database URLs, API keys) belongs in environment variables, not in code.

```text
.env                 # local values, NEVER committed
.env.example         # documented placeholders, committed
.env.test            # test-specific values (no real secrets)
```

Loading options:

| Approach | How | Use when |
|---|---|---|
| `nest start --env-file .env` | CLI option (new in v12) loads variables into the child process. Repeatable | Quick local runs |
| Node.js `--env-file` | `node --env-file=.env dist/main` | Plain `node` runs. *Verify availability on your Node.js version* |
| `@nestjs/config` `ConfigModule` | Loads, validates, and types configuration inside the app | **The standard approach for real applications** |
| Shell export | `export PORT=4000` | One-off overrides |

`ConfigModule` is covered in [Configuration Basics](../03-core-concepts/03-configuration/01-configuration-basics.md). Starting with NestJS 12, `@nestjs/config` validation uses the **Standard Schema** interface, so schema libraries such as Zod, Valibot, or ArkType work, and Joi works from version 18. See [Configuration Validation](../03-core-concepts/03-configuration/02-configuration-validation.md).

Rules for environment files:

- `.env` is always in `.gitignore`.
- `.env.example` lists every variable with a harmless placeholder and a comment.
- Variables are strings. Convert and validate them at startup (fail fast).
- Never log whole environment objects. They contain secrets.

### Version Pinning

| File | Purpose |
|---|---|
| `.nvmrc` / `.node-version` | Node.js version for version managers and CI |
| `package.json` `engines` | Declares supported Node.js range |
| `package.json` `packageManager` | Pins the package manager version (used by Corepack) |
| Lockfile | Pins exact dependency versions |

### Git Hygiene

Essentials:

```bash
git init                        # nest new does this unless --skip-git
git check-ignore -v .env dist node_modules    # confirm ignored paths
```

Commit conventions are a team choice. A common baseline is short imperative messages, small commits, and a protected main branch with required CI.

### Git Hooks (Optional)

Hooks run checks before a commit, catching problems early. Keep them **fast**: format and lint only staged files.

```bash
npm i -D husky lint-staged
npx husky init
```

```json
{
  "lint-staged": {
    "*.ts": ["prettier --write", "oxlint"]
  }
}
```

```bash
# .husky/pre-commit
npx lint-staged
```

*Package CLI details change between releases. Follow the current Husky and lint-staged documentation.* Hooks are advisory, because developers can skip them (`--no-verify`). **CI is the enforcement point.**

### Local Infrastructure

Most backends need a database or cache while you develop. Use containers so everyone has the same versions.

```yaml
# docker-compose.yml (development only)
services:
  postgres:
    image: postgres:17
    environment:
      POSTGRES_USER: app
      POSTGRES_PASSWORD: app
      POSTGRES_DB: app
    ports:
      - "5432:5432"
    volumes:
      - pgdata:/var/lib/postgresql/data
  redis:
    image: redis:7
    ports:
      - "6379:6379"
volumes:
  pgdata:
```

```bash
docker compose up -d
docker compose ps
docker compose down          # keep data
docker compose down -v       # also delete volumes
```

These credentials are for local development only. Never reuse them anywhere reachable from a network. Adjust image versions to what you run in production. Database integration is covered from [Database Foundations](../04-intermediate/02-database-foundations/README.md) onward.

### API Clients

| Tool | Notes |
|---|---|
| `curl` / `httpie` | Scriptable, great for documentation |
| Postman, Insomnia, Bruno | GUI clients. Bruno stores collections as files that you can commit |
| VS Code REST Client (`.http` files) | Requests live next to code and are versionable |
| Swagger UI | Generated from your code once you add OpenAPI. See [Swagger Setup](../04-intermediate/09-openapi-and-swagger/01-swagger-setup.md) |

Example `.http` file:

```http
### Health
GET http://localhost:3000/status

### Echo
POST http://localhost:3000/echo
Content-Type: application/json

{ "name": "Ada" }
```

Do not commit collections that contain tokens or real credentials.

### Continuous Integration Skeleton

CI runs the same commands as your local workflow, from a clean checkout.

```yaml
# .github/workflows/ci.yml (GitHub Actions example)
name: ci
on: [push, pull_request]
jobs:
  verify:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-node@v4
        with:
          node-version-file: .nvmrc
          cache: npm
      - run: npm ci
      - run: npm run lint
      - run: npx tsc --noEmit -p tsconfig.build.json
      - run: npm run build
      - run: npm test
      - run: npm run test:e2e
```

*Action versions shown are illustrative. Check the current versions.* Match the Node.js version to your `.nvmrc` and your production image. A full pipeline (images, deployment) is in [CI/CD](../07-production/04-deployment/05-ci-cd.md).

---

## How It Works

Order of operations when you save a file with the setup above:

```text
 Save file
    │
    ├─ Editor: Prettier formats on save
    ├─ Editor: TypeScript server re-checks types → red squiggles
    ├─ Editor: oxlint extension re-lints → warnings
    └─ nest start --watch: rebuilds and restarts the process

 git commit
    └─ (optional) pre-commit hook: lint-staged formats + lints staged files

 git push
    └─ CI: npm ci → lint → typecheck → build → tests  (authoritative)
```

Where environment variables come from at runtime:

```text
 shell exports / CI secrets / container env     ← highest priority in production
            │
            ▼
 --env-file (nest start / node)   or   ConfigModule's .env loading
            │
            ▼
 process.env  ──►  ConfigModule (validate + type)  ──►  injected ConfigService
```

---

## Basic Example

Set up a new project end to end:

```bash
nest new my-api --package-manager npm --no-observe
cd my-api

# 1. Pin the runtime
node --version > .nvmrc

# 2. Environment files
printf 'PORT=3000\nNODE_ENV=development\n' > .env
cp .env .env.example
git check-ignore -v .env || echo ".env is NOT ignored - fix .gitignore"

# 3. Editor settings (VS Code)
mkdir -p .vscode
cat > .vscode/settings.json <<'EOF'
{
  "typescript.tsdk": "node_modules/typescript/lib",
  "editor.formatOnSave": true,
  "editor.defaultFormatter": "esbenp.prettier-vscode"
}
EOF

# 4. Run the checks once on the pristine project
npm run lint
npm run format
npm run build
npm test

# 5. Start developing with the env file
npx nest start --watch --env-file .env

# 6. First commit
git add -A && git commit -m "chore: initial Nest project and environment setup"
```

What each step gives you:

1. `.nvmrc` makes version managers and CI use the same Node.js.
2. `.env` for local values (ignored), `.env.example` for documentation (committed).
3. Workspace TypeScript and format-on-save make the editor consistent with the build.
4. A green baseline: you will know immediately when a later change breaks something.
5. `--env-file` loads variables for the dev run.
6. A clean first commit separates scaffolding from your work.

---

## Practical Examples

### 1. Basic: Add `engines` and Enforce It

```json
{
  "engines": { "node": ">=20.19.0" }
}
```

```ini
# .npmrc
engine-strict=true
```

With `engine-strict`, `npm install` fails on an unsupported Node.js version instead of warning. Remember that the CLI's generators need a higher Node.js version than running the app does.

### 2. Common: A Documented `.env.example`

```ini
# Server
PORT=3000
NODE_ENV=development

# Database (local docker compose)
DATABASE_URL=postgres://app:app@localhost:5432/app

# Auth (generate with: openssl rand -base64 48)
JWT_SECRET=change-me

# Third-party (leave empty locally unless testing the integration)
STRIPE_API_KEY=
```

Every variable the app reads appears here. New developers copy the file to `.env` and fill the blanks.

### 3. Common: Load Different Files per Environment

```bash
nest start --watch --env-file .env --env-file .env.local
```

The option can be repeated. *Check the precedence rules for your CLI version before relying on overrides.* In application code, prefer `ConfigModule` with `envFilePath` and validation.

### 4. Real-World: Pre-Commit Checks With Husky and lint-staged

```bash
npm i -D husky lint-staged
npx husky init
echo "npx lint-staged" > .husky/pre-commit
```

```json
{
  "scripts": { "prepare": "husky" },
  "lint-staged": { "*.{ts,json,md}": "prettier --write", "*.ts": "oxlint" }
}
```

Keep hooks under a few seconds. Slow hooks get bypassed.

### 5. Real-World: A One-Command Local Setup

```json
{
  "scripts": {
    "dev:up": "docker compose up -d",
    "dev:down": "docker compose down",
    "dev": "nest start --watch --env-file .env",
    "check": "npm run lint && tsc --noEmit -p tsconfig.build.json && npm test"
  }
}
```

`npm run check` runs locally what CI runs, which shortens the feedback loop. Keep script names and CI steps in sync.

### 6. Real-World: Developer Container (Optional)

A `.devcontainer/devcontainer.json` gives every developer an identical container with Node.js, extensions, and ports preconfigured. Useful for onboarding and for cloud IDEs. Keep the Node.js version aligned with `.nvmrc` and production. *Follow the current Dev Containers specification for the file format.*

### 7. Edge Case: `.env` Was Already Committed

```bash
git rm --cached .env
echo ".env" >> .gitignore
git commit -m "chore: stop tracking .env"
```

This stops tracking the file but **does not remove it from history**. Treat every value that was ever committed as compromised: rotate the secrets. Then, if the repository is private and a history rewrite is acceptable, use a history-rewriting tool. Prefer rotation over cleanup as the primary fix.

### 8. Edge Case: Editor and CI Disagree About Types

Symptoms: the editor shows no errors but CI fails (or the reverse). Causes: different TypeScript versions (editor bundled vs workspace), editor reading `tsconfig.json` while the build reads `tsconfig.build.json`, or SWC building without type checking. Fix: set `typescript.tsdk` to the workspace TypeScript, run `npx tsc --noEmit -p tsconfig.build.json` locally, and keep the same command in CI.

---

## Syntax / API / Commands

| Command | Purpose |
|---|---|
| `npm run lint` | Run oxlint |
| `npm run format` | Run Prettier on source and tests |
| `npx tsc --noEmit -p tsconfig.build.json` | Type-check what ships |
| `npm run build` | Compile |
| `npm test` / `npm run test:e2e` | Unit and e2e tests |
| `nest start --watch --env-file .env` | Dev run with env file |
| `git check-ignore -v .env` | Is `.env` ignored? |
| `node --version > .nvmrc` | Pin Node.js |
| `docker compose up -d` / `down` | Local services |
| `npm ci` | Clean, lockfile-exact install (CI) |
| `npm audit` | Known-vulnerability report |
| `npm outdated` | Outdated dependencies |

| File | Commit it? |
|---|---|
| `.env` | **No** |
| `.env.example` | Yes |
| `.nvmrc`, `.prettierrc`, linter config, `.editorconfig` | Yes |
| `.vscode/settings.json`, `.vscode/extensions.json` | Yes (shared, non-personal) |
| Lockfile | Yes |
| `dist/`, `node_modules/`, `coverage/` | No |

---

## Important Rules

1. **Never commit secrets.** `.env` is always ignored. If one leaks, rotate it.
2. **Commit `.env.example`** with every variable documented.
3. **Run the same checks locally and in CI.** CI is the authority, hooks are a convenience.
4. **Linting does not replace type checking.** Keep `tsc --noEmit` in the pipeline.
5. **Use the workspace TypeScript** in the editor.
6. **Pin Node.js and the package manager** (`.nvmrc`, `engines`, `packageManager`).
7. **Use `npm ci` in CI**, with a committed lockfile.
8. **Keep hooks fast.** A slow hook gets disabled.
9. **Environment values are strings.** Validate and convert at startup, and fail fast on bad configuration.
10. **Local infrastructure credentials are for local use only.**
11. **One formatter, one linter, one config per repo.** No per-developer style.
12. **Do not commit personal editor settings.**

---

## Under the Hood

### Why oxlint and Prettier Are Separate

The linter analyzes code for problems. The formatter rewrites layout. Keeping them separate avoids rule conflicts and lets each be the fastest at its job. Nest 12 scaffolds oxlint for speed and Prettier for formatting.

### `--env-file`

For `nest start`, the CLI reads the specified files and exposes the variables to the **child process** it spawns, so they appear on `process.env` in your application. Node.js also supports an `--env-file` flag for direct runs. Variables already present in the environment typically win over file values. *Check precedence on your Node.js and CLI versions.*

### How `ConfigModule` Relates to `.env`

`ConfigModule` loads `.env`, merges with `process.env`, validates against a schema, and exposes typed values through `ConfigService`. This is a layer above the raw `process.env` handling. Using it means your code reads `configService.get('PORT')` instead of scattering `process.env.PORT`.

### CI Cache Keys

Package-manager caches are keyed by the lockfile hash. A changed lockfile invalidates the cache and triggers a fresh download. This is another reason to commit the lockfile and avoid unrelated lockfile churn.

---

## Common Patterns

### The Triple Check

`lint` + `typecheck` + `test`, wrapped in one script (`npm run check`), run locally before pushing and in CI.

### Pinned Toolchain

`.nvmrc` + `engines` + `packageManager` + lockfile + Docker image with the same Node.js major.

### Environment Layering

Defaults in code or config schema → `.env` for local → real environment variables in CI and production. The more specific and secure the environment, the more the secrets come from the platform (secret manager), not files.

### Compose for Dependencies, Native Process for the App

Run Postgres and Redis in Docker Compose, and run the Nest app natively with watch mode for the fastest edit loop.

### Shared Editor Configuration

`.editorconfig`, `.vscode/settings.json`, and `.vscode/extensions.json` committed so editors behave the same.

---

## Common Mistakes

| Mistake | Symptom | Why It Happens | Fix |
|---|---|---|---|
| Committing `.env` | Secrets in Git history | Not in `.gitignore` | Ignore it, rotate the secrets |
| No `.env.example` | New developers cannot start the app | Required variables undocumented | Add and maintain it |
| Editor uses a different TypeScript version | Editor errors differ from build errors | Bundled TS vs workspace TS | `typescript.tsdk` → `node_modules/typescript/lib` |
| Relying on lint for type errors | Type errors ship | oxlint is not a type checker | Add `tsc --noEmit` to CI |
| Different Node.js versions locally and in CI/production | Works locally, fails in CI | Unpinned runtime | `.nvmrc`, `engines`, same Docker base image |
| `npm install` in CI | Lockfile changes, non-reproducible builds | `install` can update the lockfile | Use `npm ci` |
| Slow pre-commit hooks | Developers use `--no-verify` | Hooks run the full test suite | Only format and lint staged files |
| Forgetting to restart after changing `.env` | Old values used | Env is read at process start | Restart the process |
| Environment values treated as numbers/booleans | `'false'` is truthy, `'3000'` is a string | All env values are strings | Convert and validate with a schema |
| Reusing local Docker credentials elsewhere | Insecure services | Convenience | Use unique secrets per environment |
| Two formatters fighting (editor vs Prettier script) | Constant reformatting diffs | Different configs or defaults | One formatter, committed config |
| `docker compose down -v` deleting data unexpectedly | Lost local database | `-v` removes volumes | Use `down` normally, `-v` deliberately |
| Not ignoring `.env.local`, `.env.*.local` | Secrets leak via a differently named file | Only `.env` ignored | Ignore the whole family except `.env.example` |
| Mixing package managers across the team | Lockfile churn | Different tools used | Standardize and enforce via `packageManager` |

---

## Debugging

### Common Problems

| Symptom | Likely cause |
|---|---|
| Format-on-save does nothing | Wrong default formatter, Prettier extension disabled, or no Prettier config |
| Lint extension shows nothing | Extension not installed or not pointed at the config |
| `process.env.X` is `undefined` | File not loaded, wrong path, variable name typo, process not restarted |
| Port from `.env` ignored | An exported shell variable or CI env overrides, or the file was not loaded |
| CI fails, local passes | Different Node.js version, stale `node_modules`, unpinned dependency, or missing type-check locally |
| `husky` hook not running | `prepare` script not run, hooks path not configured, or hooks disabled |
| Docker Compose ports busy | Another Postgres/Redis already on the same port |

### Commands

```bash
node --version; npm --version; cat .nvmrc
git check-ignore -v .env .env.local          # confirm ignores
git ls-files | grep -E '(^|/)\.env' ; true   # accidentally tracked env files?
node -e "console.log(process.env.PORT)"      # value as Node.js sees it (after exporting)
env | grep -i '^PORT='                       # leaked shell variable overriding .env?
docker compose ps && docker compose logs --tail=50 postgres
lsof -i :5432                                 # who holds the DB port? (macOS/Linux)
npm ci && npm run lint && npx tsc --noEmit -p tsconfig.build.json && npm test   # reproduce CI locally
```

### Techniques

1. **Reproduce CI locally** with a clean install (`rm -rf node_modules && npm ci`) and the same Node.js version.
2. **Print effective config** (`tsc --showConfig`, the app's config at startup with secrets redacted).
3. **Check for shadowing:** a variable exported in the shell wins over a file in most setups.
4. **Bisect tooling issues** by running the command in the terminal first, then in the editor.
5. **Check which TypeScript the editor uses** (command palette: "TypeScript: Select TypeScript Version").

---

## Performance

| Concern | Guidance |
|---|---|
| Slow editor | Use the workspace TypeScript, exclude `dist/` and `node_modules/` from watchers and search |
| Slow rebuilds | `nest start -b swc`. Keep `incremental` on |
| Slow lint | oxlint is designed to be fast. Lint only changed files in hooks |
| Slow CI | Cache the package manager store, run lint/typecheck/test jobs in parallel, avoid reinstalling |
| Slow hooks | Format and lint staged files only. Never run the full suite in a hook |
| Docker on macOS/Windows | Bind-mounting `node_modules` into containers can be slow. Run the app natively and only the dependencies in Docker |

---

## Security

- **Secrets never go in Git, images, logs, or chat.** Use `.env` locally (ignored) and a secret manager or platform secrets in deployed environments. See [Secrets Management](../07-production/01-security/06-secrets-management.md).
- **If a secret is committed, rotate it.** Deleting the file does not remove it from history.
- **Scan for secrets.** Consider a secret scanner in CI and as a pre-commit check.
- **Dependencies are an attack surface.** Commit lockfiles, run `npm audit` in CI, review new dependencies, and consider `--ignore-scripts` for untrusted installs. See [Dependency Security](../07-production/01-security/07-dependency-security.md).
- **Do not expose local services.** Bind databases and the inspector to `127.0.0.1`. Do not publish ports to `0.0.0.0` on shared networks.
- **Hooks and scripts execute code.** Review `prepare`, `postinstall`, and hook scripts in cloned repositories before running installs.
- **Do not commit API client collections with live tokens.**

---

## Production Considerations

- **Parity:** keep the Node.js version, package manager, and key dependency versions consistent from laptop to CI to production.
- **Configuration:** production configuration comes from the platform (environment variables, secret manager), validated at startup. `.env` files are a development convenience.
- **CI as the gatekeeper:** required status checks on the main branch (lint, typecheck, build, tests).
- **Build once:** build the artifact in CI and deploy that artifact. Do not rebuild per environment.
- **Reproducibility:** `npm ci`, pinned base images, lockfile committed.
- **Dependency hygiene:** scheduled updates (Renovate/Dependabot) so upgrades are small and routine.
- **Onboarding:** a README section with exact setup commands, tested by someone new at least once.

See [Production Configuration](../07-production/04-deployment/01-production-configuration.md) and [CI/CD](../07-production/04-deployment/05-ci-cd.md).

---

## Best Practices

### Recommended

```bash
node --version > .nvmrc
cp .env.example .env            # local only, ignored by Git
npm run lint && npx tsc --noEmit -p tsconfig.build.json && npm test
npm ci                          # in CI
```

```jsonc
// .vscode/settings.json
{
  "typescript.tsdk": "node_modules/typescript/lib",
  "editor.formatOnSave": true
}
```

### Avoid

```bash
git add .env                    # committing secrets
npm install                     # in CI, may modify the lockfile
# relying on lint alone for correctness, with no type check
# a pre-commit hook that runs the whole test suite
```

Why: committed secrets are compromised secrets, `npm ci` is reproducible while `npm install` is not, linting does not check types, and slow hooks get skipped.

Additional guidance:

- Run the full set of checks once on the pristine generated project. A known-green baseline makes later failures meaningful.
- Add `.env.example` and update it in the same commit as any new variable.
- Keep tooling configuration in the repository, not in personal editor settings.
- Review the generated linter and Prettier configs. They are starting points, not rules you must keep.
- Let CI be strict even if local hooks are lenient.

---

## Version / Compatibility Notes

| Item | NestJS 12 new project | Earlier |
|---|---|---|
| Linter | oxlint (both module systems) | ESLint with `typescript-eslint` |
| Formatter | Prettier | Prettier |
| Test runner | Vitest (ESM), Jest (CommonJS) | Jest |
| `nest start --env-file` | Available (new in v12 CLI) | Not available |
| `@nestjs/config` validation | Standard Schema (Zod, Valibot, ArkType, Joi 18+). Library-specific options go under `validationOptions.libraryOptions` | Joi-centric |
| `nest new` Observe prompt | Present, defaults to yes in interactive terminals | Not present |
| TypeScript | v6 | 5.x |
| Existing projects | Keep ESLint/Jest unless you migrate. `nest upgrade` does not change them | n/a |

*Verify the exact editor extension IDs, Husky and lint-staged command syntax, and GitHub Actions versions against their current documentation. These tools change faster than the framework.*

---

## Real-World Use Cases

- **Team onboarding:** clone, `nvm use`, `npm ci`, `cp .env.example .env`, `docker compose up -d`, `npm run dev`.
- **Pull request quality gates:** the CI skeleton above as required checks.
- **Consistent style across many repositories:** shared Prettier and linter configs published as internal packages.
- **Secure local development:** disposable containers for databases, `.env` never committed, secrets scanner in CI.
- **Cloud IDEs and dev containers:** reproducible environments without local installation.

---

## Interview Questions

### Beginner

1. Why shouldn't `.env` be committed?
   - It usually contains secrets. Commit `.env.example` instead.
2. What is the difference between a linter and a formatter?
   - A linter finds likely bugs and rule violations. A formatter normalizes layout.
3. What do `npm run lint` and `npm run format` do in a Nest project?
   - Run oxlint and Prettier respectively.

### Intermediate

1. Why keep `tsc --noEmit` in CI if you already lint?
   - Linters are not type checkers. Type errors need the compiler (and SWC builds do not check types by default).
2. How do you make sure everyone uses the same Node.js version?
   - `.nvmrc`, `engines` in `package.json`, the same version in Docker and CI, optionally `engine-strict`.
3. How do environment variables reach a Nest app in development?
   - Through the shell, `nest start --env-file`, Node's `--env-file`, or `ConfigModule` loading `.env`, ending up on `process.env` and then in `ConfigService`.
4. Why use `npm ci` in CI?
   - It installs exactly the lockfile versions and fails if they do not match `package.json`, which makes builds reproducible.

### Advanced

1. A secret was committed to a branch that has been pushed. What do you do?
   - Rotate the secret immediately, remove it from the working tree and ignore it, and optionally rewrite history. Treat it as compromised regardless of cleanup.
2. Why are Git hooks not a security or quality guarantee?
   - They run locally and can be skipped with `--no-verify`. CI must enforce the same checks.
3. How would you keep local development fast with Docker?
   - Run only dependencies (Postgres, Redis) in containers, run the app natively with watch mode, and avoid bind-mounting `node_modules`.
4. What are the trade-offs of moving from ESLint to oxlint?
   - Much faster linting and a simpler scaffold, but you must check rule coverage (especially type-aware rules) and keep a separate type check.
5. How do you design configuration so production secrets never touch files?
   - Validate configuration through `ConfigModule` against a schema, supply values through the platform's environment or secret manager, and keep `.env` as a development-only convenience.

---

## Quick Reference

```text
Pin runtime       .nvmrc · engines · packageManager · lockfile · same Docker image
Env files         .env (ignored) · .env.example (committed) · validate with ConfigModule
Load env          nest start --env-file .env  ·  node --env-file  ·  ConfigModule
Checks            npm run lint · npx tsc --noEmit -p tsconfig.build.json · npm run build · npm test
Lint vs format    oxlint finds problems · Prettier formats · tsc checks types
Editor            workspace TypeScript · format on save · shared .vscode settings
Hooks             husky + lint-staged: fast, staged files only · CI is the authority
CI                npm ci → lint → typecheck → build → test → e2e
Local services    docker compose up -d (dev credentials only)
Never commit      .env · dist/ · node_modules/ · secrets · personal editor settings
Leaked secret     rotate first, clean history second
```

---

## Key Takeaways

- A reproducible environment is pinned (Node.js, package manager, lockfile) and documented (`.env.example`, README).
- NestJS 12 scaffolds **oxlint** and **Prettier**. Linting, formatting, and type checking are three separate jobs.
- Keep `.env` out of Git, document every variable in `.env.example`, and validate configuration at startup.
- Use the workspace TypeScript in the editor so it agrees with the build.
- Local checks give fast feedback. CI enforces the same checks authoritatively.
- Run databases and caches in Docker Compose for development, and run the app natively in watch mode.
- If a secret is committed, rotate it. Do not rely on deleting the file.

---

## Related Topics

```text
04 Project Structure
      ↓
[05 Development Environment]
      ↓
06 Debugging  →  02-fundamentals
```

- [Getting Started Overview](./README.md)
- [Installation](./01-installation.md)
- [Project Structure](./04-project-structure.md)
- [Debugging](./06-debugging.md)
- [Configuration Basics](../03-core-concepts/03-configuration/01-configuration-basics.md)
- [Configuration Validation](../03-core-concepts/03-configuration/02-configuration-validation.md)
- [Secrets Management](../07-production/01-security/06-secrets-management.md)
- [CI/CD](../07-production/04-deployment/05-ci-cd.md)
