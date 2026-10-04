# Nest CLI

The Nest CLI (`@nestjs/cli`, the `nest` command) is the standard way to scaffold, compile, run, and upgrade NestJS applications. It generates correctly wired boilerplate (modules, controllers, services, even full CRUD resources), builds with the TypeScript compiler, SWC, or Rspack, and runs your app in watch mode. Knowing its commands and its configuration file, `nest-cli.json`, removes a lot of manual work and explains what `npm run start:dev` actually does.

---

## Overview

**What it is.** A command-line tool built around three pieces:

- **Schematics** (`@nestjs/schematics`): code generators for `nest new` and `nest generate`.
- **Builders**: compilers that turn TypeScript into JavaScript (`tsc`, `swc`, `rspack`).
- **A runner**: `nest start` compiles, spawns `node`, and restarts it on change.

**Why it exists.** NestJS is convention-heavy. Every controller must be declared in a module, tests follow a naming pattern, and builds need decorator metadata. The CLI encodes those conventions so projects look the same everywhere.

**Where it is used.** Every NestJS project. `package.json` scripts call it, CI pipelines call it, and developers call it directly to generate files.

**Why you should understand it.**

- `nest generate` edits existing files (it adds a controller to the nearest module). Knowing that avoids surprises.
- `nest-cli.json` controls the builder, assets, plugins (for example the Swagger plugin), and monorepo layout.
- Version 12 added `nest upgrade`, `nest deploy`, Rspack support, and new flags. Older tutorials do not mention them.

---

## Mental Model

```text
            ┌──────────────────────────── nest <command> ────────────────────────────┐
            │                                                                         │
 new ──────►│  Schematics  ──►  writes files + installs deps + git init               │
 generate ─►│  Schematics  ──►  writes files + EDITS nearest module (imports/arrays)  │
 add ──────►│  Library's own install schematic                                        │
 upgrade ──►│  Upgrade schematic ──►  bumps deps, migrates config, prints a report    │
            │                                                                         │
 build ────►│  Builder (tsc | swc | rspack) ──►  dist/                                │
 start ────►│  Builder  ──►  spawns `node dist/...`  (+ watch & restart)              │
            │                                                                         │
 info ─────►│  prints OS / Node / package versions                                    │
 deploy ───►│  forwards to the Mau CLI                                                │
            └─────────────────────────────────────────────────────────────────────────┘
                              reads settings from  nest-cli.json
```

Think of it as three tools sharing one config file: a **code generator**, a **compiler front end**, and a **dev-server supervisor**.

---

## Core Concepts

### Command Overview

| Command | Alias | Purpose |
|---|---|---|
| `nest new <name>` | `nest n` | Create a new project (standard mode) |
| `nest generate <schematic> <name>` | `nest g` | Generate or modify files from a schematic |
| `nest build [name...]` | | Compile into the output folder |
| `nest start [name]` | | Compile and run (optionally watching) |
| `nest add <library>` | | Install a Nest library and run its schematic |
| `nest upgrade` | `nest update` | Upgrade a v11 project to v12 (new in v12) |
| `nest deploy` | | Forward to Mau (`mau deploy`) (new in v12) |
| `nest info` | | Print system and Nest package versions |

### `nest new`

```bash
nest new <name> [options]
```

With no options, it prompts for: project name, package manager, **module system (ESM or CommonJS)**, and whether to set up NestJS Observe (`@nestjs/observe`).

| Option | Alias | Description |
|---|---|---|
| `--directory [directory]` | | Destination directory |
| `--dry-run` | `-d` | Report changes without touching the filesystem |
| `--skip-git` | `-g` | Do not initialize a Git repository |
| `--skip-install` | `-s` | Do not install packages |
| `--skip-tests` | `-t` | Do not generate test files |
| `--package-manager [pm]` | `-p` | `npm`, `yarn`, `pnpm`, or `bun` (must be installed globally) |
| `--language [lang]` | `-l` | `TS` (default) or `JS` |
| `--collection [name]` | `-c` | Use another schematics collection |
| `--strict` | | TypeScript `strict` mode (on by default, with `strictPropertyInitialization` off). To opt out, set `"strict": false` in `tsconfig.json` |
| `--format` | | Format generated files with Prettier |
| `--observe` / `--no-observe` | | Set up `@nestjs/observe` or skip the prompt |

