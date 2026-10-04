# CSS Essentials

Whatever styling approach you use — CSS Modules, Tailwind, CSS-in-JS — it all compiles down to CSS, and most "my styles are broken" bugs are CSS bugs, not React bugs. This file covers the parts of CSS that matter most when building components: how rules are resolved, the box model, modern layout, custom properties, and how CSS meets React.

## Prerequisites

Basic HTML and CSS: selectors, properties, and values. [`../01-fundamentals/01-jsx.md`](../01-fundamentals/01-jsx.md) for `className`.

---

## 1. How CSS decides: the cascade

When several rules target the same property on the same element, the browser picks a winner in this order:

1. **Importance** — `!important` beats normal declarations (avoid it).
2. **Origin and layers** — author styles beat browser defaults; `@layer` ordering applies here.
3. **Specificity** — the more specific selector wins.
4. **Source order** — if everything else ties, the **last** rule wins.

### Specificity

Count, from most to least significant: **inline style** → **IDs** → **classes, attributes, pseudo-classes** → **elements, pseudo-elements**.

| Selector | Specificity (IDs, classes, elements) |
|----------|-------------------------------------|
| `p` | 0, 0, 1 |
| `.card` | 0, 1, 0 |
| `.card p` | 0, 1, 1 |
| `.card.active` | 0, 2, 0 |
| `#header` | 1, 0, 0 |
| `style="..."` attribute | beats all of the above |

Practical rules:

- **Style with single classes** (`.button`) so specificity stays low and predictable. CSS Modules and Tailwind both work this way.
- **Don't use IDs or deep selectors** for styling; they make overrides painful.
- `:where(...)` has **zero** specificity; `:is(...)` takes the most specific argument. Useful for resets and library defaults.
- Reach for **`@layer`** (cascade layers) to order whole groups of styles (reset < base < components < utilities) regardless of specificity.

### Inheritance

Some properties (`color`, `font-*`, `line-height`, `visibility`) are **inherited** from the parent; others (`margin`, `padding`, `border`, `background`, `width`) are not. Use `inherit`, `initial`, or `unset` to control it explicitly.

---

## 2. The box model

Every element is a box: content, padding, border, margin.

```css
*, *::before, *::after {
  box-sizing: border-box;   /* width includes padding and border */
}
```

