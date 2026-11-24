# Build Output Anatomy & Route Table (`build-output-anatomy-and-route-table`)

> Deep dive into Next.js compilation artifacts (`.next/`), manifest files, route table symbol decoding (`○`, `ƒ`, `◐`), Partial Prerendering (PPR), and production deployment structure.

| Metadata | Details |
| :--- | :--- |
| **Next.js Version** | 15.2+ / 16.0 |
| **React Version** | React 19.0.0 |
| **Node Version** | Node.js 20+ LTS |
| **Router** | App Router (`app/`) |
| **Runtime** | Node.js Server / Standalone |
| **Tested With** | Next.js 15.2, Vite 6, TypeScript 5.7 |

---

## 1. Concept Overview & Mental Model

### 1.1 The Anatomy of `next build`
When running `next build`, Next.js compiles the application into the hidden `.next/` directory. Understanding this output is essential for production debugging, bundle size auditing, and containerized self-hosting.

### 1.2 Route Table Symbols Breakdown
At the end of every `next build`, Next.js prints the authoritative **Route Table**:

| Symbol | Rendering Strategy | Description |
| :---: | :--- | :--- |
| **`○`** | **Static (SSG / Prerendered)** | Page prerendered as static HTML + RSC payload at build time. Zero server execution at runtime. |
| **`ƒ`** | **Dynamic (SSR / On-Demand)** | Rendered dynamically on the server for each incoming request (triggered by `await cookies()`, `await headers()`, dynamic params, or uncached queries). |
| **`◐`** | **Partial Prerender (PPR)** | Static shell prerendered at build time with dynamic streaming holes filled via React Suspense boundaries at runtime. |

```
Route (app)                              Size     First Load JS
┌ ○ /                                    5.4 kB         92.1 kB
├ ○ /about                               1.2 kB         87.9 kB
├ ◐ /products/[id]                       3.8 kB         95.4 kB
└ ƒ /api/checkout                        0 B            0 B
+ First Load JS shared by all            86.7 kB
  ├ chunks/framework-xyz.js              55.2 kB
  └ chunks/main-app-abc.js               31.5 kB

○  (Static)   prerendered as static content
ƒ  (Dynamic)  server-rendered on demand
◐  (Partial Prerender)  prerendered with dynamic streaming
```

---

## 2. Naive vs. Idiomatic Comparison

### Naive: Accidental Dynamic De-optimization
```tsx
// app/dashboard/page.tsx
import { cookies } from 'next/headers';

// Reading cookies at the page root forces the ENTIRE route to dynamic `ƒ`
export default async function DashboardPage() {
  const cookieJar = await cookies();
  const user = cookieJar.get('user');

  return (
    <div>
      <StaticSidebar />      {/* De-optimized: Now rendered dynamically every request */}
      <StaticHeader />       {/* De-optimized */}
      <UserProfile user={user} />
    </div>
  );
}
```

### Idiomatic: Preserving Static Shell with Suspense (or PPR `◐`)
```tsx
// app/dashboard/page.tsx
import { Suspense } from 'react';
import { UserProfileContainer } from './UserProfileContainer';

export default function DashboardPage() {
  return (
    <div>
      <StaticSidebar />      {/* Prerendered statically at build time (○) */}
      <StaticHeader />       {/* Prerendered statically at build time (○) */}
      <Suspense fallback={<p>Loading profile...</p>}>
        <UserProfileContainer /> {/* Dynamic streaming boundary (ƒ or ◐) */}
      </Suspense>
    </div>
  );
}
```

---

## 3. Five Progressive Examples

### Example 1: Inspecting `.next/` Build Artifacts
```text
.next/
├── build-manifest.json      # Maps chunks to route URLs for client scripts
├── app-build-manifest.json  # Manifest of App Router RSC chunks and dependencies
├── server/
│   ├── app/                 # Compiled Server Components & Flight payloads
│   │   ├── page.html        # Prerendered static HTML
│   │   ├── page.rsc         # Serialized React Flight Protocol payload
│   │   └── page.meta        # Headers and route configuration
│   └── middleware-manifest.json # Proxy/middleware routing routes and matchers
└── static/
    └── chunks/              # Client JavaScript bundles & CSS chunks
```

### Example 2: Enabling Partial Prerendering (PPR) in `next.config.ts`
```ts
// next.config.ts
import type { NextConfig } from 'next';

const nextConfig: NextConfig = {
  experimental: {
    ppr: true, // Enables Partial Prerendering (produces ◐ symbol in route table)
  },
};

export default nextConfig;
```

### Example 3: Auditing First Load JS Size
```bash
# Analyze chunk sizes visually
ANALYZE=true next build
```

### Example 4: Outputting Standalone Server for Docker
```ts
// next.config.ts
import type { NextConfig } from 'next';

const nextConfig: NextConfig = {
  output: 'standalone', // Generates .next/standalone/ containing minimal server runtime
};

export default nextConfig;
```

### Example 5: Verifying Route Symbols Programmatically
```ts
// scripts/verify-routes.ts
import fs from 'fs';
import path from 'path';

const manifest = JSON.parse(
  fs.readFileSync(path.join(process.cwd(), '.next/server/middleware-manifest.json'), 'utf8')
);
console.log('Dynamic routes registered:', Object.keys(manifest.functions));
```

---