Module system choice:

| Choice | Test runner | Notes |
|---|---|---|
| **ESM** (default) | Vitest | `"type": "module"`, imports need `.js` extensions |
| **CommonJS** | Jest | The traditional layout |

Both variants use **oxlint** for linting. In non-interactive environments pass the project name and `--package-manager`. The module system then defaults to ESM.

### `nest generate`

```bash
nest generate <schematic> <name> [options]
nest g <schematic> <name> [options]
```

| Schematic | Alias | Generates |
|---|---|---|
| `module` | `mo` | Module class |
| `controller` | `co` | Controller (+ spec), registered in the nearest module |
| `service` | `s` | Injectable service (+ spec), registered in the nearest module |
| `provider` | `pr` | Provider declaration |
| `resource` | `res` | Full CRUD resource: module, controller, service, DTOs, entity (TypeScript only) |
| `guard` | `gu` | Guard |
| `pipe` | `pi` | Pipe |
| `interceptor` | `itc` | Interceptor |
| `filter` | `f` | Exception filter |
| `middleware` | `mi` | Middleware |
| `decorator` | `d` | Custom decorator (as of v12, uses the `Reflector.createDecorator()` form) |
| `gateway` | `ga` | WebSocket gateway |
| `resolver` | `r` | GraphQL resolver |
| `class` | `cl` | Plain class |
| `interface` | `itf` | Interface |
| `app` | | New application inside a monorepo (converts standard mode to monorepo) |
| `library` | `lib` | New library inside a monorepo |

| Option | Description |
|---|---|
| `--dry-run` / `-d` | Show what would change. **Use it before generating in an unfamiliar project** |
| `--flat` / `--no-flat` | Do not / do create a folder for the element |
| `--no-spec` | Skip the spec file (default is to generate one) |
| `--spec-file-suffix [s]` | Custom spec suffix |
| `--skip-import` | Do not register the element in its closest module |
| `--project [p]` / `-p` | Target project in a monorepo |
| `--collection [c]` / `-c` | Alternative schematics collection |
| `--format` | Format output with Prettier |
| `--type <type>` | (`resource` only) `rest`, `graphql-code-first`, `graphql-schema-first`, `microservice`, or `ws` |
| `--crud [true\|false]` | (`resource` only) Generate CRUD entry points |

Key behavior: **generators edit existing files.** `nest g service users` creates `users.service.ts` and then adds `UsersService` to the `providers` array of the nearest module file it finds. This is why where you run the command matters.

### `nest build` and `nest start`

```bash
nest build [name...] [options]
nest start [name] [options]
```

| Option | Applies to | Description |
|---|---|---|
| `--watch` / `-w` | both | Rebuild (and for `start`, restart) on change |
| `--builder [name]` / `-b` | both | `tsc`, `swc`, or `rspack` |
| `--tsc` | both | Force the TypeScript compiler |
| `--path [path]` / `-p` | both | Path to a `tsconfig` file |
| `--config [path]` / `-c` | both | Path to the `nest-cli.json` to use |
| `--watchAssets` | both | Also watch non-TS files (for example `.graphql`) |
| `--type-check` / `--no-type-check` | both | Toggle type checking when using SWC |
| `--emit-declarations` | both | Emit `.d.ts` files with SWC (new in v12) |
| `--rspackPath [path]` | both | Rspack config path (new in v12) |
| `--webpack`, `--webpackPath` | both | **Deprecated.** Use `--builder rspack` |
| `--silent` | both | Suppress informational compiler logs (new in v12) |
| `--preserveWatchOutput` | both | Keep old console output in `tsc` watch mode |
| `--all` | `build` | Build all projects in a monorepo |
| `--parallel [n]` | `build` | Build monorepo projects in parallel with `--all` (new in v12) |
| `--debug [hostport]` / `-d` | `start` | Run with `--inspect` |
| `--sourceRoot [dir]` | `start` | Override `sourceRoot` |
| `--entryFile [file]` | `start` | Override the entry file |
| `--exec [binary]` / `-e` | `start` | Binary to run (default `node`) |
| `--no-shell` | `start` | Do not spawn child processes in a shell |
| `--env-file [path]` | `start` | Load env vars from a file (repeatable) (new in v12) |
| `-- [key=value]` | `start` | Arguments readable from `process.argv` |

