# Development Workflow

A good Next.js workflow separates three activities: **developing** (`next dev`), **verifying** (`next build` plus checks), and **running production** (`next start`). Most "works on my machine" problems come from only ever using the first one.

> Written for Next.js 16, where Turbopack is the default bundler for both `dev` and `build`.

## The daily loop

```bash
npm run dev          # write code, the browser updates as you save
npm run typecheck    # tsc --noEmit
npm run lint
npm run build        # before pushing anything important
npm run start        # optional: try the real production server
```

A practical `package.json`:

```json
{
  "scripts": {
    "dev": "next dev",
    "build": "next build",
    "start": "next start",
    "lint": "eslint",
    "typecheck": "tsc --noEmit",
    "format": "prettier --write ."
  }
}
```

## What `next dev` does

- Compiles routes **on demand**: a page is built the first time you visit it.
- **Fast Refresh** applies edits instantly and keeps component state when possible.
- Shows an error overlay with stack traces and hints for hydration, build and runtime errors.
- Runs with development-only behavior: React runs in Strict Mode (components render twice to surface impure code), and caching and rendering are less aggressive than in production.

It does **not**: type-check, lint, or reproduce production caching and static optimization.

### Bundler

Next.js 16 uses **Turbopack** by default. To opt out and use webpack (for a plugin that is not supported yet):

```bash
next dev --webpack
next build --webpack
```

## Verifying with `next build`

```bash
npm run build
```

The build type-checks, compiles and prerenders what it can. Read the route table it prints:

```text
Route (app)
┌ ○ /
├ ○ /about
├ ƒ /dashboard
└ ƒ /blog/[slug]

○  (Static)   prerendered as static content
ƒ  (Dynamic)  server-rendered on demand
```

If a route you expected to be static shows as dynamic, something in it uses request-time data (cookies, headers, uncached fetches). Newer versions may show an additional symbol for partially prerendered routes. See [Rendering Overview](../04-rendering/00-rendering-overview.md).

Then try it for real:

```bash
npm run start
```

Things that can differ from `dev`: caching, static vs dynamic rendering, error pages, performance, and environment variables baked in at build time.

## Debugging

### Server code (Server Components, Route Handlers, Actions)

Start the dev server with the Node inspector:

```bash
NODE_OPTIONS='--inspect' next dev
```

Then open `chrome://inspect` in Chrome, or attach VS Code. A ready-made VS Code launch configuration:

```json
// .vscode/launch.json
{
  "version": "0.2.0",
  "configurations": [
    {
      "name": "Next.js: debug server-side",
      "type": "node-terminal",
      "request": "launch",
      "command": "npm run dev"
    }
  ]
}
```

Breakpoints in server code then work from the editor. Client code is debugged with browser DevTools as usual.

### Reading logs

`console.log` in a Server Component prints in the **terminal** running `next dev`, not in the browser. `console.log` in a Client Component prints in the **browser console** (and also once in the terminal during server prerender). Knowing where to look saves a lot of confusion. More in [Debugging Tools](../19-debugging/00-debugging-tools.md).

### Bug reports

```bash
npx next info
```

Prints your Next, React, Node and OS versions in a form ready to paste into an issue.

## The `.next` folder

`.next/` holds build output and caches. It is generated and git-ignored. If you see behavior that makes no sense after big changes (stale pages, odd module errors):

```bash
rm -rf .next
npm run dev
```

## Version control hygiene

Make sure these are ignored (the generated `.gitignore` already covers them):

```text
node_modules
.next
.env*.local
next-env.d.ts
```

Commit: lockfile, `.env.example`, config files, `.nvmrc`.

## Suggested CI order

Cheap checks first, so failures surface quickly:

```text
install → lint → typecheck → format:check → build → tests
```

Remember that `next build` does not run linting in Next 16, so `lint` must be its own step. See [Linting and Formatting](./02-linting-and-formatting.md).

## Common mistakes and debugging

| Symptom | Cause | Fix |
|---|---|---|
| Works in `dev`, fails in `build` | Type error, or code relying on dev-only behavior | Run `build` before pushing |
| Works in `build`, wrong in production | Env var missing on host, or build-time value baked in | Compare host env vars; see [Environment Variables](./03-environment-variables.md) |
| Log not visible | Looking in the wrong place | Server code → terminal; client code → browser console |
| Page does not update, or odd module errors | Stale `.next` cache | `rm -rf .next` |
| Component logs twice in dev | React Strict Mode | Expected; not present in production |
| Hydration mismatch overlay | Server and client output differ | See [Hydration Errors](../19-debugging/01-hydration-errors.md) |
| Config change ignored | Config read at startup | Restart the dev server |
| Port in use | Another dev server still running | Stop it or use `-p` |

## Quick Summary

- `dev` is for writing code; it type-checks nothing and behaves differently from production.
- Use `build` + `start` to verify real behavior; read the route table to see static vs dynamic.
- Turbopack is the default in Next 16; `--webpack` opts out.
- Server logs go to the terminal, client logs to the browser; attach the inspector for server breakpoints.
- Delete `.next` when behavior is inexplicable; restart after config or env changes.
- CI: lint, typecheck, format check, build, tests.

## Next

- [Fundamentals: Overview](../01-fundamentals/00-overview.md)
- [Debugging Tools](../19-debugging/00-debugging-tools.md)
- [Production Build](../22-production/00-production-build.md)