## 4. Under the Hood & Internals
During compilation, Next.js executes a headless test-render of every page without an incoming HTTP request.
1. If the route executes without reading any un-cached dynamic source (e.g. `cookies()`, `headers()`, or `searchParams`), it emits a static `.html` and `.rsc` file (`○`).
2. If the route accesses a dynamic request API outside of a Suspense boundary, Next.js aborts static prerendering and marks the entry in `app-paths-manifest.json` as a dynamic server function (`ƒ`).
3. If PPR is enabled and the dynamic request API is enclosed in a `<Suspense>` boundary, the static shell is written to `.html` and the dynamic tree is flagged as a streaming hole (`◐`).

---

## 5. Security & Build Safety
- **No Secrets in Client Manifests**: Inspect `build-manifest.json` and client chunks to guarantee that no internal database connection strings or secret keys were inadvertently bundled into `static/chunks/`.
- **Enforcing `server-only`**: Always annotate database and crypto modules with `import 'server-only'` so `next build` fails immediately if an unauthorized client import occurs.

---

## 6. Common Mistakes & Anti-Patterns (Table)

| Anti-Pattern | Root Cause | Observable Symptom | Correct Architecture |
| :--- | :--- | :--- | :--- |
| Accidental `ƒ` route | Reading `searchParams` at page top | Route table shows `ƒ` instead of `○` | Wrap parameter consumption in Suspense |
| Deploying entire `.next/` directory | Missing `output: 'standalone'` | Container image is 1.5GB+ with dev dependencies | Use `output: 'standalone'` (image < 150MB) |
| Missing `.rsc` files on CDN | Reverse proxy misconfiguration | Client-side navigation fails with 404 | Ensure reverse proxy forwards all `.rsc` requests |
| Client bundle bloated by utils | Importing server lodash in Client Component | First Load JS increases by 70kB | Import only lightweight helpers or keep on server |

---

## 7. Performance & Optimization Guide
Keep **First Load JS shared by all** below **100 kB**. Route-specific JS should ideally remain under **10 kB** per page. Any page exceeding 150 kB First Load JS degrades Largest Contentful Paint (LCP) on mobile networks.

---

## 8. TypeScript & Strict Typing Recipes
```ts
// Route segment export options controlling prerendering
export const dynamic = 'auto'; // 'auto' | 'force-dynamic' | 'error' | 'force-static'
export const dynamicParams = true; // true | false (returns 404 for unknown dynamic slugs)
export const revalidate = false; // false | 0 | number (in seconds)
```

---

## 9. Testing & Quality Verification

```bash
# Production build check
npm run build

# Output must report all static routes with ○ and dynamic routes with ƒ
```

---

## 10. When NOT to Use This Pattern
- Do not force `export const dynamic = 'force-static'` on pages that legitimately display user-specific account data, as this will leak cached session information across users.

---

## 11. Frequently Asked Interview Questions

<details>
<summary>1. What does the `◐` (half-circle) symbol in the `next build` route table indicate?</summary>
It indicates Partial Prerendering (PPR). The page has a static prerendered shell compiled at build time, with dynamic streaming holes filled at runtime by the server using React Suspense.
</details>

<details>
<summary>2. What is inside `.next/standalone`?</summary>
`.next/standalone` contains a minimal Node.js server production bundle including only the necessary `node_modules` dependencies (traced via NFT - Node File Trace), allowing lightweight Docker containers without copying the entire workspace.
</details>

<details>
<summary>3. Why are `.rsc` files emitted alongside `.html` in `.next/server/app`?</summary>
The `.html` file is used for the initial full-page load. The `.rsc` file contains the serialized React Flight Protocol stream consumed during client-side SPA navigation via `next/link`.
</details>

<details>
<summary>4. What causes a route to flip from `○` (Static) to `ƒ` (Dynamic)?</summary>
A route flips to dynamic if it accesses dynamic request APIs (`await cookies()`, `await headers()`, un-awaited `searchParams`), executes an uncached `fetch()` (default in Next.js 15+), or uses `export const dynamic = 'force-dynamic'`.
</details>

<details>
<summary>5. What is `app-build-manifest.json`?</summary>
It is an internal JSON file that maps every route in the App Router to the specific JavaScript chunks and CSS files required to render that route on the client.
</details>

<details>
<summary>6. How can you enforce that a page remains static at build time?</summary>
Export `export const dynamic = 'error'`. Next.js will throw a build error if any dynamic API or uncached fetch is inadvertently added to that route.
</details>

<details>
<summary>7. What does "First Load JS shared by all" represent?</summary>
It is the baseline JavaScript required by every route in the application (including React runtime, Next.js client router, and root layout chunks).
</details>

<details>
<summary>8. How do you prevent sensitive environment variables from leaking into client chunks?</summary>
Do not prefix private variables with `NEXT_PUBLIC_`, use `import 'server-only'` in data access files, and inspect client chunks in `.next/static/chunks`.
</details>

---

## 12. Hands-on Practice Exercises

<details>
<summary>Exercise 1 (Easy): Run next build and decode the route table</summary>
Run `npm run build` on a sample app and identify every `○` static and `ƒ` dynamic route.
</details>

<details>
<summary>Exercise 2 (Medium): Convert a dynamic route to Partial Prerendering</summary>
Wrap a dynamic user greeting component in a `<Suspense>` boundary and verify the route table outputs `◐`.
</details>

<details>
<summary>Exercise 3 (Hard): Containerize a standalone Next.js build</summary>
Create a multi-stage Dockerfile utilizing `output: 'standalone'` and verify the final image is under 150MB.
</details>

---

## 13. Cross-References & Further Reading
- [Rendering: Partial Prerendering & Streaming](file:///E:/Learning/engineering-notes/02-frontend/nextjs/04-rendering/README.md)
- [Official Build Output Documentation](https://nextjs.org/docs/app/api-reference/cli/next#next-build)