`nest build` also handles path alias mapping through `tsconfig-paths`, and applies the Swagger and GraphQL **CLI plugins** that annotate your DTOs.

### Builders

| Builder | What it is | When to use |
|---|---|---|
| **`tsc`** | TypeScript compiler. Full type checking and decorator metadata | Default. Most compatible |
| **`swc`** | Rust-based transpiler. Much faster, type checking optional (`--type-check`) | Large projects, fast watch mode. Pass `-b swc` |
| **`rspack`** | Rust-based bundler | Monorepos and bundled output. Replaces webpack-centric workflows |
| `webpack` | Legacy | **Deprecated** in v12 CLI workflows. Migrate to `--builder rspack` |

Speed up development builds with SWC: `npm run start -- -b swc`.

### `nest-cli.json`

The CLI's project configuration file, in the project root.

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

| Key | Purpose |
|---|---|
| `collection` | Schematics collection used by `generate` |
| `sourceRoot` | Source directory (`src`) |
| `entryFile` | Entry file name (default `main`) |
| `compilerOptions.builder` | Default builder (`tsc`, `swc`, `rspack`, or an object with options) |
| `compilerOptions.deleteOutDir` | Clean `dist` before each build |
| `compilerOptions.typeCheck` | Type check when using SWC |
| `compilerOptions.tsConfigPath` | tsconfig used for builds |
| `compilerOptions.assets` / `watchAssets` | Copy and watch non-TS files into `dist` |
| `compilerOptions.plugins` | CLI plugins, for example `@nestjs/swagger` |
| `generateOptions.spec` | Default for generating spec files (`false` disables) |
| `projects`, `monorepo`, `root` | Monorepo mode project registry |
| `includeLibraryAssets` | Copy library assets into an application build (new in v12) |

> New v12 projects may include a `builder` entry in the generated `nest-cli.json`. The CLI release notes state that Rspack replaces webpack as the builder `nest new` scaffolds. *Open your generated `nest-cli.json` to see what your CLI version produced.*

### `nest upgrade` (new in v12)

```bash
nest upgrade --dry-run     # report only
nest upgrade               # apply
```

| Option | Description |
|---|---|
| `--dry-run` / `-d` | Report changes without modifying files |
| `--skip-install` / `-s` | Do not install updated dependencies |
| `--observe` / `--no-observe` | Set up or skip `@nestjs/observe` (prompt defaults to no) |
| `--tag [tag]` / `-t` | Use an npm dist-tag (for example `next`) |
| `--collection [c]` / `-c` | Use a different schematics collection |

What it does (summarized from the CLI documentation): bumps all recognized `@nestjs/*` packages to v12, bumps TypeScript to v6, migrates webpack options in `nest-cli.json` to `--builder rspack`, applies GraphQL/NATS/`@nestjs/config` migrations, bumps Jest (to v30) and Joi (to v18), checks `tsconfig` files for settings TypeScript 6 no longer supports, and prints a report of changes plus behavior changes you must review by hand. It **does not** convert your app to ESM, Vitest, or oxlint.

### `nest add`, `nest deploy`, `nest info`

- `nest add <library>` installs a library packaged as a Nest library and runs its install schematic.
- `nest deploy` is a thin wrapper that forwards arguments to `mau deploy` (offers to install `@nestjs/mau` as a dev dependency on first use, fails in non-interactive environments if not installed).
- `nest info` prints OS, Node.js, npm, and Nest package versions. Run it first when debugging environment issues.

---

## How It Works

`nest start --watch` step by step:

```text
nest start --watch
     │
     ├─ 1. Read nest-cli.json (sourceRoot, entryFile, builder, assets, plugins)
     ├─ 2. Read tsconfig.json / tsconfig.build.json
     ├─ 3. Compile with the builder → output folder (dist/)
     │        └─ apply CLI plugins (Swagger/GraphQL) during compilation
     ├─ 4. Spawn: node <entry file in dist>         (child process)
     ├─ 5. Watch source files
     │        └─ on change: recompile → kill child → spawn a new one
     └─ 6. Forward signals and exit codes
```

`nest generate controller users`:

