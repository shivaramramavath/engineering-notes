# CSS Engineering Learning Track

Welcome to the **CSS Engineering Learning Track**, a production-grade, authoritative curriculum covering modern Cascading Style Sheets from first principles to advanced layout algorithms, design systems, performance optimization, and Web Platform Baseline standards (2024–2026).

---

## Curriculum Architecture & Scope

This track is structured into 17 modular domains:

| Module | Title | Focus Area |
|:---|:---|:---|
| **[00-setup](00-setup/README.md)** | Setup & Browser Engine Architecture | Critical Rendering Path, Style/Layout/Paint/Composite, DevTools, Lightning CSS |
| **[01-fundamentals](01-fundamentals/README.md)** | Fundamentals, Specificity & Cascade | Cascade resolution, `@layer`, `:has()`, nesting, units (`dvh`, `cq*`), `oklch()`, `@property` |
| **[02-box-model-and-layout-basics](02-box-model-and-layout-basics/README.md)** | Box Model & Layout Basics | Box-sizing, margin collapsing, BFC/IFC, positioning, stacking contexts, logical properties |
| **[03-flexbox](03-flexbox/README.md)** | Flexbox Layout in Depth | Flex mental model, flex sizing algorithm, alignment, auto margins, flex patterns |
| **[04-grid](04-grid/README.md)** | CSS Grid & Subgrid | Two-dimensional tracks, `fr` units, template areas, auto-fill/fit, Subgrid architecture |
| **[05-responsive-design](05-responsive-design/README.md)** | Responsive & Adaptive Design | Container Queries (`@container`), fluid clamp typography, intrinsic layout, testing |
| **[06-typography-and-text](06-typography-and-text/README.md)** | Typography & Internationalization | Variable fonts, font-display, `text-wrap: balance`, writing modes, vertical rhythm |
| **[07-visual-styling](07-visual-styling/README.md)** | Visual Styling & Effects | Gradients, box-shadows, backdrop-filters, blend modes, clip-paths, form control styling |
| **[08-motion](08-motion/README.md)** | Transitions, Keyframes & Motion | GPU compositor animation, `linear()` easing, scroll-driven animations, View Transitions |
| **[09-modern-css](09-modern-css/README.md)** | Modern CSS & Emerging Features | `@scope`, anchor positioning, Popover/dialog styling, `light-dark()`, `field-sizing`, `if()` |
| **[10-architecture-and-methodologies](10-architecture-and-methodologies/README.md)** | Architecture & Methodologies | BEM, CUBE CSS, Tailwind vs native, Design Tokens, CSS Modules, managing specificity |
| **[11-accessibility-and-ux-in-css](11-accessibility-and-ux-in-css/README.md)** | Accessibility (a11y) & UX | `:focus-visible`, WCAG contrast math, `prefers-reduced-motion`, `forced-colors` (WHCM) |
| **[12-performance-and-debugging](12-performance-and-debugging/README.md)** | Performance & Debugging | Layout thrashing, critical CSS, `content-visibility`, DevTools profiling, Core Web Vitals |
| **[13-layout-and-component-recipes](13-layout-and-component-recipes/README.md)** | Layout & Component Recipes | Centering, holy grail, card grids, sticky headers, dashboards, responsive tables |
| **[14-projects](14-projects/README.md)** | Production Reference Projects | 10 complete 4-file projects with verified computed styles and responsive viewports |
| **[15-interview](15-interview/README.md)** | Technical Interview Questions | 10 rigorous interview categories with deep `<details>` answers and layout challenges |
| **[16-cheatsheets](16-cheatsheets/README.md)** | Operational Cheatsheets | 12 quick-reference cheatsheets covering selectors, units, grid, flexbox, and modern APIs |

---

## Standards & Web Platform Baseline

All features in this track are documented with explicit **Web Platform Baseline** statuses:
- **Widely Available**: Interoperable across all major engines (Chromium, Gecko, WebKit) for over 30 months (e.g., Cascade Layers, `:has()`, Nesting, Subgrid, Container Queries).
- **Newly Available**: Interoperable across all major browsers within the last 30 months (e.g., `@property`, `color-mix()`, Relative Color Syntax, `light-dark()`, `@starting-style`).
- **Limited Availability / Experimental**: Non-baseline features documented with progressive enhancement `@supports` fallbacks (e.g., `display: grid-lanes`, `if()`, `text-wrap: pretty`).

---

## Cross-Curriculum Navigation

- **Markup & Semantics**: See [HTML Learning Track](../html/README.md)
- **DOM Scripting**: See [JavaScript Learning Track](../javascript/README.md)
- **Framework Component Styling**:
  - React CSS Modules & Tailwind: See [ReactJS Styling Track](../../02-frontend/reactjs/07-styling/00-css-essentials.md)
  - Next.js CSS & Assets: See [Next.js Styling Track](../../02-frontend/nextjs/09-styling-and-assets/00-css.md)

---

[Start Learning: 00-setup ->](00-setup/README.md)
