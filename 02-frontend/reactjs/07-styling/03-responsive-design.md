# Responsive Design

Users open your app on phones, tablets, laptops, and wall-sized monitors, in portrait and landscape, with touch or a mouse, and with their own font-size settings. Responsive design means the layout adapts to all of that. This file covers the approach (mobile-first and fluid before breakpoints), the tools (media queries, container queries, modern layout functions), responsive images, and how it looks in plain CSS and in Tailwind.

## Prerequisites

[`00-css-essentials.md`](./00-css-essentials.md) (flexbox, grid, units)

---

## 1. Foundation: the viewport meta tag

Without it, mobile browsers pretend to be a ~980px desktop and shrink the page. Vite's template includes this in `index.html`; keep it:

```html
<meta name="viewport" content="width=device-width, initial-scale=1" />
```

Don't add `user-scalable=no` or `maximum-scale=1`; blocking zoom is an accessibility failure.

---

## 2. Principles

1. **Mobile-first.** Write the base styles for the smallest screen, then add complexity at larger widths with `min-width` queries. Small screens are the constrained case; enhancing upward is simpler than undoing desktop layouts.
2. **Fluid before fixed.** Use flexible layouts (`%`, `fr`, `flex`, `minmax`, `clamp`) that adapt on their own. Breakpoints are for when the *design* needs to change, not for every width.
3. **Content decides the breakpoints**, not device names. Add a breakpoint where the layout starts to look wrong, not at "iPad width".
4. **Test real conditions:** touch, slow networks, large text, zoom, landscape.

---

## 3. Media queries (mobile-first)

```css
.nav { display: flex; flex-direction: column; gap: 0.5rem; }   /* phones: stacked */

@media (min-width: 48rem) {                                    /* ≥ 768px */
  .nav { flex-direction: row; gap: 1.5rem; }
}
```

