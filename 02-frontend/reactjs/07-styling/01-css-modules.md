# CSS Modules

CSS Modules are ordinary CSS files whose class names are **automatically made unique** at build time, so styles apply only to the component that imports them. You write normal CSS, get no collisions, and ship nothing extra at runtime. Vite supports them out of the box.

## Prerequisites

[`00-css-essentials.md`](./00-css-essentials.md) and [`../05-component-design/00-component-api-design.md`](../05-component-design/00-component-api-design.md) (variants and `className` forwarding)

---

## The problem they solve

With a global stylesheet, every class name lives in one namespace:

```css
/* Header.css */ .title { font-size: 2rem; }
/* Card.css   */ .title { font-size: 1rem; }   /* ⚠ overrides or is overridden by Header's .title */
```

Teams work around this with naming conventions (BEM: `.card__title--large`). CSS Modules remove the problem at the source.

---

## Basics

Name the file `*.module.css` and import it as an object:

```css
/* Button.module.css */
.button {
  padding: 0.5rem 1rem;
  border: 0;
  border-radius: 0.375rem;
  background: var(--color-primary);
  color: white;
  cursor: pointer;
}

.button:hover { filter: brightness(1.1); }
.button:focus-visible { outline: 2px solid var(--color-focus); outline-offset: 2px; }
```

```tsx
import styles from "./Button.module.css";

export function Button({ children }: { children: ReactNode }) {
  return <button className={styles.button}>{children}</button>;
}
```

At build time `.button` becomes something like `.Button_button__x7d2a`, and `styles.button` evaluates to that generated string. Another file's `.button` gets a different hash, so they can't collide.

**You can use short, generic names** (`.root`, `.title`, `.button`) — the module scope makes them unique. A common convention is `.root` for the component's outermost element.

---

## Class names with hyphens

JavaScript identifiers can't contain `-`, so either use camelCase in the CSS, or bracket access:

```css
.primaryButton { … }
.primary-button { … }
```

```tsx
styles.primaryButton
styles["primary-button"]
```

camelCase is simpler. (Vite can also convert names automatically via the `css.modules.localsConvention` option.)

---

## Multiple and conditional classes

```tsx
// Multiple
<div className={`${styles.card} ${styles.elevated}`} />

// Conditional — a helper keeps it readable (clsx; see 05-styling-approaches-compared.md)
import clsx from "clsx";

<button
  className={clsx(styles.button, {
    [styles.active]: isActive,
    [styles.disabled]: disabled,
  })}
/>
```

### Variants via a class map

```tsx
type ButtonProps = ComponentProps<"button"> & {
  variant?: "primary" | "secondary";
  size?: "sm" | "md";
};

export function Button({ variant = "primary", size = "md", className, ...props }: ButtonProps) {
  return (
    <button
      className={clsx(styles.button, styles[variant], styles[size], className)}
      {...props}
    />
  );
}
```

```css
.primary { background: var(--color-primary); color: white; }
.secondary { background: transparent; border: 1px solid currentColor; }
.sm { padding: 0.25rem 0.5rem; font-size: 0.875rem; }
.md { padding: 0.5rem 1rem; }
```

`styles[variant]` indexes into the module by the variant name, so adding a variant means adding a CSS class. Accepting and merging `className` lets callers add layout styles from outside (see below).

---

## Composition: `composes`

Reuse another class's styles without duplicating declarations:

```css
.base { padding: 0.5rem 1rem; border-radius: 0.375rem; }

.primary {
  composes: base;
  background: var(--color-primary);
}

.shared {
  composes: card from "./shared.module.css";   /* from another file */
}
```

`composes` makes the element receive **both** generated class names. It only works with **single-class** selectors. Many teams skip `composes` in favor of plain CSS custom properties and shared utility classes; use it if it makes sense for you.

---

## Going global: `:global`

By default every class is local. For selectors that must remain global (styling third-party markup, or an attribute set by a library), opt out:

```css
.list :global(.react-select__menu) { z-index: 10; }

:global(body.dark) .card { background: #111; }
```

Keep global usage rare and narrow.

---

## Element and attribute selectors still leak

Only **class names** (and ids and keyframe names) are scoped. Selectors on bare elements apply globally:

```css
/* ❌ Applies to every <p> in the app, not just this component's */
p { margin: 0; }
```

Always anchor selectors in a class: `.root p { margin: 0; }`. Descendant selectors inside your module stay local because they begin with a scoped class.

---

