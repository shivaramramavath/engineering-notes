# Project Structure

A folder structure is a **map**. A good one lets a new contributor find where code lives without asking, makes the right place for new code obvious, and scales from 10 files to 1,000. There is no single correct layout, but a few principles and proven templates cover most projects.

See also: [Modules](../13_modules/00_README.md), [Module Patterns](../13_modules/05_module-patterns.md), [Dependency Injection](../20_design-patterns/09_dependency-injection.md), [Tooling](../00_setup/05_tooling.md).

## Principles

| Principle                                    | Meaning                                                                                                       |
| -------------------------------------------- | ------------------------------------------------------------------------------------------------------------- |
| **Separate source, config, and output**      | Code in `src/`, tool config at the root, generated files in `dist/` (gitignored)                              |
| **Group by feature as you grow**             | Keep everything about "users" together instead of scattering it across `controllers/`, `models/`, `services/` |
| **One place for each cross-cutting concern** | One config module, one logger, one error type, one place that wires the app together                          |
| **Dependencies point one way**               | Routes call services, services call repositories. Never the reverse                                           |
| **Keep the root tidy**                       | Only entry docs and config at the top level                                                                   |
| **Co-locate tests** (or mirror the tree)     | A test should be easy to find from the code it covers                                                         |
| **Do not over-nest**                         | Three or four levels is usually enough. Deep trees hide code                                                  |
| **Start flat, split when it hurts**          | Do not create 12 folders for 3 files                                                                          |

## The root of (almost) every project

```
my-project/
├── .github/
│   └── workflows/
│       └── ci.yml              # lint, test, build on every push and PR
├── .husky/                     # git hooks (see 03_git-hooks-and-commits.md)
│   ├── pre-commit
│   └── commit-msg
├── .vscode/
│   ├── extensions.json         # recommended extensions
│   └── settings.json           # shared editor settings (format on save)
├── docs/                       # architecture notes, ADRs, guides
├── scripts/                    # one-off or maintenance scripts (seed, migrate, release)
├── src/                        # application source
├── test/                       # integration and end-to-end tests, helpers
├── .editorconfig               # indentation, line endings
├── .env.example                # committed template: every variable, no secrets
├── .gitignore
├── .npmrc                      # package manager settings (engine-strict, etc.)
├── .nvmrc                      # Node version for nvm, fnm, CI
├── .prettierrc                 # formatting rules
├── .prettierignore
├── commitlint.config.js
├── eslint.config.js            # ESLint flat config
├── package.json
├── package-lock.json           # commit it (or pnpm-lock.yaml / yarn.lock)
├── README.md
├── CHANGELOG.md                # optional: generated from commits
├── LICENSE
└── tsconfig.json               # only if you use TypeScript or type-check JS
```

