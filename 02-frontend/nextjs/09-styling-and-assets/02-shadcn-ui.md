# shadcn/ui

shadcn/ui is **not a component library you install**. It is a collection of well-built components (Button, Dialog, Form, Table...) that a CLI **copies into your project** as source files. You own the code and edit it freely. It is built on Tailwind CSS and accessible primitives.

> The install steps follow the shadcn/ui docs for Next.js. The CLI evolves quickly; prompts and generated files can differ by version, so treat the details below as the stable shape and check `npx shadcn@latest --help` for current options.

## What it is, and why people use it

| Traditional library (MUI, Chakra) | shadcn/ui |
|---|---|
| Import from `node_modules` | Source is **copied into your repo** (`components/ui/*`) |
| Customize through props and theme overrides | Customize by **editing the file** |
| Upgrades arrive via a version bump | You pull updates deliberately (and re-apply your edits) |
| Styling system bundled | Tailwind classes + CSS variables you already control |

The trade: total control and no styling lock-in, at the cost of owning the maintenance of those files.

## Prerequisites

- A Next.js project with Tailwind CSS ([Tailwind note](./01-tailwindcss.md)); `create-next-app` with defaults is enough.
- The `@/*` path alias pointing at your project root (or `src/` if you use it); `create-next-app` sets this up.

## Install

New project from the CLI template:

```bash
pnpm dlx shadcn@latest init -t next
```

Existing project:

```bash
pnpm dlx shadcn@latest init
```

The CLI asks a few questions (design preset, base color and similar; the options vary by version), then:

- creates `components.json` (the CLI's config),
- adds a `cn()` helper in `lib/utils.ts`,
- adds design tokens (CSS variables) to your global CSS,
- installs the dependencies it needs.

Equivalent commands for npm/yarn/bun: replace `pnpm dlx` with `npx`, `yarn dlx` or `bunx`.

## Add components

Add only what you use:

```bash
pnpm dlx shadcn@latest add button
pnpm dlx shadcn@latest add dialog input label
```

Each command writes files such as `components/ui/button.tsx` and installs any extra dependencies. Use them via the alias:

```tsx
import { Button } from "@/components/ui/button";

export default function Page() {
  return <Button variant="outline">Click me</Button>;
}
```

## What gets generated

```text
components.json          CLI config: where files go, alias paths, style options
lib/utils.ts             cn() = clsx + tailwind-merge
components/ui/           the components you added (yours to edit)
app/globals.css          Tailwind import + theme tokens (CSS variables)
```

### `components.json`

It tells the CLI where things live. The exact fields depend on the CLI version, but typically include the style, whether you use React Server Components and TypeScript, the path to your global CSS file, and aliases such as `components`, `ui`, `lib` and `utils`. Change an alias here and the CLI will write future files there.

### `cn()`

Merges class names and resolves Tailwind conflicts, which is what lets a caller pass `className="px-8"` and have it win over a component's default `px-4`:

```ts
// lib/utils.ts
import { clsx, type ClassValue } from "clsx";
import { twMerge } from "tailwind-merge";

export function cn(...inputs: ClassValue[]) {
  return twMerge(clsx(inputs));
}
```

## Anatomy of a component

Components are small, readable wrappers. A typical one:

```tsx
// components/ui/button.tsx (abridged)
import * as React from "react";
import { cva, type VariantProps } from "class-variance-authority";
import { cn } from "@/lib/utils";

const buttonVariants = cva(
  "inline-flex items-center justify-center rounded-md text-sm font-medium transition-colors focus-visible:outline-none focus-visible:ring-2 disabled:opacity-50",
  {
    variants: {
      variant: {
        default: "bg-primary text-primary-foreground hover:bg-primary/90",
        outline: "border border-input bg-background hover:bg-accent",
        ghost: "hover:bg-accent",
      },
      size: { default: "h-10 px-4 py-2", sm: "h-9 px-3", lg: "h-11 px-8" },
    },
    defaultVariants: { variant: "default", size: "default" },
  },
);

export function Button({
  className, variant, size, ...props
}: React.ComponentProps<"button"> & VariantProps<typeof buttonVariants>) {
  return <button className={cn(buttonVariants({ variant, size }), className)} {...props} />;
}
```

The generated code in your project may differ in detail, but the pieces are the same: `cva` for variants, `cn` for merging, semantic color tokens (`bg-primary`, `text-primary-foreground`) instead of hard-coded colors.

Many interactive components (Dialog, Dropdown, Select, Tooltip) wrap accessible primitives for focus handling, keyboard navigation and ARIA. That is what you are really getting: behavior that is hard to build correctly, styled with your tokens.

## Server vs Client Components

- Presentational components (Button, Card, Badge) have no client behavior and can be used in **Server Components**.
- Interactive ones (Dialog, Dropdown, Tabs, Popover) use state, effects or event handlers, so their files start with `"use client"`. You can still **render them from a Server Component**; the boundary is inside the generated file.
- If you wrap an interactive component with your own state, your wrapper must be a Client Component. See [Composition Patterns](../03-components/03-composition-patterns.md).

## Customizing

Because the code is yours:

1. **Change the look globally** by editing the CSS variables (see [Theming](./03-theming.md)). Colors, radius and fonts flow through every component.
2. **Change one component** by editing its file in `components/ui/`: add a variant, change spacing, swap an icon.
3. **Compose, don't fork.** Build app-level components (`<ConfirmDialog>`, `<PageHeader>`) from the primitives, so `components/ui` stays close to upstream and easy to update.

Keep a short note of intentional edits to `components/ui/*` so you can re-apply them if you re-run `add` to pull a newer version.

## Forms with shadcn/ui

shadcn's form components commonly combine with React Hook Form and a schema library (Zod), which pairs naturally with the server-side validation from [Server Actions](../07-server-actions/02-validation.md): validate on the client for UX, **re-validate on the server** in the action. Check the current shadcn Form/Field docs for the recommended pattern in your CLI version, since it has changed over time.

## Updating

There is no `npm update` for shadcn components. To get upstream changes, re-run `add <component>` (the CLI can show a diff or ask before overwriting, depending on version) and merge it with your edits. Update only when you need a fix or a feature.

## Debugging

| Symptom | Likely cause | Fix |
|---|---|---|
| `Cannot find module '@/components/ui/button'` | Alias not configured | Check `tsconfig.json` `paths` and `components.json` aliases |
| Components look unstyled | Tailwind not set up, or tokens missing from global CSS | Ensure `globals.css` is imported and contains the theme variables |
| Colors wrong in dark mode | `.dark` tokens or dark variant missing | See [Theming](./03-theming.md) |
| "You're importing a component that needs `useState`..." | Interactive component imported into a Server Component file without a `"use client"` boundary | Use the generated file (it has the directive) or wrap in a Client Component |
| `className` override ignored | `cn()` not used, or conflicting class types | Pass classes through `cn()` |
| CLI cannot find the project | Wrong directory or no `package.json` | Run from the project root |
| Peer dependency warnings with React 19 | A dependency lags behind React | Follow the CLI prompt or docs guidance for your package manager |

## Common mistakes

| Mistake | Fix |
|---|---|
| Treating it as an npm package | It is source in your repo; edit and own it |
| Editing `components/ui/*` heavily for app logic | Compose app components on top |
| Adding every component up front | `add` only what you use |
| Hard-coding colors in components | Use the semantic tokens so theming works |
| Forgetting `"use client"` on custom interactive wrappers | Add it where state or handlers live |
| Client-only validation | Re-validate on the server |
| Never updating | Re-run `add` when you need upstream fixes |

## Quick Summary

- shadcn/ui copies component source into your repo; you own and edit it.
- `init` creates `components.json`, `lib/utils.ts` (`cn`) and theme tokens; `add <name>` writes `components/ui/<name>.tsx`.
- Built on Tailwind, `cva` variants and accessible primitives; styled with semantic color tokens.
- Interactive components are Client Components internally but can be rendered from Server Components.
- Customize globally with CSS variables, locally by editing the file, and structurally by composing wrappers.

## Next

- [Theming](./03-theming.md)
- [Tailwind CSS](./01-tailwindcss.md)
- [Composition Patterns](../03-components/03-composition-patterns.md)
