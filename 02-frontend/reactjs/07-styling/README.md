# 07 — Styling

How to make React applications visually refined, performant, and maintainable. This module covers core CSS principles, CSS Modules, Tailwind CSS v4, responsive layouts, theme systems, and the shift from legacy runtime CSS-in-JS to modern zero-runtime compile-time engines.

---

## What you'll be able to do after this module

- **Predict & Control CSS Cascade & Specificity**: Understand specificity hierarchy, layer overrides, and CSS custom properties.
- **Master Modern Flexbox & Grid Layouts**: Build responsive multi-column layouts, sticky toolbars, and container query cards without layout thrashing.
- **Scope Styles with CSS Modules**: Build isolated, locally-scoped component stylesheets with zero class clashing.
- **Master Tailwind CSS v4**: Leverage the Rust-based Oxide engine, CSS-first `@theme` configuration, OKLCH color palettes, and `@tailwindcss/vite`.
- **Implement Robust Theme Systems**: Build dark, light, and system themes with zero Flash of Unstyled Content (FOUC).
- **Evaluate Zero-Runtime Engines**: Understand why runtime CSS-in-JS (Emotion, styled-components) is fading in the era of React Server Components, and adopt zero-runtime alternatives (Tailwind v4, Vanilla Extract, StyleX, Panda CSS).
- **Merge Class Names Safely**: Master `clsx` and `tailwind-merge` (`cn`) to resolve conflicting utility overrides cleanly.

---

## Module Syllabus

| # | Note | Description | Engine / Model |
|---|------|-------------|:---:|
| 00 | [00-css-essentials.md](./00-css-essentials.md) | Cascade, specificity, box model, Flexbox, Grid, CSS custom properties | Native CSS |
| 01 | [01-css-modules.md](./01-css-modules.md) | Locally-scoped styles, composition, hash classes, Vite integration | CSS Modules |
| 02 | [02-tailwindcss.md](./02-tailwindcss.md) | Utility-first styling principles, arbitrary values, responsive prefixes | Utility-first |
| 03 | [03-responsive-design.md](./03-responsive-design.md) | Mobile-first layouts, breakpoints, container queries, viewport units | Responsive |
| 04 | [04-theming-and-dark-mode.md](./04-theming-and-dark-mode.md) | Design tokens, CSS variables, system preferences, preventing FOUC | Theming |
| 05 | [05-styling-approaches-compared.md](./05-styling-approaches-compared.md) | Decision matrix, class merging with `clsx` and `twMerge` (`cn`) | Architecture |
| 06 | [06-tailwind-v4-changes-and-theming.md](./06-tailwind-v4-changes-and-theming.md) | Tailwind v4 Oxide engine, CSS-first `@theme`, elimination of `tailwind.config.js` | Tailwind v4 |
| 07 | [07-css-in-js-status-zero-runtime.md](./07-css-in-js-status-zero-runtime.md) | Zero-runtime styling landscape: StyleX, Vanilla Extract, Panda CSS, RSC compatibility | Zero-Runtime |

---

## Styling Architecture Comparison Matrix

| Approach | Build Step | JS Runtime | RSC Support | Type Safety | Ideal Use Case |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **Tailwind CSS v4** | Vite / Oxide | 0 KB | 100% | IntelliSense | Modern SPAs & full-stack apps |
| **CSS Modules** | Standard Bundler | 0 KB | 100% | Typed CSS Modules | Traditional isolated styles |
| **Vanilla Extract** | Vite / Rollup | 0 KB | 100% | 100% TypeScript | Strict design systems |
| **Meta StyleX** | Babel / SWC | 0 KB | 100% | 100% TypeScript | Large-scale enterprise atomic CSS |
| **Emotion / Styled Comp**| Babel | ~15 KB | 0% (Fails) | Basic | Legacy Webpack apps (Deprecated for RSC) |

---

## Boundary with Other Modules

- **Component Variants (`cva`) & Design System Kits**: Read [`09-ui-components/`](../09-ui-components/README.md).
- **Animations & Transitions**: Read [`21-specializations/animation/`](../21-specializations/animation/README.md).
- **Accessibility of Styling (Contrast, Focus Rings)**: Read [`08-accessibility/`](../08-accessibility/README.md).
- **Bundle Optimization**: Read [`14-performance/`](../14-performance/README.md).

---

## Practical Exercises

1. **Cascade Verification**: Predict winning CSS declarations across classes, inline styles, and `!important`, verifying in browser DevTools.
2. **Container Query Card Grid**: Build a responsive card layout adjusting column count based on parent `@container` width rather than viewport width.
3. **Tailwind v4 OKLCH Palette**: Declare a custom brand theme in `@theme` using the OKLCH color space with 500, 600, and 700 shades.
4. **FOUC-Free Dark Mode**: Implement a dark mode script injected into `<head>` that synchronizes with `localStorage` and `prefers-color-scheme` without flashing white.
5. **Class Merger Utility**: Implement the canonical `cn(...inputs)` helper combining `clsx` and `tailwind-merge` and test resolving conflicting padding utilities.

---

## Next Steps

Advance to **[08-accessibility/README.md](../08-accessibility/README.md)** to master screen readers, keyboard navigation, WAI-ARIA 1.2, and automated Playwright accessibility testing.
