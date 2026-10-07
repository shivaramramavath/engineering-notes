# Tailwind CSS

Tailwind is a utility-first CSS framework: you style elements by composing small classes (`flex`, `p-4`, `text-lg`) directly in markup. The Next.js docs recommend it for most styling needs, and `create-next-app` offers it as a default.

> Verified against the Next.js 16.4 docs. This note covers **Tailwind v4**, the current setup. v3 differs (config file, `content` globs); see the v3 section.

## What and why

- **No naming problem.** You do not invent class names.
- **Small output.** Only classes you use are generated.
- **Consistent design.** Spacing, colors and type come from one scale.
- **Works everywhere.** It is plain CSS at build time, so it works in Server Components and Client Components alike.

The cost is long `className` strings. Manage them with component extraction (below), not with `@apply` everywhere.

## Setup (v4)

With `create-next-app`, choose Tailwind when prompted and it is done. For an existing project:

```bash
npm install -D tailwindcss @tailwindcss/postcss
```

```js
// postcss.config.mjs
export default {
  plugins: {
    "@tailwindcss/postcss": {},
  },
};
```

```css
/* app/globals.css */
@import "tailwindcss";
```

```tsx
// app/layout.tsx
import "./globals.css";
```

That is all. v4 has **no `tailwind.config.js` by default** and detects your source files automatically. Customize in CSS (below).

For very old browsers, the docs point to a Tailwind v3 setup, since v4 targets modern browsers.

## Using it

```tsx
export default function Page() {
  return (
    <main className="mx-auto flex min-h-screen max-w-2xl flex-col gap-6 p-6">
      <h1 className="text-3xl font-bold tracking-tight">Welcome</h1>
      <p className="text-neutral-600">A short description.</p>
      <button className="self-start rounded-md bg-black px-4 py-2 text-white hover:bg-neutral-800 focus-visible:outline-2 focus-visible:outline-offset-2">
        Get started
      </button>
    </main>
  );
}
```

### Variants

Prefix a utility to apply it conditionally:

| Prefix | When |
|---|---|
| `hover:` `focus:` `focus-visible:` `active:` `disabled:` | Interaction states |
| `sm:` `md:` `lg:` `xl:` | Viewport **at least** that wide (mobile-first) |
| `dark:` | Dark mode (see [Theming](./03-theming.md)) |
| `group-hover:` / `peer-checked:` | Based on a parent (`group`) or sibling (`peer`) |
| `has-[...]:` / `aria-[...]:` / `data-[...]:` | Based on descendants, ARIA or data attributes |

Mobile-first means unprefixed classes are the base and breakpoints add changes upward: `class="p-4 md:p-8"`.

```tsx
<ul className="grid grid-cols-1 gap-4 sm:grid-cols-2 lg:grid-cols-3">
```

## Customizing in CSS (v4)

Design tokens live in an `@theme` block in your CSS:

```css
@import "tailwindcss";

@theme {
  --color-brand: oklch(0.62 0.19 260);
  --font-display: "Fraunces", serif;
  --breakpoint-3xl: 120rem;
}
```

These become utilities automatically: `bg-brand`, `text-brand`, `font-display`, `3xl:...`.

Use `@theme inline` when a token should reference **another CSS variable** at use time (needed for `next/font` variables and theming):

```css
@theme inline {
  --font-sans: var(--font-inter);       /* defined by next/font on <html> */
  --color-background: var(--background); /* swapped by .dark */
}
```

Custom utilities and variants:

```css
@utility text-balance {
  text-wrap: balance;
}

@custom-variant dark (&:where(.dark, .dark *));   /* class-based dark mode */
```

If you still need a JS config (for a plugin or migration), load it with `@config "./tailwind.config.ts";` in your CSS.

## Dynamic class names: the one rule

Tailwind scans your source for **complete class strings**. It cannot see classes assembled at runtime.

```tsx
// Broken: Tailwind never sees "bg-red-500" or "bg-green-500" as whole strings
<div className={`bg-${color}-500`} />

// Works: write the full names
const styles = {
  red: "bg-red-500 text-white",
  green: "bg-green-500 text-white",
} as const;

<div className={styles[color]} />
```

For values that are truly dynamic (a color from a database), use a CSS variable and an arbitrary-value class:

```tsx
<div style={{ "--accent": accent } as React.CSSProperties} className="bg-(--accent)" />
```

