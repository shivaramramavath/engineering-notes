# Bundle Optimization

Every kilobyte of JavaScript you ship has to be **downloaded, parsed, compiled, and executed**, and on a mid-range phone the execution cost is often bigger than the download. A smaller bundle means a faster first load, better LCP, and less main-thread work. This note is about finding what's heavy and shrinking it.

([Code splitting](./03-code-splitting-and-lazy-loading.md) moves code out of the initial load. This note is about making the code smaller, or not shipping it at all.)

## Analyze first

Don't guess what's big. Visualize it.

```bash
npm install -D rollup-plugin-visualizer
```

```ts
// vite.config.ts
import { defineConfig } from "vite"
import react from "@vitejs/plugin-react"
import { visualizer } from "rollup-plugin-visualizer"

export default defineConfig({
  plugins: [
    react(),
    visualizer({ filename: "stats.html", gzipSize: true, brotliSize: true, template: "treemap" }),
  ],
})
```

```bash
npm run build      # then open stats.html
```

The treemap shows every module as a rectangle sized by its contribution. What to look for:

- **A few giant rectangles.** One dependency is usually 30–60% of the bundle (charting, editor, icon sets, date libraries, a whole UI kit).
- **The same package twice** (different versions), which means duplicated bytes.
- **Libraries you forgot you installed**, or server-only code that leaked into the client.
- **Which chunk** each module landed in. Heavy libs should live in lazy chunks, not the entry.

The `vite build` output also lists each chunk's raw and gzip size, and warns when a chunk passes the limit (`build.chunkSizeWarningLimit`, 500 kB by default). Treat that warning as a prompt to open the analyzer, not as noise. Re-run the analyzer **whenever you add a dependency**.

Quick check before adding a library: look up its **minified + gzipped** size on [bundlephobia.com](https://bundlephobia.com) (or its equivalent), and ask whether the platform or a smaller alternative can do the job.

## Ship less code

The highest-leverage lever, in rough order of payoff:

### 1. Don't import what you don't need

```ts
import _ from "lodash"                       // ✗ pulls in the whole library
import debounce from "lodash/debounce"       // ✓ one function
import { debounce } from "lodash-es"         // ✓ ES-module build, tree-shakeable
```

Prefer the **ES-module** build of libraries (`lodash-es`) so the bundler can drop unused exports. Better still, for tiny utilities like `debounce`, write ten lines yourself or use a native API.

### 2. Replace heavy libraries

| Heavy | Lighter option |
|---|---|
| `moment` (large, not tree-shakeable) | `date-fns`, `dayjs`, or native `Intl` / `Temporal` where supported |
| Whole icon packs imported as one | Per-icon imports (`lucide-react` named imports tree-shake well) |
| `axios` for simple calls | `fetch` + a small wrapper ([API client](../11-api-integration/02-api-client.md)) |
| A full charting suite for one bar chart | A smaller chart lib, or SVG/CSS |
| Large UI kit imported wholesale | Copy-in components ([shadcn/ui](../09-ui-components/00-shadcn-ui.md)) with only what you use |

Always verify with the analyzer; "lighter" libraries differ by version and usage.

### 3. Use the platform

Native APIs cost zero bytes: `Intl.DateTimeFormat` / `NumberFormat` / `RelativeTimeFormat`, `URL` / `URLSearchParams`, `structuredClone`, `AbortController`, `crypto.randomUUID()`, CSS instead of JS animation, `<dialog>` and the Popover API, `fetch`. Check browser support for your targets, but many libraries exist only to paper over problems that browsers have since solved.

### 4. Lazy-load what's rarely needed

Heavy features move to [lazy chunks](./03-code-splitting-and-lazy-loading.md) so only users who use them pay: export libraries, editors, charts, PDF viewers, admin screens.

### 5. Remove duplicates and dead code

```bash
npm ls react-dom          # see if multiple versions are installed
npm dedupe
```

Delete unused dependencies (`npx depcheck` can help find them) and dead code paths. Features behind a flag you've turned off permanently still ship.

## Tree shaking: how it works and how to help it

**Tree shaking** drops exports that nothing imports. It works when:

- The code is **ES modules** (`import`/`export`), not CommonJS (`require`), which can't be analyzed statically.
- Imports are **named** (`import { x } from "lib"`).
- The package marks itself side-effect free: `"sideEffects": false` in its `package.json` (or lists the files that do have side effects).
- Your own code avoids **top-level side effects**: code that runs on import (registering things, mutating globals) can't be safely removed.

Things that defeat it:

- **CommonJS dependencies** (`require`): they get included whole.
- **Barrel files** (`index.ts` re-exporting everything) in *your* code or libraries can pull in more than expected, depending on the tooling and side-effect markings. If importing one component drags in dozens, import from the specific file or check the analyzer.
- **Namespace imports used dynamically** (`import * as utils` then `utils[name]`).
- **Class-heavy or side-effectful** library designs.

Libraries often document their tree-shaking story; if the analyzer shows a library fully included despite named imports, check whether it publishes an ESM build.

## Build configuration

### Modern browser targets

Transpiling for old browsers inflates code with polyfills and downleveled syntax. Set `build.target` (or your browserslist) to what you actually support:

```ts
export default defineConfig({
  build: { target: "es2022" },    // match your real support matrix
})
```

Vite's defaults target reasonably modern browsers. Only widen the range if your analytics show you need to, and only add polyfills for features you really use.

### Minification

Production builds already minify (`vite build`). Verify you're shipping the production build (`process.env.NODE_ENV === "production"` so React's dev-only checks are stripped). A dev build of React is **much** larger and slower.

