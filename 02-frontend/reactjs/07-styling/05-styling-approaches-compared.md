# Styling Approaches Compared

There is no single best way to style React. Each approach trades off authoring speed, runtime cost, scoping, theming, and compatibility with server components. This file compares the main options, gives a decision guide, and covers the small helpers (`clsx`, `tailwind-merge`, the `cn` function, `cva`) that most modern projects share regardless of approach.

## Prerequisites

[`00-css-essentials.md`](./00-css-essentials.md), [`01-css-modules.md`](./01-css-modules.md), and [`02-tailwindcss.md`](./02-tailwindcss.md)

> The library landscape changes quickly, and some tools below have changed maintenance status or direction. Treat names as examples and check each project's current status, documentation, and React-version support before adopting it.

---

## 1. The options

### Global CSS (plain stylesheets)

One or more `.css` files, imported once.

- **Pros:** zero tooling; full CSS; cacheable; easy for static sites.
- **Cons:** one global namespace, so collisions and ever-growing specificity; hard to know what's unused; needs a naming convention (BEM) to stay sane.
- **Good for:** small sites, base styles, resets, design tokens (custom properties), third-party overrides — **in combination with** something else.

### CSS Modules

Locally scoped CSS files ([`01-css-modules.md`](./01-css-modules.md)).

- **Pros:** real CSS, scoped by default, **no runtime**, works everywhere (including server components), easy to adopt.
- **Cons:** separate files to juggle; dynamic values need custom properties; no built-in variant or token system.
- **Good for:** teams comfortable with CSS; apps that want minimal tooling and maximum portability.

### Utility-first CSS (Tailwind)

Compose utility classes in markup ([`02-tailwindcss.md`](./02-tailwindcss.md)).

- **Pros:** very fast iteration; constrained design scale; small CSS output; **no runtime**; huge ecosystem (shadcn/ui); no naming or dead CSS.
- **Cons:** verbose markup; learning the vocabulary; dynamic class gotchas; needs `tailwind-merge` for overrides.
- **Good for:** most new product apps, especially with a component library on top.

### Runtime CSS-in-JS (styled-components, Emotion)

Write CSS in JavaScript template strings or objects, generated into `<style>` tags **at runtime**.

- **Pros:** styles colocated with components; dynamic styling driven by props; automatic scoping; theming through a provider.
- **Cons:** **runtime cost** (parsing and injecting styles during render); more complexity with concurrent rendering and server rendering; generally **incompatible with React Server Components**, which can't run client-side style injection; larger bundles. Several popular libraries have slowed development or entered maintenance mode, so verify status.
- **Good for:** existing codebases already using them. **Generally not recommended for new projects** that want server components or the best performance.

### Zero-runtime CSS-in-JS (vanilla-extract, Panda CSS, Linaria, StyleX)

Write styles in TypeScript/JavaScript, but **extract them to static CSS at build time**.

- **Pros:** type-safe styles and tokens, colocation, no runtime cost, usable with server components, strong for design systems.
- **Cons:** build-tool integration; smaller ecosystems; different mental models and learning curve; less flexible with truly runtime values.
- **Good for:** design-system-heavy teams that want typed tokens without runtime overhead.

### Component libraries with built-in styling (MUI, Chakra, Mantine, Ant Design)

Pre-built, pre-styled components with their own theming.

- **Pros:** complete, accessible components immediately; consistent look; docs and ecosystem.
- **Cons:** look and feel of the library; overriding deep styles can be painful; bundle size; coupling to the library's theming and sometimes its CSS-in-JS engine; upgrades.
- **Good for:** admin tools, internal apps, prototypes, teams without a designer.

### Copy-in components on utilities (shadcn/ui: Radix + Tailwind)

You own the component source, built on unstyled accessible primitives and Tailwind ([`../09-ui-components/00-shadcn-ui.md`](../09-ui-components/00-shadcn-ui.md)).

- **Pros:** full control with a strong accessible starting point; no runtime styling engine; tokens through CSS variables.
- **Cons:** you maintain the code; updates are manual diffs.
- **Good for:** product apps that want custom branding without writing primitives.

### Inline styles (`style` prop)

- **Pros:** truly dynamic values; no tooling.
- **Cons:** no pseudo-classes, media queries, or keyframes; hard to override; no reuse.
- **Good for:** the **dynamic part only** (computed positions, sizes) — combined with classes or custom properties.

---

## 2. Side-by-side

| | Global CSS | CSS Modules | Tailwind | Runtime CSS-in-JS | Zero-runtime CSS-in-JS | Component library |
|---|:---:|:---:|:---:|:---:|:---:|:---:|
| Scoping | ❌ manual | ✅ | ✅ (utilities) | ✅ | ✅ | ✅ |
| Runtime cost | none | none | none | **yes** | none | varies |
| Server components | ✅ | ✅ | ✅ | ❌ (generally) | ✅ | varies |
| Dynamic styles | custom props | custom props | `style`/custom props | ✅ easy | limited | via props |
| Type-safe styles | ❌ | ⚠ via plugin | ⚠ via tooling | ✅ | ✅ | partly |
| Authoring speed | medium | medium | **fast** | medium | medium | **fastest** (if it fits) |
| Theming | custom props | custom props | tokens + custom props | provider | tokens | built-in |
| Learning curve | low | low | medium | medium | medium–high | low–medium |
| Escape hatch for odd designs | ✅ | ✅ | ✅ | ✅ | ✅ | ⚠ often painful |

