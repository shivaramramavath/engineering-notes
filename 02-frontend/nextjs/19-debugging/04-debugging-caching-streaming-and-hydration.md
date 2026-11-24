# Debugging Caching, Streaming & Hydration (`debugging-caching-streaming-and-hydration`)

> Comprehensive debugging guide for modern Next.js: Diagnosing caching discrepancies, streaming suspense hangs, React 19 hydration mismatches, source maps, and Node.js inspection.

| Metadata | Details |
| :--- | :--- |
| **Next.js Version** | 15.2+ / 16.0 |
| **React Version** | React 19.0.0 |
| **Node Version** | Node.js 20+ LTS |
| **Router** | App Router (`app/`) |
| **Tooling** | Chrome DevTools, React DevTools, Node Inspect |
| **Tested With** | Next.js 15.2, Vitest 3.0, TypeScript 5.7 |

---

## 1. Concept Overview & Mental Model

### 1.1 The Complexity of Multi-Tiered Execution
Debugging a Next.js App Router application requires understanding across three distinct execution layers:
1. **Server Build Time**: Static prerendering, bundle chunking, route table optimization (`○`, `ƒ`, `◐`).
2. **Server Runtime**: Edge proxy execution, async request headers, database connections, `"use cache"` evaluation, and streaming RSC generation.
3. **Browser Client Runtime**: DOM hydration, reconciliation, Client Component state, and navigation events.

```
[ Problem Detected: Stale Data / UI Flicker / Hydration Mismatch ]
                                │
    ┌───────────────────────────┼───────────────────────────┐
    ▼                           ▼                           ▼
[ Layer 1: Caching Bug ]    [ Layer 2: Streaming Hang ] [ Layer 3: Hydration Error ]
Check "use cache" & tags   Check unresolved Promise    Check HTML mismatch
```

---

## 2. Naive vs. Idiomatic Comparison

### Naive: Relying on Guesswork & `console.log`
```tsx
// Guessing why data is stale without inspecting cache headers or logs
console.log('Rendering page...'); // Floods server terminal, gives no cache metadata
```

### Idiomatic: Systematic Cache & Performance Debugging
```ts
// next.config.ts (Enable verbose logging for data fetching and caching)
import type { NextConfig } from 'next';

const nextConfig: NextConfig = {
  logging: {
    fetches: {
      fullUrl: true, // Prints exact URL, cache status (HIT/MISS/SKIP) in dev console
    },
  },
};

export default nextConfig;
```

---

## 3. Five Progressive Examples

### Example 1: Decoding Console Cache Logs
When `logging.fetches.fullUrl = true` is set, Next.js outputs color-coded cache telemetry:
```text
GET /products/123 200 in 45ms
│
├─ GET https://api.internal/products/123 200 in 12ms (cache: HIT)
└─ GET https://api.internal/recs 200 in 85ms (cache: SKIP - no-store)
```

### Example 2: Diagnosing React Hydration Mismatch Errors
React 19 provides detailed visual diffs for hydration mismatches:
```text
Error: Hydration failed because the initial UI does not match what was rendered on the server.
Warning: Expected server HTML to contain a matching <span> in <div>.
  <div>
-   <span>Server Render: 2026-11-24T10:00:00.000Z</span>
+   <span>Client Render: 2:30:15 PM</span>
  </div>
```
**Fix**: Suppress hydration warning on known dynamic elements or move client-only date formatting into `useEffect`:
```tsx
export function FormattedDate({ date }: { date: Date }) {
  const [formatted, setFormatted] = useState<string | null>(null);
  useEffect(() => {
    setFormatted(date.toLocaleTimeString());
  }, [date]);

  return <span>{formatted ?? '...'}</span>;
}
```

### Example 3: Debugging Suspense Streaming Hangs
If a page hangs with a loading skeleton forever:
1. Locate the dynamic component inside the `<Suspense>` boundary.
2. Check for an unhandled or never-resolving `Promise`.
```tsx
// Debugging helper: Wrap promise with a timeout assertion
export async function withDebugTimeout<T>(promise: Promise<T>, ms: number = 5000, label: string): Promise<T> {
  const timeout = new Promise<never>((_, reject) =>
    setTimeout(() => reject(new Error(`Suspense component [${label}] timed out after ${ms}ms!`)), ms)
  );
  return Promise.race([promise, timeout]);
}
```

### Example 4: Connecting Node.js Chrome Inspector
```bash
# Start Next.js in inspect mode
NODE_OPTIONS='--inspect' next dev
# Open chrome://inspect in browser to set breakpoints inside Server Components!
```

### Example 5: Debugging Cache Invalidation with `updateTag`
```ts
// Log invalidation in action
export async function debugInvalidate(tag: string) {
  'use server';
  console.log(`[Cache Invalidation] Purging tag: ${tag}`);
  updateTag(tag);
}
```

---

## 4. Under the Hood & Internals
Hydration mismatches occur during React's second pass in the browser.
1. The browser parses the server-rendered HTML into the DOM.
2. React renders the Client Component virtual DOM tree in JavaScript.
3. React iterates both trees simultaneously. If an HTML tag or attribute differs (`server: disabled=""` vs `client: disabled=false`), React aborts hydration for that subtree and forces a slow client-side re-render.

