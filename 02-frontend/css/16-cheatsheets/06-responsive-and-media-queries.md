# Cheatsheet 06: Responsive and Media Queries

| Spec Level | Media Queries Level 4 & 5 | Baseline Status | Widely Available |
|---|---|---|---|
| Browsers Tested | Chrome 120+, Firefox 125+, Safari 17.4+ | Primary Usage | Device adaptation, preference overrides, input mode detection |

---

## 1. Modern Range Syntax vs Legacy Syntax

Modern CSS supports mathematical comparison operators (`>`, `<`, `>=`, `<=`), eliminating confusing `min-width` / `max-width` 1px overlap bugs.

| Intent | Legacy Syntax | Modern Range Syntax (Baseline) |
|---|---|---|
| Min-width (Desktop and up) | `@media (min-width: 768px)` | `@media (width >= 768px)` |
| Max-width (Mobile only) | `@media (max-width: 767.98px)`| `@media (width < 768px)` |
| Inclusive Range (Tablet only) | `@media (min-width: 768px) and (max-width: 1024px)` | `@media (768px <= width <= 1024px)` |

```css
/* Clean Modern Media Query Example */
@media (width >= 64rem) {
  .site-layout {
    grid-template-columns: 300px 1fr;
  }
}
```

---

## 2. Standard Industry Responsive Breakpoints

| Breakpoint Name | Token Range | Target Form Factor | Layout Pattern |
|---|---|---|---|
| `sm` | `width >= 640px` | Large smartphones / phablets | 2-column small cards, stack navbars |
| `md` | `width >= 768px` | Tablets portrait | 2-column content, hamburger menu threshold |
| `lg` | `width >= 1024px` | Tablets landscape, laptops | 3-4 column grid, horizontal desktop nav |
| `xl` | `width >= 1280px` | Standard desktop monitors | Max-width content wrapper (1200px) |
| `2xl` | `width >= 1536px` | Ultra-wide displays | Expanded sidebars, high-res canvas |

---

## 3. User Preference Media Queries Cheat Sheet

```css
/* 1. Dark Mode / Color Scheme */
@media (prefers-color-scheme: dark) {
  :root {
    --bg-primary: #0f172a;
    --text-primary: #f8fafc;
  }
}

/* 2. Vestibular Motion Sensitivity */
@media (prefers-reduced-motion: reduce) {
  *, *::before, *::after {
    animation-duration: 0.01ms !important;
    animation-iteration-count: 1 !important;
    transition-duration: 0.01ms !important;
    scroll-behavior: auto !important;
  }
}

/* 3. High Contrast / Low Vision Accessibility */
@media (prefers-contrast: more) {
  .btn {
    border: 2px solid currentColor;
    font-weight: 700;
  }
}

/* 4. Data Saver / Metered Connection */
@media (prefers-reduced-data: reduce) {
  .hero-banner {
    background-image: none; /* Suppress heavy high-res hero images */
  }
}

/* 5. Windows High Contrast / Forced Colors Mode */
@media (forced-colors: active) {
  .badge {
    border: 1px solid ButtonText; /* Guarantees boundary visibility */
  }
}
```

---

## 4. Interaction & Input Capabilities Queries

Detect input modalities (touch vs mouse) instead of guessing based on screen width:

```css
/* Coarse Pointer = Touchscreens (Mobile, iPad, Touch kiosks) */
@media (pointer: coarse) {
  button, .clickable-target {
    min-width: 44px;
    min-height: 44px; /* WCAG 2.5.5 AAA touch target compliance */
    padding: 12px;
  }
}

/* Hover: None = Touchscreen without pointer cursor */
@media (hover: none) {
  /* Avoid hidden dropdown menus that rely on :hover triggers */
  .dropdown-menu {
    display: block;
    position: static;
  }
}

/* Hover: Hover AND Pointer: Fine = True Mouse / Trackpad Desktop */
@media (hover: hover) and (pointer: fine) {
  .card:hover {
    transform: translateY(-4px);
    box-shadow: 0 12px 24px rgba(0, 0, 0, 0.15);
  }
}
```

---

## 5. Container Queries Cheat Sheet

Style an element based on its parent container's dimensions rather than the global viewport.

```css
/* 1. Establish Container Context */
.card-wrapper {
  container-type: inline-size;
  container-name: card-container;
}

/* 2. Query Container Dimensions */
@container card-container (width >= 400px) {
  .card {
    display: grid;
    grid-template-columns: 120px 1fr;
    gap: 1rem;
  }
}

@container card-container (width >= 700px) {
  .card {
    grid-template-columns: 240px 1fr auto;
  }
}
```

---

[← Previous: Cheatsheet 05 - CSS Grid](file:///e:/Learning/engineering-notes/01-languages/css/16-cheatsheets/05-grid.md) | [Next: Cheatsheet 07 - Typography →](file:///e:/Learning/engineering-notes/01-languages/css/16-cheatsheets/07-typography.md)