---

## 3. Decision guide

Ask these questions in order:

1. **Do you use (or plan to use) React Server Components or a framework like Next.js App Router?** → Avoid runtime CSS-in-JS. Choose CSS Modules, Tailwind, or zero-runtime options.
2. **Do you need a complete, accessible component set quickly, and does a library's look suit you?** → A component library (MUI, Mantine, Chakra) — or shadcn/ui if you want ownership and custom branding.
3. **Do you have a dedicated design system team and want typed tokens?** → Consider zero-runtime CSS-in-JS (vanilla-extract, Panda) or CSS Modules with CSS variables.
4. **Is the team strong in CSS and prefers separation of concerns?** → CSS Modules.
5. **Do you want to move fast with a constrained scale and a huge ecosystem?** → Tailwind (plus `cn` and `cva`).
6. **Joining an existing codebase?** → **Match what's there** and follow its conventions. Consistency beats a "better" approach used in one corner.

If still unsure: **Tailwind + shadcn/ui** or **CSS Modules + CSS variables** are safe, widely used, zero-runtime defaults.

### Mixing approaches

Mixing is normal and healthy when each tool has a clear job:

- **CSS variables** for tokens (colors, spacing), shared by everything.
- **Tailwind or CSS Modules** for component styling.
- **`style` prop** (or custom properties) for runtime values.
- **A library's CSS** for a single third-party widget (scoped with `@layer` or a wrapper class).

Avoid two systems doing the *same* job in one app (Tailwind **and** CSS Modules **and** styled-components all styling ordinary components): reviewers have to know three systems and specificity conflicts multiply. Use `@layer` to control order when you must mix.

---

## 4. Helper tools every approach shares

### `clsx`: conditional class names

```bash
npm install clsx
```

```tsx
import clsx from "clsx";

clsx("btn", isActive && "btn-active", { "btn-disabled": disabled }, className);
// → "btn btn-active btn-disabled px-4"   (falsy values are dropped)
```

It handles strings, arrays, objects, and conditionals, and never emits `"undefined"` or `"false"`. Works with plain classes, CSS Modules (`styles.x`), and Tailwind.

### `tailwind-merge`: resolving Tailwind conflicts

```bash
npm install tailwind-merge
```

```tsx
import { twMerge } from "tailwind-merge";

twMerge("px-4 py-2", "px-8");        // → "py-2 px-8"   (later conflicting class wins)
twMerge("text-sm", cond && "text-lg");
```

It understands which utilities conflict (`px-4` vs `px-8`, `text-red-500` vs `text-blue-500`) and keeps the last one — fixing the problem described in [`02-tailwindcss.md`](./02-tailwindcss.md). It's Tailwind-specific; don't use it with non-Tailwind classes.

### `cn`: the standard combo

Almost every Tailwind + React project defines one helper (shadcn/ui generates it for you):

```ts
// src/lib/cn.ts
import { clsx, type ClassValue } from "clsx";
import { twMerge } from "tailwind-merge";

export function cn(...inputs: ClassValue[]) {
  return twMerge(clsx(inputs));
}
```

```tsx
import { cn } from "@/lib/cn";

function Card({ className, ...props }: ComponentProps<"div">) {
  return (
    <div
      className={cn("rounded-lg border bg-card p-4 shadow-sm", className)}
      {...props}
    />
  );
}

<Card className="p-8" />   // caller's p-8 overrides the default p-4
```

The `@/` path alias is configured in Vite and TypeScript ([`../00-setup/02-vite.md`](../00-setup/02-vite.md), [`../00-setup/05-typescript-and-linting-setup.md`](../00-setup/05-typescript-and-linting-setup.md)). With CSS Modules, use `clsx` alone; with plain CSS, `clsx` or template strings.

### `cva` (class-variance-authority): typed variants

Describe a component's variants declaratively instead of hand-rolled maps ([`../05-component-design/00-component-api-design.md`](../05-component-design/00-component-api-design.md)):

```tsx
import { cva, type VariantProps } from "class-variance-authority";

const button = cva(
  "inline-flex items-center justify-center rounded-md font-medium transition-colors focus-visible:outline-2 disabled:opacity-50",
  {
    variants: {
      variant: {
        primary: "bg-blue-600 text-white hover:bg-blue-700",
        secondary: "bg-gray-100 text-gray-900 hover:bg-gray-200",
        ghost: "hover:bg-gray-100",
      },
      size: { sm: "h-8 px-3 text-sm", md: "h-10 px-4", lg: "h-12 px-6 text-lg" },
    },
    defaultVariants: { variant: "primary", size: "md" },
  }
);

type ButtonProps = ComponentProps<"button"> & VariantProps<typeof button>;

function Button({ variant, size, className, ...props }: ButtonProps) {
  return <button className={cn(button({ variant, size }), className)} {...props} />;
}
```

