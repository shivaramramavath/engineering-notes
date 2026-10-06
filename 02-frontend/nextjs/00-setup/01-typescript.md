# TypeScript in Next.js

Next.js has first-class TypeScript support with no extra build configuration. You write `.ts`/`.tsx`, and Next compiles it. It does **not** type-check during `next dev`; type errors surface in your editor and during `next build`. This note covers how the setup works, the `tsconfig.json` options that matter, and the types you will use constantly.

> Written for Next.js 16 (requires TypeScript 5.1+). Type helpers marked below need a recent 15.x or newer.

## How it is set up

`create-next-app` with the TypeScript option installs everything. To add TypeScript to an existing JavaScript project:

```bash
npm install -D typescript @types/react @types/node @types/react-dom
touch tsconfig.json
npm run dev        # Next fills in tsconfig.json and creates next-env.d.ts
```

Then rename files to `.ts`/`.tsx` gradually. `allowJs` is enabled, so JS and TS can coexist during migration.

Two files are Next-managed:

- `tsconfig.json`: Next adds or adjusts required options on startup.
- `next-env.d.ts`: generated type references. Do not edit it; it is normally git-ignored and recreated by `next dev` / `next build`.

## The `tsconfig.json` that matters

The generated file differs slightly between versions, but these options are the ones to understand:

```json
{
  "compilerOptions": {
    "strict": true,
    "noEmit": true,
    "moduleResolution": "bundler",
    "module": "esnext",
    "isolatedModules": true,
    "skipLibCheck": true,
    "plugins": [{ "name": "next" }],
    "paths": { "@/*": ["./*"] }
  }
}
```

| Option | Why it is there |
|---|---|
| `strict` | Turn it on and leave it on. Retrofitting later is painful |
| `noEmit` | TypeScript only checks; Next (SWC/Turbopack) does the compiling |
| `moduleResolution: "bundler"` | Matches how bundlers resolve imports |
| `isolatedModules` | Each file must be compilable on its own, which the compiler requires |
| `plugins: [{ name: "next" }]` | Adds Next.js-specific diagnostics in your editor |
| `paths` | Defines the `@/` import alias |

### Path alias

```tsx
// without alias
import { Button } from "../../../components/ui/button";

// with alias (project root; use "./src/*" if you chose src/)
import { Button } from "@/components/ui/button";
```

The alias comes from `paths`. Next reads it, so you do not need extra bundler config.

### VS Code uses the wrong TypeScript

The `next` plugin only loads if the editor uses the workspace's TypeScript. In VS Code: open a `.ts` file, run **TypeScript: Select TypeScript Version** and choose **Use Workspace Version**.

## Types you will use constantly

```tsx
import type { Metadata, NextConfig } from "next";
import type { NextRequest } from "next/server";
```

Typing a page. In Next.js 15 and later, `params` and `searchParams` are **Promises**:

```tsx
// app/blog/[slug]/page.tsx
export default async function Page({
  params,
}: {
  params: Promise<{ slug: string }>;
}) {
  const { slug } = await params;
  return <h1>{slug}</h1>;
}
```

Recent versions also generate global helper types so you do not hand-write that shape (available in Next 15.5+; generated during `next dev`, `next build` or `next typegen`):

```tsx
export default async function Page(props: PageProps<"/blog/[slug]">) {
  const { slug } = await props.params;
  return <h1>{slug}</h1>;
}
```

`LayoutProps` and `RouteContext` work the same way for layouts and route handlers. Typing routes and parameters in depth is in [Routes and Params](../13-typescript/01-routes-and-params.md).

## Type checking in practice

| Where | When it runs |
|---|---|
| Editor | Continuously |
| `next dev` | **Not run**; errors only appear in the editor |
| `next build` | Fails the build on type errors |
| `tsc --noEmit` | Whenever you call it: use in CI and pre-commit |

Add an explicit script so you do not discover errors only at build time:

```json
{
  "scripts": {
    "typecheck": "tsc --noEmit"
  }
}
```

Next has `typescript.ignoreBuildErrors` in `next.config`. Do not use it to make a build pass; you are shipping type errors.

## Common mistakes

| Mistake | What happens | Fix |
|---|---|---|
| Treating `params` as a plain object (pre-15 style) | Type error or runtime warning in Next 15+ | `await params` |
| Expecting `next dev` to report type errors | Errors only show in editor / build | Run `typecheck` regularly |
| Editing `next-env.d.ts` | Changes overwritten | Add your own `*.d.ts` file instead |
| Disabling `strict` to silence errors | Hides real bugs | Fix the types |
| Alias works in editor, fails at runtime (or vice versa) | `paths` out of sync or wrong base | Check `paths` matches your folder (`./src/*` with `src/`) and restart dev |

## Quick Summary

- Next compiles TypeScript itself; `tsc` only checks types.
- No type checking in `next dev`; the build fails on type errors.
- Keep `strict: true`; use the `@/*` alias via `paths`.
- Pick the workspace TypeScript version in VS Code for Next plugin diagnostics.
- `params`/`searchParams` are Promises in 15+; use the generated `PageProps` helpers where available.

## Next

- [Linting and Formatting](./02-linting-and-formatting.md)
- [Routes and Params typing](../13-typescript/01-routes-and-params.md)