```text
 1. Resolve target folder (cwd + schematic rules; --flat / --no-flat)
 2. Render templates → users.controller.ts (+ users.controller.spec.ts unless --no-spec)
 3. Find the closest module file (walks up from the target folder)
 4. Edit that module: add import statement + add UsersController to `controllers: [...]`
 5. Print CREATE / UPDATE lines
```

---

## Basic Example

Generate a `users` feature in an existing project and preview first:

```bash
# Preview: shows CREATE/UPDATE lines, changes nothing
nest g module users --dry-run

# Do it
nest g module users
nest g controller users
nest g service users

# Or generate everything in one go (CRUD resource)
nest g resource users
```

Typical output of `nest g resource users` (with prompts answered: REST API, yes to CRUD entry points):

```text
CREATE src/users/users.controller.spec.ts
CREATE src/users/users.controller.ts
CREATE src/users/users.module.ts
CREATE src/users/users.service.spec.ts
CREATE src/users/users.service.ts
CREATE src/users/dto/create-user.dto.ts
CREATE src/users/dto/update-user.dto.ts
CREATE src/users/entities/user.entity.ts
UPDATE src/app.module.ts
```

Output varies by CLI version and your answers. `UPDATE src/app.module.ts` shows the generator registering `UsersModule` in the root module.

What happens:

1. The generator creates a **folder per feature** (`src/users/`), which is the convention Nest encourages.
2. It registers the new module in `AppModule.imports`.
3. The DTO and entity files are plain classes you will fill in later. See [DTOs](../03-core-concepts/02-validation-and-serialization/01-dto.md).

---

## Practical Examples

### 1. Basic: Create, Start, Stop

```bash
nest new my-api --package-manager npm
cd my-api
npm run start:dev       # runs `nest start --watch`
# Ctrl+C to stop
```

### 2. Common: Generate Without Tests or Folders

```bash
nest g service billing --no-spec --flat
nest g guard auth/roles --no-spec
```

Set it once for the whole project in `nest-cli.json`:

```json
{
  "generateOptions": { "spec": false }
}
```

### 3. Common: Generate Into a Subfolder

```bash
nest g controller admin/users          # src/admin/users/users.controller.ts
nest g module admin/users
```

The path before the name becomes the folder. The generator registers the controller in the closest module it finds, so generate the module **first** when you want a new feature module to own the controller.

### 4. Real-World: Faster Dev Loop With SWC

```bash
npm run start:dev -- -b swc
# or permanently, in nest-cli.json:
```

```json
{
  "compilerOptions": {
    "builder": "swc",
    "typeCheck": true
  }
}
```

With `typeCheck: true` (or `--type-check`), SWC still reports type errors. Without it, SWC only strips types and **does no type checking**. Run `tsc --noEmit` in CI regardless.

### 5. Real-World: Environment File With `nest start`

```bash
nest start --watch --env-file .env.development
```

`--env-file` (new in v12) loads variables into `process.env` for the child process and can be repeated. Application-level configuration still belongs in `ConfigModule`. See [Configuration Basics](../03-core-concepts/03-configuration/01-configuration-basics.md).

### 6. Real-World: Enable the Swagger CLI Plugin

```json
{
  "compilerOptions": {
    "plugins": ["@nestjs/swagger"]
  }
}
```

The plugin annotates DTOs with OpenAPI metadata at build time so you write fewer `@ApiProperty()` decorators. See [DTO Documentation](../04-intermediate/09-openapi-and-swagger/03-dto-documentation.md).

### 7. Real-World: Monorepo Commands

```bash
nest g app admin-api         # converts to monorepo mode, adds apps/admin-api
nest g lib shared            # adds libs/shared
nest build --all             # build every project
nest build --all --parallel 4
nest start admin-api --watch
```

Generating the first `app` converts a standard project into a monorepo with an `apps/` and `libs/` layout and a `projects` section in `nest-cli.json`. Plan this before you have lots of code.

### 8. Edge Case: Pass Arguments to the App

```bash
nest start -- --seed=true
```

```typescript
console.log(process.argv);   // [..., '--seed=true']
```

### 9. Edge Case: A Custom Collection

```bash
nest g -c @my-org/nest-schematics service payments
```

Organizations publish their own schematics collections to enforce house style. The collection must be an installed npm package that contains the schematic.

---

## Syntax / API / Commands

