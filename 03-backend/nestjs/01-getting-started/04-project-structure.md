# Project Structure

A freshly generated Nest project contains about a dozen configuration files and five source files. Each one has a specific job, and several of them (`tsconfig.json`, `tsconfig.build.json`, `nest-cli.json`, `package.json`) directly change how your application compiles and behaves. This file explains every generated file, what the important settings mean, how the source folder is meant to grow, and what to check when the build output or startup is not what you expected.

---

## Overview

**What it is.** The layout produced by `nest new`: source code in `src/`, end-to-end tests in `test/`, tooling configuration at the root, and build output in `dist/`.

**Why it exists.** Convention makes every Nest project navigable. A developer who has seen one can find the entry file, the build config, and the test setup in any other.

**Why you should understand it.**

- Misconfigured `tsconfig` options (`experimentalDecorators`, `emitDecoratorMetadata`, `module`) cause the most confusing runtime errors.
- Knowing which file controls what avoids editing the wrong one (changing `tsconfig.json` when the build reads `tsconfig.build.json`).
- The structure you grow `src/` into decides how maintainable the codebase is. See [Modular Monolith](../08-architecture-and-patterns/01-architecture/02-modular-monolith.md).

> The layout below reflects a NestJS 12 project. File names for linter and test configs depend on the module system you chose and on the CLI version. **Treat your own generated project as the source of truth.**

---

## Mental Model

```text
 my-app/
 │
 │   WHAT RUNS                      HOW IT IS BUILT              HOW IT IS CHECKED
 │   ────────                       ───────────────              ─────────────────
 ├── src/                           nest-cli.json                oxlint  (lint)
 │   ├── main.ts   ← entry          tsconfig.json  (editor,      prettier (format)
 │   ├── app.module.ts                              tests)       vitest/jest (test/ + *.spec.ts)
 │   ├── app.controller.ts          tsconfig.build.json (build)
 │   ├── app.service.ts             package.json (scripts, deps)
 │   └── app.controller.spec.ts
 │
 ├── test/                          dist/  ← build output (generated, ignored by Git)
 │   └── app.e2e-spec.ts
 └── node_modules/                  ← installed packages (ignored by Git)
```

Three layers: **source** (what you write), **configuration** (how it is compiled, linted, tested), and **output** (what Node.js actually runs).

---

## Core Concepts

### Typical Generated Layout

```text
my-app/
├── src/
│   ├── app.controller.spec.ts      # Unit test for AppController
│   ├── app.controller.ts           # Controller with one route: GET /
│   ├── app.module.ts               # Root module
│   ├── app.service.ts              # Service with one method
│   └── main.ts                     # Entry point: bootstraps the application
├── test/
│   └── app.e2e-spec.ts             # End-to-end test (boots the app, calls it with supertest)
├── nest-cli.json                   # Nest CLI configuration
├── package.json                    # Dependencies, scripts, engines, "type"
├── tsconfig.json                   # TypeScript compiler options (editor, tests, tooling)
├── tsconfig.build.json             # Extends tsconfig.json, narrows what `nest build` compiles
├── vitest.config.ts                # (ESM projects) unit test config
├── vitest.config.e2e.ts            # (ESM projects) e2e test config
├── (oxlint config file)            # Lint rules (check your project for the file name)
├── .prettierrc                     # Prettier formatting options
├── .gitignore
└── README.md
```

CommonJS projects use Jest configuration in place of the Vitest config files. Exact filenames and where Jest options live (in `package.json` or a separate file) vary by CLI version.

### `src/` Files

| File | Role |
|---|---|
| `main.ts` | **Entry point.** Creates the app with `NestFactory.create(AppModule)` and calls `listen()`. Global setup (prefix, CORS, pipes, shutdown hooks) goes here |
| `app.module.ts` | **Root module.** Lists `imports`, `controllers`, `providers`. Every other module is reachable from here |
| `app.controller.ts` | Sample controller with `GET /` |
| `app.service.ts` | Sample provider returning `'Hello World!'` |
| `app.controller.spec.ts` | Unit test next to the code it tests (`*.spec.ts` convention) |

The CLI encourages **one directory per module**. Generating a resource creates `src/<feature>/` with its own module, controller, service, DTOs, and entities.

