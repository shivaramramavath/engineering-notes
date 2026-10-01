# Bundlers and Tree Shaking

A **bundler** reads your module graph starting from entry points and produces optimized files for the browser (or for Node). Tree shaking, code splitting and minification make production bundles small and fast.

```
src/main.js ─┐
src/ui.js    ├──► bundler ──► dist/main.[hash].js  (+ chunks, CSS, assets, source maps)
node_modules ┘
```

## Why bundle?

| Reason | Detail |
|--------|--------|
| **Bare specifiers** | browsers cannot resolve `import "lodash-es"` without import maps |
| **Fewer requests / waterfalls** | deeply nested imports cause sequential network round trips |
| **Optimization** | minify, tree-shake, scope-hoist, compress |
| **Transform** | TypeScript, JSX, modern syntax down-leveling, CSS, images, assets |
| **Cache busting** | content-hashed file names |
| **Compatibility** | polyfills and target-specific builds |
| **Dev experience** | hot module replacement, source maps |

Native ESM plus HTTP/2/3 reduces the need for bundling small apps, but production apps with hundreds of modules still benefit.

## The landscape

| Tool | Notes |
|------|-------|
| **Vite** | dev server serving native ESM + esbuild pre-bundling, production build by Rollup (Rolldown in newer versions); the common default for apps |
| **Rollup** | best for libraries, excellent tree shaking, ESM-first |
| **esbuild** | extremely fast (Go), great for libraries, simple builds |
| **webpack** | mature, huge plugin ecosystem, flexible, slower |
| **Rspack** | webpack-compatible, Rust, faster |
| **Parcel** | zero-config |
| **Bun** | built-in bundler with the runtime |
| **tsup / unbuild / tsdown** | library builders on top of esbuild/Rollup/Rolldown |
| **SWC / Babel** | transpilers (not bundlers) |

## Minimal config examples

```bash
npm create vite@latest my-app && cd my-app && npm i && npm run dev
npm run build            # production bundle in dist/
```

```js
// vite.config.js
import { defineConfig } from "vite";
export default defineConfig({
  build: { target: "es2022", sourcemap: true },
  resolve: { alias: { "@": new URL("./src", import.meta.url).pathname } },
});
```

```bash
npx esbuild src/index.js --bundle --minify --sourcemap --outfile=dist/out.js
```

## Tree shaking

**Tree shaking** removes exports that are never used ("dead code elimination" guided by the module graph).

```js
// utils.js
export function used() { return 1; }
export function unused() { return 2; }      // removed from the bundle if nobody imports it

// main.js
import { used } from "./utils.js";
console.log(used());
```

### What makes it work

| Requirement | Why |
|-------------|-----|
| **ES modules** (static `import`/`export`) | the bundler can see what is used before running anything |
| **No side effects at module top level** | otherwise the bundler must keep the code in case it matters |
| `"sideEffects": false` (or a file list) in `package.json` | promises that unused imports of the package are safe to drop |
| `/* @__PURE__ */` annotations | marks a call as removable if its result is unused |
| Production mode / minifier | the final dead-code removal is done by the minifier (Terser, esbuild, SWC) |

```json
{ "sideEffects": false }
{ "sideEffects": ["./src/polyfills.js", "**/*.css"] }
```

```js
export const instance = /* @__PURE__ */ createThing();    // removable when unused
```

### What breaks tree shaking

| Cause | Fix |
|-------|-----|
| CommonJS modules (`require`, `module.exports`) | use ESM builds (`lodash-es`, not `lodash`) |
| `import * as ns` with dynamic property access (`ns[name]`) | import specific names |
| Top-level side effects (`window.foo = ...`, registering things) | isolate in separate modules, mark in `sideEffects` |
| Barrel files (`index.js` re-exporting everything) with side effects | import from specific files or set `sideEffects: false` |
| Classes with static initialization / decorators | keep them lean or annotate |
| Transpiling ESM to CJS **before** bundling (Babel `modules: "commonjs"`) | keep ESM (`modules: false`) |
| Importing a default object (`import _ from "lodash"; _.map`) | named imports from an ESM build |

```js
// bad: pulls the whole library
import _ from "lodash";
_.debounce(fn, 100);

// good
import debounce from "lodash-es/debounce.js";
import { debounce } from "lodash-es";            // fine with a tree-shakable ESM build
```

## Code splitting

Break the bundle into **chunks** loaded on demand.

```js
// route-level splitting
const routes = {
  "/": () => import("./pages/Home.js"),
  "/settings": () => import("./pages/Settings.js"),
};

// component-level
button.onclick = async () => {
  const { renderChart } = await import("./chart.js");
  renderChart(data);
};
```

| Kind | How |
|------|-----|
| **Entry splitting** | multiple entry points (multi-page apps) |
| **Dynamic import** | each `import()` becomes a chunk |
| **Vendor chunks** | `manualChunks` / `splitChunks` group dependencies for caching |
| **Shared chunks** | modules used by several chunks are extracted automatically |
| **Prefetch / preload** | `<link rel="modulepreload">`, `/* webpackPrefetch: true */` hints |

```js
// vite.config.js
build: { rollupOptions: { output: { manualChunks: { vendor: ["react", "react-dom"] } } } }
```

Frameworks (React.lazy, Vue async components, SvelteKit, Next.js) build on `import()`.

## Production optimizations

