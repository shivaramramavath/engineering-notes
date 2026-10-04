# 01 — Fundamentals

With Node installed and npm/`package.json` understood, this section covers how Node actually works under the hood — what makes it different from running JavaScript in a browser, what's available globally without an import, how its module system works, and how configuration reaches your app via environment variables.

## In this section

| File                          | Covers                                                                                     |
| ----------------------------- | ------------------------------------------------------------------------------------------ |
| `01-node-runtime.md`          | What Node actually is — V8 plus a set of APIs — and how it differs from browser JavaScript |
| `02-global-objects.md`        | What's available everywhere without an import: `global`, `console`, timers, and more       |
| `03-module-system.md`         | CommonJS vs ES Modules — `require` vs `import`, and how Node decides which one applies     |
| `04-environment-variables.md` | `process.env`, `.env` files, and how configuration reaches a running app                   |

## Why this comes before core modules

`02-core-modules` covers specific built-in modules (`fs`, `http`, `events`, and so on) in depth — this section covers the concepts that make sense of _why_ those modules work the way they do: what "the event loop" even runs on top of, why some things are globally available and others need an import, and why you'll see both `require()` and `import` across different Node codebases.

## What you should be able to do after this section

- Explain, at a high level, what Node actually is and why it can run JavaScript outside a browser
- Use globals like `console`, `setTimeout`, and `process` without needing to import them
- Recognize whether a given file is CommonJS or ES Modules, and know why that matters
- Read and set environment variables, and understand why `process.env` values are always strings

## Next

**`02-core-modules`** puts these fundamentals to work through Node's actual built-in modules — `fs`, `http`, `events`, `streams`, and more.