### `test/`

End-to-end tests live outside `src/`. They boot the whole application (usually through `Test.createTestingModule` with `AppModule`) and call it over HTTP in-process with `supertest`. The file suffix convention is `.e2e-spec.ts`. In ESM projects, check how `supertest` is imported (default import) in the generated file.

### `package.json`

Typical content (abridged and illustrative; **yours will differ**):

```json
{
  "name": "my-app",
  "version": "0.0.1",
  "private": true,
  "type": "module",
  "scripts": {
    "build": "nest build",
    "format": "prettier --write \"src/**/*.ts\" \"test/**/*.ts\"",
    "start": "nest start",
    "start:dev": "nest start --watch",
    "start:debug": "nest start --debug --watch",
    "start:prod": "node dist/main",
    "lint": "oxlint",
    "test": "vitest run",
    "test:watch": "vitest",
    "test:cov": "vitest run --coverage",
    "test:e2e": "vitest run --config ./vitest.config.e2e.ts"
  },
  "dependencies": {
    "@nestjs/common": "^12.0.0",
    "@nestjs/core": "^12.0.0",
    "@nestjs/platform-express": "^12.0.0",
    "reflect-metadata": "...",
    "rxjs": "..."
  },
  "devDependencies": {
    "@nestjs/cli": "^12.0.0",
    "@nestjs/schematics": "^12.0.0",
    "@nestjs/testing": "^12.0.0",
    "typescript": "^6.0.0"
  }
}
```

| Field | Meaning |
|---|---|
| `"type": "module"` | **ESM projects.** Tells Node.js (and TypeScript with `nodenext`) to treat `.js` as ES modules. Absent in CommonJS projects |
| `scripts` | Entry points for development, build, test, lint |
| `dependencies` | Needed at runtime: `@nestjs/common`, `@nestjs/core`, a platform adapter, `reflect-metadata`, `rxjs` |
| `devDependencies` | Build and test tools: CLI, schematics, `@nestjs/testing`, TypeScript, test runner, linter, formatter |
| `engines` | Optional Node.js range. Add it yourself if missing |

The core runtime packages:

| Package | Role |
|---|---|
| `@nestjs/common` | Decorators, pipes, guards, exceptions, `Logger`, shared utilities |
| `@nestjs/core` | `NestFactory`, DI container, module scanner, router |
| `@nestjs/platform-express` | Express adapter (default HTTP platform). Replace with `@nestjs/platform-fastify` for Fastify |
| `@nestjs/testing` | `Test.createTestingModule` and testing helpers |
| `reflect-metadata` | The metadata API that decorators rely on |
| `rxjs` | Used by interceptors and microservices. Nest depends on it internally |

### `tsconfig.json`

For an ESM project generated by NestJS 12, the official documentation lists these compiler options:

```json
{
  "compilerOptions": {
    "module": "nodenext",
    "moduleResolution": "nodenext",
    "resolvePackageJsonExports": true,
    "esModuleInterop": true,
    "isolatedModules": true,
    "declaration": true,
    "removeComments": true,
    "emitDecoratorMetadata": true,
    "experimentalDecorators": true,
    "allowSyntheticDefaultImports": true,
    "target": "ES2023",
    "sourceMap": true,
    "outDir": "./dist",
    "rootDir": ".",
    "incremental": true,
    "skipLibCheck": true,
    "strict": true,
    "strictPropertyInitialization": false,
    "types": ["vitest/globals", "node"]
  }
}
```

A CommonJS project uses `["node", "jest"]` for `types`. Per the official docs, `nest-cli.json` and `tsconfig.build.json` are identical in both variants.