`VariantProps` derives the prop types from the definition, so `variant="danger"` is a compile error until you add it. Full coverage: [`../09-ui-components/01-component-variants.md`](../09-ui-components/01-component-variants.md).

---

## 5. Cross-cutting concerns

### Performance

- **Zero-runtime options** (CSS Modules, Tailwind, extracted CSS-in-JS) ship static CSS that browsers cache and parse in parallel with JavaScript. They add nothing to your render path.
- **Runtime CSS-in-JS** serializes styles and touches the DOM during render, and can add overhead on interaction-heavy screens.
- Ship **less CSS**: Tailwind and CSS Modules (with code splitting) both help; plain global CSS tends to grow forever. See [`../14-performance/05-bundle-optimization.md`](../14-performance/05-bundle-optimization.md).
- **Critical CSS** and avoiding render-blocking styles matter more for first paint than the authoring approach.

### Server rendering and server components

Server components can't use hooks or context for styling. Static CSS approaches (Modules, Tailwind, zero-runtime) "just work"; runtime CSS-in-JS needs client-component boundaries or a different approach ([`../15-concurrent-and-modern-react/07-server-components-and-ssr.md`](../15-concurrent-and-modern-react/07-server-components-and-ssr.md)).

### Theming

Every approach can be themed with **CSS custom properties** — the common denominator ([`04-theming-and-dark-mode.md`](./04-theming-and-dark-mode.md)). Prefer them over JavaScript-only theme objects, which require React re-renders and can't be seen by plain CSS.

### Team and maintainability

- **Consistency** matters more than the specific tool. Document conventions in `CONTRIBUTING.md`.
- **Onboarding:** CSS Modules and plain CSS need almost no new vocabulary; Tailwind requires learning its classes; CSS-in-JS needs library-specific knowledge.
- **Refactoring and deleting:** colocated approaches (Tailwind, CSS-in-JS, CSS Modules next to the component) make deleting a component delete its styles.
- **Design handoff:** shared tokens (colors, spacing) aligned with the design tool reduce drift.
- **Linting:** `stylelint` for CSS; `eslint-plugin-tailwindcss` or the Prettier plugin for class ordering.

### Migration

Migrating gradually is realistic: pick the target approach for **new** components, convert components when you touch them, keep tokens as shared CSS variables so both worlds look identical, and avoid big-bang rewrites. Use `@layer` to make old and new styles coexist predictably.

---

## 6. A reasonable default stack

For a new Vite + React + TypeScript app:

1. **Tailwind** for styling, configured with semantic tokens as CSS variables.
2. **`cn` (`clsx` + `tailwind-merge`)** for class composition.
3. **`cva`** for component variants.
4. **shadcn/ui (Radix primitives)** for accessible components you own.
5. **`style` or custom properties** for genuinely dynamic values.

Prefer CSS Modules if your team favors writing CSS; you lose nothing in portability and gain familiarity. Both stay fast, work with server components, and avoid lock-in.

---

## Common mistakes

- **Adopting runtime CSS-in-JS for a new app that will use server components** — hits compatibility and performance limits.
- **Using several styling systems for ordinary components in one app** — conflicting specificity and a steep onboarding curve.
- **Using `tailwind-merge` with non-Tailwind classes**, or forgetting it where Tailwind callers override defaults.
- **Hard-coding colors instead of tokens**, making future theming painful.
- **Choosing a tool on hype** instead of team skills, constraints, and project needs.
- **Rewriting all styles at once** when switching approaches — migrate incrementally.
- **Storing theme values only in JavaScript** — plain CSS and other tools can't see them; use custom properties.
- **Ignoring bundle and CSS growth** until it's a problem.
- **Treating a library's look as final** and then fighting its styles for every custom design.

## Quick summary

- Options: global CSS, CSS Modules, Tailwind, runtime CSS-in-JS, zero-runtime CSS-in-JS, component libraries, shadcn/ui-style copy-in components, and inline styles for dynamic bits
- Prefer **zero-runtime** approaches (CSS Modules, Tailwind, extracted CSS-in-JS), especially with server components
- Decide by: server components, need for ready-made components, design-system needs, team CSS skills, and existing codebase conventions
- Mix deliberately: CSS variables for tokens, one main styling system for components, `style` for runtime values
- Shared helpers: `clsx`, `tailwind-merge`, the `cn` function, `cva` for variants
- Theme with CSS custom properties, and migrate gradually

## Next

You've finished the styling chapter. Continue to **[`../08-accessibility/README.md`](../08-accessibility/README.md)** to make sure what you've styled works for everyone. For building on these tools, see [`../09-ui-components/01-component-variants.md`](../09-ui-components/01-component-variants.md) and [`../09-ui-components/09-design-system.md`](../09-ui-components/09-design-system.md).