| File                                            | Purpose                                             | Commit it?                               |
| ----------------------------------------------- | --------------------------------------------------- | ---------------------------------------- |
| `package.json`                                  | Metadata, dependencies, scripts, entry points       | Yes                                      |
| Lockfile                                        | Exact dependency versions for reproducible installs | **Yes** (applications and libraries' CI) |
| `.env`                                          | Real local values and secrets                       | **No**                                   |
| `.env.example`                                  | Names and example values of every variable          | **Yes**                                  |
| `.nvmrc` / `engines`                            | Required Node version                               | Yes                                      |
| `dist/`, `build/`, `coverage/`, `node_modules/` | Generated or installed                              | **No**                                   |
| `.editorconfig`                                 | Editor-neutral formatting basics                    | Yes                                      |
| `.husky/`                                       | Shared git hooks                                    | Yes                                      |
| `.vscode/settings.json`                         | Shared editor settings (keep personal ones out)     | Yes (minimal)                            |

### A solid `.gitignore`

```gitignore
# dependencies
node_modules/

# build output
dist/
build/
coverage/
*.tsbuildinfo

# environment and secrets
.env
.env.*
!.env.example

# logs and temp
*.log
npm-debug.log*
.tmp/
.cache/

# OS and editor
.DS_Store
Thumbs.db
.idea/
.vscode/*
!.vscode/settings.json
!.vscode/extensions.json
```

### `.nvmrc`, `.npmrc`, and `engines`

```
# .nvmrc
22
```

```ini
# .npmrc
engine-strict=true       # fail install on an unsupported Node version
save-exact=false         # set true to pin exact versions on install
```

```json
{ "engines": { "node": ">=20" }, "packageManager": "npm@10.9.0" }
```

`packageManager` is read by Corepack so every contributor uses the same package manager version.

## Backend API: feature-based layout (recommended)

```
my-api/
├── src/
│   ├── index.js                  # entry point: loads env, builds the app, calls listen()
│   ├── app.js                    # createApp(deps): wires middleware and routes, no listen()
│   │
│   ├── config/
│   │   └── env.js                # the ONLY file that reads process.env (validated)
│   │
│   ├── lib/                      # thin wrappers around third-party clients
│   │   ├── logger.js             # pino instance
│   │   ├── db.js                 # database pool
│   │   └── redis.js
│   │
│   ├── middleware/               # cross-cutting HTTP concerns
│   │   ├── error-handler.js
│   │   ├── not-found.js
│   │   ├── request-id.js
│   │   ├── authenticate.js
│   │   └── validate.js           # schema validation middleware
│   │
│   ├── modules/                  # one folder per business feature
│   │   ├── users/
│   │   │   ├── users.routes.js       # URL → controller wiring
│   │   │   ├── users.controller.js   # HTTP in/out: parse request, call service, shape response
│   │   │   ├── users.service.js      # business rules (no HTTP, no SQL)
│   │   │   ├── users.repository.js   # data access (SQL / ORM calls)
│   │   │   ├── users.schema.js       # zod schemas for requests and responses
│   │   │   ├── users.test.js         # unit tests next to the code
│   │   │   └── index.js              # public API of the module
│   │   └── orders/
│   │       └── ...
│   │
│   └── shared/                   # code used by several modules
│       ├── errors.js             # AppError, NotFoundError, ValidationError
│       └── pagination.js
│
├── test/
│   ├── integration/              # HTTP and database tests
│   ├── helpers/                  # builders, fakes, test server setup
│   └── setup.js
├── scripts/
│   ├── seed.js
│   └── migrate.js
├── migrations/                   # database migrations (SQL or ORM-generated)
└── ...root files from above
```

### What each layer is responsible for

| Layer          | Knows about                                    | Must not                                                |
| -------------- | ---------------------------------------------- | ------------------------------------------------------- |
| **Routes**     | URLs, HTTP methods, which middleware to run    | Contain logic                                           |
| **Controller** | `req` and `res`, status codes                  | Contain business rules or SQL                           |
| **Service**    | Business rules, orchestration                  | Import Express or touch `req`/`res`; write SQL directly |
| **Repository** | Database queries, ORM, mapping rows to objects | Know about HTTP or business policy                      |
| **Schema**     | Shape and validation of data                   | Contain behavior                                        |

Dependencies flow one way: **routes → controller → service → repository**. A service can be reused by an HTTP route, a CLI script, or a queue worker precisely because it does not know about HTTP.

### Layer-based vs feature-based

```
Layer-based (small apps)          Feature-based (growing apps)
src/                              src/modules/
├── controllers/                  ├── users/
│   ├── users.js                  │   ├── users.controller.js
│   └── orders.js                 │   ├── users.service.js
├── services/                     │   └── users.repository.js
│   ├── users.js                  └── orders/
│   └── orders.js                     ├── orders.controller.js
└── repositories/                     ├── orders.service.js
    ├── users.js                      └── orders.repository.js
    └── orders.js
```

|                                      | Layer-based                    | Feature-based                  |
| ------------------------------------ | ------------------------------ | ------------------------------ |
| Finding everything about one feature | Open 3+ folders                | One folder                     |
| Deleting or extracting a feature     | Hunt across folders            | Move one folder                |
| Team ownership                       | Awkward                        | Natural (one team, one module) |
| Small projects                       | Fine                           | Slight overhead                |
| Large projects                       | Folders become dumping grounds | Scales well                    |

Rule of thumb: start layer-based for a tiny service; move to feature-based once you have more than a couple of features.

### The entry point and the app factory

Separate **creating** the app from **starting** it. Tests import `createApp`; only `index.js` listens on a port ([Integration Testing](../21_testing/03_integration-testing.md)).

```js
// src/config/env.js: see 02_environment-variables.md
import { cleanEnv, str, port, url } from "envalid";

export const env = cleanEnv(process.env, {
  NODE_ENV: str({
    choices: ["development", "test", "production"],
    default: "development",
  }),
  PORT: port({ default: 3000 }),
  DATABASE_URL: url(),
  LOG_LEVEL: str({
    choices: ["debug", "info", "warn", "error"],
    default: "info",
  }),
});
```

```js
// src/app.js
import express from "express";
import helmet from "helmet";
import cors from "cors";
import { requestId } from "./middleware/request-id.js";
import { notFound } from "./middleware/not-found.js";
import { errorHandler } from "./middleware/error-handler.js";
import { createUsersRouter } from "./modules/users/index.js";

export function createApp({ logger, db }) {
  const app = express();

  app.use(helmet());
  app.use(cors());
  app.use(express.json({ limit: "100kb" }));
  app.use(requestId);

  app.get("/health", (req, res) => res.json({ ok: true }));
  app.use("/users", createUsersRouter({ db, logger }));

  app.use(notFound);
  app.use(errorHandler({ logger }));
  return app;
}
```

```js
// src/index.js: the composition root: the only place that wires real dependencies
import { env } from "./config/env.js";
import { logger } from "./lib/logger.js";
import { createDb } from "./lib/db.js";
import { createApp } from "./app.js";

const db = await createDb(env.DATABASE_URL);
const app = createApp({ logger, db });

const server = app.listen(env.PORT, () =>
  logger.info({ port: env.PORT }, "server started"),
);

for (const signal of ["SIGINT", "SIGTERM"]) {
  process.on(signal, () => {
    logger.info({ signal }, "shutting down");
    server.close(async () => {
      await db.end();
      process.exit(0);
    });
  });
}
```

Dependencies are passed in rather than imported inside modules, which keeps modules testable ([Dependency Injection](../20_design-patterns/09_dependency-injection.md)).

### A module's public surface

Other modules import from `modules/users/index.js`, never from deep inside:

```js
// modules/users/index.js
export { createUsersRouter } from "./users.routes.js";
export { createUsersService } from "./users.service.js";
// repository and controller stay internal
```

```js
// good: through the public entry
import { createUsersService } from "../users/index.js";
// bad: reaching into internals
import { findByEmail } from "../users/users.repository.js";
```

This is the [module pattern](../20_design-patterns/01_module-pattern.md) at folder scale. Keep these barrels **small and intentional**. Large "export everything" barrels slow builds and cause circular imports.

## Frontend (Vite) layout

```
my-web/
├── index.html
├── public/                       # static files served as-is (favicon, robots.txt)
├── src/
│   ├── main.js                   # entry: mounts the app
│   ├── app/                      # app shell: routes, providers, layout
│   ├── features/                 # one folder per feature
│   │   ├── auth/
│   │   │   ├── components/
│   │   │   ├── hooks/
│   │   │   ├── api.js            # network calls for this feature
│   │   │   ├── store.js
│   │   │   └── index.js
│   │   └── cart/
│   ├── components/               # shared, feature-agnostic UI (Button, Modal)
│   ├── lib/                      # http client, date helpers, formatters
│   ├── styles/
│   ├── assets/                   # images and fonts imported by code
│   └── config/
│       └── env.js                # reads and validates import.meta.env
├── test/
├── vite.config.js
└── ...root files
```

Everything exposed through `import.meta.env.VITE_*` is **embedded in the public bundle**. Never put secrets there. See [Environment Variables](./02_environment-variables.md).

## Reusable library (published to npm)

```
my-lib/
├── src/
│   ├── index.js                  # public API: only what you export here is supported
│   └── internal/                 # private helpers
├── test/
├── dist/                         # build output (gitignored, published)
├── package.json
├── README.md
├── CHANGELOG.md
└── LICENSE
```

```json
{
  "name": "my-lib",
  "version": "1.0.0",
  "type": "module",
  "exports": {
    ".": {
      "types": "./dist/index.d.ts",
      "import": "./dist/index.js",
      "require": "./dist/index.cjs"
    },
    "./package.json": "./package.json"
  },
  "files": ["dist"],
  "sideEffects": false,
  "engines": { "node": ">=20" },
  "scripts": {
    "build": "tsup src/index.js --format esm,cjs --dts",
    "prepublishOnly": "npm run lint && npm test && npm run build"
  }
}
```

| Field                           | Why it matters                                                                    |
| ------------------------------- | --------------------------------------------------------------------------------- |
| `exports`                       | Defines the **only** importable entry points (blocks deep imports into internals) |
| `files`                         | Whitelist of what gets published; keeps tests and config out of the package       |
| `sideEffects: false`            | Lets bundlers tree-shake unused exports                                           |
| `types` first in the conditions | TypeScript reads conditions in order                                              |

Check what will be published with `npm pack --dry-run`. See [ESM vs CommonJS](../13_modules/03_esm-vs-commonjs.md).

## CLI tool

```
my-cli/
├── bin/
│   └── my-cli.js                 # thin entry with a shebang
├── src/
│   ├── cli.js                    # builds the command tree
│   ├── commands/
│   │   ├── init.js
│   │   └── build.js
│   └── lib/
├── test/
└── package.json
```

```js
#!/usr/bin/env node
// bin/my-cli.js
import { run } from "../src/cli.js";

run(process.argv.slice(2)).catch((err) => {
  console.error(err.message);
  process.exitCode = 1;
});
```

```json
{ "bin": { "my-cli": "./bin/my-cli.js" }, "files": ["bin", "src"] }
```

Keep `bin/` as a thin shim so the real logic in `src/` is easy to test. See also the [CLI tool project](../24_projects/01_cli-tool/).

## Monorepo (several apps and shared packages)

```
my-monorepo/
├── apps/
│   ├── api/                      # a deployable service
│   │   └── package.json
│   └── web/
│       └── package.json
├── packages/
│   ├── shared/                   # code used by both (types, validators, constants)
│   │   └── package.json
│   ├── ui/                       # shared components
│   └── config/                   # shared ESLint, TypeScript, Prettier presets
├── package.json                  # root: workspaces, shared devDependencies, husky
├── pnpm-workspace.yaml           # (pnpm) or "workspaces" in package.json (npm, yarn)
├── turbo.json                    # optional: task pipeline and caching (Turborepo)
└── ...root files
```

```yaml
# pnpm-workspace.yaml
packages:
  - "apps/*"
  - "packages/*"
```

```json
{
  "name": "@acme/api",
  "dependencies": { "@acme/shared": "workspace:*" }
}
```

| Rule                                                         | Why                                                     |
| ------------------------------------------------------------ | ------------------------------------------------------- |
| Apps depend on packages; packages never depend on apps       | No cycles                                               |
| Install **Husky, lint-staged, Prettier, ESLint** at the root | One set of hooks and rules for everyone                 |
| Share config through a `packages/config` package             | One source of truth for linting and TypeScript settings |
| Use `workspace:*` (pnpm) for internal dependencies           | Always links to the local copy                          |
| Use a task runner (Turborepo, Nx) when builds get slow       | Caching and dependency-ordered tasks                    |

Do not start with a monorepo for a single app. Move to one when you genuinely share code between deployables.

## Naming conventions

| Thing                   | Convention                                                | Example                               |
| ----------------------- | --------------------------------------------------------- | ------------------------------------- |
| Files and folders       | `kebab-case`                                              | `user-service.js`, `order-items/`     |
| Role suffixes           | `name.role.js` (dotted)                                   | `users.routes.js`, `users.service.js` |
| Tests                   | `*.test.js` next to the code                              | `users.service.test.js`               |
| Classes and components  | `PascalCase` (file may stay kebab-case in non-React code) | `UserService`, `Button.jsx`           |
| Variables and functions | `camelCase`                                               | `getUserById`                         |
| Constants               | `UPPER_SNAKE_CASE`                                        | `MAX_RETRIES`                         |
| Environment variables   | `UPPER_SNAKE_CASE`                                        | `DATABASE_URL`                        |
| Folders of many things  | Plural nouns                                              | `modules/`, `middleware/`, `scripts/` |
| Booleans                | Question form                                             | `isActive`, `hasAccess`               |

Pick one style per project and enforce it with ESLint plugins (for example `eslint-plugin-unicorn`'s `filename-case`).

## Import aliases without a bundler

Deep relative imports (`../../../shared/errors.js`) are fragile. Node supports **subpath imports** natively through the `imports` field. Names must start with `#`.

```json
{
  "imports": {
    "#config/*": "./src/config/*.js",
    "#lib/*": "./src/lib/*.js",
    "#shared/*": "./src/shared/*.js"
  }
}
```

```js
import { env } from "#config/env.js";
import { logger } from "#lib/logger.js";
import { NotFoundError } from "#shared/errors.js";
```

This works in plain Node with no build step. With TypeScript, set `"moduleResolution": "NodeNext"` so it understands `imports`. In Vite, use `resolve.alias`, and in tsconfig-based setups, `paths` plus a matching alias in your bundler or runner.

## Standard `package.json` scripts

Use the same script names in every project so anyone can run them without reading the file.

```json
{
  "scripts": {
    "dev": "node --env-file=.env --watch src/index.js",
    "start": "node src/index.js",
    "build": "tsc -p tsconfig.build.json",
    "test": "vitest run",
    "test:watch": "vitest",
    "test:coverage": "vitest run --coverage",
    "lint": "eslint .",
    "lint:fix": "eslint . --fix",
    "format": "prettier --write .",
    "format:check": "prettier --check .",
    "typecheck": "tsc --noEmit",
    "validate": "npm run format:check && npm run lint && npm run typecheck && npm test",
    "prepare": "husky"
  }
}
```

`validate` is the one command CI runs and developers run before pushing. More in [Dev Tooling](./08_dev-tooling.md).

## Where each library lives

| Library                      | Where its code or config goes                                     |
| ---------------------------- | ----------------------------------------------------------------- |
| dotenv / envalid / zod (env) | `src/config/env.js`, plus `.env.example` at the root              |
| pino                         | `src/lib/logger.js`                                               |
| zod (request validation)     | `modules/<feature>/<feature>.schema.js`, `middleware/validate.js` |
| helmet, cors, rate limit     | `src/app.js` or `middleware/security.js`                          |
| Database client or ORM       | `src/lib/db.js`, queries in `*.repository.js`                     |
| Husky                        | `.husky/`                                                         |
| lint-staged                  | `package.json` or `lint-staged.config.js`                         |
| commitlint                   | `commitlint.config.js`                                            |
| ESLint, Prettier             | `eslint.config.js`, `.prettierrc` at the root                     |
| Vitest                       | `vitest.config.js`, `test/setup.js`                               |
| pm2                          | `ecosystem.config.cjs` at the root                                |

## Growing the structure

| Symptom                          | Move                                                                                 |
| -------------------------------- | ------------------------------------------------------------------------------------ |
| One file over ~300 lines         | Split by responsibility                                                              |
| `utils.js` keeps growing         | Name things by purpose (`pagination.js`, `dates.js`) or move into the owning feature |
| Two features share code          | Extract to `shared/` only when **both** really need it                               |
| Circular imports                 | Extract the shared piece into a third module                                         |
| Slow, tangled builds across apps | Consider a monorepo with a task runner                                               |
| Folder with 20+ files            | Add subfolders by sub-feature                                                        |

## Pitfalls

| Pitfall                                   | Why it hurts                                     | Better                                                 |
| ----------------------------------------- | ------------------------------------------------ | ------------------------------------------------------ |
| Dozens of folders for a tiny project      | Navigation overhead                              | Start flat and split when it hurts                     |
| One giant `utils/` or `helpers/`          | A dumping ground nobody can search               | Name by purpose; keep helpers beside their feature     |
| `process.env` read all over the codebase  | Config bugs surface at random runtime moments    | One validated `config/env.js`                          |
| Calling `listen()` inside the app module  | Tests cannot import the app cleanly              | Split `app.js` and `index.js`                          |
| Business logic in controllers or routes   | Not reusable or testable                         | Move it to services                                    |
| Services importing Express or `req`/`res` | HTTP leaks into the domain                       | Pass plain values and return plain results             |
| Reaching into another module's internals  | Tight coupling, breaks on refactors              | Import from the module's `index.js`                    |
| Barrel files re-exporting everything      | Circular imports, slow builds, poor tree-shaking | Small, deliberate public entry points                  |
| Committing `.env` or `dist/`              | Leaked secrets, noisy diffs                      | `.gitignore` plus `.env.example`                       |
| No lockfile committed                     | "Works on my machine" installs                   | Commit the lockfile; use `npm ci` in CI                |
| Missing `.nvmrc` and `engines`            | Different Node versions between machines         | Pin and enforce                                        |
| Premature monorepo                        | Tooling overhead without benefit                 | Add one when you share code across deployables         |
| Mixing generated and hand-written code    | Confusing diffs                                  | Generated output in `dist/` or a clearly marked folder |

## Key takeaways

- Keep the root tidy: config files at the top, code in `src/`, generated files in `dist/` (ignored)
- Prefer **feature-based** folders as the project grows; keep **routes → controller → service → repository** flowing one way
- Split `app.js` (build) from `index.js` (start) so tests can import the app
- One module reads and validates the environment, one creates the logger, and one place wires dependencies together
- Expose a small public API per module through `index.js` and avoid reaching into internals
- Use Node's `imports` field (`#alias/*`) for clean imports without a bundler
- Use the same script names everywhere (`dev`, `test`, `lint`, `format`, `validate`), and commit the lockfile, `.nvmrc`, and `.env.example`
- Start simple; add structure (feature folders, monorepo) only when the pain appears

**Next:** [Environment Variables](./02_environment-variables.md)
