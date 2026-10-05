# Creating a Next.js App

`create-next-app` is the official scaffolding CLI. It generates a working Next.js project (config, TypeScript, linting, starter routes) so you can go from nothing to `localhost:3000` in under a minute. This note covers the Node.js prerequisites, the generator, what it produces, and how to fix the usual setup failures.

> Written for Next.js 16 (App Router). Flags and defaults change between releases, so run `npx create-next-app@latest --help` to see what your version supports.

---

## Prerequisites

| Requirement | Notes |
|---|---|
| Node.js | Next.js 16 needs **20.9 or newer**. Use an LTS release. |
| A package manager | npm ships with Node. pnpm, yarn and bun also work. |
| A code editor | VS Code is the common choice. |

Check what you have:

```bash
node -v
npm -v
```

If Node is missing or too old, install it through a version manager instead of the system installer. It lets you switch Node versions per project:

```bash
# nvm
nvm install --lts
nvm use --lts

# or fnm
fnm install --lts
fnm use lts-latest
```

**Package manager choice:** pick one per project and stay with it. Mixing managers produces conflicting lockfiles (`package-lock.json`, `pnpm-lock.yaml`, `yarn.lock`). Examples in this repo use npm unless stated.

---

## Creating the project

```bash
npx create-next-app@latest my-app
cd my-app
npm run dev
```

Open <http://localhost:3000>.

A few things people get wrong:

- **It is not a global install.** `npx` downloads and runs the CLI once; nothing stays on your machine.
- **`@latest` matters.** Without it, `npx` may reuse an older cached copy of the generator.
- **The project name must be a valid npm package name:** lowercase, no spaces. `My App` fails; `my-app` works.
- **Run it in the parent directory.** It creates a new folder. To scaffold into the current folder, pass `.` as the name, and make sure the folder is empty.

### The prompts

The CLI first asks whether to use the recommended defaults. In current versions these are TypeScript, ESLint, Tailwind CSS, the App Router, and Turbopack. Choose to customize if you want to change any of them. The options you can adjust include:

- TypeScript or JavaScript
- Linter (ESLint, or others depending on version)
- Tailwind CSS
- A `src/` directory
- App Router vs Pages Router (use the App Router; see [Pages Router](../01-fundamentals/04-pages-router.md))
- Import alias (default `@/*`)

Prompt wording changes between versions. The flags below skip prompts entirely, which is better for repeatable setups.

### Skipping the prompts with flags

```bash
npx create-next-app@latest my-app --ts --tailwind --eslint --app --src-dir --import-alias "@/*" --use-pnpm
```

| Flag | Effect |
|---|---|
| `--ts` / `--js` | TypeScript or JavaScript |
| `--tailwind` | Add Tailwind CSS |
| `--eslint` | Add ESLint |
| `--app` | Use the App Router |
| `--src-dir` | Put code under `src/` |
| `--import-alias "@/*"` | Set the path alias |
| `--use-npm` / `--use-pnpm` / `--use-yarn` / `--use-bun` | Choose the package manager |
| `--example <name-or-url>` | Start from an example in the Next.js repo or a GitHub URL |
| `--yes` | Accept defaults (or your saved preferences) without asking |
| `--skip-install` | Create files but do not install dependencies |
| `--disable-git` | Do not initialize a git repository |

### Starting from an example

```bash
npx create-next-app@latest --example with-supabase my-app
```

Useful for learning a specific integration. Treat examples as reference code: read them before building on top.

### Manual setup (no CLI)

You rarely need this, but it shows what the CLI does. A Next.js app is just three packages plus an `app/` folder:

```bash
npm install next@latest react@latest react-dom@latest
```

```json
{
  "scripts": {
    "dev": "next dev",
    "build": "next build",
    "start": "next start"
  }
}
```

```tsx
// app/layout.tsx
export default function RootLayout({ children }: { children: React.ReactNode }) {
  return (
    <html lang="en">
      <body>{children}</body>
    </html>
  )
}
```

```tsx
// app/page.tsx
export default function Page() {
  return <h1>Hello, Next.js</h1>
}
```

The root layout is required and must contain `<html>` and `<body>`.

---

## What gets generated

With the recommended defaults, you get roughly:

```text
my-app/
├── app/
│   ├── layout.tsx        # root layout (required)
│   ├── page.tsx          # the "/" route
│   └── globals.css       # global styles (Tailwind imported here)
├── public/               # static files served from "/"
├── next.config.ts        # Next.js configuration
├── tsconfig.json
├── eslint.config.mjs
├── postcss.config.mjs    # present when Tailwind is selected
├── package.json
└── .gitignore
```

Exact files vary with your answers and version. Two files you will also see but should not edit:

- `next-env.d.ts`: type declarations Next.js regenerates.
- `.next/`: the build output and dev cache (already git-ignored).

Folder responsibilities are covered in [Project Structure](../01-fundamentals/02-project-structure.md). The App Router itself is in [App Router](../01-fundamentals/01-app-router.md).

### The scripts

| Script | Command | Purpose |
|---|---|---|
| `dev` | `next dev` | Dev server with fast refresh. Turbopack is the default bundler in Next.js 16. |
| `build` | `next build` | Production build. Fails on type and build errors. |
| `start` | `next start` | Serve the production build. Run `build` first. |
| `lint` | `eslint` | Lint the project. |

On `lint`: Next.js 16 removed the `next lint` command, so generated projects call ESLint directly. If you see older tutorials using `next lint`, that is why they differ. Check your own `package.json`.

---

## First run checklist

```bash
npm run dev      # starts on http://localhost:3000
```

1. Edit `app/page.tsx` and save. The browser should update without a full reload.
2. Run `npm run build`. A clean build confirms TypeScript, lint and config are healthy before you write any real code.
3. Run `npm run start` to see the production build locally.

Doing the production build once at the start means later failures are clearly caused by your changes.

---

## Common mistakes and fixes

| Symptom | Cause | Fix |
|---|---|---|
| `You are using Node.js X. Next.js requires Node.js 20.9.0 or higher` | Node too old | Upgrade with nvm/fnm, then reinstall dependencies |
| `Could not create a project called "My App"` | Invalid npm name | Use lowercase, no spaces |
| `The directory my-app contains files that could conflict` | Target folder is not empty | Use a new folder name or empty the folder |
| `EADDRINUSE: address already in use :::3000` | Another process holds port 3000 | Stop it, or run `npm run dev -- -p 3001` |
| Old template, old defaults | Cached generator | Always use `create-next-app@latest` |
| Two lockfiles in the repo | Mixed package managers | Delete the one you do not use |
| Weird dev errors after switching Node versions | Stale install or cache | `rm -rf node_modules .next` and reinstall |
| Dependency or peer-version errors | React/Next mismatch | Make sure `next`, `react`, `react-dom` are on compatible versions |

Check what is actually installed:

```bash
npm ls next react react-dom
```

---

## Quick Summary

- `npx create-next-app@latest <name>` is the standard way to start. It is not a global install.
- Next.js 16 requires Node.js 20.9+; use nvm or fnm to manage versions.
- Prompts can be skipped with flags, which makes setups repeatable.
- The generated project has `app/`, `public/`, and config files; `dev`, `build` and `start` are the core scripts.
- Run a production build right after scaffolding to confirm a healthy baseline.
- Pick one package manager and keep one lockfile.

**Next:** [TypeScript setup](./01-typescript.md), then [Linting and formatting](./02-linting-and-formatting.md).