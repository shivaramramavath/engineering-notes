# Module 26: Next.js Cheatsheets Catalog

> High-density, field-tested quick reference cards and syntax cheatsheets for Next.js 15+ and React 19 development.

| Metadata | Details |
| :--- | :--- |
| **Category** | Quick Reference & Cheatsheets |
| **Target Level** | All Levels (Junior through Staff Engineer) |
| **Prerequisites** | Familiarity with TypeScript and React |
| **Next.js Baseline** | Next.js 15.2+ / 16.0, React 19.0.0, Node.js 20+ LTS |

---

## Cheatsheet Catalog

| Index | Cheatsheet | Description & Key Topics |
| :--- | :--- | :--- |
| **00** | [00-nextjs.md](./00-nextjs.md) | Ecosystem baseline, directory layout, `next.config.ts`, Edge vs Node.js runtime. |
| **01** | [01-routing.md](./01-routing.md) | App Router special files, dynamic/catch-all segments, async request APIs (`await params`). |
| **02** | [02-components.md](./02-components.md) | Server vs Client Components, composition rules, prop serialization, `server-only` guards. |
| **03** | [03-rendering.md](./03-rendering.md) | Static (`○`), Dynamic (`ƒ`), PPR (`◐`), Suspense streaming, segment options. |
| **04** | [04-caching.md](./04-caching.md) | Next.js 15 caching shift, `"use cache"`, `cacheLife()` profiles, `updateTag` vs `revalidateTag`. |
| **05** | [05-server-actions.md](./05-server-actions.md) | `"use server"`, `useActionState`, `useOptimistic`, CSRF defenses, form mutations. |
| **06** | [06-route-handlers.md](./06-route-handlers.md) | HTTP methods (`GET`, `POST`), SSE streaming, dynamic route context, webhook verification. |
| **07** | [07-authentication.md](./07-authentication.md) | Defense-in-depth, DAL verification with `React.cache()`, session cookies, Better-Auth vs Auth.js. |
| **08** | [08-performance.md](./08-performance.md) | Core Web Vitals (LCP, INP, CLS), `next/image`, `next/font`, `next/dynamic`, bundle analyzer. |
| **09** | [09-deployment.md](./09-deployment.md) | Production Dockerfile (`output: 'standalone'`), Docker Compose stack, OpenTelemetry boot. |
| **10** | [10-cli.md](./10-cli.md) | Next.js CLI flags, Turbopack commands, ESLint 9 Flat Config, Biome integration. |

---

## How to Use These Cheatsheets
- **Fast Syntax Lookup**: Keep these cards open during day-to-day development for immediate copy-pasteable snippets.
- **Incident Response & Debugging**: Refer to `04-caching.md` and `07-authentication.md` to triage caching staleness and session errors.
- **Production Checklist**: Review `09-deployment.md` and `10-cli.md` before deploying containers to staging or production clusters.