- Prefer **`min-width`** queries (mobile-first) over `max-width` (desktop-first).
- Use **`rem`/`em`** in queries so layouts respond to user font-size settings, not only to pixel widths.
- Typical breakpoints (Tailwind's defaults): `sm` 40rem (640px), `md` 48rem (768px), `lg` 64rem (1024px), `xl` 80rem (1280px), `2xl` 96rem (1536px). Treat them as starting points.

### Feature queries about the user, not the screen

```css
@media (hover: hover) and (pointer: fine) { .card:hover { transform: translateY(-2px); } }
@media (prefers-reduced-motion: reduce)   { * { animation: none !important; } }
@media (prefers-color-scheme: dark)       { :root { color-scheme: dark; } }
@media print                              { .no-print { display: none; } }
```

`(hover: hover)` keeps hover-only effects off touch devices, where "hover" sticks after a tap.

---

## 4. Fluid layouts that need no breakpoints

### Wrapping rows

```css
.tags { display: flex; flex-wrap: wrap; gap: 0.5rem; }
```

### Auto-fitting grids

```css
.grid {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(min(16rem, 100%), 1fr));
  gap: 1rem;
}
```

The `min(16rem, 100%)` prevents overflow when the container is narrower than `16rem`.

### Readable widths and fluid containers

```css
.container { width: min(100% - 2rem, 70rem); margin-inline: auto; }
.prose { max-width: 65ch; }
```

### Fluid type and spacing with `clamp()`

```css
h1 { font-size: clamp(1.75rem, 1.2rem + 2.5vw, 3rem); }
section { padding-block: clamp(2rem, 6vw, 5rem); }
```

`clamp(min, preferred, max)` scales smoothly between a floor and a ceiling. Use `rem` plus a `vw` component (not `vw` alone) so user zoom and font settings still work.

---

## 5. Container queries: components that respond to their own space

Media queries look at the **viewport**. A reusable component can't know whether it's in a wide main column or a narrow sidebar. **Container queries** let it adapt to its **container's** width:

```css
.card-wrapper { container-type: inline-size; }

.card { display: grid; gap: 0.75rem; }

@container (min-width: 30rem) {
  .card { grid-template-columns: 8rem 1fr; }   /* image beside text when there's room */
}
```

The same `Card` can be dropped anywhere and lay itself out correctly, which is a better fit for component-based development. Tailwind exposes this with `@container` on the parent and `@md:` variants on children (v4; check the docs for syntax in your version). Support is good in modern browsers; check caniuse.com for your targets.

**Rule of thumb:** use **media queries** for page-level layout (header, sidebar, number of columns) and **container queries** for reusable components.

---

## 6. Responsive images and media

```tsx
<img
  src="/hero-800.jpg"
  srcSet="/hero-400.jpg 400w, /hero-800.jpg 800w, /hero-1600.jpg 1600w"
  sizes="(min-width: 64rem) 50vw, 100vw"
  width={800}
  height={450}
  alt="A team reviewing a dashboard"
  loading="lazy"
  decoding="async"
/>
```

- **`srcSet` + `sizes`** let the browser pick an appropriately sized file; don't ship a 3000px image to a phone.
- **`width` and `height`** (or CSS `aspect-ratio`) reserve space so the layout doesn't jump when the image loads — protecting your Cumulative Layout Shift score ([`../14-performance/06-network-performance.md`](../14-performance/06-network-performance.md)).
- **`loading="lazy"`** for below-the-fold images; not for the main hero image.
- Use modern formats (WebP/AVIF) and `<picture>` for art direction.
- Make images fluid with `max-width: 100%; height: auto`.
- Every meaningful image needs useful `alt` text; decorative ones use `alt=""`.

---

## 7. Touch, input, and text

- **Touch targets** at least about 44×44 CSS px (WCAG 2.2 sets a 24×24 minimum; larger is better on mobile). Add padding rather than enlarging visuals.
- **Don't hide essential features** on small screens; reorganize them (menu, bottom sheet).
- **Input types** trigger the right mobile keyboard: `type="email"`, `type="tel"`, `inputMode="numeric"`.
- **Text size of at least 16px in inputs**: iOS Safari zooms the page when a smaller input is focused.
- **Full-height layouts** use `100dvh`, not `100vh` ([`00-css-essentials.md`](./00-css-essentials.md)).
- **Safe areas** (notches, home indicators): `padding-bottom: env(safe-area-inset-bottom)`, with `viewport-fit=cover`.
- Avoid horizontal scrolling of the page; wide tables and code blocks scroll inside their own `overflow-x: auto` container.

---

## 8. In Tailwind

Tailwind is mobile-first: **unprefixed utilities apply at all sizes; a prefix like `md:` applies at that width and up.**

```tsx
<nav className="flex flex-col gap-2 md:flex-row md:gap-6">…</nav>

<ul className="grid grid-cols-1 gap-4 sm:grid-cols-2 lg:grid-cols-4">…</ul>

<h1 className="text-2xl font-semibold md:text-4xl">Title</h1>

<aside className="hidden lg:block">…</aside>          {/* show only on large screens */}
```

Common mistake: using `sm:` to mean "on small screens". It means "**small and up**". Write the **phone** styles unprefixed, then override upward. Use `max-md:` only when you truly need a "below this width" rule. For fluid behavior prefer `grid-cols-[repeat(auto-fit,minmax(16rem,1fr))]` to stacks of breakpoints. See [`02-tailwindcss.md`](./02-tailwindcss.md).

---

## 9. Responsive behavior in React (when CSS isn't enough)

Use CSS for visual responsiveness. Reach for JavaScript only when the **component tree itself must differ** — rendering a bottom-sheet instead of a dropdown, or loading a heavy component only on desktop:

```tsx
const isDesktop = useMediaQuery("(min-width: 64rem)");
return isDesktop ? <Sidebar /> : <MobileDrawer />;
```

Cautions:

- Rendering **both** and hiding one with CSS keeps both mounted and running effects; conditional rendering unmounts the hidden one ([`../01-fundamentals/04-conditional-rendering.md`](../01-fundamentals/04-conditional-rendering.md)).
- With **server rendering**, the server doesn't know the screen size. A JS-driven layout can mismatch and flash on hydration; prefer CSS-based approaches there ([`../15-concurrent-and-modern-react/07-server-components-and-ssr.md`](../15-concurrent-and-modern-react/07-server-components-and-ssr.md)).
- `useMediaQuery` implementation: [`../03-hooks/10-hook-recipes.md`](../03-hooks/10-hook-recipes.md).

---

## 10. Testing responsive layouts

- Browser DevTools **device toolbar** for widths, touch emulation, and throttling.
- Resize continuously, not only at preset sizes; look for the points where the layout breaks.
- Test **200% zoom** and large default font sizes (reflow must not hide content).
- Test portrait and landscape; test real devices when you can.
- Visual regression tests catch breakpoint regressions ([`../18-testing-and-debugging/05-integration-testing.md`](../18-testing-and-debugging/05-integration-testing.md)).

---

## Common mistakes

- **Desktop-first CSS** — a pile of `max-width` overrides that undo earlier rules.
- **Breakpoints named after devices** (iPhone, iPad) instead of content needs.
- **Pixel-based media queries and font sizes** — ignores user font-size preferences; use `rem`/`em`.
- **Forgetting the viewport meta tag** — mobile pages render as shrunken desktops.
- **Blocking zoom** with `user-scalable=no`.
- **Fixed widths and heights** that overflow on small screens.
- **Images without dimensions** — layout shift on load.
- **Hover-only interactions** — unreachable on touch devices.
- **`100vh` for full-height mobile layouts** — jumps with the browser UI.
- **Tailwind `sm:` read as "small screens only"** — it's "small and up".
- **JS-based layout switching during SSR** — hydration mismatches.
- **Tiny touch targets and sub-16px inputs** — frustrating and triggers iOS zoom.

## Quick summary

- Keep the viewport meta tag; never block zoom
- Mobile-first: base styles for small screens, `min-width` queries to add complexity
- Prefer fluid layouts (`flex-wrap`, `auto-fit/minmax`, `clamp`, `min()`) over many breakpoints
- Media queries for page layout, container queries for reusable components
- Responsive images: `srcSet`/`sizes`, explicit dimensions, lazy loading
- Respect touch ergonomics, `dvh`, safe areas, and user preferences (`prefers-reduced-motion`, `hover: hover`)
- Tailwind prefixes mean "this width and up"; use JS media queries only when the component tree must change

## Next

**[`04-theming-and-dark-mode.md`](./04-theming-and-dark-mode.md)** covers design tokens and building a light/dark/system theme that doesn't flash on load.
