# Tailwind CSS

Tailwind is a **utility-first** CSS framework: instead of writing CSS rules and naming classes, you compose small single-purpose classes (`flex`, `p-4`, `text-sm`, `hover:bg-blue-700`) directly in your markup. It generates only the CSS you actually use, keeps design values consistent through a shared scale, and pairs naturally with React components. It's also the styling layer under shadcn/ui ([`../09-ui-components/00-shadcn-ui.md`](../09-ui-components/00-shadcn-ui.md)).

## Prerequisites

[`00-css-essentials.md`](./00-css-essentials.md) — Tailwind utilities map directly onto CSS properties, so CSS knowledge transfers one-to-one. [`../05-component-design/00-component-api-design.md`](../05-component-design/00-component-api-design.md) for variants.

> **Version note.** This file describes the current CSS-first setup (Tailwind v4). Older projects use v3, with a `tailwind.config.js` and `@tailwind` directives. The utility class names are mostly the same; configuration and setup differ. Check the official docs for the version you install, especially for install steps and for `dark:` variant configuration.

---

## The idea

```tsx
<button className="rounded-md bg-blue-600 px-4 py-2 text-sm font-medium text-white hover:bg-blue-700 focus-visible:outline-2 focus-visible:outline-offset-2 focus-visible:outline-blue-600 disabled:opacity-50">
  Save
</button>
```

Each class does one thing: `px-4` (horizontal padding), `text-sm` (font size), `hover:bg-blue-700` (background on hover). There's no separate stylesheet to name, find, or keep in sync, and no dead CSS: delete the markup and its styles go with it.

The common objection is that long class lists are ugly. In React that's mitigated by **components**: you write the class list **once** inside `Button`, and use `<Button>` everywhere ([Components, not `@apply`](#components-not-apply)).

---

## Setup with Vite (v4)

```bash
npm install tailwindcss @tailwindcss/vite
```

```ts
// vite.config.ts
import { defineConfig } from "vite";
import react from "@vitejs/plugin-react";
import tailwindcss from "@tailwindcss/vite";

export default defineConfig({
  plugins: [react(), tailwindcss()],
});
```

```css
/* src/index.css */
@import "tailwindcss";
```

Import `index.css` in `main.tsx` ([`../00-setup/03-project-structure.md`](../00-setup/03-project-structure.md)). Tailwind scans your source files for class names and emits only what it finds. Also install the editor extension (Tailwind CSS IntelliSense) for autocomplete and previews, and consider `prettier-plugin-tailwindcss` to sort classes consistently ([`../00-setup/05-typescript-and-linting-setup.md`](../00-setup/05-typescript-and-linting-setup.md)).

---

## The core utilities

Tailwind's names follow CSS closely. A small sample:

| Area | Utilities |
|------|-----------|
| Spacing | `p-4`, `px-2`, `mt-6`, `gap-3`, `space-y-2` (scale: `1` = 0.25rem, `4` = 1rem) |
| Sizing | `w-full`, `h-10`, `size-8`, `max-w-md`, `min-h-screen` |
| Layout | `flex`, `grid`, `grid-cols-3`, `items-center`, `justify-between`, `hidden`, `block` |
| Position | `relative`, `absolute`, `inset-0`, `sticky`, `top-0`, `z-10` |
| Typography | `text-lg`, `font-semibold`, `leading-6`, `tracking-tight`, `truncate`, `text-center` |
| Color | `bg-white`, `text-gray-700`, `border-gray-200`, `ring-blue-500`, `bg-blue-600/80` (opacity) |
| Borders & effects | `rounded-lg`, `border`, `shadow-md`, `ring-2`, `opacity-50` |
| Transitions | `transition`, `duration-200`, `ease-in-out` |

Example layout:

```tsx
<div className="mx-auto flex max-w-3xl flex-col gap-4 p-6">
  <header className="flex items-center justify-between">
    <h1 className="text-2xl font-semibold tracking-tight">Dashboard</h1>
    <Button>New project</Button>
  </header>
  <ul className="grid grid-cols-1 gap-4 sm:grid-cols-2">
    {projects.map((p) => <ProjectCard key={p.id} project={p} />)}
  </ul>
</div>
```