| Option | Why it matters |
|---|---|
| `experimentalDecorators` | Enables the legacy decorator syntax Nest is built on. **Must be `true`** |
| `emitDecoratorMetadata` | Emits `design:paramtypes` so constructor injection works. **Must be `true`** |
| `module` / `moduleResolution: nodenext` | Output module format follows the nearest `package.json` `"type"`. Requires explicit `.js` extensions on relative imports in ESM |
| `resolvePackageJsonExports` | Respect the `exports` field of dependencies (needed for ESM-only packages) |
| `isolatedModules` | Each file must be transpilable on its own (required by SWC, esbuild, Vitest-style transformers). Interacts with `import type` rules for decorator metadata |
| `target: ES2023` | Syntax level of emitted JavaScript. Keep it consistent with your Node.js version |
| `outDir: ./dist` | Where compiled output goes |
| `rootDir: .` | Root used to compute the output folder structure. **It affects the path of the compiled entry file** (see below) |
| `strict: true` | Enables the strict family of checks |
| `strictPropertyInitialization: false` | Lets DTO/entity fields be declared without initializers (the framework or ORM assigns them) |
| `skipLibCheck` | Skips checking `.d.ts` files in dependencies. Faster builds |
| `sourceMap` | Enables mapping stack traces to `.ts` |
| `incremental` | Faster repeat compilation |
| `types` | Which ambient type packages are included (test runner globals, Node.js types) |

> Never remove `experimentalDecorators` or `emitDecoratorMetadata`. Removing either breaks dependency injection in confusing ways. See [TypeScript Decorators](../00-prerequisites/02-typescript-decorators.md).

### `tsconfig.build.json`

Extends `tsconfig.json` and narrows what `nest build` compiles. Typically it excludes tests and build artifacts, for example:

```json
{
  "extends": "./tsconfig.json",
  "exclude": ["node_modules", "test", "dist", "**/*spec.ts"]
}
```

*Illustrative. Open your generated file.* Why it exists: you want tests and e2e files type-checked by the editor and test runner (through `tsconfig.json`) but **not** compiled into production output.

### `nest-cli.json`

```json
{
  "$schema": "https://json.schemastore.org/nest-cli",
  "collection": "@nestjs/schematics",
  "sourceRoot": "src",
  "compilerOptions": {
    "deleteOutDir": true
  }
}
```

Controls the CLI: generators, source root, builder, plugins, assets, monorepo projects. The CLI release notes state that Rspack is the builder scaffolded for new projects in v12, so your generated file may contain a `builder` entry. See [Nest CLI](./02-nest-cli.md).

### Test and Lint Configuration

| Project type | Unit tests | E2E tests | Linter |
|---|---|---|---|
| ESM | Vitest, `vitest.config.ts` | Vitest, `vitest.config.e2e.ts` | oxlint |
| CommonJS | Jest | Jest (separate e2e config) | oxlint |

Formatting is handled by Prettier in both. Scripts `npm run lint` and `npm run format` wrap them.

### `dist/`

The build output. It is generated, ignored by Git, and rebuilt (and by default cleaned, via `deleteOutDir`) on each build. **Never edit it.**

The compiled entry path depends on `rootDir`, `tsconfig.build.json`, and CLI settings. After your first `npm run build`, look inside `dist/` and confirm the path that `start:prod` uses (for example `dist/main` versus `dist/src/main`). The `nest upgrade` command specifically checks for a missing `rootDir` in `tsconfig` files for this reason.

### `.gitignore`, `README.md`, Prettier

- `.gitignore` excludes `node_modules/`, `dist/`, logs, coverage, IDE files, and `.env` variants. **Confirm `.env` files are ignored before adding secrets.**
- `.prettierrc` holds formatting options (quotes, trailing commas). Commit it so everyone formats identically.
- `README.md` is a generic Nest readme. Replace it with project-specific setup instructions.

---

## How It Works

How the configuration files cooperate during `npm run build` and `npm run start:dev`:

```text
 npm run start:dev
        │
        ▼
 package.json "start:dev": "nest start --watch"
        │
        ▼
 nest-cli.json          → sourceRoot, entryFile, builder, plugins, assets
        │
        ▼
 tsconfig.build.json    → files to compile (excludes tests)
        │  extends
        ▼
 tsconfig.json          → compiler options (decorators, module, strict ...)
        │
        ▼
 Builder (tsc | swc | rspack)  → dist/
        │
        ▼
 node <compiled entry>  (restarted on change)

 npm test
        │
        ▼
 vitest.config.ts (or Jest config)  +  tsconfig.json   → runs *.spec.ts

 npm run lint  → oxlint          npm run format → prettier
```

Key detail: **the editor and test runner use `tsconfig.json`; `nest build` uses `tsconfig.build.json`.** A setting changed only in one of them may not take effect in the other.