With `border-box`, `width: 200px` means the box is 200px wide in total. Without it, padding and border are *added* to the width — a constant source of layout surprises. Nearly every reset (and Tailwind's base styles) sets this.

- **Margin collapsing:** vertical margins between block siblings can collapse into one. Flex and grid children don't collapse. Prefer `gap` over margins for spacing between items.
- **`display`** controls the box type: `block`, `inline`, `inline-block`, `flex`, `grid`, `none`.

---

## 3. Units

| Unit | Relative to | Use for |
|------|-------------|---------|
| `rem` | Root font size | Font sizes, spacing, most layout — respects user font settings |
| `em` | Element's font size | Spacing that scales with the component's text |
| `px` | Device pixel (CSS pixel) | Borders, shadows, hairlines |
| `%` | Parent's size | Fluid widths |
| `vw` / `vh` | Viewport width/height | Full-bleed sections |
| `dvh` / `svh` / `lvh` | Dynamic/small/large viewport height | Full-height layouts on mobile (browser UI shows and hides) |
| `ch` | Width of the "0" glyph | Readable line lengths (`max-width: 65ch`) |
| `fr` | Share of free space in grid | Grid tracks |

Prefer **`rem` for font sizes** so users' browser font-size settings work. Avoid `100vh` for full-height mobile layouts; `100dvh` accounts for collapsing browser toolbars.

---

## 4. Flexbox: one-dimensional layout

Flexbox lays out items in a **row or column**.

```css
.toolbar {
  display: flex;
  align-items: center;       /* cross axis (vertical for rows) */
  justify-content: space-between; /* main axis (horizontal for rows) */
  gap: 0.5rem;
}

.toolbar .spacer { flex: 1; }          /* take the remaining space */
```

| Property | Applies to | Purpose |
|----------|------------|---------|
| `flex-direction` | container | `row` (default) or `column` |
| `justify-content` | container | Distribute along the **main** axis |
| `align-items` | container | Align along the **cross** axis |
| `flex-wrap` | container | Allow items to wrap |
| `gap` | container | Space between items |
| `flex: 1` / `flex-grow` | item | Grow to fill space |
| `flex-shrink: 0` | item | Don't shrink |
| `min-width: 0` | item | **Allow shrinking below content size** (fixes overflowing text in flex children) |

Common recipes:

```css
.center { display: flex; justify-content: center; align-items: center; }
.stack { display: flex; flex-direction: column; gap: 1rem; }
.row-wrap { display: flex; flex-wrap: wrap; gap: 0.75rem; }
```

---

## 5. Grid: two-dimensional layout

Grid defines rows and columns.

```css
.layout {
  display: grid;
  grid-template-columns: 240px 1fr;       /* sidebar + content */
  grid-template-rows: auto 1fr auto;
  min-height: 100dvh;
  gap: 1rem;
}
```

### Responsive card grid without media queries

```css
.cards {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(16rem, 1fr));
  gap: 1rem;
}
```

Each card is at least `16rem` wide; the browser fits as many columns as possible and stretches them to fill the row. This single rule replaces several breakpoints.

### Named areas

```css
.page {
  display: grid;
  grid-template-areas:
    "header header"
    "nav    main"
    "footer footer";
  grid-template-columns: 200px 1fr;
}
.page > header { grid-area: header; }
```

**Flexbox vs grid:** use **flex** when content drives the layout in one dimension (toolbars, button rows, centering); use **grid** when you define the structure (page layouts, card grids, aligned forms). They combine well.

---

## 6. Positioning, overflow, and stacking

- **`position: relative`** makes an element the anchor for absolutely positioned descendants.
- **`position: absolute`** removes the element from flow and places it relative to the nearest positioned ancestor.
- **`position: sticky`** sticks within its scroll container (`top: 0` for headers). It fails silently if an ancestor has `overflow: hidden`, `auto`, or `scroll`.
- **`position: fixed`** is relative to the viewport.
- **`overflow`** controls clipping and scrolling; `overflow: hidden` also creates a new formatting context, which can clip dropdowns and focus rings.
- **`z-index`** only works on positioned elements (and flex/grid items), and only compares within the same **stacking context**. Properties like `transform`, `opacity < 1`, `filter`, and `position` with `z-index` each create new stacking contexts — the usual reason a modal appears *under* something with a smaller `z-index`. Keep a small, named scale of z-index values (`--z-dropdown`, `--z-modal`), and render overlays through a **portal** to escape ancestor contexts ([`../16-advanced-react/00-portals.md`](../16-advanced-react/00-portals.md)).

---

## 7. Custom properties (CSS variables)

```css
:root {
  --color-primary: #2563eb;
  --space-2: 0.5rem;
  --radius: 0.5rem;
}

.button {
  background: var(--color-primary);
  padding: var(--space-2) calc(var(--space-2) * 2);
  border-radius: var(--radius);
}
```

Unlike Sass variables, custom properties are **live at runtime**: they cascade, can be overridden per element or subtree, and can be changed from JavaScript. That makes them the foundation of **theming** ([`04-theming-and-dark-mode.md`](./04-theming-and-dark-mode.md)) and a clean bridge between React state and CSS:

```tsx
<div className={styles.bar} style={{ "--progress": `${percent}%` } as React.CSSProperties} />
```

```css
.bar::after { width: var(--progress); }
```

Use a fallback when a variable might be missing: `var(--gap, 1rem)`.

---

## 8. Modern CSS worth knowing

| Feature | Example | Why it helps |
|---------|---------|--------------|
| `clamp()` | `font-size: clamp(1rem, 2.5vw, 1.5rem)` | Fluid sizes with min and max |
| `aspect-ratio` | `aspect-ratio: 16 / 9` | Reserve space for media |
| Logical properties | `margin-inline`, `padding-block` | Layouts that work in right-to-left languages |
| `:focus-visible` | `button:focus-visible { outline: … }` | Focus rings for keyboard users only |
| `:has()` | `.field:has(:invalid) { … }` | Style a parent from its children's state |
| `:is()` / `:where()` | `:where(h1, h2, h3) { margin: 0 }` | Shorter selectors, controlled specificity |
| Native nesting | `.card { & .title { … } }` | Sass-like nesting without a preprocessor |
| `@container` | `@container (min-width: 30rem) { … }` | Component-level responsiveness ([`03-responsive-design.md`](./03-responsive-design.md)) |
| `color-scheme` | `:root { color-scheme: light dark }` | Native form controls and scrollbars match the theme |
| `gap` | `gap: 1rem` | Spacing in flex and grid without margin hacks |

Check browser support for newer features on caniuse.com before relying on them without fallbacks.

---

## 9. Resets and base styles

Browsers ship default styles. A small reset makes behavior predictable:

```css
*, *::before, *::after { box-sizing: border-box; }
body { margin: 0; line-height: 1.5; -webkit-font-smoothing: antialiased; }
img, svg, video { display: block; max-width: 100%; height: auto; }
button, input, select, textarea { font: inherit; }
```

Tailwind includes its own base layer (Preflight). Don't stack multiple resets.

---

## 10. CSS in React: the mechanics

### Global stylesheets

Import a CSS file once (usually in `main.tsx`); Vite bundles it:

```tsx
import "./index.css";
```

Anything imported is **global** — class names collide across the whole app. That's what CSS Modules and utility classes solve ([`01-css-modules.md`](./01-css-modules.md), [`02-tailwindcss.md`](./02-tailwindcss.md)).

### `className`, not `class`

```tsx
<button className="btn btn-primary">Save</button>
```

### Conditional classes

```tsx
// Template string (fine for one or two conditions)
<button className={`btn ${isActive ? "btn-active" : ""}`} />

// A helper like clsx scales better (see 05-styling-approaches-compared.md)
<button className={clsx("btn", isActive && "btn-active", disabled && "btn-disabled")} />
```

Avoid producing `"undefined"` or `"false"` as a literal class name by concatenating raw values.

### The `style` prop

```tsx
<div style={{ width: `${percent}%`, backgroundColor: color }} />
```

Good for **truly dynamic values** (computed sizes, positions, colors from data). Limitations: **no** pseudo-classes (`:hover`), pseudo-elements, media queries, or keyframes; every render allocates a new object; styles can't be overridden by normal CSS without `!important`. Prefer classes for everything static, and combine with custom properties for dynamic values (above).

### Dev tools

Use the browser's **Elements → Styles** panel to see which rule wins, what's overridden (struck through), and the computed box model. Most CSS debugging is: *inspect, find the winning rule, understand why it wins*.

---

## 11. Focus, contrast, and motion basics

Styling choices affect accessibility directly:

- **Never remove focus outlines** (`outline: none`) without a replacement. Style `:focus-visible` instead ([`../08-accessibility/02-keyboard-and-focus-management.md`](../08-accessibility/02-keyboard-and-focus-management.md)).
- **Text contrast** of at least 4.5:1 for normal text (3:1 for large text), per WCAG ([`../08-accessibility/04-accessibility-checklist.md`](../08-accessibility/04-accessibility-checklist.md)).
- Respect **reduced motion**:

```css
@media (prefers-reduced-motion: reduce) {
  * { animation-duration: 0.01ms !important; transition-duration: 0.01ms !important; }
}
```

- Don't convey meaning with color alone.

---

## Common mistakes

- **Fighting specificity with `!important` and IDs** — keep selectors to single classes and fix the ordering instead.
- **Forgetting `box-sizing: border-box`** — widths don't match what you set.
- **Using margins for spacing between flex/grid items** — use `gap`.
- **Flex children overflowing** because of long text — add `min-width: 0`.
- **`z-index` wars** — usually a stacking-context problem; use a z-index scale and portals.
- **`position: sticky` not sticking** — an ancestor has `overflow` set.
- **Using `100vh` on mobile** — the layout jumps; prefer `100dvh`.
- **Overusing the `style` prop** — no hover/media support; use classes plus custom properties.
- **Removing focus outlines** — breaks keyboard navigation.
- **Hard-coding `px` font sizes** — ignores user preferences; use `rem`.

## Quick summary

- The cascade: importance → layers → specificity → source order; keep specificity low with single classes
- `box-sizing: border-box`, `gap`, and `rem` solve most basic layout problems
- Flexbox for one-dimensional layouts, grid for two-dimensional ones; `repeat(auto-fit, minmax())` makes responsive grids without media queries
- Custom properties are live, cascade, and power theming and React-to-CSS dynamic values
- `z-index` problems are stacking-context problems; use portals for overlays
- In React: `className` plus scoped styles; reserve `style` for dynamic values
- Keep focus visible, contrast sufficient, and motion optional

## Next

**[`01-css-modules.md`](./01-css-modules.md)** scopes your CSS to components so class names never collide. If your project uses Tailwind, go to **[`02-tailwindcss.md`](./02-tailwindcss.md)**.