---

## State and context variants

Prefix a utility with a **variant** to apply it conditionally:

```tsx
<button className="bg-blue-600 hover:bg-blue-700 active:bg-blue-800 focus-visible:ring-2 disabled:opacity-50 disabled:pointer-events-none" />
```

| Variant | Applies when |
|---------|--------------|
| `hover:` `focus:` `focus-visible:` `active:` | Interaction states |
| `disabled:` `checked:` `invalid:` `required:` | Form states |
| `first:` `last:` `odd:` `even:` | Position among siblings |
| `sm:` `md:` `lg:` `xl:` | Viewport width ([`03-responsive-design.md`](./03-responsive-design.md)) |
| `dark:` | Dark mode ([`04-theming-and-dark-mode.md`](./04-theming-and-dark-mode.md)) |
| `motion-reduce:` `motion-safe:` | User motion preference |
| `aria-expanded:` `data-[state=open]:` | ARIA and data attributes (great with Radix/shadcn components) |
| `group-hover:` / `peer-checked:` | Style based on a parent (`group`) or sibling (`peer`) |
| `has-[:checked]:` | Style based on descendants |

Variants **stack**: `dark:md:hover:bg-gray-800`.

### Styling from a parent's state: `group`

```tsx
<a className="group flex items-center gap-2 rounded p-2 hover:bg-gray-100">
  <Icon className="text-gray-400 group-hover:text-gray-900" />
  <span>Settings</span>
</a>
```

### Data attributes from component libraries

```tsx
<div data-state={open ? "open" : "closed"} className="data-[state=open]:bg-gray-100 data-[state=closed]:opacity-70" />
```

---

## Arbitrary values

Escape the scale when you need a one-off with square brackets:

```tsx
<div className="w-[22rem] bg-[#1da1f2] top-[117px] grid-cols-[1fr_auto]" />
```

If you find yourself writing the same arbitrary value repeatedly, add it to your theme instead.

---

## Customizing the theme

In v4, extend the design tokens in CSS with `@theme`:

```css
@import "tailwindcss";

@theme {
  --color-brand-500: #2563eb;
  --color-brand-600: #1d4ed8;
  --font-display: "Inter", sans-serif;
  --breakpoint-3xl: 120rem;
}
```

This generates utilities automatically: `bg-brand-500`, `font-display`, `3xl:` variant. Because the tokens are real CSS custom properties, they work at runtime (`var(--color-brand-500)`) and power theming ([`04-theming-and-dark-mode.md`](./04-theming-and-dark-mode.md)). (v3 used `theme.extend` in `tailwind.config.js`.)

---

## Dynamic class names: the one rule you must know

Tailwind finds classes by scanning your files for **complete class-name strings**. It doesn't execute your code. So dynamically assembled names are invisible to it:

```tsx
// ❌ Tailwind never sees "text-red-600" or "text-green-600" as full strings
<p className={`text-${color}-600`} />

// ✅ Use complete class names in a lookup
const colors = {
  red: "text-red-600",
  green: "text-green-600",
} as const;

<p className={colors[color]} />
```

The same applies to conditional classes: write each full class somewhere in the source. For truly runtime values (user-chosen colors, pixel sizes from data), use the **`style` prop or CSS custom properties**: `style={{ width: `${percent}%` }}` or `className="w-(--bar)" style={{ "--bar": `${percent}%` }}`.

---

## Components, not `@apply`

`@apply` lets you build a class from utilities:

```css
.btn { @apply rounded-md bg-blue-600 px-4 py-2 text-white; }
```

Tailwind's own guidance is to **avoid it** in React apps. It recreates the "naming CSS classes" problem that utilities remove, and gives you indirection plus extra CSS. The React way to avoid repetition is a **component**:

```tsx
function Button({ className, ...props }: ComponentProps<"button">) {
  return (
    <button
      className={cn(
        "rounded-md bg-blue-600 px-4 py-2 text-sm font-medium text-white hover:bg-blue-700 disabled:opacity-50",
        className
      )}
      {...props}
    />
  );
}
```

Reserve `@apply` for styling markup you don't control (third-party widgets, rendered Markdown) — and consider the `prose` typography plugin for the latter.

---

## Merging classes: the conflict problem

Plain string concatenation can't resolve conflicts:

```tsx
<button className={`px-4 ${className}`} />   // caller passes "px-8" → both px-4 and px-8 are present
```

CSS decides the winner by **stylesheet order**, not by the order in your `className` string — so the caller's override may silently lose. The standard fix is **`tailwind-merge`**, combined with **`clsx`** for conditionals, wrapped in a small helper conventionally called `cn`. The helper, plus variant tooling (`cva`), is covered in [`05-styling-approaches-compared.md`](./05-styling-approaches-compared.md) and [`../09-ui-components/01-component-variants.md`](../09-ui-components/01-component-variants.md).

---

## Dark mode, responsive, and theming

- **Responsive:** mobile-first prefixes (`md:flex-row`) → [`03-responsive-design.md`](./03-responsive-design.md)
- **Dark mode and tokens:** `dark:` variant and semantic color variables → [`04-theming-and-dark-mode.md`](./04-theming-and-dark-mode.md)

---

## Accessibility with Tailwind

- Use **`focus-visible:`** rings; don't strip outlines (`outline-none`) without replacing them.
- **`sr-only`** hides content visually but keeps it for screen readers (labels for icon buttons): `<span className="sr-only">Close</span>`.
- Check **contrast** — palette colors aren't automatically accessible in every combination (`text-gray-400` on white fails for body text).
- Use **`motion-reduce:`** to turn off animations for users who prefer reduced motion.
- Semantic HTML still matters; utilities don't add roles or labels ([`../08-accessibility/`](../08-accessibility/README.md)).

---

## Strengths and weaknesses

| Strengths | Weaknesses |
|-----------|------------|
| Fast to build; no context switching to CSS files | Long class lists; markup readability suffers without components |
| Consistent design scale; small final CSS | Learning the vocabulary takes a few days |
| No naming, no dead CSS, no collisions | Dynamic class names need care |
| Zero runtime; works with server components | Class conflicts require `tailwind-merge` |
| Excellent ecosystem (shadcn/ui, plugins) | Heavy customization of design tokens still needs planning |

Comparison with CSS Modules and CSS-in-JS: [`05-styling-approaches-compared.md`](./05-styling-approaches-compared.md).

---

## Common mistakes

- **Building class names with template strings** (`bg-${color}-500`) — they're never generated; use full names in a map.
- **Using `@apply` to recreate CSS classes** — extract a React component instead.
- **Concatenating caller `className` without `tailwind-merge`** — overrides silently fail.
- **Removing outlines** (`outline-none`) with no `focus-visible:` replacement.
- **Using Tailwind classes for values that are truly runtime data** — use `style` or custom properties.
- **Not using the `cn`/`clsx` helper for conditionals** — messy string handling.
- **Mixing v3 and v4 instructions** — check the docs for your version.
- **Reaching for arbitrary values constantly** — define tokens in the theme.
- **Ignoring contrast** because "it's from the palette".

## Quick summary

- Tailwind composes small utility classes in markup; unused styles aren't shipped
- Variants (`hover:`, `md:`, `dark:`, `disabled:`, `data-[...]:`) apply utilities conditionally and stack
- Customize tokens with `@theme` (v4) or the config file (v3)
- Class names must appear as complete strings in source; use maps for dynamic variants
- Remove repetition with React components, not `@apply`
- Merge classes with `tailwind-merge` via a `cn` helper
- Keep accessibility: `focus-visible`, `sr-only`, contrast, reduced motion

## Next

**[`03-responsive-design.md`](./03-responsive-design.md)** shows how to make layouts adapt from phones to large screens, with both plain CSS and Tailwind.