## Dynamic values with custom properties

CSS Modules are static files, so pass runtime values through CSS variables:

```tsx
<div
  className={styles.progress}
  style={{ "--value": `${percent}%` } as React.CSSProperties}
/>
```

```css
.progress::after {
  content: "";
  display: block;
  width: var(--value);
  height: 100%;
  background: var(--color-primary);
}
```

Declare a small helper type if you do this often (`type CSSVars = React.CSSProperties & Record<`--${string}`, string>`). This also keeps the "dynamic" part tiny while all styling stays in CSS.

---

## Theming

Use design tokens as global custom properties (defined once on `:root`), and reference them from your modules ([`04-theming-and-dark-mode.md`](./04-theming-and-dark-mode.md)):

```css
.card {
  background: var(--surface);
  color: var(--text);
  border: 1px solid var(--border);
}
```

Switching theme changes the variables, and every module follows with no component changes.

---

## Overriding from outside

Because class names are hashed, a parent can't target a child's internal class with a normal selector. The clean way to allow outside customization is a **`className` prop** merged onto the component's root:

```tsx
<Card className={layoutStyles.sidebarCard} />
```

Keep the **component's own styles** in its module and **layout** concerns (margins, grid placement) in the parent's module. Don't reach into a child's internals with `:global` hacks.

When two classes of equal specificity conflict (the child's and the passed-in one), the one that comes **later in the generated stylesheet** wins, and that order depends on import order, which is fragile. Design APIs where outside classes handle placement and inner classes handle appearance, so they rarely overlap. (Tailwind's `tailwind-merge` solves the equivalent problem for utilities — see [`05-styling-approaches-compared.md`](./05-styling-approaches-compared.md).)

---

## TypeScript

Vite's client types declare `*.module.css` imports as `Record<string, string>`, so `styles.anything` type-checks even if it doesn't exist (you get `undefined` at runtime). For checked class names, use a plugin that generates `.d.ts` files (for example, `typed-css-modules` or `typescript-plugin-css-modules`). Make sure `vite/client` types are included, which the Vite template does in `src/vite-env.d.ts` ([`../00-setup/03-project-structure.md`](../00-setup/03-project-structure.md)).

---

## Sass and other preprocessors

`*.module.scss` works the same way after installing `sass`. Native CSS now provides nesting and variables, so you may not need Sass. If you use it, keep nesting shallow to avoid high specificity.

---

## Strengths and weaknesses

| Strengths | Weaknesses |
|-----------|------------|
| Real CSS — full feature set, no new syntax | Switching between `.tsx` and `.module.css` files |
| Scoped by default; no naming conventions | Dynamic styling requires custom properties |
| **No runtime cost**; plain CSS files, cacheable | Dead-CSS detection is manual-ish |
| Works with server components and any framework | No built-in variant or design-token system |
| Easy to adopt incrementally | Cross-component overrides need deliberate API design |

Compared with Tailwind and CSS-in-JS: [`05-styling-approaches-compared.md`](./05-styling-approaches-compared.md).

---

## Common mistakes

- **Using hyphenated class names with dot access** (`styles.primary-button`) — use camelCase or bracket notation.
- **Styling bare elements** (`p`, `button`) in a module — those rules are global.
- **Expecting `:global` or hashed names to be targetable** from other files — pass a `className` instead.
- **Building class names with string concatenation** (`styles["btn-" + size]`) when the key may not exist — you get `undefined`, rendering `"undefined"` as a class.
- **Mixing too many `composes`** — hard to trace; prefer shared tokens and small classes.
- **Putting dynamic values in inline styles for everything** — use custom properties to keep styling in CSS.
- **Not forwarding `className`** — callers can't adjust layout without forking the component.
- **Relying on import order** to resolve specificity ties — fragile.

## Quick summary

- Name files `*.module.css`; `import styles from "./X.module.css"`; classes are locally scoped via generated names
- Prefer camelCase class names; use `clsx` for conditional and variant classes
- Only classes are scoped; element selectors stay global, so always anchor in a class
- Pass dynamic values with CSS custom properties; theme with global tokens
- Allow outside customization through a merged `className` prop, not by reaching into internals
- Zero runtime cost and framework-agnostic, but you write and organize the CSS yourself

## Next

**[`02-tailwindcss.md`](./02-tailwindcss.md)** covers the utility-first alternative. If you're set on CSS Modules, continue to **[`03-responsive-design.md`](./03-responsive-design.md)**.