### `package.json` Scripts That Wrap the CLI (typical)

| Script | Runs | Purpose |
|---|---|---|
| `build` | `nest build` | Compile to `dist/` |
| `start` | `nest start` | Compile and run once |
| `start:dev` | `nest start --watch` | Watch mode |
| `start:debug` | `nest start --debug --watch` | Watch mode with the inspector |
| `start:prod` | `node dist/...` | Run compiled output (path depends on the generated project) |
| `lint` | `oxlint` | Lint |
| `format` | `prettier --write ...` | Format |
| `test`, `test:watch`, `test:cov`, `test:e2e` | Vitest or Jest | Tests |

*The exact script names and commands are generated per project. Open your `package.json` to confirm.*

### Handy One-Liners

| Command | Purpose |
|---|---|
| `nest g --help` | List schematics and options |
| `nest start --help` | List start options |
| `nest g res orders --dry-run` | Preview a full CRUD resource |
| `npm run start -- -b swc` | One-off SWC dev run |
| `npm run build -- --silent` | Quieter builds |
| `nest info` | Version diagnostics |

---

## Important Rules

1. **Generators modify existing files.** They add imports and array entries to the nearest module. Review the `UPDATE` lines and commit generated changes deliberately.
2. **Use `--dry-run` first** whenever you are unsure where files will land.
3. **Create the module before its controllers and services** if you want the new module to own them. Otherwise they register in the closest ancestor module.
4. **`nest generate resource` is TypeScript only.**
5. **The builder decides type checking.** `tsc` type-checks. SWC does so only with `typeCheck`/`--type-check`.
6. **Decorator metadata must be emitted by whichever builder you use.** If you change builders, confirm dependency injection still works.
7. **Webpack options are deprecated.** Use `--builder rspack` and `--rspackPath`.
8. **`nest upgrade` needs the global CLI updated first.** It only bumps the local CLI dependency.
9. **The CLI has a higher Node.js floor than the runtime.** See [Installation](./01-installation.md).
10. **Scripts use the local CLI, your terminal uses the global CLI.** Keep them in sync.

---

## Under the Hood

### Schematics

Schematics are template-based file transforms from the Angular devkit. A schematic receives options, builds a virtual file tree, applies templates and edits (such as inserting an import and an array entry through AST manipulation), and commits the tree to disk. `--dry-run` prints the virtual tree's diff without committing.

### Builders and Decorator Metadata

| Builder | Metadata handling |
|---|---|
| `tsc` | TypeScript emits `design:*` metadata natively |
| `swc` | The CLI configures SWC to emit decorator metadata. SWC itself does not type-check unless asked |
| `rspack` | Used mainly for bundled builds. Verify metadata behavior when switching |

If dependency injection fails only after switching builders, suspect metadata emission first. See [TypeScript Decorators](../00-prerequisites/02-typescript-decorators.md).

### Watch Mode

`nest start --watch` runs the builder in watch mode and restarts a **child process** after each successful build. State in the old process is lost on every restart (in-memory caches, open connections, WebSocket clients). Use `--watchAssets` if non-TS files matter.

### Plugins

CLI plugins (`compilerOptions.plugins`) hook into compilation to rewrite DTOs and controllers (for OpenAPI or GraphQL metadata). They run only through the Nest CLI's builders. Running `tsc` directly or a separate test transformer does not apply them. This is why plugin-dependent behavior sometimes differs between `nest start` and unit tests.

---

## Common Patterns

### Feature-First Generation

```bash
nest g module orders
nest g controller orders --no-spec    # registers in OrdersModule
nest g service orders
```

or `nest g resource orders` for a quick CRUD skeleton to refine.

### Dry-Run Review Habit

```bash
nest g resource orders --dry-run
nest g resource orders
git diff            # review UPDATE changes
```

### Shared Team Defaults in `nest-cli.json`

Set `generateOptions`, `builder`, `typeCheck`, and plugins once so every developer gets the same results.

### CI Skeleton

```bash
npm ci
npm run lint
npx tsc --noEmit -p tsconfig.build.json    # type-check even if you build with SWC
npm run build
npm test
```

---

## Common Mistakes