---

## Basic Example

A quick tour you can do on a fresh project:

```bash
cd my-first-api
ls -a                          # see the root files
cat package.json               # scripts and dependencies
cat tsconfig.json              # compiler options (find the two decorator flags)
cat tsconfig.build.json        # what gets compiled
cat nest-cli.json              # CLI config
npm run build
ls dist                        # where is the compiled entry file?
grep start:prod package.json   # does the path match?
```

What you learn:

1. Which scripts exist and which commands they run.
2. Where decorator metadata is switched on.
3. Whether the build output path matches the `start:prod` command.
4. That `dist/` contains compiled `.js` (and `.js.map`, and possibly `.d.ts`) files, not your TypeScript.

---

## Practical Examples

### 1. Basic: Grow `src/` by Feature

```text
src/
├── main.ts
├── app.module.ts
├── users/
│   ├── users.module.ts
│   ├── users.controller.ts
│   ├── users.controller.spec.ts
│   ├── users.service.ts
│   ├── users.service.spec.ts
│   ├── dto/
│   │   ├── create-user.dto.ts
│   │   └── update-user.dto.ts
│   └── entities/
│       └── user.entity.ts
└── orders/
    └── ...
```

Generated by `nest g resource users`. Each feature owns its module, and `AppModule` imports the feature modules.

### 2. Common: Add Shared Code

```text
src/
├── common/               # cross-cutting: guards, pipes, interceptors, filters, decorators
│   ├── filters/
│   ├── guards/
│   └── decorators/
├── config/               # configuration factories and validation
└── database/             # connection setup
```

Keep truly shared, feature-agnostic code in `common/`. Anything feature-specific stays in its feature folder. See [Feature and Shared Modules](../03-core-concepts/04-modules-and-di/01-feature-and-shared-modules.md).

### 3. Common: Add a `.env` File Safely

```bash
printf 'PORT=3001\n' > .env
grep -n '^\.env' .gitignore || echo "WARNING: .env is not ignored"
cp .env .env.example           # commit the example, not the secrets
```

Commit `.env.example` with placeholder values. See [Development Environment](./05-development-environment.md) and [Configuration Basics](../03-core-concepts/03-configuration/01-configuration-basics.md).

### 4. Real-World: Add Path Aliases

```jsonc
// tsconfig.json
{
  "compilerOptions": {
    "baseUrl": ".",
    "paths": { "@common/*": ["src/common/*"] }
  }
}
```

`nest build` maps these through `tsconfig-paths`. Test runners need their own alias configuration (for example `resolve.alias` in the Vitest config or `moduleNameMapper` in Jest). In ESM projects, check how aliases resolve at runtime on your CLI version. Aliases are a frequent source of "works in the editor, fails at runtime". Use them sparingly.

### 5. Real-World: Enable the Swagger Plugin and Assets

```json
{
  "compilerOptions": {
    "deleteOutDir": true,
    "plugins": ["@nestjs/swagger"],
    "assets": [{ "include": "templates/**/*", "outDir": "dist" }],
    "watchAssets": true
  }
}
```

Non-TypeScript files (templates, `.proto`, `.graphql`) are not copied to `dist/` unless listed in `assets`. In ESM projects, use `import.meta.dirname` (not `__dirname`) to locate such files at runtime.

### 6. Edge Case: `start:prod` Cannot Find the Entry File

```text
Error: Cannot find module '/app/dist/main'
```

Check, in order:

```bash
npm run build
find dist -name 'main.*'          # where did the entry file land?
grep -n start:prod package.json   # does the command point there?
grep -n rootDir tsconfig.json tsconfig.build.json
grep -n entryFile nest-cli.json
```

If the build produced `dist/src/main.js`, either adjust the script or the `rootDir`/`tsconfig.build.json` settings so the layout is what you intended.

### 7. Edge Case: Editor Shows No Errors, Build Fails (or the Reverse)

The editor and test runner read `tsconfig.json`; the build reads `tsconfig.build.json`. A file excluded in the build config can have type errors in the editor but not in the build, and a flag set only in one file applies only where that file is used. Run `npx tsc --noEmit -p tsconfig.build.json` and `npx tsc --noEmit -p tsconfig.json` to see both views.