| Step | Effect |
|------|--------|
| **Minification** | shorter identifiers, removed whitespace/dead branches (Terser, esbuild, SWC) |
| **Scope hoisting** | concatenates modules into one scope to cut wrapper overhead |
| **Content hashing** | `main.a1b2c3.js` for long-term caching |
| **Compression** | gzip/brotli at the server or CDN |
| **Source maps** | map minified code back to source (upload privately to error trackers) |
| **Target selection** | `target: "es2022"` avoids unnecessary syntax down-leveling |
| **CSS extraction/minification** | separate CSS files, purging unused CSS |
| **Asset handling** | images/fonts hashed, small ones inlined as data URIs |
| **Environment replacement** | `process.env.NODE_ENV` and flags replaced at build time so dead branches vanish |

```js
if (process.env.NODE_ENV !== "production") {
  validateProps();                 // removed from production bundles
}
```

## Dev server vs production build

| | Dev | Production |
|---|-----|-----------|
| Vite | native ESM per module, instant start, HMR | Rollup bundle with splitting/minification |
| webpack | in-memory bundle with HMR | optimized chunks |
| Goal | fast feedback | small, cacheable output |

Differences between dev and prod can hide bugs: always test the **production build** (`vite preview`).

## Library builds

| Practice | Detail |
|----------|--------|
| Emit **ESM** (and CJS if needed) + type declarations | see the previous file |
| Mark dependencies as **external** | do not bundle `react`, `lodash-es`; list in `dependencies`/`peerDependencies` |
| Preserve modules (`preserveModules`/unbundled output) | lets consumers tree-shake at file granularity |
| Set `sideEffects` accurately | enables consumer tree shaking |
| Avoid top-level side effects | |
| Keep the public surface in `exports` | |

```js
// rollup.config.js
export default {
  input: "src/index.js",
  external: [/^react/, "lodash-es"],
  output: [{ dir: "dist", format: "es", preserveModules: true }],
};
```

## Analyzing bundle size

| Tool | Use |
|------|-----|
| `rollup-plugin-visualizer`, `vite-bundle-visualizer` | treemap of what is in the bundle |
| `webpack-bundle-analyzer` | same for webpack |
| `source-map-explorer` | inspect via source maps |
| bundlephobia / `packagephobia` | package cost before installing |
| `esbuild --analyze`, `--metafile` | quick analysis |
| Lighthouse / WebPageTest | real-user impact |

Typical wins: replace heavy libraries (moment → date-fns/Temporal), lazy-load rarely used features, import only needed functions, use modern browser targets, remove duplicate versions of a dependency.

## Performance budgets

Set a limit and fail CI when exceeded:

```json
{ "bundlesize": [{ "path": "dist/*.js", "maxSize": "170 kB", "compression": "brotli" }] }
```

Watch **JavaScript execution time** (parse + run), not only transfer size.

## Polyfills

| Approach | Notes |
|----------|-------|
| Target modern browsers | smallest output |
| `core-js` with `@babel/preset-env` `useBuiltIns: "usage"` | adds only needed polyfills |
| Differential loading | `<script type="module">` for modern, `nomodule` fallback for legacy (rarely needed now) |
| Polyfill services | on-demand polyfills by user agent |

## Unbundled and no-build options

- **Import maps + native ESM** for small apps, prototypes, demos
- **CDNs** (`esm.sh`, `jsdelivr`, `unpkg`) serving ESM builds
- **Deno/Bun** run TypeScript and ESM directly on the server
- Trade-off: more requests, no tree shaking, no minification

## Common bundler errors

| Error | Cause |
|-------|-------|
| `Failed to resolve import "x"` | missing dependency or bad alias/extension |
| `"x" is not exported by "y"` | importing a name the module lacks (CJS interop or typo) |
| `Cannot use import statement outside a module` | file treated as CJS or script |
| `require is not defined` in the browser | CJS code not transformed |
| Circular dependency warnings | restructure modules |
| Duplicate package versions in the bundle | dedupe (`npm dedupe`, `resolutions`/`overrides`) |
| Huge chunk size warning | split or lazy-load |

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| Importing CJS-only packages | No tree shaking, big bundles | Choose ESM-ready libraries |
| Barrel files re-exporting hundreds of modules | Slow dev server, accidental inclusion | Import directly or ensure `sideEffects: false` |
| Missing `sideEffects` field in your library | Consumers cannot shake it | Declare it accurately |
| Forgetting that CSS/polyfill imports are side effects | Dropped by `sideEffects: false` | List them in the array form |
| Shipping dev-only code to production | Bigger bundles | Guard with `process.env.NODE_ENV`/`import.meta.env.DEV` |
| Bundling dependencies into a library | Duplicates for consumers | Externalize |
| Not testing the production build | Dev/prod differences | `vite preview`, CI smoke tests |
| Source maps leaked publicly with secrets in code | Information exposure | Upload privately, keep secrets out of bundles |
| One giant bundle | Slow first load | Route-level code splitting |
| Over-splitting into tiny chunks | Request overhead | Reasonable chunk sizes, preload critical ones |

## Key takeaways

- Bundlers resolve imports, transform code, and emit optimized, hashed files
- Tree shaking needs static ESM and side-effect-free modules (`sideEffects`, `/*@__PURE__*/`)
- Use dynamic `import()` for code splitting and analyze bundles regularly
- Libraries should ship ESM, externalize dependencies and declare `sideEffects`

**Next:** [Module Patterns](./05_module-patterns.md)