| Mistake | Symptom | Why It Happens | Fix |
|---|---|---|---|
| Generating a controller before its module | Controller registered in `AppModule` | Closest-module rule | Generate the module first, or move the registration |
| Running `nest g` from the wrong directory | Files in an unexpected folder | Paths are relative to the working directory | Use `--dry-run`. `cd` to the right place or pass a path (`admin/users`) |
| Expecting tests for a generated file that has none | No spec file | `--no-spec` or `generateOptions.spec: false` | Remove the option or write the spec |
| Using SWC without type checking and shipping | Type errors reach production | SWC strips types only | Enable `typeCheck`, and run `tsc --noEmit` in CI |
| Switching to SWC/Rspack and DI breaks | `Nest can't resolve dependencies` | Metadata not emitted by the new setup | Check builder and config, compare with `-b tsc` |
| Using deprecated `--webpack` | Deprecation warnings, future breakage | Webpack workflows are deprecated in v12 | Migrate to `--builder rspack` (or let `nest upgrade` do it) |
| Running `nest upgrade` with an old global CLI | Older upgrade logic or missing command | The command ships with the CLI | `npm i -g @nestjs/cli@latest` first |
| Running `nest upgrade` on an old Node.js | Command refuses to run | Needs `require(esm)` support | Upgrade Node.js |
| Editing `dist/` by hand | Changes vanish on next build | `deleteOutDir` and rebuilds overwrite it | Edit `src/` only |
| Forgetting `--watchAssets` | Changed `.graphql`/asset files not picked up | Only TS files are watched by default | Add `--watchAssets` |
| Assuming the first `nest g app` is harmless | Project layout changes to monorepo | It converts standard mode to monorepo | Decide on monorepo up front, commit before running |
| Mixed CLI versions (global vs local) | Behavior differs between terminal and scripts | Two installed copies | `nest --version` vs `npx nest --version`, align them |

---

## Debugging

### Commands

```bash
nest info                          # environment and package versions
nest --version
npx nest --version                 # the LOCAL CLI used by npm scripts
nest g service billing --dry-run   # see exactly what would change
nest start --debug --watch         # attach a debugger (see Debugging file)
nest build --silent=false          # (default) show compiler output
npx tsc --noEmit -p tsconfig.build.json     # type-check independent of the builder
nest start --path tsconfig.build.json --config nest-cli.json    # be explicit about config
```

### Common Errors

| Symptom | Likely cause |
|---|---|
| `Cannot find module '.../dist/main'` | Build failed or `entryFile`/`sourceRoot` mismatch, or output path differs from the script's expectation |
| Watch mode does not restart | Error in the build, or the changed file is outside the watched scope |
| `Error: listen EADDRINUSE: address already in use :::3000` | Another process holds the port. See [Debugging](./06-debugging.md) |
| Generator says "schematic not found" | Wrong `--collection`, or the collection package is not installed |
| Spec files missing on generation | `generateOptions.spec: false` or `--no-spec` |
| New file generated but not registered | `--skip-import`, or the nearest module is not the one you expected |

### Techniques

1. **Always read the `CREATE` / `UPDATE` lines.** They tell you which module was edited.
2. **Compare builders:** run once with `--tsc`, then with `-b swc`, to isolate builder-specific problems.
3. **Inspect the effective config** (`nest-cli.json`, `tsconfig.json`, `tsconfig.build.json`) rather than assuming defaults.
4. **Check the local CLI**, not just the global one, when scripts behave oddly.
5. **Use `nest info`** and include it in bug reports.

---

## Performance

| Concern | Guidance |
|---|---|
| Slow rebuilds in watch mode | Use `-b swc`, and keep `incremental` enabled for `tsc` |
| Slow cold builds | SWC or Rspack. Narrow `tsconfig.build.json` `include`/`exclude` |
| Type checking cost with SWC | Keep `typeCheck` on for correctness, or run `tsc --noEmit` in a separate process/CI |
| Monorepo builds | `nest build --all --parallel [n]` |
| Restart cost | Avoid heavy startup work (large cache warm-up) in development |

Measure rather than guess: time `nest build` under each builder in your project.

---

## Security