---

## Syntax / API / Commands

| Command | Purpose |
|---|---|
| `npx tsc --showConfig` | Effective merged TypeScript configuration |
| `npx tsc --showConfig -p tsconfig.build.json` | Effective build configuration |
| `npx tsc --noEmit -p tsconfig.build.json` | Type-check exactly what is built |
| `npm run build && ls dist` | Inspect build output |
| `npm ls @nestjs/core` | Confirm installed framework version |
| `git check-ignore -v .env` | Confirm `.env` is ignored |
| `nest info` | Environment and package versions |

---

## Important Rules

1. **`experimentalDecorators` and `emitDecoratorMetadata` must be `true`.**
2. **The build reads `tsconfig.build.json`, the editor and tests read `tsconfig.json`.** Change the right file.
3. **`dist/` is generated.** Never edit or commit it.
4. **`.env` files with secrets must be ignored by Git.** Commit `.env.example` only.
5. **In ESM projects, relative imports need `.js` extensions**, and `__dirname` is unavailable.
6. **Keep the `start:prod` path aligned with the real build output.**
7. **One module per feature folder**, imported into `AppModule` (or a parent module).
8. **Specs live next to the code (`*.spec.ts`), e2e tests in `test/`.**
9. **Runtime dependencies go in `dependencies`, tooling in `devDependencies`.** Production images should not need the CLI.
10. **Do not move `reflect-metadata` or `rxjs` out of `dependencies`.** Nest requires them at runtime.
11. **Commit the lockfile.**
12. **Keep `strict` on.** Turn off individual checks only with a reason.

---

## Under the Hood

### Why Two tsconfig Files

Tests and tooling need to type-check spec files and test setup. The production build must not emit them. `tsconfig.build.json` extends the base config and excludes what should not ship.

### `isolatedModules` and Decorator Metadata

With `isolatedModules`, each file is compiled without knowledge of others. TypeScript therefore cannot always tell whether an imported name is a class (value) or a type. If a type-only symbol is imported normally and used in a decorated signature, TypeScript reports an error asking for `import type`. The reverse mistake is equally real: using `import type` for a **class** you want injected erases the reference and breaks metadata. Rule: **classes you inject use a normal `import`; interfaces and types use `import type`.**

### `rootDir` and Output Layout

TypeScript preserves the directory structure relative to `rootDir` when emitting. If `rootDir` is `.` and sources are under `src/`, output may be nested under `dist/src/`. If `rootDir` is `src`, output is flat under `dist/`. This is why the entry path in `start:prod` can differ between projects and why migrations check `rootDir`.

### Module Format Follows `package.json`

With `"module": "nodenext"`, TypeScript decides per file whether to emit ESM or CommonJS by looking at the nearest `package.json` `"type"` field. Adding `"type": "module"` changes how **every** `.ts` file in the project is emitted. This is why switching an existing project to ESM requires fixing imports in one pass.

### Why `strictPropertyInitialization` Is Off

Classes such as DTOs and entities have fields assigned by the framework or ORM, not by the constructor. With the check on you would have to write `field!: string` everywhere. The Nest template turns this single check off while keeping the rest of `strict`.

---

## Common Patterns

### Feature Module Layout

`<feature>.module.ts`, `<feature>.controller.ts`, `<feature>.service.ts`, `dto/`, `entities/` (and later `repositories/`, `guards/`). One folder per feature.

### `common/` for Cross-Cutting Code

Filters, guards, interceptors, pipes, and decorators used by multiple features.

### `config/` for Configuration

Typed configuration factories and validation schemas. See [Custom Configuration](../03-core-concepts/03-configuration/03-custom-configuration.md).

### Barrel Files With Care

`index.ts` re-exports shorten imports, but they can create circular imports that surface as `undefined` providers at startup. Prefer direct imports within a feature.

### Separate Test Helpers

Keep shared test fixtures in `test/` (or a `testing/` folder) and exclude them in `tsconfig.build.json`.

---

## Common Mistakes