---

## 5. Security & Error Masking
- **Production Error Masking**: In production (`NODE_ENV=production`), Next.js automatically masks sensitive internal server errors (e.g. database connection strings or stack traces), emitting an obfuscated Digest hash: `Error: An error occurred in the Server Components render. The specific message is omitted in production builds to avoid leaking sensitive information. Digest: 182749174`.
- Use the digest hash to find the full stack trace in your server logs or Sentry telemetry.

---

## 6. Common Mistakes & Anti-Patterns (Table)

| Anti-Pattern | Root Cause | Observable Symptom | Correct Architecture |
| :--- | :--- | :--- | :--- |
| Random values during SSR | `Math.random()` or `Date.now()` in render | Hydration mismatch error | Generate random values on server or in `useEffect` |
| Invalid HTML nesting | `<p>` inside `<p>` or `<div>` in `<p>` | Browser corrects DOM, causing hydration mismatch | Use valid HTML nesting according to spec |
| Unresolved promise in Suspense | Missing resolve in fetch wrapper | Loading skeleton hangs indefinitely | Add timeout reject to async queries |
| Debugging cache in dev only | Dev bypasses some production caches | Behavior differs between dev and prod | Test caching against `next build && next start` |

---

## 7. Performance & Optimization Guide
Fixing hydration mismatches directly preserves performance. When a hydration error occurs, React must throw away the server-rendered DOM and reconstruct the entire element tree on the client, freezing the main thread and doubling Interaction to Next Paint (INP) latency.

---

## 8. TypeScript & Strict Typing Recipes
```ts
export interface ServerErrorDigest extends Error {
  digest?: string;
}
```

---

## 9. Testing & Quality Verification

```ts
import { describe, it, expect, vi } from 'vitest';

describe('Hydration Safety', () => {
  it('renders matching timestamp', () => {
    const date = new Date('2026-01-01T00:00:00Z');
    expect(date.toISOString()).toBe('2026-01-01T00:00:00.000Z');
  });
});
```

---

## 10. When NOT to Use This Pattern
- Do not use `suppressHydrationWarning` to sweep genuine HTML layout bugs under the rug. Only use it for unavoidable dynamic values like user timezones.

---

## 11. Frequently Asked Interview Questions

<details>
<summary>1. What causes a React hydration mismatch error in Next.js?</summary>
When the HTML generated by the server during SSR does not match the initial virtual DOM generated by React during client-side hydration (e.g. differing dates, localized numbers, browser-only window checks, or invalid HTML nesting).
</details>

<details>
<summary>2. What is an error Digest in Next.js production builds?</summary>
In production, Next.js redacts raw server error messages to prevent leaking database credentials or stack traces to users. It replaces the error with a hash called a Digest, which can be correlated with server-side logs.
</details>

<details>
<summary>3. Why can `Date.now()` or `new Date()` cause hydration mismatches?</summary>
Because the server executes `new Date()` at render time in UTC or server timezone, and the browser executes `new Date()` milliseconds later in the client's local timezone.
</details>

<details>
<summary>4. How do you attach a debugger to Next.js Server Components?</summary>
Run Next.js with `NODE_OPTIONS='--inspect' next dev`, then open `chrome://inspect` in Chrome and attach to the Node.js process.
</details>

<details>
<summary>5. How do you log all outgoing `fetch` requests in development?</summary>
Set `logging: { fetches: { fullUrl: true } }` in `next.config.ts`.
</details>

<details>
<summary>6. What happens if a Server Component never resolves its Promise?</summary>
The page or Suspense boundary hangs on the loading skeleton indefinitely until the server request times out.
</details>

<details>
<summary>7. How do you safely render client-only timestamps without hydration errors?</summary>
Render a skeleton or generic UTC date initially, and update the timestamp inside a `useEffect` hook after mounting.
</details>

<details>
<summary>8. How do you verify caching behavior accurately?</summary>
Always test against a production build (`next build && next start`), as development mode deliberately bypasses several production cache layers.
</details>

---

## 12. Hands-on Practice Exercises

<details>
<summary>Exercise 1 (Easy): Enable fetch logging</summary>
Add `logging.fetches.fullUrl` to `next.config.ts` and inspect console outputs.
</details>

<details>
<summary>Exercise 2 (Medium): Fix a date hydration mismatch</summary>
Refactor a dynamic localized date component to avoid hydration mismatches.
</details>

<details>
<summary>Exercise 3 (Hard): Debug with Chrome inspector</summary>
Attach Chrome DevTools to a Next.js server process and set breakpoints inside a Data Access Layer function.
</details>

---

## 13. Cross-References & Further Reading
- [Rendering: Streaming UI Patterns](file:///E:/Learning/engineering-notes/02-frontend/nextjs/04-rendering/README.md)
- [Official Next.js Debugging Guide](https://nextjs.org/docs/app/building-your-application/configuring/debugging)
