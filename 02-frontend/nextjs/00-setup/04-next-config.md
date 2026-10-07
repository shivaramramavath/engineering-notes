# next.config

`next.config.ts` is the central place to configure the framework: redirects, headers, allowed image hosts, build output, experimental features. It is plain Node code that runs when the server starts, not part of your app bundle.

> Written for Next.js 16. Some options have been renamed, stabilized or removed between versions. Always check the config reference for your installed version.

## Basic shape

```ts
// next.config.ts
import type { NextConfig } from "next";

const nextConfig: NextConfig = {
  reactStrictMode: true,
};

export default nextConfig;
```

Supported file names: `next.config.ts`, `next.config.mjs`, `next.config.js` (CommonJS). Use ES module syntax in `.ts`/`.mjs`. `NextConfig` gives you autocomplete and type errors for typos.

Key properties of the file:

- It runs in **Node.js at server start**. It is not bundled into your app and cannot import browser code.
- The `@/` import alias is **not** available inside it.
- It is read once. **Restart the dev server after changing it.**

## Options you will actually use

### Images from external hosts

`next/image` blocks remote URLs unless you allow them:

```ts
const nextConfig: NextConfig = {
  images: {
    remotePatterns: [
      {
        protocol: "https",
        hostname: "images.example.com",
        pathname: "/uploads/**",
      },
    ],
  },
};
```

Be specific with `hostname` and `pathname`; wide patterns let anyone use your server as an image proxy. More in [Images](../09-styling-and-assets/04-images.md).

### Redirects

```ts
const nextConfig: NextConfig = {
  async redirects() {
    return [
      {
        source: "/old-blog/:slug",
        destination: "/blog/:slug",
        permanent: true, // 308
      },
    ];
  },
};
```

Use config redirects for static, known URL changes. For conditional redirects (auth, locale), use the proxy or `redirect()`; see [Redirects and Rewrites](../02-routing/04-redirects-and-rewrites.md).

### Rewrites

A rewrite serves a different destination while the browser URL stays the same:

```ts
async rewrites() {
  return [
    { source: "/api/legacy/:path*", destination: "https://legacy.example.com/:path*" },
  ];
},
```

### Headers

```ts
async headers() {
  return [
    {
      source: "/(.*)",
      headers: [
        { key: "X-Content-Type-Options", value: "nosniff" },
        { key: "Referrer-Policy", value: "strict-origin-when-cross-origin" },
      ],
    },
  ];
},
```

Security headers belong here; see [Security Fundamentals](../21-security/00-security-fundamentals.md).

### Build and deployment

```ts
const nextConfig: NextConfig = {
  output: "standalone", // minimal server bundle, ideal for Docker
  basePath: "/docs", // serve the app under a sub-path
  poweredByHeader: false, // drop the X-Powered-By header
};
```

`output: "standalone"` copies only the files needed to run the server, which keeps container images small. See [Docker](../22-production/02-docker.md).

### Dependencies

```ts
const nextConfig: NextConfig = {
  serverExternalPackages: ["sharp"], // do not bundle; require at runtime
  transpilePackages: ["my-ui-lib"], // compile a package shipped as untranspiled source
};
```

Reach for these when a dependency breaks during bundling.

### Newer feature flags

Depending on your version, these are top-level options:

```ts
const nextConfig: NextConfig = {
  typedRoutes: true, // type-safe route strings in <Link> and navigation
  reactCompiler: true, // enable the React Compiler (needs its Babel plugin installed)
  cacheComponents: true, // enable Cache Components / `use cache`
  partialPrefetching: true, // set explicitly alongside cacheComponents
};
```

Per the Next.js 16.4 docs, projects created with the recommended `create-next-app` defaults already have `cacheComponents` and `partialPrefetching` enabled, and both are planned to become the only behavior in the next major version. Cache Components requires the Node.js runtime (no `runtime = "edge"`) and replaces route segment config such as `dynamic` and `revalidate`; see [Cache Components](../06-caching/05-cache-components.md).

Features still in development live under `experimental`. Treat everything there as subject to change and pin your Next version when you rely on it.

## Dynamic config

The default export can be a function, useful for per-phase settings:

```ts
import type { NextConfig } from "next";
import { PHASE_DEVELOPMENT_SERVER } from "next/constants";

export default (phase: string): NextConfig => {
  if (phase === PHASE_DEVELOPMENT_SERVER) {
    return { reactStrictMode: true };
  }
  return { output: "standalone" };
};
```

## What does *not* belong here

- **Secrets.** The legacy `env` option inlines values into the client bundle at build time. Use `.env*` files; see [Environment Variables](./03-environment-variables.md).
- **Per-request logic** (reading cookies, auth checks). Config is static; use the proxy or route code.
- **Type-check or lint bypasses** (`typescript.ignoreBuildErrors`). They hide real problems.

## Common mistakes and debugging

| Symptom | Cause | Fix |
|---|---|---|
| Change has no effect | Config read at startup | Restart `next dev` |
| `Invalid src prop ... hostname is not configured under images` | Host not in `remotePatterns` | Add the host and restart |
| Redirect works in dev, loops in production | Overlapping `source` patterns or trailing-slash settings | Check pattern order and `trailingSlash` |
| `Cannot find module '@/...'` inside config | Aliases do not apply to the config file | Use relative imports |
| Unknown option warning | Option renamed or removed in your version | Check the config reference for your version |
| `basePath` breaks assets or links | Hard-coded `/` URLs in your code | Use `next/link` and `next/image`, which add it automatically |

## Quick Summary

- `next.config.ts` runs in Node at server start; restart after edits.
- Typed with `NextConfig`; use ES module syntax.
- Daily-use options: `images.remotePatterns`, `redirects`, `rewrites`, `headers`, `output`.
- Config is static. Dynamic behavior belongs in the proxy or application code.
- Option names change between versions; verify against your installed version.

## Next

- [Development Workflow](./05-development-workflow.md)
- [Redirects and Rewrites](../02-routing/04-redirects-and-rewrites.md)
- [Production Build](../22-production/00-production-build.md)