- **Treat generated code as code you own.** Review it. Generated DTOs and controllers have no validation or authorization yet.
- **Third-party schematics collections run code** on your machine through `--collection`. Install only trusted collections.
- **`nest add` runs a library's install schematic.** Review what it changes (`--dry-run`).
- **`nest deploy` forwards to Mau and may prompt to install `@nestjs/mau`.** Understand the service before deploying anything through it.
- **Do not put secrets in `nest-cli.json`** or in `--env-file` files that are committed.
- **Keep dependencies updated** (see [Dependency Security](../07-production/01-security/07-dependency-security.md)). `nest upgrade` helps with the Nest packages.

---

## Production Considerations

- **Build once, run the output.** Production runs compiled JavaScript with `node`, not `nest start`.
- **The CLI is a build-time tool.** It belongs in `devDependencies` and is typically unnecessary in the final runtime image. See [Docker](../07-production/04-deployment/03-docker.md).
- **CI:** run lint, a type check, `nest build`, and tests. Do not rely on SWC alone for type safety.
- **Reproducibility:** pin the CLI version in `devDependencies`. Commit the lockfile.
- **Monorepo builds:** use `--all` with parallelism cautiously. Watch memory in CI.
- **Upgrades:** run `nest upgrade --dry-run` on a branch and review the report. Check lifecycle hook ordering, `@Optional()` inheritance, and test-runner requirements by hand.
- **Deployment helpers:** `nest deploy` is optional. Standard container or platform deployments do not need it.

---

## Best Practices

### Recommended

```bash
nest g resource orders --dry-run     # preview
nest g resource orders               # generate
git diff                             # review what was UPDATEd
```

```json
{
  "compilerOptions": { "builder": "swc", "typeCheck": true, "deleteOutDir": true },
  "generateOptions": { "spec": true }
}
```

### Avoid

```bash
nest g controller orders             # from the wrong folder, without --dry-run
nest build -b swc --no-type-check    # and never type-check anywhere else
nest build --webpack                 # deprecated path
```

Why: previewing avoids misplaced files, type checking prevents type errors from shipping, and deprecated flags will eventually be removed.

Additional guidance:

- Use `nest g resource` for quick scaffolding, then edit. Do not treat the output as final design.
- Keep generated spec files and write real tests in them.
- One feature, one folder, one module.
- Commit after each generation step so diffs stay reviewable.
- Keep `nest-cli.json` small. Add keys only when you need them.

---

## Version / Compatibility Notes

| Version | Behavior |
|---|---|
| CLI 11.x | Webpack-centric options available. `nest upgrade` not present. Upgrades used `npm-check-updates` |
| **CLI 12.0** | CLI is a native ES module, version aligned with Nest 12. **New:** `nest upgrade` (alias `update`), `nest deploy`. `nest new` prompts for CJS vs ESM. `--builder rspack`, `--rspackPath`. `--emit-declarations`, `--no-type-check`, `--silent`, `--parallel`, `--env-file`, `--skip-tests` for `nest new`, `--observe`/`--no-observe`. `bun` accepted as a package manager. `decorator` schematic uses `Reflector.createDecorator()`. `angular` schematic removed. `--webpack`/`--webpackPath` deprecated |
| Node.js | CLI generators need 22.22.3+, 24.15+, or 26+. Running an app needs 20.19+/22.12+ |
| TypeScript | v6 required by the v12 CLI and schematics |

*Sourced from the official NestJS CLI documentation, the CLI and schematics release notes, and the v12 migration guide. Verify details against your installed version with `nest --help` and `nest <command> --help`.*

---

## Real-World Use Cases

- **Day-to-day scaffolding:** `nest g resource`, `nest g guard`, and friends keep file layout and registration consistent across a team.
- **Fast dev loops:** `nest start --watch -b swc` in large codebases.
- **Monorepos:** several apps (API, worker, admin) and shared libraries built with `nest build --all`.
- **Framework upgrades:** `nest upgrade` as the starting point of each major migration.
- **Organization templates:** custom schematics collections that generate company-standard modules.
- **CI/CD:** build and test commands wrapped in `npm run` scripts.
- **Standalone apps and CLIs:** `nest start` with `--entryFile` to run alternative entry points (a worker, a REPL, a console command).

---

## Interview Questions

### Beginner

1. What is the Nest CLI used for?
   - Scaffolding projects and files, building, running in watch mode, and upgrading.
2. How do you generate a controller named `cats`?
   - `nest generate controller cats` (alias `nest g co cats`).
