# Build

Your source code (TypeScript, JSX, modern CSS, hundreds of modules) isn't what browsers run in production. A **build** turns it into a small set of optimized static files. Understanding what the build does, and what it doesn't, prevents most "works in dev, breaks in prod" surprises.

## Dev vs production

| | `vite` (dev server) | `vite build` (production) |
|---|---|---|
| Modules | Served **individually** over native ESM, transformed on demand | **Bundled** into a few optimized chunks |
| Speed priority | Instant startup, fast updates (HMR) | Small output, fast loading |
| Minification | No | Yes |
| Tree shaking / code splitting | Not applied | Applied |
| React | Development build: extra warnings, StrictMode double-invoking | Production build: smaller, faster |
| Env | `import.meta.env.DEV = true` | `PROD = true` |
| Type checking | **No** (transpile only) | **No**, unless you add it |

Because dev and production differ, bugs can appear only in production (minification issues, missing env vars, chunk-loading, base paths). **Always test the production build before shipping**, not just the dev server.

## What `vite build` does

```bash
npm run build      # typically: tsc -b && vite build
```

1. **Transpiles** TypeScript and JSX to JavaScript (no type checking).
2. **Resolves and bundles** the module graph into chunks; each dynamic `import()` becomes its own chunk ([code splitting](../14-performance/03-code-splitting-and-lazy-loading.md)).
3. **Tree-shakes** unused exports ([bundle optimization](../14-performance/05-bundle-optimization.md#tree-shaking-how-it-works-and-how-to-help-it)).
4. **Minifies** JavaScript and CSS.
5. **Inlines env variables** (`import.meta.env.VITE_*`, [00](./00-environment-variables.md#build-time-vs-runtime-configuration)).
6. **Hashes file names** for caching (`index-a1b2c3d4.js`).
7. **Emits** to `dist/`, rewriting `index.html` to reference the hashed files (plus `modulepreload` links for critical chunks).

The output:

```text
dist/
├── index.html
├── assets/
│   ├── index-a1b2c3d4.js          # entry chunk
│   ├── reports-9f8e7d6c.js        # lazy route chunk
│   ├── vendor-5b4a3c2d.js         # shared dependencies
│   └── index-e1f2a3b4.css
└── (files copied from public/: favicon, robots.txt, config.js …)
```

Vite's internal bundler has changed across major versions, so **configuration details** (such as chunking options) differ slightly. Check the docs for your version, and treat the *behavior* above as stable.

## Type checking is separate

Vite **only strips types**; it doesn't check them. A type error won't fail `vite build`. That's why the standard script chains them:

```json
{
  "scripts": {
    "dev": "vite",
    "build": "tsc -b && vite build",
    "preview": "vite preview",
    "typecheck": "tsc -b --noEmit",
    "lint": "eslint ."
  }
}
```

`tsc -b` runs first, so type errors fail the build. In CI, run **typecheck, lint, tests, and build as separate steps** for clear failures ([CI/CD](./04-ci-cd.md)).

## Preview the production build

```bash
npm run build
npm run preview        # serves dist/ at http://localhost:4173
```

`vite preview` is for **local verification only**. It is not a production server (no caching headers, compression, or hardening). Use it to check:

- The app loads and routes work (including a **refresh** on a deep link, which tests the SPA fallback).
- No console errors; env values are correct; lazy chunks load.
- Performance on the real bundle ([profiling](../14-performance/00-profiling-and-measuring.md#test-under-realistic-conditions)), and E2E tests ([Playwright](../18-testing-and-debugging/06-e2e-testing-playwright.md#configuration)) run best against this.

## Important build options

```ts
// vite.config.ts
export default defineConfig({
  base: "/",                       // public path prefix; "/app/" if deployed under a subpath
  build: {
    outDir: "dist",
    target: "es2022",              // browsers you support (smaller output for modern targets)
    sourcemap: "hidden",           // generate maps, but don't reference them from the JS
    chunkSizeWarningLimit: 500,    // kB: warn on large chunks
    assetsInlineLimit: 4096,       // inline assets smaller than 4 kB as base64
  },
})
```

### `base`: deploying under a subpath

If the app lives at `https://example.com/app/` (not the domain root), set `base: "/app/"` **and** the router's `basename`:

```ts
createBrowserRouter(routes, { basename: import.meta.env.BASE_URL })
```

A wrong `base` is the classic "blank page, 404 on every JS file" bug, so check the Network panel for asset requests to the wrong path.

### `target`

Defines which JavaScript syntax the output may use. Modern targets mean smaller, faster code; older targets add transforms and bytes. Choose from your real browser support data ([bundle optimization](../14-performance/05-bundle-optimization.md#modern-browser-targets)).

### `sourcemap`

| Setting | Effect |
|---|---|
| `false` (default) | No source maps. Production errors show minified stack traces |
| `true` | Emits `.map` files **and** references them in the JS, so the browser downloads them when DevTools opens, and your source becomes easy to read |
| `"hidden"` | Emits `.map` files but **doesn't reference** them. Upload them to your error-monitoring service and don't serve them publicly |
| `"inline"` | Embeds maps in the JS (huge; dev only) |

Recommended for production: **`"hidden"`**, uploaded to your monitoring tool and then **deleted from `dist/`** before deploying ([error monitoring](./06-error-monitoring-and-logging.md#source-maps)). That gives readable traces without publishing source.

## Static assets: `public/` vs imports

```tsx
import logo from "./assets/logo.svg"          // processed: hashed, optimized, small ones inlined
<img src={logo} />

<img src="/favicon.svg" />                     // from public/: copied as-is, no hashing, referenced by absolute path
```

- **Imported assets** get content hashes, so they can be cached forever ([network performance](../14-performance/06-network-performance.md#http-caching)) and unused ones are dropped.
- **`public/`** files are copied untouched. Use it for things that need a fixed URL (`robots.txt`, `favicon`, `config.js`, files referenced from outside your code). Because they're not hashed, their caching needs care.

## Reproducible builds

The same commit should produce the same output, on your laptop and in CI.

- **Commit the lockfile** (`package-lock.json`/`pnpm-lock.yaml`) and install with **`npm ci`** (clean install exactly from the lockfile; fails if it's out of sync) in CI and Docker, never `npm install`.
- **Pin the Node version** (`.nvmrc` or `"engines"` in `package.json`, and the same in CI and Docker). Use a current LTS.
- **Don't depend on the environment**: no reading from untracked local files, absolute paths, or timestamps in output (except deliberately, below).
- **Build once, then promote** the artifact through environments where you can ([CI/CD](./04-ci-cd.md#build-once-deploy-many)).

## Version stamping

Knowing *which build* is running is essential for debugging and error monitoring:

```ts
// vite.config.ts
import { execSync } from "node:child_process"

const commit = process.env.GITHUB_SHA ?? execSync("git rev-parse --short HEAD").toString().trim()

export default defineConfig({
  define: {
    __APP_VERSION__: JSON.stringify(commit),
    __BUILD_TIME__: JSON.stringify(new Date().toISOString()),
  },
})
```

```ts
// src/vite-env.d.ts
declare const __APP_VERSION__: string
declare const __BUILD_TIME__: string
```

Use it as the **release** identifier in error reports ([06](./06-error-monitoring-and-logging.md#context-that-makes-errors-actionable)) and performance metrics ([07](./07-performance-monitoring.md)), and show it somewhere in the UI (a footer or about page) so support can ask "which version are you on?". The build timestamp makes the output non-deterministic, so use the commit SHA alone if you need byte-identical builds.

## Verifying a build

Before deploying, check:

- **Size**: look at the chunk sizes in the build output, run the [analyzer](../14-performance/05-bundle-optimization.md#analyze-first), and compare with your budget.
- **Chunks load**: navigate to lazy routes in the preview build.
- **Env**: confirm the correct API URL is baked in (search `dist/` for the expected URL, and **for anything that shouldn't be there**: `grep -r "sk_live" dist/`).
- **No source maps leaking** if you intended hidden ones: `ls dist/assets/*.map`.
- **No dev code**: no devtools, no `console.log` spam, no test fixtures.
- **Refresh on a deep link** works (the SPA fallback, [deployment](./02-deployment.md#spa-fallback-routing)).
- **Tests pass against the built output** (E2E).

## Beyond the basics

- **Legacy browser support** (a legacy plugin generating separate bundles with polyfills) increases build size and complexity. Only add it if your analytics show you need it.
- **CSS** is extracted per chunk and minified. Tailwind keeps only the classes it finds in your source.
- **Compression** (gzip/Brotli) is usually applied by the host or CDN at serve time, but you can pre-compress at build time with a plugin if your server serves static `.br`/`.gz` files.
- **Build performance**: caching `node_modules` in CI and avoiding giant dependencies keep builds fast.
- **Monorepos** add task-orchestration concerns (build only what changed). That's tooling-specific, so check your tool's docs.

## Common mistakes

- **Only testing the dev server**, and shipping a build never run.
- **Assuming `vite build` type-checks.** It doesn't; add `tsc`.
- **Using `npm install` in CI** instead of `npm ci`, so builds aren't reproducible.
- **Unpinned Node versions** that differ between local, CI, and Docker.
- **Wrong `base`** for a subpath deploy, causing blank pages and asset 404s.
- **Serving source maps publicly** (`sourcemap: true`) and exposing your source.
- **No source maps anywhere**, so production errors are unreadable.
- **Using `vite preview` as a production server.**
- **Secrets in `VITE_*` variables or committed files** that end up in `dist/`.
- **Not stamping a version**, so you can't tell which release produced an error.
- **Ignoring chunk-size warnings** until the bundle is huge.
- **Forgetting that `public/` files aren't hashed**, so caching and invalidation differ.

## Quick summary

- Dev serves modules individually; **`vite build`** bundles, tree-shakes, code-splits, minifies, inlines env, hashes file names, and emits `dist/`.
- **Vite doesn't type-check**: chain `tsc -b && vite build`, and run lint and tests as separate CI steps.
- **Test the production build** with `vite preview` (local verification only).
- Set `base` for subpath deploys, `target` for your browsers, and **`sourcemap: "hidden"`** (upload to monitoring; don't serve publicly).
- Imported assets are hashed; `public/` files are copied as-is.
- Make builds **reproducible** (`npm ci`, committed lockfile, pinned Node) and **stamp a version** (commit SHA) for monitoring.
- Verify the output: size, chunks, env, no leaked secrets or maps.

## Next

[02 — Deployment](./02-deployment.md)
