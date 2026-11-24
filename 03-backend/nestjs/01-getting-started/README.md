# 01 — Getting Started

This stage takes you from an empty machine to a running, debuggable NestJS application. You will install the toolchain, learn the Nest CLI, generate and run your first project, understand every file the CLI creates, set up a productive development environment, and learn how to debug. No framework concepts are taught here beyond what you need to read the generated code. Controllers, providers, and modules are covered properly in [02-fundamentals](../02-fundamentals/README.md).

---

## Prerequisites

Complete (or pass the skip-check of) [00-prerequisites](../00-prerequisites/README.md) first. In particular you should be comfortable with:

- TypeScript classes and parameter properties ([TypeScript Essentials](../00-prerequisites/01-typescript-essentials.md))
- What a decorator is ([TypeScript Decorators](../00-prerequisites/02-typescript-decorators.md))
- Promises and `async`/`await` ([Node.js Async and Event Loop](../00-prerequisites/03-nodejs-async-and-event-loop.md))
- Methods, status codes, and `curl` ([HTTP and REST Basics](../00-prerequisites/04-http-and-rest-basics.md))

---

## Learning Order

```text
00-prerequisites
      │
      ▼
01  Installation              ← Node.js, a package manager, the Nest CLI
      │
      ▼
02  Nest CLI                  ← new, generate, build, start, upgrade, nest-cli.json
      │
      ▼
03  First Project             ← generate, run, call, modify, test, build
      │
      ▼
04  Project Structure         ← what every generated file does
      │
      ▼
05  Development Environment   ← editor, lint, format, env files, Git, CI basics
      │
      ▼
06  Debugging                 ← logs, inspector, VS Code, tests, common errors
      │
      ▼
02-fundamentals
```

| # | File | What you gain | Approx. time |
|---|---|---|---|
| 01 | [Installation](./01-installation.md) | Correct Node.js and CLI versions, verification, troubleshooting | 30–45 min |
| 02 | [Nest CLI](./02-nest-cli.md) | Every command and flag you will use, `nest-cli.json`, builders | 1–1.5 h |
| 03 | [First Project](./03-first-project.md) | A running API you modified, tested, and built | 1–1.5 h |
| 04 | [Project Structure](./04-project-structure.md) | Meaning of every generated file and config | 1 h |
| 05 | [Development Environment](./05-development-environment.md) | Editor, linting, formatting, `.env`, Git, CI skeleton | 1 h |
| 06 | [Debugging](./06-debugging.md) | Breakpoints, logging, test debugging, error decoder | 1–1.5 h |

---

## What You Should Be Able to Do After This Stage

1. Install a supported Node.js version and the Nest CLI, and verify both with `node --version` and `nest info`.
2. Create a project non-interactively and interactively, and choose between ESM and CommonJS deliberately.
3. Generate modules, controllers, services, and a full CRUD resource, and preview changes with `--dry-run`.
4. Start the app in watch mode, change a route, and see the change without restarting manually.
5. Explain what `main.ts`, `app.module.ts`, `nest-cli.json`, `tsconfig.json`, and `tsconfig.build.json` each do.
6. Attach a debugger, set a breakpoint inside a controller, and step through a request.
7. Diagnose the five most common beginner errors (port in use, unresolved dependency, module not found, wrong Node.js version, route returns 404).

---

## A Note on NestJS 12 (ESM, Vitest, oxlint)

NestJS 12 changed what a new project looks like. This stage documents the **current defaults** and calls out what differs for older projects.

| Topic | NestJS 12 new project | Older (v10/v11) project |
|---|---|---|
| Module system | `nest new` **asks** for ESM (default) or CommonJS | CommonJS |
| Test runner | **Vitest** for ESM projects, **Jest** for CommonJS projects | Jest |
| Linter | **oxlint** (all generated projects) | ESLint |
| TypeScript | v6 required by the v12 CLI and schematics | 5.x |
| Strict mode | Enabled by default (with `strictPropertyInitialization` off) | Often relaxed |
| Imports in ESM projects | Relative imports need a `.js` extension: `./app.module.js` | No extension |
| `main.ts` | Top-level `await bootstrap()` in ESM | `bootstrap();` |

Everything in this stage was checked against the official NestJS documentation for version 12. Statements marked *verify* depend on release details that may change between 12.x patch versions. Confirm them against the output of `nest info` and the generated files in your own project.

**Convention used in this stage:** code that mirrors generated files uses **ESM style** (with `.js` import extensions), because that is the default. If you chose CommonJS, drop the extensions, keep `__dirname`, and call `bootstrap()` without `await`. Later stages show imports without extensions for readability. Add `.js` yourself in an ESM project.

---

## How This Stage Connects to the Rest of the Repository

| Concept learned here | Where it goes deeper |
|---|---|
| `AppModule`, controller, service | [02-fundamentals](../02-fundamentals/README.md) |
| `NestFactory`, bootstrap options | [Application Lifecycle](../03-core-concepts/04-modules-and-di/09-application-lifecycle.md), [How Nest Boots](../06-internals/01-how-nest-boots.md) |
| Generated test files | [04-intermediate/01-testing](../04-intermediate/01-testing/README.md) |
| `.env` and `--env-file` | [03-configuration](../03-core-concepts/03-configuration/README.md) |
| Logger and debugging flags | [Observability](../07-production/03-observability/README.md) |
| Build output and Docker | [Deployment](../07-production/04-deployment/README.md) |
| Monorepo mode and Rspack | [Nest CLI](./02-nest-cli.md), [Modular Monolith](../08-architecture-and-patterns/01-architecture/02-modular-monolith.md) |

---

## Next Stage

[02 — Fundamentals](../02-fundamentals/README.md)
