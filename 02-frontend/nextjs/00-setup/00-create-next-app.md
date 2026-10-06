# Create a Next.js App

`create-next-app` is the official scaffolding tool for Next.js. It generates a working project (dependencies, config, a starter route) so you can run `npm run dev` within a minute. This note covers the prerequisites, the generator, what it produces, and the problems you are most likely to hit on day one.

> Written for Next.js 16. Prompts and defaults change between major versions. Check `npx create-next-app@latest --help` if yours differ.

## Prerequisites

You need **Node.js** and a **package manager**.

```bash
node -v    # Next.js 16 requires Node.js 20.9 or newer
npm -v
```

Next.js only runs on supported Node versions, and old ones are dropped in major releases. If `node -v` is too old, install a version manager instead of upgrading system Node by hand:

```bash
# fnm / nvm: switch Node versions per project
fnm install 22
fnm use 22
```

`npm` ships with Node. `pnpm`, `yarn` and `bun` also work. Pick one per project and stay with it; mixing them leaves multiple lockfiles and inconsistent installs.

## Creating a project

```bash
npx create-next-app@latest my-app
```

`@latest` matters. Without it, `npx` may reuse a cached older generator and scaffold an outdated project.

Recent versions first ask whether to use the **recommended defaults** (TypeScript, ESLint, Tailwind CSS, App Router, Turbopack, `@/*` import alias). Choosing to customize shows the individual prompts:

| Prompt | What it controls |
|---|---|
| TypeScript | `.tsx` files and `tsconfig.json` instead of JavaScript |
| Linter | Which linter is configured (ESLint, or an alternative / none, depending on version) |
| Tailwind CSS | Installs and configures Tailwind |
| `src/` directory | Puts `app/` inside `src/` instead of the project root |
| App Router | Uses `app/` (recommended) instead of the legacy `pages/` |
| Import alias | Enables `@/*` imports so you avoid `../../../` paths |

Everything can be answered up front with flags, which is useful for scripts and for reproducing a setup:

```bash
npx create-next-app@latest my-app \
  --ts --tailwind --eslint --app --src-dir \
  --import-alias "@/*" --use-pnpm
```

`--yes` accepts defaults without prompting. To scaffold into the current empty folder, use `.` as the name:

```bash
mkdir my-app && cd my-app
npx create-next-app@latest .
```

## What you get

With TypeScript, the App Router and no `src/` directory:

```text
my-app/
├── app/
│   ├── layout.tsx       # root layout, wraps every page
│   ├── page.tsx         # the "/" route
│   └── globals.css
├── public/              # static files served from "/"
├── next.config.ts       # framework configuration
├── tsconfig.json
├── next-env.d.ts        # generated types, do not edit
├── package.json
└── package-lock.json    # or pnpm-lock.yaml / yarn.lock
```

`package.json` has three runtime dependencies and a few scripts:

```json
{
  "scripts": {
    "dev": "next dev",
    "build": "next build",
    "start": "next start"
  },
  "dependencies": {
    "next": "...",
    "react": "...",
    "react-dom": "..."
  }
}
```

Generated scripts vary slightly by version (for example the lint script), so read your own `package.json` once. The folder layout and special files (`layout.tsx`, `page.tsx`) are explained in [Project Structure](../01-fundamentals/02-project-structure.md).

## Running it

```bash
cd my-app
npm run dev
```

Open <http://localhost:3000>. Editing `app/page.tsx` updates the browser without a manual refresh.

The three scripts are three different modes:

| Script | Mode | Use it for |
|---|---|---|
| `dev` | Development server with fast refresh and detailed errors | Day-to-day coding |
| `build` | Produces an optimized production build | CI, deploying, catching build errors |
| `start` | Serves the output of `build` | Testing production behavior locally |

`start` needs a prior `build`; without one it fails.

A common misconception: `dev` is not "production, only slower". Caching, rendering and error behavior differ between `dev` and a production build, so verify anything important with `build` followed by `start`. See [Development Workflow](./05-development-workflow.md).

## Changing the port

```bash
npm run dev -- -p 4000
```

The extra `--` passes the flag through npm to `next dev`.

## Common mistakes and debugging

| Symptom | Likely cause | Fix |
|---|---|---|
| `You are using Node.js X. Next.js requires Node.js Y` | Node too old | Upgrade via fnm/nvm, reopen the terminal |
| Scaffolded project looks outdated | Cached generator | Re-run with `create-next-app@latest` |
| `name can no longer contain capital letters` | npm package names must be lowercase | Use a lowercase project name |
| `EADDRINUSE: port 3000 already in use` | Another process owns the port | Stop it, or run with `-p` |
| `next start` says there is no production build | `build` was not run first | `npm run build`, then `npm start` |
| Odd install errors after switching package managers | Two lockfiles or stale `node_modules` | Delete `node_modules` and the extra lockfile, reinstall with one manager |

Pin the Node version so the whole team gets the same behavior. A `.nvmrc` file is enough:

```text
22
```

## Quick Summary

- Requires a supported Node.js version (20.9+ for Next.js 16); manage versions with fnm or nvm.
- Use `npx create-next-app@latest`, answer prompts or pass flags.
- The result is minimal: `app/`, `public/`, `next.config.ts`, `tsconfig.json` and three core scripts.
- `dev` for coding, `build` to verify, `start` to run the build.
- One package manager per project; commit its lockfile.

## Next

- [TypeScript](./01-typescript.md)
- [Linting and Formatting](./02-linting-and-formatting.md)
- [Project Structure](../01-fundamentals/02-project-structure.md)
