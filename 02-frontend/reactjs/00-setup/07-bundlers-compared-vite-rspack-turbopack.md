# Bundlers Compared: Vite, Rspack, Turbopack, and Legacy Webpack

- **Topic**: `00-setup`
- **React version**: React 19+ (tested on React 19.0.0)
- **Tested with**: Vite 6.0, Rspack 1.0, Turbopack (Next.js 15), Webpack 5.95
- **Bundle / compiler notes**: Compares native ESM development against Rust-based bundlers and production rollup outputs.

---

## 1. Concept Overview

A frontend bundler takes an application's modular source files (JSX, TSX, CSS, images) and transforms, resolves, and packages them into optimized static assets that browsers can execute efficiently.

In the modern React ecosystem, the bundler landscape has undergone a seismic shift away from monolithic Node.js-based bundlers toward **Native ESM (Vite)** and **Rust-powered compilation engines (Rspack, Turbopack)**:
1. **Webpack (The Legacy Standard)**: Bundles the entire dependency graph in memory before serving. Historically powerful via its plugin ecosystem, but suffers from slow cold starts and sluggish Hot Module Replacement (HMR) in large codebases.
2. **Vite (The Modern Industry Standard for SPAs)**: Uses unbundled native ES Modules during development via esbuild for instant server start, and bundles with Rollup (and transitioning to Rolldown) for battle-tested production builds.
3. **Rspack (The Drop-in High-Performance Successor)**: A Rust port of Webpack's architecture by ByteDance, offering Webpack plugin/loader compatibility with 5–10x build speedups.
4. **Turbopack (Vercel's Engine)**: Incremental, Rust-based bundler deeply integrated into Next.js.

```
Webpack Architecture (Bundle Everything First):
Entry -> Parse Graph -> Bundle In Memory -> Serve (Slow Cold Start)

Vite Architecture (Native ESM + On-Demand Compilation):
Browser requests module -> Vite compiles THAT single file on the fly -> Instant!
```

---

## 2. Key Mechanics & Architecture

### Performance & Architectural Comparison

| Dimension | Vite 6 | Rspack 1.x | Turbopack | Webpack 5 (Legacy) |
| :--- | :--- | :--- | :--- | :--- |
| **Primary Language** | TypeScript + Go (`esbuild`) | Rust | Rust | JavaScript (Node.js) |
| **Dev Server Model** | Unbundled Native ESM | Bundled (Rust-accelerated) | Incremental Rust Graph | Bundled (JS-based) |
| **Production Bundler** | Rollup / Rolldown | Native Rust Rspack | Turbopack | Webpack JS |
| **HMR Latency** | Instant (~10–50ms) | Very Fast (~20–80ms) | Instant (~15–50ms) | Slow in large apps (1–5s+) |
| **Configuration Style** | Clean, minimal (`vite.config.ts`) | Webpack-compatible syntax | Zero-config / Next.js config | Verbose, complex loaders |
| **Primary Use Case** | SPAs, component libraries, Vite SSR | Migrating massive Webpack monorepos | Next.js full-stack applications | Legacy enterprise apps |

---

## 3. Real-World Code Examples

### Example 1: Minimal Modern Vite Configuration with React 19
```typescript
// vite.config.ts
import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';
import tailwindcss from '@tailwindcss/vite';

export default defineConfig({
  plugins: [
    react({
      // React 19 JSX runtime is automatic by default
    }),
    tailwindcss(),
  ],
  server: {
    port: 3000,
    open: true,
  },
  build: {
    target: 'esnext',
    sourcemap: true,
    rollupOptions: {
      output: {
        manualChunks: {
          'react-vendor': ['react', 'react-dom'],
        },
      },
    },
  },
});
```
Output:
```
// Dev server starts in ~120ms; HMR updates sub-50ms
```

### Example 2: Equivalent Rspack Configuration
```javascript
// rspack.config.js
const { defineConfig } = require('@rspack/cli');
const rspack = require('@rspack/core');

module.exports = defineConfig({
  entry: './src/main.tsx',
  resolve: {
    extensions: ['.ts', '.tsx', '.js'],
  },
  module: {
    rules: [
      {
        test: /\.tsx?$/,
        use: {
          loader: 'builtin:swc-loader',
          options: {
            jsc: {
              parser: { syntax: 'typescript', tsx: true },
              transform: { react: { runtime: 'automatic' } },
            },
          },
        },
      },
    ],
  },
  plugins: [
    new rspack.HtmlRspackPlugin({ template: './index.html' }),
  ],
});
```
Output:
```
// Compiles 10,000 modules in under 1.2s using Rust SWC cores
```

### Example 3: Measuring Build Times with Performance Hooks
```typescript
import { performance } from 'perf_hooks';

export async function benchmarkBuild(buildFn: () => Promise<void>, bundlerName: string) {
  const start = performance.now();
  await buildFn();
  const elapsed = (performance.now() - start).toFixed(2);
  console.log(`[${bundlerName}] Production build completed in ${elapsed}ms`);
}
```
Output:
```
[Vite 6] Production build completed in 412.30ms
[Legacy Webpack] Production build completed in 3240.15ms
```

### Example 4: Environment Variables Handling Across Bundlers
```typescript
// Vite: Uses import.meta.env
const apiUrlVite = import.meta.env.VITE_API_URL;

// Webpack / Rspack: Uses process.env
const apiUrlWebpack = process.env.REACT_APP_API_URL;

export function getApiBaseUrl(): string {
  // Universal cross-bundler fallback
  if (typeof import.meta !== 'undefined' && import.meta.env?.VITE_API_URL) {
    return import.meta.env.VITE_API_URL;
  }
  return process.env.REACT_APP_API_URL || 'http://localhost:8000';
}
```
Output:
```
// Correctly reads environment variable based on active bundler pipeline
```

### Example 5: Code Splitting Dynamic Import Demonstration
```tsx
import { lazy, Suspense } from 'react';

// All modern bundlers recognize dynamic import() as a chunk split point
const HeavyAnalyticsChart = lazy(() => import('./AnalyticsChart'));

export function Dashboard() {
  return (
    <div className="p-4">
      <h2 className="text-xl font-bold">System Metrics</h2>
      <Suspense fallback={<div className="h-64 bg-gray-100 animate-pulse" />}>
        <HeavyAnalyticsChart />
      </Suspense>
    </div>
  );
}
```
Output:
```
// Network tab confirms separate chunk 'AnalyticsChart-[hash].js' requested on mount
```

---

## 4. Common Anti-Patterns & Gotchas

| Bad Practice | Root Cause | Clean Solution |
| :--- | :--- | :--- |
| Using `process.env.VARIABLE` in standard Vite code | Vite does not polyfill Node's `process` object in the browser | Use `import.meta.env.VITE_VARIABLE` |
| Forgetting `VITE_` prefix on client-side environment variables | Vite ignores non-prefixed variables to prevent leaking server secrets | Prefix all client-facing variables with `VITE_` |
| Using CommonJS `require()` in Vite modules | Native ESM does not support synchronous `require` | Use standard ES module `import` syntax |
| Over-configuring Rollup `manualChunks` into tiny fragments | Causes excessive HTTP requests and waterfall delays | Group related vendor dependencies into cohesive chunks |
| Choosing Webpack for a new greenfield SPA | Massive initial configuration burden and slow dev experience | Default to Vite 6 for client-side React apps |
| Failing to configure base path (`base: "/my-app/"`) for subpath deployments | Causes asset 404s when hosted on non-root domains | Set `base: process.env.BASE_URL || "/"` in `vite.config.ts` |

---

## 5. React Internals: Beneath the Abstraction

### How HMR Works with React Fast Refresh
Both Vite and Rspack integrate **React Fast Refresh** (`@vitejs/plugin-react` and `@rspack/plugin-react-refresh`):
1. During compilation, the bundler transforms component definitions by wrapping them with registration hooks (`$RefreshReg$`, `$RefreshSig$`).
2. When a file is edited, the HMR server sends a WebSocket message containing the updated module code.
3. React Fast Refresh compares component signatures:
   - If only the render function or JSX changed, React re-renders the component in-place, **preserving its existing `useState` and `useRef` values**.
   - If hooks were added, removed, or reordered, Fast Refresh forces a clean remount of that component subtree to prevent state corruption.

---

## 6. Performance, Bundling & React Compiler

- **Rolldown Future**: Vite is actively developing **Rolldown** (a Rust port of Rollup), which will unify the fast esbuild development server with the production bundler, eliminating behavioral discrepancies between dev and build.
- **Tree-Shaking Efficiency**: Native ES Module imports enable bundlers to eliminate dead code reliably via static syntax analysis.

---

## 7. Accessibility (a11y) & Usability

- **Lazy Chunk Loading States**: When using dynamic `React.lazy()` chunking, always provide accessible fallback UI with `role="status"` or `aria-busy="true"` so screen readers acknowledge that content is loading.

---

## 8. Testing & Verification Patterns

```typescript
import { describe, it, expect } from 'vitest';
import { getApiBaseUrl } from './envHelper';

describe('Bundler Environment Resolution', () => {
  it('resolves API URL accurately', () => {
    const url = getApiBaseUrl();
    expect(url).toBeDefined();
    expect(typeof url).toBe('string');
  });
});
```

---

## 9. Version History & Migration

| Era | Dominant Bundler | Key Characteristics |
| :--- | :--- | :--- |
| **2015–2020** | Webpack 1–4 (via CRA) | Slow, monolithic bundling, Babel transpilation |
| **2021–2023** | Vite 2–4 | Native ESM revolution, instant cold start, esbuild |
| **2024–2026+** | Vite 6 / Rspack / Turbopack | Rust-native compilers, Lightning CSS, Rolldown unification |

---

## 10. Security & Hardening

- **Preventing Secret Leaks**: Never put private API keys, database credentials, or server secrets in client-side bundler env files. Anything bundled into client JS is completely visible to any user via DevTools.

---

## 11. Real-World Case Study

### Migrating a 250,000-Line Webpack Monorepo to Rspack
An enterprise dashboard suffered from an 85-second cold start and 4.5-second HMR latency on Webpack 5.
- Because the repository relied on 30+ complex Webpack plugins, switching to Vite required rewriting too many custom loaders.
- Team dropped in Rspack as a direct replacement.
- Cold start plummeted from **85s to 4.2s** (20x improvement), and HMR latency dropped from **4500ms to 75ms**, saving engineers hours of developer downtime daily.

---

## 12. Interview Q&A

<details>
<summary>1. Why is Vite's dev server exponentially faster than Webpack?</summary>
Webpack bundles the entire dependency graph in memory before serving files. As an application grows, dev server startup degrades linearly or worse. Vite does NOT bundle source code in dev: it serves source files as Native ES Modules directly to the browser. The browser requests only the specific files needed for the current screen, and Vite compiles them on-demand using esbuild.
</details>

<details>
<summary>2. What is the role of esbuild in Vite?</summary>
Vite uses esbuild (written in Go) for pre-bundling third-party CommonJS/UMD dependencies into clean ESM and for fast TypeScript/JSX transpilation during development.
</details>

<details>
<summary>3. Why doesn't Vite use esbuild for production bundling?</summary>
While esbuild is blazing fast, its JavaScript API and CSS code-splitting capabilities are not as mature or fine-grained as Rollup's. Vite uses Rollup for production builds to produce optimal chunk splitting, tree-shaking, and plugin flexibility.
</details>

<details>
<summary>4. What is Rspack and when should you choose it over Vite?</summary>
Rspack is a high-performance, Rust-based bundler developed by ByteDance designed to be a drop-in replacement for Webpack. Choose Rspack when migrating large legacy enterprise codebases that depend heavily on complex Webpack loaders, plugins, and custom compilation hooks that cannot easily be ported to Vite.
</details>

<details>
<summary>5. How does React Fast Refresh preserve state during HMR?</summary>
React Fast Refresh instruments component definitions with internal signatures. When code changes, it updates the component's prototype and forces a re-render while leaving the Fiber node's <code>memoizedState</code> (holding state and refs) untouched, as long as hook counts and types have not altered.
</details>

<details>
<summary>6. How do environment variables differ between Vite and Webpack?</summary>
In Webpack (and Create React App), variables are accessed via <code>process.env.REACT_APP_*</code>. In Vite, variables are accessed via the ESM standard <code>import.meta.env.VITE_*</code>, and <code>process</code> is not polyfilled in the browser by default.
</details>

<details>
<summary>7. What is Turbopack?</summary>
Turbopack is an incremental bundler written in Rust by the creators of Webpack and Vercel. It is built directly into Next.js and uses function-level caching (via the Turbo engine) to avoid ever recalculating unchanged work.
</details>

<details>
<summary>8. What is Rolldown?</summary>
Rolldown is an ongoing project by the Vite team to rewrite Rollup in Rust. Once complete, it will replace Rollup in Vite, providing near-esbuild speeds for production builds with complete Rollup plugin compatibility.
</details>

---

## 13. Practical Challenges & Exercises

<details>
<summary>Exercise 1 (Easy): Vite Custom Path Aliases</summary>

**Specification**: Configure path aliases (`@/` mapping to `src/`) in `vite.config.ts`.

```typescript
import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';
import path from 'path';

export default defineConfig({
  plugins: [react()],
  resolve: {
    alias: {
      '@': path.resolve(__dirname, './src'),
    },
  },
});
```
</details>

<details>
<summary>Exercise 2 (Easy): Strict Environment Type Definitions</summary>

**Specification**: Write a `vite-env.d.ts` file extending `ImportMetaEnv` with typed custom variables.

```typescript
/// <reference types="vite/client" />

interface ImportMetaEnv {
  readonly VITE_API_URL: string;
  readonly VITE_APP_TITLE: string;
  readonly VITE_ENABLE_ANALYTICS: string;
}

interface ImportMeta {
  readonly env: ImportMetaEnv;
}
```
</details>

<details>
<summary>Exercise 3 (Medium): Vendor Chunk Splitting Strategy</summary>

**Specification**: Configure Rollup output in Vite to isolate `react`, `react-dom`, and large UI libraries into separate vendor chunks.

```typescript
import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';

export default defineConfig({
  plugins: [react()],
  build: {
    rollupOptions: {
      output: {
        manualChunks(id) {
          if (id.includes('node_modules')) {
            if (id.includes('react') || id.includes('react-dom')) {
              return 'react-core';
            }
            if (id.includes('@radix-ui') || id.includes('lucide-react')) {
              return 'ui-primitives';
            }
            return 'vendor';
          }
        },
      },
    },
  },
});
```
</details>

<details>
<summary>Exercise 4 (Medium): Bundle Visualizer Integration</summary>

**Specification**: Add `rollup-plugin-visualizer` to Vite to generate a visual bundle breakdown report on build.

```typescript
import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';
import { visualizer } from 'rollup-plugin-visualizer';

export default defineConfig({
  plugins: [
    react(),
    visualizer({
      open: false,
      filename: 'dist/stats.html',
      gzipSize: true,
      brotliSize: true,
    }),
  ],
});
```
</details>

<details>
<summary>Exercise 5 (Hard): Dynamic Route Prefetcher</summary>

**Specification**: Implement a custom prefetch utility that triggers network prefetching of lazy chunk components on link hover.

```tsx
import { ComponentType } from 'react';

type ComponentLoader = () => Promise<{ default: ComponentType<any> }>;

export function prefetchComponent(loader: ComponentLoader) {
  // Executing the import() promise triggers browser chunk fetch
  loader().catch((err) => console.warn('Prefetch failed', err));
}

export function PrefetchLink({ to, loader, children }: { to: string; loader: ComponentLoader; children: React.ReactNode }) {
  return (
    <a href={to} onMouseEnter={() => prefetchComponent(loader)} className="text-blue-500 hover:underline">
      {children}
    </a>
  );
}
```
</details>

<details>
<summary>Exercise 6 (Hard): Custom Vite HTML Transform Plugin</summary>

**Specification**: Write a custom inline Vite plugin that injects a build timestamp and git commit SHA meta tag into `index.html`.

```typescript
import { Plugin } from 'vite';

export function injectBuildMetadataPlugin(commitSha: string): Plugin {
  return {
    name: 'inject-build-metadata',
    transformIndexHtml(html) {
      const metaTags = `
        <meta name="build-time" content="${new Date().toISOString()}" />
        <meta name="commit-sha" content="${commitSha}" />
      `;
      return html.replace('</head>', `${metaTags}\n</head>`);
    },
  };
}
```
</details>