## Merging and conditional classes

Two small helpers keep long class lists manageable:

```bash
npm install clsx tailwind-merge
```

```ts
// lib/utils.ts
import { clsx, type ClassValue } from "clsx";
import { twMerge } from "tailwind-merge";

export function cn(...inputs: ClassValue[]) {
  return twMerge(clsx(inputs));
}
```

```tsx
import { cn } from "@/lib/utils";

export function Button({ className, variant = "primary", ...props }: React.ComponentProps<"button"> & { variant?: "primary" | "ghost" }) {
  return (
    <button
      className={cn(
        "rounded-md px-4 py-2 text-sm font-medium",
        variant === "primary" && "bg-black text-white hover:bg-neutral-800",
        variant === "ghost" && "hover:bg-neutral-100",
        className,                       // caller overrides win
      )}
      {...props}
    />
  );
}
```

`clsx` handles conditionals; `tailwind-merge` resolves conflicts (`px-4` then `px-2` keeps `px-2`). This is the same `cn` helper shadcn/ui uses ([next note](./02-shadcn-ui.md)).

## Organizing long class lists

1. **Extract a component**, not a CSS class. `<Button>`, `<Card>` carry their classes once.
2. Use **variants objects** or a library such as `class-variance-authority` when a component has many variants.
3. Use `@apply` sparingly, mostly for third-party markup you cannot add classes to.
4. Use the Prettier plugin `prettier-plugin-tailwindcss` to sort classes consistently (see [Linting and Formatting](../00-setup/02-linting-and-formatting.md)).

## When to leave Tailwind

Use a CSS Module (or plain CSS) for things utilities express badly: complex keyframe animations, intricate selectors, third-party DOM you cannot touch. Mixing is fine.

## Tailwind v3 differences

| | v3 | v4 |
|---|---|---|
| Setup | `tailwindcss` + `postcss` + `autoprefixer`, `tailwind.config.js` | `tailwindcss` + `@tailwindcss/postcss` |
| CSS entry | `@tailwind base; @tailwind components; @tailwind utilities;` | `@import "tailwindcss";` |
| Source files | `content: [...]` globs | Detected automatically |
| Theme | JS `theme.extend` | `@theme` in CSS |
| Fonts with `next/font` | `fontFamily: { sans: ["var(--font-inter)"] }` in config | `@theme inline { --font-sans: var(--font-inter); }` |

If you upgrade, use Tailwind's official upgrade tool rather than editing by hand.

## Debugging

| Symptom | Likely cause | Fix |
|---|---|---|
| No styles at all | `globals.css` not imported, or `@import "tailwindcss"` missing | Import it in the root layout |
| A class has no effect | Class built dynamically, or file not scanned | Write full class names; check the file is in the project |
| Works in dev, missing in production | Dynamic class names | Same fix |
| `dark:` does nothing | Dark variant is media-based but you toggle a class | Add the `@custom-variant dark` line |
| Overrides ignored | Conflicting utilities in a string | `cn()` with `tailwind-merge` |
| Font not applied | `--font-sans` not mapped | `@theme inline { --font-sans: var(--font-inter); }` |
| Build error on `@apply` or `@theme` | Old v3 tooling still installed | Remove v3 packages and config |

## Common mistakes

| Mistake | Fix |
|---|---|
| `` `bg-${color}-500` `` | Map to complete class names |
| Mixing v3 and v4 setup | Choose one; follow its setup |
| Giant class strings repeated everywhere | Extract components |
| `@apply` for everything | Use components instead |
| Forgetting mobile-first (`md:` meaning "up", not "only") | Start from the base, add breakpoints upward |
| Overriding without `tailwind-merge` | `cn()` |
| Not sorting classes | Prettier Tailwind plugin |

## Quick Summary

- v4 setup: `tailwindcss` + `@tailwindcss/postcss`, `@import "tailwindcss"` in `globals.css`, import it in the root layout.
- Customize with `@theme` (and `@theme inline` for variable references) in CSS; no config file needed.
- Write complete class names; never build them from string fragments.
- Use `cn()` (`clsx` + `tailwind-merge`) for conditional and overridable classes.
- Extract components instead of copying class strings or abusing `@apply`.

## Next

- [shadcn/ui](./02-shadcn-ui.md)
- [Theming](./03-theming.md)
- [Fonts](./05-fonts.md)