| Mistake | Symptom | Why It Happens | Fix |
|---|---|---|---|
| Removing `emitDecoratorMetadata` | `Nest can't resolve dependencies` everywhere | Constructor param types no longer emitted | Set it back to `true` |
| Editing `tsconfig.json` to change build output | No effect on `nest build` | Build uses `tsconfig.build.json` | Edit the build config (or both, intentionally) |
| Committing `dist/` or `node_modules/` | Huge diffs, merge conflicts | Missing `.gitignore` entries | Fix `.gitignore`, `git rm -r --cached dist` |
| Committing `.env` | Secrets in Git history | `.env` not ignored | Ignore it, rotate leaked secrets, commit `.env.example` |
| `start:prod` path wrong | `Cannot find module '.../dist/main'` | Output layout depends on `rootDir` | Inspect `dist/`, align script or config |
| Missing `.js` extension in ESM | `ERR_MODULE_NOT_FOUND` | ESM needs explicit extensions | Add `.js` to relative imports |
| `import type` on an injected class | Dependency unresolved | Type-only import erased | Use a normal import |
| Non-TS assets missing in `dist/` | Templates/protos not found at runtime | Assets not copied | Configure `assets` in `nest-cli.json` |
| Tests compiled into production output | Larger bundle, test code in `dist/` | Specs not excluded from the build config | Exclude `**/*spec.ts` and `test` in `tsconfig.build.json` |
| Moving files by hand and forgetting module registrations | Routes vanish, DI errors | Module arrays not updated | Update `imports`/`providers`/`controllers`, or regenerate |
| Circular imports through barrels | `undefined` providers at startup | Evaluation order | Import directly, restructure modules |
| Disabling `strict` to silence errors | Latent bugs | Hides real type issues | Fix the types, relax single options only when justified |
| Mixing global and local test runners | Different results locally vs CI | Different versions | Use `npm test` scripts |

---

## Debugging

### Commands

```bash
npx tsc --showConfig                          # base config as TypeScript sees it
npx tsc --showConfig -p tsconfig.build.json   # build config
npx tsc --noEmit -p tsconfig.build.json
npm run build && find dist -maxdepth 2 | head -30
git status --ignored | head -20               # is dist/ node_modules/ .env ignored?
git ls-files | grep -E '(^|/)\.env' || echo "no .env tracked"
node -e "console.log(require('./package.json').type)"
```

### Common Errors

| Error | Likely cause |
|---|---|
| `ERR_MODULE_NOT_FOUND ... imported from ...` | Missing `.js` extension (ESM) or wrong path |
| `ReferenceError: __dirname is not defined in ES module scope` | CommonJS global used in ESM |
| `Cannot find module '.../dist/main'` | Output path mismatch, or build failed |
| `Nest can't resolve dependencies of X (?)` | Metadata flag off, provider not registered, `import type` on a class, or circular import |
| `TS1272` about `import type` | `isolatedModules` + decorated signature using a type imported as a value |
| `Cannot find name 'describe'/'it'` in specs | `types` in `tsconfig.json` lacks the test runner globals |

### Techniques

1. **Print the effective config** with `--showConfig` before guessing.
2. **Inspect `dist/`** after a build to confirm layout and that no spec files shipped.
3. **Verify the decorator flags** in both configs (build extends base, so the base normally holds them).
4. **Bisect imports** when a provider is `undefined`: temporarily import the class directly from its file instead of a barrel.
5. **Run the build command CI runs**, not only `start:dev`.

---

## Performance

- `incremental` and `skipLibCheck` keep `tsc` builds fast.
- Narrow `tsconfig.build.json` `include`/`exclude` to compile only what ships.
- SWC or Rspack builders speed up large projects ([Nest CLI](./02-nest-cli.md)).
- Avoid very large barrel files that pull the whole module graph into every import (slower type checks and startup).
- Keep `node_modules` out of editor indexing and file watchers where possible.

---

## Security

- **Ignore and never commit `.env`**, credentials, private keys, or service account files. Verify with `git check-ignore -v .env`.
- **Do not ship tests, fixtures, or sample data** in `dist/`. Fixtures may contain realistic personal data or credentials.
- **`dist/` source maps** can expose source structure if served publicly. Do not expose the build folder through static hosting.
- **Dependencies:** audit regularly (`npm audit`) and commit the lockfile. See [Dependency Security](../07-production/01-security/07-dependency-security.md).
- **`README.md` and examples:** do not paste real tokens or URLs of internal systems.
- **Generated code is unprotected scaffolding.** Add validation and authorization before exposing anything.

