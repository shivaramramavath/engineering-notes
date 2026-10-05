# 21 · Libraries and Project Setup

Real projects are made of more than application code. Around every codebase sits a layer of **tooling and libraries** that loads configuration, validates input, formats and lints code, guards commits, logs events, and keeps dependencies healthy. This chapter explains the libraries that appear in nearly every JavaScript and Node.js project, what each one is for, how to set it up, and how to arrange them all in a **clean folder structure**.

## What you will learn

- How to lay out a project so it stays easy to navigate as it grows (API, frontend, library, CLI, monorepo)
- Managing configuration and secrets with **dotenv**, Node's built-in `--env-file`, and validating them with **envalid** or **zod**
- Enforcing quality before code leaves your machine with **Husky**, **lint-staged**, and **commitlint**
- Consistent code style with **ESLint**, **Prettier**, and **EditorConfig**
- Validating untrusted input with **zod**, **Joi**, **Ajv**, and **Valibot**
- Structured logging with **pino**, **winston**, and **morgan**
- Server essentials: **helmet**, **cors**, **express-rate-limit**
- Dev and build tooling: **tsx**, **nodemon**, **concurrently**, **cross-env**, **esbuild**, **pm2**
- Everyday utility libraries, and when the platform already does the job natively
- Choosing, auditing, and updating dependencies safely

## Contents

| #   | File                                                     | Topic                                                                             |
| --- | -------------------------------------------------------- | --------------------------------------------------------------------------------- |
| 01  | [Project Structure](./01_project-structure.md)           | Root files, API, frontend, library, CLI, monorepo layouts, naming, import aliases |
| 02  | [Environment Variables](./02_environment-variables.md)   | dotenv, `--env-file`, dotenv-expand, dotenvx, envalid, zod, secrets               |
| 03  | [Git Hooks and Commits](./03_git-hooks-and-commits.md)   | Husky, lint-staged, commitlint, Conventional Commits, Lefthook                    |
| 04  | [Linting and Formatting](./04_linting-and-formatting.md) | ESLint flat config, Prettier, EditorConfig, Biome, editor setup                   |
| 05  | [Validation Libraries](./05_validation-libraries.md)     | zod, Valibot, Joi, Yup, Ajv, validation middleware                                |
| 06  | [Logging](./06_logging.md)                               | pino, winston, morgan, debug, request IDs, redaction                              |
| 07  | [Server Middleware](./07_server-middleware.md)           | helmet, cors, rate limiting, compression, middleware order                        |
| 08  | [Dev Tooling](./08_dev-tooling.md)                       | tsx, nodemon, concurrently, cross-env, rimraf, esbuild, pm2, scripts              |
| 09  | [Utility Libraries](./09_utility-libraries.md)           | Native-first choices, es-toolkit, date-fns, nanoid, p-limit, ky, argon2, jose     |
| 10  | [Managing Dependencies](./10_managing-dependencies.md)   | Evaluating packages, semver, lockfiles, audit, Renovate, Dependabot               |

## Prerequisites

- [npm and Package Managers](../00_setup/03_npm-and-package-managers.md) and [Tooling](../00_setup/05_tooling.md)
- [Modules](../13_modules/00_README.md): ES modules and CommonJS
- [Node.js](../16_nodejs/00_README.md), especially [Process and Env](../16_nodejs/03_process-and-env.md)
- [Security](../22_security/00_README.md): input validation and dependency security

## Bootstrap a project in one minute

```bash
mkdir my-app && cd my-app
git init
npm init -y
npm pkg set type=module
npm pkg set engines.node=">=20"

# runtime libraries
npm install dotenv envalid pino zod

# developer tooling
npm install --save-dev husky lint-staged prettier eslint @eslint/js globals \
  @commitlint/cli @commitlint/config-conventional

npx husky init                      # creates .husky/ and the "prepare" script
```

Each file in this chapter explains what these commands set up and how to configure it.

## Quick picker

| I need to...                                     | Reach for                                    |
| ------------------------------------------------ | -------------------------------------------- |
| Load variables from a `.env` file                | Node `--env-file`, or **dotenv**             |
| Fail fast when configuration is missing or wrong | **envalid** or **zod**                       |
| Run checks before every commit                   | **Husky** + **lint-staged**                  |
| Enforce commit message format                    | **commitlint**                               |
| Catch bugs and enforce code rules                | **ESLint**                                   |
| Format code consistently                         | **Prettier** (or **Biome**)                  |
| Validate request bodies and external data        | **zod** (or Valibot, Joi, Ajv)               |
| Log in production                                | **pino**                                     |
| Add security headers, CORS, rate limits          | **helmet**, **cors**, **express-rate-limit** |
| Restart on change, run TypeScript directly       | `node --watch`, **nodemon**, **tsx**         |
| Run several scripts at once                      | **concurrently**                             |
| Keep a Node process alive on a server            | **pm2**, systemd, or containers              |
| Limit concurrent async work                      | **p-limit**                                  |
| Keep dependencies updated                        | **Renovate** or **Dependabot**               |

## Guiding principles

1. **Platform first.** Check whether Node or the browser already does it (`fetch`, `crypto.randomUUID`, `--env-file`, `util.parseArgs`) before adding a dependency
2. **Fail fast.** Validate configuration and input at the edges so bad data never travels inward
3. **Automate quality.** Hooks and CI should catch problems, not code review
4. **One owner per concern.** One file reads environment variables, one file creates the logger, one file wires everything together
5. **Keep the dependency list short** and know why each package is there

## Key takeaways

- A good folder structure and a small, deliberate set of libraries make a project predictable for every new contributor
- Load configuration once, validate it at startup, and expose a typed, frozen config object
- Hooks give fast local feedback; CI is the real gate
- Use ESLint for bugs and Prettier for formatting, and keep their responsibilities separate
- Validate all untrusted input with a schema library
- Prefer native platform features; add a dependency only when it earns its place

**Next:** [Project Structure](./01_project-structure.md)
