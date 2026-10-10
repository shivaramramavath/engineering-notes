# 13 · TypeScript

How to type a Next.js App Router app so the compiler catches broken props, wrong route params, invalid links and malformed Server Action state before users do.

> Verified against the Next.js 16.4 TypeScript reference, `page`, `layout`, `route` and `next` CLI references, the `typedRoutes` option, and React's list of serializable props. React type details (`useActionState`, `ComponentProps`) are from the React and `@types/react` documentation.

## Start here: what do you want typed?

```text
Component props, children, hooks, events ──► 00-components-and-props
params, searchParams, links, handlers ─────► 01-routes-and-params
Server Actions, forms, action state ───────► 02-actions-and-forms
Project setup, CI, config ─────────────────► this page
```

## Reading order

| # | Note | Covers |
|---|---|---|
| 00 | [Components and Props](./00-components-and-props.md) | Props and `children`, native element props, generics, hooks, what can cross the server/client boundary, async Server Components |
| 01 | [Routes and Params](./01-routes-and-params.md) | Promise `params`/`searchParams`, `PageProps`, `LayoutProps`, `RouteContext`, typed links, metadata, validated search params |
| 02 | [Actions and Forms](./02-actions-and-forms.md) | Action signatures, `useActionState`, result types, schema-inferred input, `useOptimistic` |

## Project setup and tooling

### What Next.js gives you

- TypeScript is built in. Rename a file to `.ts` or `.tsx`, run `next dev` or `next build`, and Next.js installs what it needs and creates `tsconfig.json`. `create-next-app` sets all of this up for you.
- **`next-env.d.ts`** is generated and managed by Next.js. Do not edit it; add it to `.gitignore`. It must be listed in `tsconfig.json`'s `include`.
- **Route-aware global helpers** `PageProps`, `LayoutProps` and `RouteContext` need no import. They are generated during `next dev`, `next build` or `next typegen`.
- **`next build` fails on type errors.** `typescript.ignoreBuildErrors: true` disables the check; the docs call it dangerous, so only use it if CI type-checks separately.
- A **TypeScript plugin** for editors (VS Code: "TypeScript: Select TypeScript Version" → "Use Workspace Version") warns about invalid segment config and misuse of `'use client'` and client hooks.

### `tsconfig.json`

```json
{
  "compilerOptions": {
    "strict": true,
    "skipLibCheck": true,
    "paths": { "@/*": ["./src/*"] }
  },
  "include": [
    "next-env.d.ts",
    ".next/types/**/*.ts",
    "**/*.ts",
    "**/*.tsx"
  ],
  "exclude": ["node_modules"]
}
```

Keep what `create-next-app` generated and add to it. The docs' guidance: if you set up without `create-next-app`, ensure `.next/types/**/*.ts` is in `include` so generated route types are seen. Turn **`strict` on**; most of this chapter's guarantees (null checks, no implicit `any`) depend on it. Put your own declarations in a separate file such as `new-types.d.ts` and list it in `include`.

### Config in TypeScript

```ts
// next.config.ts
import type { NextConfig } from "next";

const nextConfig: NextConfig = {
  typedRoutes: true,            // statically typed links (stable; not under experimental)
};

export default nextConfig;
```

`next.config.ts` has been supported since v15. Module resolution there is CommonJS unless you use Node's native TypeScript resolver (Node 22.10+, enabled by default from 22.18); use `next.config.mts` for ESM features in a CommonJS project.

### Type checking in CI

Route types used to exist only after `next dev` or `next build`, so a bare `tsc --noEmit` could not validate them. Use `next typegen`:

```bash
next typegen && tsc --noEmit
```

Typegen writes `<distDir>/types` (`.next/types` for production builds) and `next-env.d.ts`, and it loads your `next.config` using the production build phase, so env vars it needs must be present.

### TypeScript 7

The docs note TypeScript 7 does not provide the JavaScript compiler API. To use it for `next build`, install `typescript@^7`; Next.js then uses the project-local `tsc` CLI by default. CLI type checking prints native `tsc` diagnostics without Next.js-specific code frames, and checks the whole project selected by `tsconfig` (including test files). Set `experimental.useTypeScriptCli: false` to use the JavaScript compiler API instead. That option is experimental.

### Other options worth knowing

| Option | Purpose |
|---|---|
| `typescript.tsconfigPath` | Use another file for builds (monorepos, gradual strictness); restart dev after editing it |
| `experimental.typedEnv` | Generates types for loaded environment variables in `.next/types` (experimental) |
| `incremental: true` | Faster repeated type checks in large apps |

## The rules to remember

1. **`strict: true`** and no `any`; use `unknown` and narrow.
2. **`params` and `searchParams` are Promises.** Type them as such and `await` them (or `use()` them in Client Components).
3. **Prefer the generated helpers** (`PageProps<'/blog/[slug]'>`, `LayoutProps`, `RouteContext`) over hand-typed params.
4. **Turn on `typedRoutes`** so `Link` and the router reject routes that do not exist.
5. **Types are not validation.** Anything from the URL, `FormData` or a request body is `unknown` until a schema (Zod and similar) parses it.
6. **Derive types from one source:** schema → `z.infer`, database → ORM types, DAL → `Awaited<ReturnType<...>>`.
7. **Props crossing server to client must be serializable;** the compiler does not check that, so type DTOs deliberately.
8. **Run `next typegen && tsc --noEmit` in CI.**

## Next

[14 · Next chapter](../README.md): see the repo root README for the folder name.