---

## Production Considerations

- **Build in CI, run the output.** Production images contain `dist/`, production `node_modules`, and `package.json`, not the CLI, tests, or source.
- **Confirm the runtime entry path** in the container matches `dist/` layout.
- **Match Node.js versions** between build and runtime stages.
- **`engines` and `.nvmrc`** document the supported runtime.
- **Config via environment variables**, validated at startup. See [Configuration Validation](../03-core-concepts/03-configuration/02-configuration-validation.md).
- **Keep structure consistent across services** so on-call engineers can navigate any repository.
- **Document structure decisions** (feature folders, `common/`, alias rules) in `CONTRIBUTING.md`.

---

## Best Practices

### Recommended

```text
src/
├── main.ts
├── app.module.ts
├── common/                 # shared filters, guards, pipes, decorators
├── config/                 # typed configuration
└── users/                  # one folder per feature
    ├── users.module.ts
    ├── users.controller.ts
    ├── users.service.ts
    ├── dto/
    └── entities/
```

```json
{ "extends": "./tsconfig.json", "exclude": ["node_modules", "test", "dist", "**/*spec.ts"] }
```

### Avoid

```text
src/
├── controllers/            # layer-first: every feature spread over many folders
├── services/
├── models/
└── utils.ts                # growing catch-all file
```

Why: feature-first folders keep everything that changes together in one place, make module boundaries obvious, and scale to larger teams. Layer-first folders scatter each feature and hide dependencies.

Additional guidance:

- Keep `main.ts` small. Move configuration into modules and helper functions as it grows.
- Name files by role (`*.controller.ts`, `*.service.ts`, `*.module.ts`, `*.dto.ts`, `*.entity.ts`, `*.guard.ts`). The CLI and tooling depend on those suffixes.
- Do not rename generated suffixes. Spec and e2e tooling use them.
- Do not create `utils.ts`/`helpers.ts` catch-alls. Give code a named home.

---

## Version / Compatibility Notes

| Item | NestJS 12 (new project) | Earlier (v10/v11) |
|---|---|---|
| `package.json` `"type"` | `"module"` for ESM, absent for CommonJS | Absent |
| `tsconfig` `module` | `nodenext` (ESM variant) | `commonjs` (older projects) |
| `tsconfig` `target` | `ES2023` | Older ES targets |
| Test runner files | `vitest.config.ts`, `vitest.config.e2e.ts` (ESM) | Jest config |
| Test `types` | `["vitest/globals", "node"]` (ESM), `["node", "jest"]` (CommonJS) | `["jest", "node"]` |
| Linter | oxlint | ESLint |
| Strictness | `strict: true`, `strictPropertyInitialization: false` | Often relaxed |
| `nest-cli.json` / `tsconfig.build.json` | Identical between CJS and ESM variants | n/a |
| Projects generated by recent v11 CLIs (`@nestjs/schematics` 11.0.6+) | Already use `nodenext` resolution | n/a |

*Compiler options for the ESM variant come from the official NestJS 12 migration guide. Verify against your generated files, since the CLI evolves between patch versions.*

---

## Real-World Use Cases

- **Standardized service repositories:** every microservice created from the same layout is instantly navigable.
- **Onboarding:** new developers find entry points, scripts, and tests without a guide.
- **Monorepos:** the same structure repeated under `apps/` and `libs/`.
- **Build pipelines:** CI relies on `package.json` scripts and the `dist/` layout.
- **Migrations:** `nest upgrade` inspects these files, which is why keeping them conventional makes upgrades smoother.

---

## Interview Questions

### Beginner

1. What does `main.ts` do?
   - It is the entry point. It creates the Nest application and starts listening.
2. Where do unit tests and e2e tests live?
   - Unit tests next to the code as `*.spec.ts` in `src/`. E2E tests in `test/` as `*.e2e-spec.ts`.
3. What is `dist/`?
   - Compiled JavaScript output generated by the build. It is not edited or committed.
4. What is `app.module.ts`?
   - The root module that imports other modules and registers the app's controllers and providers.

