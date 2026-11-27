# Design System & Specifications: Responsive SaaS Landing Page

## 1. Color Palette (OKLCH Tokens)

The landing page leverages a modern, accessible color system defined in OKLCH:

| Token Name | Value | Purpose | WCAG Contrast against Canvas |
|---|---|---|---|
| `--color-canvas-bg` | `oklch(99% 0.005 240)` | Main page background | Reference (1.0:1) |
| `--color-surface-card` | `oklch(100% 0 0)` | Card containers and elevated panels | Reference (1.0:1) |
| `--color-text-primary` | `oklch(18% 0.03 260)` | Headings, hero title, primary body | 14.8:1 (AAA) |
| `--color-text-muted` | `oklch(45% 0.04 250)` | Secondary descriptions, captions | 5.8:1 (AA) |
| `--color-brand-primary`| `oklch(55% 0.22 260)` | Primary buttons, active tabs, highlights| 4.6:1 (AA) |
| `--color-brand-hover` | `oklch(48% 0.24 260)` | Button hover and active press | 5.9:1 (AA) |
| `--color-border-subtle`| `oklch(90% 0.015 250)`| Card borders, header divider line | 1.2:1 (Structural) |

---

## 2. Fluid Typography Scale

All typography uses fluid scaling calculated between a minimum mobile viewport of `375px` and a desktop boundary of `1440px`:

```css
:root {
  /* H1 Hero Headline: 36px to 64px */
  --font-size-h1: clamp(2.25rem, 1.25rem + 3.8vw, 4rem);

  /* H2 Section Titles: 28px to 44px */
  --font-size-h2: clamp(1.75rem, 1.15rem + 2.2vw, 2.75rem);

  /* H3 Card Headers: 20px to 26px */
  --font-size-h3: clamp(1.25rem, 1rem + 1vw, 1.625rem);

  /* Body Regular: 16px to 18px */
  --font-size-body: clamp(1rem, 0.95rem + 0.25vw, 1.125rem);

  /* Micro Copy & Badges: 12px to 14px */
  --font-size-sm: clamp(0.75rem, 0.7rem + 0.2vw, 0.875rem);
}
```

---

## 3. Responsive Breakpoint & Container Matrix

Rather than device-specific hacks, layout breakpoints align to content flow thresholds:

| Breakpoint | Target Inline Width | Layout Adaptations |
|---|---|---|
| **Compact Mobile** | `< 640px` | Single-column stack, mobile hamburger menu, vertical CTA buttons |
| **Tablet Portrait**| `640px – 899px` | Two-column hero with stacked mockup, 2-column feature grid |
| **Desktop Shell**  | `900px – 1280px`| Asymmetric 1.2fr : 0.8fr split hero, 3-column feature grid |
| **Wide Desktop**   | `> 1280px` | Max-width content boundary (1200px), centered with `margin-inline: auto` |
