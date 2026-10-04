# Vite

Vite is the build tool and dev server used by most single-page React apps. It starts almost instantly, updates the browser as you edit (hot module replacement), and produces an optimized bundle for production. This file covers creating a project, the commands you'll use daily, and the config file.

## Prerequisites

[`01-node-and-npm.md`](./01-node-and-npm.md) — a working Node.js install.

---

## Why Vite

- **Fast dev server** — serves your source as native ES modules instead of bundling everything up front, so startup doesn't slow down as the app grows.
- **Hot module replacement (HMR)** — edits appear in the browser without a full reload, and React component state is usually preserved.
- **Production build** — bundles, minifies, and code-splits your app for deployment.
- **TypeScript and JSX out of the box** — no extra setup.

If you later use a full framework (Next.js, Remix, React Router framework mode), it provides its own tooling. Vite is the right choice for learning React itself and for client-rendered apps.

---

## Creating a project

```bash
npm create vite@latest my-app -- --template react-ts
cd my-app
npm install
npm run dev
```

`react-ts` is React with TypeScript. Use `react` for plain JavaScript. The dev server prints a local URL (typically `http://localhost:5173`); open it and you should see the starter page.

---

## Daily commands

| Command | What it does |
|---------|--------------|
| `npm run dev` | Start the dev server with HMR |
| `npm run build` | Type-check and produce an optimized build in `dist/` |
| `npm run preview` | Serve the production build locally to test it |
| `npm run lint` | Run ESLint |

Always run `preview` before deploying — the production build can behave differently from dev (minification, environment variables, stricter bundling).

---

## `vite.config.ts`

```ts
import { defineConfig } from "vite";
import react from "@vitejs/plugin-react";
import path from "node:path";

export default defineConfig({
  plugins: [react()],
  resolve: {
    alias: {
      "@": path.resolve(__dirname, "./src"),
    },
  },
  server: {
    port: 5173,
    proxy: {
      "/api": "http://localhost:3000",
    },
  },
});
```

- **`plugins: [react()]`** — enables JSX transform and Fast Refresh.
- **`resolve.alias`** — lets you write `import { Button } from "@/components/Button"` instead of long `../../` paths. TypeScript needs a matching `paths` entry (see [`05-typescript-and-linting-setup.md`](./05-typescript-and-linting-setup.md)).
- **`server.proxy`** — forwards `/api` requests to a backend during development, avoiding CORS problems.

---

## Environment variables

Vite exposes only variables prefixed with `VITE_` to client code:

```bash
# .env
VITE_API_URL=https://api.example.com
```

```ts
const apiUrl = import.meta.env.VITE_API_URL;
```

Anything in client code is visible to users. **Never put secrets in `VITE_` variables.** Full coverage in `../19-production/00-environment-variables.md`.

---

## Common mistakes

- **Expecting `process.env`** — in Vite client code use `import.meta.env`.
- **Putting secrets in `VITE_` variables** — they are bundled into public JavaScript.
- **Skipping `npm run preview`** — dev and production builds can differ.
- **Adding an alias in Vite but not in `tsconfig`** — the app runs but the editor reports unresolved imports.
- **Editing files in `dist/`** — it's regenerated on every build.

## Quick summary

- Vite provides a fast dev server with HMR and an optimized production build
- Create projects with `npm create vite@latest -- --template react-ts`
- Daily loop: `dev` while coding, `build` + `preview` before shipping
- Configure plugins, aliases, and the dev proxy in `vite.config.ts`
- Only `VITE_`-prefixed variables reach the browser, and they are public

## Next

**[`03-project-structure.md`](./03-project-structure.md)** walks through every file Vite generated.