### Intermediate

1. Why are there both `tsconfig.json` and `tsconfig.build.json`?
   - The base config serves editors and tests. The build config extends it and excludes tests so they are not compiled into production output.
2. Which two `tsconfig` options does NestJS depend on and why?
   - `experimentalDecorators` for the decorator syntax and `emitDecoratorMetadata` so constructor parameter types are emitted for dependency injection.
3. What does `"type": "module"` change?
   - Node.js treats `.js` files as ES modules. With `nodenext`, every TypeScript file is emitted as ESM, relative imports need `.js` extensions, and `__dirname` is unavailable.
4. How should `src/` grow?
   - By feature: one folder and one module per feature, with shared cross-cutting code in `common/`.
5. Why is `strictPropertyInitialization` disabled in the template?
   - DTO and entity fields are assigned by the framework or ORM, not by constructors.

### Advanced

1. Why can `start:prod` fail with `Cannot find module` after a successful build?
   - The compiled layout depends on `rootDir` and build settings, so the entry file may be `dist/src/main.js` while the script expects `dist/main`.
2. How can `isolatedModules` interact with decorator metadata?
   - Files are compiled in isolation, so TypeScript cannot always know whether an import is a type or a class. Classes you inject need normal imports. Types need `import type`. Mixing them up breaks the build or erases metadata.
3. Why must non-TypeScript assets be configured separately?
   - The compiler only emits TypeScript outputs. Assets are copied by the CLI only when configured in `nest-cli.json`.
4. How do path aliases complicate testing and ESM?
   - The build maps aliases through `tsconfig-paths`, but test runners and ESM runtimes need their own resolution configuration, leading to errors that appear only at runtime or in tests.
5. What would you change in the structure when the codebase grows to many teams?
   - Move toward modular-monolith boundaries (public module APIs), possibly a monorepo with libraries, and enforce boundaries with lint rules.

---

## Quick Reference

```text
src/main.ts              entry: NestFactory.create + listen
src/app.module.ts        root module
src/<feature>/           module + controller + service + dto/ + entities/
test/                    e2e tests (*.e2e-spec.ts)
nest-cli.json            CLI: sourceRoot · builder · plugins · assets · projects
tsconfig.json            editor + tests (decorator flags live here)
tsconfig.build.json      what `nest build` compiles (excludes tests)
package.json             scripts · deps · "type": "module" (ESM)
dist/                    build output (generated, never edit)

Must be true            experimentalDecorators · emitDecoratorMetadata
ESM rules               .js import extensions · import.meta.dirname · await bootstrap()
Classes injected        normal import          Types/interfaces → import type
Never commit            dist/ · node_modules/ · .env
Check output path       npm run build && find dist -name 'main.*'
```

---

## Key Takeaways

- A generated project separates **source** (`src/`, `test/`), **configuration** (`tsconfig*.json`, `nest-cli.json`, `package.json`, test and lint configs), and **output** (`dist/`).
- `experimentalDecorators` and `emitDecoratorMetadata` are non-negotiable. They are what make decorators and dependency injection work.
- The build uses `tsconfig.build.json`. The editor and test runner use `tsconfig.json`. Know which one you are changing.
- ESM projects (`"type": "module"`, `nodenext`) need `.js` import extensions and `import.meta.dirname`.
- Grow `src/` feature-first, with one module per feature and a small `common/` for shared code.
- `dist/` is generated. Verify the compiled entry path matches your `start:prod` script.
- Keep `.env` out of Git and commit the lockfile.

---

## Related Topics

```text
03 First Project
      ↓
[04 Project Structure]
      ↓
05 Development Environment  →  02-fundamentals/02 Modules
```

- [Getting Started Overview](./README.md)
- [Nest CLI](./02-nest-cli.md)
- [First Project](./03-first-project.md)
- [Development Environment](./05-development-environment.md)
- [Modules](../02-fundamentals/02-modules.md)
- [Feature and Shared Modules](../03-core-concepts/04-modules-and-di/01-feature-and-shared-modules.md)
- [Configuration Basics](../03-core-concepts/03-configuration/01-configuration-basics.md)
- [TypeScript Decorators](../00-prerequisites/02-typescript-decorators.md)