3. What does `nest start --watch` do?
   - Compiles the app, runs it, and recompiles and restarts when files change.
4. What is `nest g resource` for?
   - Generating a complete CRUD feature: module, controller, service, DTOs, and entity.

### Intermediate

1. What does `nest generate` change besides creating files?
   - It edits the nearest module to import and register the new element.
2. What is `nest-cli.json` for?
   - CLI configuration: collection, source root, builder and compiler options, assets, plugins, generate options, and monorepo project definitions.
3. `tsc` vs SWC as the builder?
   - `tsc` type-checks and is the most compatible. SWC is much faster but only strips types unless type checking is enabled.
4. What does `--dry-run` do and why use it?
   - Reports what would change without writing files, so you can review file placement and module edits.
5. What changed about test runners and linters in NestJS 12 scaffolds?
   - ESM projects use Vitest, CommonJS projects use Jest, and all projects use oxlint.

### Advanced

1. What does `nest upgrade` do and not do?
   - Moves Nest packages to the same major, bumps TypeScript, migrates config and known breaking changes, and prints a report. It does not convert your project to ESM, Vitest, or oxlint.
2. How do CLI plugins (for example Swagger) work and why can tests behave differently?
   - They rewrite code during compilation through the CLI's builder. Test runners using a different transformer do not apply them.
3. What are the trade-offs of switching the builder to SWC or Rspack?
   - Faster builds, but you must confirm decorator metadata emission, type-check separately, and verify plugin behavior.
4. How would you structure CI for a project that builds with SWC?
   - Lint, `tsc --noEmit` against the build tsconfig, `nest build`, and tests, so type safety does not depend on the builder.
5. What happens to application state during `nest start --watch` rebuilds?
   - The child process is killed and respawned, so in-memory state, connections, and sockets are lost.
6. What are the consequences of `nest g app` in a standard project?
   - It converts the project to monorepo mode with `apps/` and `libs/`, and updates `nest-cli.json`.

---

## Quick Reference

```text
Create          nest new <name> [-p npm|yarn|pnpm|bun] [--skip-git] [--skip-tests] [--observe]
Generate        nest g <schematic> <name> [--dry-run] [--no-spec] [--flat] [--skip-import]
                module mo · controller co · service s · provider pr · resource res
                guard gu · pipe pi · interceptor itc · filter f · middleware mi
                decorator d · gateway ga · resolver r · class cl · interface itf
Build           nest build [--builder tsc|swc|rspack] [--all] [--parallel n]
Run             nest start [--watch] [--debug] [-b swc] [--env-file .env] [-- key=value]
Upgrade         npm i -g @nestjs/cli@latest → nest upgrade --dry-run → nest upgrade
Diagnose        nest info
Config          nest-cli.json: collection · sourceRoot · compilerOptions · generateOptions · projects
Deprecated      --webpack · --webpackPath   → use --builder rspack
Rules           generators edit the nearest module · SWC needs typeCheck · CLI is build-time only
```

---

## Key Takeaways

- The CLI is a generator (schematics), a compiler front end (builders), and a watch-mode runner, configured by `nest-cli.json`.
- `nest generate` creates files **and edits the nearest module**. Use `--dry-run` and review `UPDATE` lines.
- `nest new` now asks for ESM (Vitest) or CommonJS (Jest). Both use oxlint.
- SWC and Rspack make builds faster, but type checking and decorator metadata need explicit attention.
- Version 12 adds `nest upgrade`, `nest deploy`, `--builder rspack`, `--env-file`, and more. Webpack flags are deprecated.
- The CLI is a build-time tool. Production runs the compiled output.
- Keep the global and local CLI versions aligned, and update the global one before `nest upgrade`.

---

## Related Topics

```text
01 Installation
      ↓
[02 Nest CLI]
      ↓
03 First Project  →  04 Project Structure
```

- [Getting Started Overview](./README.md)
- [Installation](./01-installation.md)
- [First Project](./03-first-project.md)
- [Project Structure](./04-project-structure.md)
- [Debugging](./06-debugging.md)
- [Modular Monolith](../08-architecture-and-patterns/01-architecture/02-modular-monolith.md)
- [DTO Documentation (Swagger plugin)](../04-intermediate/09-openapi-and-swagger/03-dto-documentation.md)
