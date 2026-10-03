# 17 — TypeScript

JavaScript lets you pass anything anywhere, and finds out it was wrong at runtime — usually in production, usually at 3 a.m. TypeScript moves a whole class of those mistakes to **compile time**: the editor underlines the bug before the code ever runs. This folder covers TypeScript specifically in the context of a Node.js/Express backend, not the language in the abstract.

## What this folder covers

| File | Topic |
|------|-------|
| `01-setup-and-types.md` | Installing TypeScript, `tsconfig.json`, basic types, running TS in dev |
| `02-interfaces-and-generics.md` | Modeling data with interfaces/types, generics, utility types |
| `03-express-types.md` | Typing `req`, `res`, middleware, and extending `Request` |
| `04-error-types.md` | Typed custom errors, `unknown` in `catch`, error-handling middleware |
| `05-production-config.md` | Strict `tsconfig`, build pipeline, path aliases, linting, Docker |

## Why TypeScript for backend work

- **Refactoring confidence** — rename a field on a model and the compiler lists every place that breaks
- **Self-documenting code** — a function signature says what it needs and returns, without reading the body
- **Better editor tooling** — autocomplete for `req.body`, database results, and library APIs
- **Fewer runtime surprises** — `undefined is not a function` and typos in property names are caught early

## What TypeScript does *not* do

It is worth being clear about the limits up front:

- **Types are erased at runtime.** The compiled JavaScript contains no type information. A `req.body` typed as `CreateUserDto` is *not* validated — that is still the job of a validation library (`06-express/05-validation.md`, `09-api-development/04-validation.md`).
- **It does not replace tests.** Types catch shape mistakes, not logic mistakes (`13-testing/`).
- **It is not a performance feature.** The output is plain JavaScript running on the same V8 engine.

## Suggested reading order

1. `01-setup-and-types.md` — get a project compiling
2. `02-interfaces-and-generics.md` — model your data
3. `03-express-types.md` — apply it to the Express code from `06-express/`
4. `04-error-types.md` — type the error flow from `06-express/04-error-handling.md`
5. `05-production-config.md` — harden it for deployment (`16-production/`)

## Next

**`01-setup-and-types.md`** starts from an empty folder and gets a TypeScript Node project running.
