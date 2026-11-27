# Design System & Specifications: Mini CSS Design System

## 1. The 5-Layer Cascade Hierarchy

```css
@layer reset, tokens, base, components, utilities;
```

### Layer Priority Rules:
- Rules inside `@layer reset` have the lowest priority.
- Rules inside `@layer utilities` have the highest priority.
- A single unlayered rule (`.custom-override`) automatically wins over any layered component rule regardless of specificity.
- Inside any layer, standard specificity `[A, B, C]` resolves ties.

---

## 2. Token Architecture & Component Scope

| Layer | Responsibilities | Target Selectors |
|---|---|---|
| **reset** | Box-sizing, margin resets, image scaling, font smoothing | `*, *::before, *::after, html, body, img` |
| **tokens** | Primitive and semantic custom properties | `:root` |
| **base** | Default typography, link colors, form element resets | `h1–h6, p, a, input, button, table` |
| **components**| Reusable design system UI classes | `.btn`, `.badge`, `.card`, `.dialog`, `.accordion` |
| **utilities** | Single-purpose utility overrides | `.u-flex`, `.u-grid`, `.u-sr-only`, `.u-text-center` |