### Dev-only code

```ts
if (import.meta.env.DEV) {
  // devtools, extra logging; removed from production builds
}
```

Guard dev tooling (like the Query devtools) so it doesn't ship. Many libraries already no-op in production.

### Source maps

Source maps don't affect what users download unless the browser's DevTools are open, but **don't leave them publicly accessible** if they expose proprietary source. Upload them to your error-monitoring service and serve them privately ([error monitoring](../19-production/06-error-monitoring-and-logging.md)).

## Compression and caching

The size users download is the **compressed** size.

- Serve with **Brotli** (best ratio) or gzip. Most CDNs/hosts do this automatically; verify `content-encoding` in the Network panel.
- Static assets have **content hashes** in file names, so they can be cached for a very long time (`Cache-Control: public, max-age=31536000, immutable`), while `index.html` must not be (`no-cache`). Details in [06 — Network performance](./06-network-performance.md#http-caching).

Good chunking also helps caching: when you deploy, only chunks whose content changed get new hashes. A big dependency in its own stable chunk stays cached across your releases, while your app code changes. Vite's default chunking is a decent start, and tuning it (for instance a separate vendor chunk) only pays off when the analyzer shows it would.

## CSS

- **Tailwind** generates only the classes it finds in your source, so unused styles are already gone. Avoid building class names dynamically (`` `bg-${color}-500` ``), which is both invisible to Tailwind and a common source of missing styles.
- Remove unused global CSS and CSS-in-JS libraries you no longer need (runtime CSS-in-JS also costs JavaScript execution time).
- Code-split CSS comes for free with lazy chunks in Vite.

## Fonts

Web fonts affect LCP and CLS:

- **Self-host** and subset to the characters you need (Latin only, for example).
- Use **WOFF2**.
- `font-display: swap` shows fallback text immediately (avoid invisible text), and pick a fallback with similar metrics to reduce the layout jump when the real font loads.
- Preload only the one or two critical font files ([06](./06-network-performance.md#resource-hints)).
- Use fewer weights and styles: each is a separate download. A variable font can replace several static files.

## Images

Often the heaviest assets on a page, and bundle analyzers won't show them.

- Use modern formats: **WebP** or **AVIF** (with a fallback if needed).
- Serve **responsive sizes** with `srcset` and `sizes`, so phones don't download desktop-sized images.
- Always specify `width` and `height` (or `aspect-ratio`) to prevent layout shift.
- `loading="lazy"` for below-the-fold images; **never** lazy-load the LCP image (usually the hero). Give *that* one `fetchpriority="high"`.
- Don't import large images into JS; reference them as static assets. Inline only tiny ones (icons, a few hundred bytes).
- SVG for icons and illustrations, optimized (SVGO), or as a sprite.

```html
<img
  src="/hero-800.webp"
  srcset="/hero-400.webp 400w, /hero-800.webp 800w, /hero-1600.webp 1600w"
  sizes="(min-width: 768px) 50vw, 100vw"
  width="1600" height="900"
  fetchpriority="high"
  alt="…"
/>
```

## Third-party scripts

Analytics, chat widgets, tag managers, A/B testing, and ad scripts often outweigh your own code and block the main thread. Treat each as a cost:

- Load with `async`/`defer`, or after the page is interactive.
- Audit regularly. Remove anything no one uses.
- Prefer lightweight or self-hosted options where possible.
- Watch them in the Performance panel (long tasks) and Network panel.

## Set a budget and watch it

Bundles grow slowly, one reasonable dependency at a time.

- Decide a budget (for example, "initial JS under 200 kB gzipped") and fail CI when it's exceeded (size-limit–style tools or a build-output check).
- Check the analyzer in pull requests that add dependencies.
- Track the effect in production with [Web Vitals](./00-profiling-and-measuring.md#what-to-measure-core-web-vitals): LCP in particular.

## Common mistakes

- **Optimizing without looking at the analyzer**, shaving bytes from small modules while one library dominates.
- **Importing whole libraries** (`import _ from "lodash"`, icon packs, entire UI kits) for a few functions.
- **Using CommonJS builds** of dependencies, which can't be tree-shaken.
- **Adding a dependency without checking its size**, or one that duplicates something the browser already provides.
- **Multiple versions of the same package** in the bundle.
- **Shipping dev-only tooling or large dev dependencies** to production.
- **Unoptimized images and fonts** while obsessing over JS.
- **Lazy-loading the LCP image**, or omitting image dimensions (CLS).
- **Forgetting third-party scripts** in the audit.
- **Leaving public source maps** exposing source.
- **One-time cleanup with no budget**, so the bundle regrows.

## Quick summary

- **Analyze before optimizing**: a treemap (`rollup-plugin-visualizer`) shows what's actually big; re-check whenever dependencies change.
- Ship less: import narrowly, prefer ESM builds, replace heavy libraries, use native APIs, and delete unused code and duplicates.
- Tree shaking needs ES modules, named imports, side-effect-free code, so watch for CommonJS and barrel files.
- Target modern browsers, ship the production build, guard dev-only code, and keep source maps private.
- Compress (Brotli/gzip), cache hashed assets for a long time, and keep stable chunks stable.
- Images and fonts are often the real weight: modern formats, responsive sizes, dimensions, subsetting, careful preloading.
- Set a budget and enforce it in CI.

## Next

[06 — Network performance](./06-network-performance.md)
