# 00 · Setup

Get a Next.js project running and configured the way a real project should be: TypeScript, linting, environment variables, framework config and a daily development workflow. Nothing here is Next.js-specific *theory*; it is the groundwork the rest of the repository assumes.

> Written for Next.js 16 (Node.js 20.9+). Version-specific differences are called out inside each note.

## Reading order

| # | Note | You will learn |
|---|---|---|
| 00 | [Create a Next.js App](./00-create-next-app.md) | Prerequisites, `create-next-app`, what it generates, the three core scripts |
| 01 | [TypeScript](./01-typescript.md) | How Next.js configures TypeScript, `tsconfig.json`, path aliases, type helpers |
| 02 | [Linting and Formatting](./02-linting-and-formatting.md) | ESLint flat config, Prettier, how they coexist, CI usage |
| 03 | [Environment Variables](./03-environment-variables.md) | `.env*` files, load order, `NEXT_PUBLIC_`, build-time vs runtime |
| 04 | [next.config](./04-next-config.md) | `next.config.ts`, the options you will actually use |
| 05 | [Development Workflow](./05-development-workflow.md) | Dev vs build vs start, Turbopack, debugging, a sensible daily loop |

Read them in order the first time. Later, use them as a reference:

- Variable is `undefined`? → [03](./03-environment-variables.md)
- Need a redirect, header or remote image host? → [04](./04-next-config.md)
- Editor and CI disagree about errors? → [01](./01-typescript.md) and [02](./02-linting-and-formatting.md)

## After this chapter you will have

- A project that runs with `npm run dev` and builds with `npm run build`
- Strict TypeScript, a working `@/` import alias
- ESLint (and optionally Prettier) wired into scripts
- Correct `.env` handling that keeps secrets out of the browser and out of git
- A mental model of dev vs production behavior

## Next

[01 · Fundamentals](../01-fundamentals/README.md): what Next.js is and how the App Router is organized.
