# CSS in Next.js

Next.js supports several ways to write CSS. This note covers how each works, how they interact with Server and Client Components, and how CSS ordering behaves between development and production.

> Verified against the Next.js 16.4 docs.

## The options

| Option | What it is | Use it for |
|---|---|---|
| [Tailwind CSS](./01-tailwindcss.md) | Utility classes in your markup | **Most component styling** (the docs' recommendation) |
| **CSS Modules** | Scoped `.module.css` files | Component-specific CSS that utilities cannot express |
| **Global CSS** | A plain stylesheet imported once | Truly global rules: resets, design tokens, Tailwind base |
| **External stylesheets** | CSS from npm packages | Third-party UI kits |
| **Sass** | `.scss` / `.module.scss` | Teams already on Sass (`npm i sass`) |
| **CSS-in-JS** | Runtime style libraries | Legacy codebases; needs care with Server Components |

The practical default for a new project: **Tailwind for almost everything, CSS Modules when needed, a small global file for tokens and base styles.**

## Global CSS

Import it once, in the root layout. It then applies to every route.

```css
/* app/globals.css */
:root {
  --radius: 0.5rem;
}

html {
  color-scheme: light dark;
}

body {
  margin: 0;
  line-height: 1.5;
}
```

```tsx
// app/layout.tsx
import "./globals.css";

export default function RootLayout({ children }: { children: React.ReactNode }) {
  return (
    <html lang="en">
      <body>{children}</body>
    </html>
  );
}
```

Global styles can technically be imported in any layout, page or component inside `app/`. The docs warn against it: Next.js integrates stylesheets with React's Suspense support and **does not remove a global stylesheet when you navigate away**, so styles from one route can leak into another and conflict. Keep global CSS for rules that are meant to be global, and import it from the root layout only.

## CSS Modules

A file named `*.module.css` is **locally scoped**: class names are rewritten to be unique, so `.card` in two files never collides.

```css
/* app/blog/blog.module.css */
.blog {
  padding: 24px;
}

.title {
  font-size: 1.5rem;
}

.title:hover {
  text-decoration: underline;
}
```

```tsx
// app/blog/page.tsx
import styles from "./blog.module.css";

export default function Page() {
  return (
    <main className={styles.blog}>
      <h1 className={styles.title}>Blog</h1>
    </main>
  );
}
```

Details:

- Use **class selectors**. Plain element selectors (`h1 { ... }`) are not allowed on their own in a module; scope them (`.blog h1 { ... }`).
- Combine classes with template strings or a helper: `` `${styles.card} ${styles.active}` ``.
- Names with dashes are accessed as `styles["card-title"]`; prefer camelCase (`cardTitle`) so you can write `styles.cardTitle`.
- To share values, use CSS custom properties from your global file (`var(--radius)`).
- Name the module after its component (`button.module.css` beside `button.tsx`).

CSS Modules work in **both Server and Client Components**, because the CSS is extracted at build time.

## External stylesheets

CSS from packages can be imported anywhere in `app/`:

```tsx
import "bootstrap/dist/css/bootstrap.css";
```

In React 19 you can also render `<link rel="stylesheet" href="..." />` directly. Remember that third-party global CSS is global: it can clash with your own styles.

## Sass

```bash
npm install --save-dev sass
```

Then rename to `.scss` or `.module.scss` and import as usual. Sass variables are build-time only; for runtime theming prefer CSS custom properties.

## CSS-in-JS

Runtime libraries that inject styles while rendering (styled-components, Emotion) rely on React context and client-side behavior. In the App Router they need a client-side registry and only work in **Client Components**, which pushes your tree toward the client and conflicts with Server Components. Check your library's App Router guidance before adopting it in a new project. Build-time or zero-runtime solutions, CSS Modules and Tailwind do not have this problem.

## Ordering and merging

In production, Next.js merges and code-splits your CSS. **The final order follows the order of your imports**, not the order of files on disk.

```tsx
// page.tsx
import { BaseButton } from "./base-button";   // its CSS comes first
import styles from "./page.module.css";        // then this one

export default function Page() {
  return <BaseButton className={styles.primary} />;
}
```

If two rules have equal specificity, the later one wins, so import order can change the visual result. To keep it predictable:

- Put CSS imports in as few entry files as possible; import global CSS and Tailwind in the root layout.
- Prefer Tailwind and scoped modules, which avoid specificity fights.
- Do not rely on one module overriding another module's class; extract shared styles into a shared component.
- Turn off linters or formatters that auto-sort imports (such as ESLint's `sort-imports`), since reordering imports can reorder CSS.
- The `cssChunking` option in `next.config` controls how CSS is chunked if you need to tune it.

### Dev vs production

| | `next dev` | `next build` + `next start` |
|---|---|---|
| Updates | Instant via Fast Refresh | Rebuild required |
| Output | Served as needed | Minified, concatenated, code-split `.css` files per route |
| JavaScript off | Fast Refresh needs JS | CSS still loads |
| Ordering | May differ | The truth |

**Always verify visual bugs with a production build.** "Works in dev, breaks in prod" is usually an ordering issue.

## Dynamic and conditional styles

Pick by how often the value changes:

| Need | Approach |
|---|---|
| Toggle between a few states | Conditional class names (`isActive ? styles.active : ""`) |
| A value known at render time (a brand color from a database) | CSS custom property via `style`: `style={{ "--brand": color } as React.CSSProperties}`, used as `var(--brand)` in CSS |
| Arbitrary values on every frame (drag position) | Inline `style` or a CSS variable updated in an event handler |

Avoid generating class names from user data (`` `text-${color}-500` ``) with Tailwind, because Tailwind only sees complete class strings in your source. See the next note.

## Debugging

| Symptom | Likely cause | Fix |
|---|---|---|
| Looks right in dev, wrong in production | Import order changed CSS order | Check the build; simplify imports; raise specificity deliberately |
| Style from another page appears | Global CSS imported in a nested component | Move to root layout or scope with a module |
| `styles.foo` is `undefined` | Class name not in the module, or a dashed name | Check spelling; use camelCase |
| Module build error about selectors | "Selector is not pure" | Include a class in the selector |
| Styles flash unstyled | Heavy runtime CSS-in-JS | Prefer build-time CSS |
| CSS missing on a route | Imported in a file that route never loads | Import where used or in the root layout |
| Sass import error | `sass` not installed | `npm i -D sass` |

## Common mistakes

| Mistake | Fix |
|---|---|
| Importing global CSS in many components | Import once in the root layout |
| Element selectors in a CSS Module | Use classes, or nest under a class |
| Relying on cross-module override order | Use one shared component or higher specificity on purpose |
| Testing CSS only in `next dev` | Verify with `next build && next start` |
| Dashed class names (`styles["my-class"]`) everywhere | Use camelCase |
| Runtime CSS-in-JS in Server Components | Use CSS Modules or Tailwind |
| Auto-sorting imports with a linter | Disable it for CSS imports |

## Quick Summary

- Default stack: Tailwind for components, CSS Modules for scoped custom CSS, one global file imported in the root layout.
- CSS Modules scope class names at build time and work in Server and Client Components.
- Global CSS is not removed on navigation; keep it truly global.
- Production CSS order follows import order; verify with a production build.
- Runtime CSS-in-JS needs Client Components; prefer build-time approaches.

## Next

- [Tailwind CSS](./01-tailwindcss.md)
- [Theming](./03-theming.md)
- [Client Components](../03-components/01-client-components.md)
