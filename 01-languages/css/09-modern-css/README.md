# Module 09: Modern CSS Features (`09-modern-css/`)

| Metadata | Details |
| :--- | :--- |
| **Track** | CSS Curriculum |
| **Module** | 09 — Modern CSS Features |
| **Spec Level** | CSS Cascading and Inheritance L6, Selectors L4, Anchor Positioning L1, Color L4/L5, Scoping L1 |
| **Baseline Status** | Baseline 2023–2026 Newly and Widely Available Features |
| **Prerequisites** | `01-fundamentals`, `02-box-model`, `03-flexbox`, `04-grid`, `07-visual-styling`, `08-motion` |

---

## Executive Overview

Between 2022 and 2026, the Web Platform underwent the largest renaissance in the history of Cascading Style Sheets. For two decades, frontend architectures were forced to rely on heavy preprocessors (Sass, Less), postprocessors (PostCSS), JavaScript polyfills, and CSS-in-JS runtimes to overcome foundational language limitations.

Today, CSS has absorbed and superseded these workarounds directly into the browser engine:
- **Native Nesting** eliminates the need for Sass compilers.
- **`:has()` (The Relational Selector)** enables parent and sibling querying without JavaScript event listeners.
- **CSS Scope (`@scope`)** solves modular encapsulation without CSS Modules or Shadow DOM overhead.
- **CSS Anchor Positioning** renders floating menus, tooltips, and popovers tied directly to anchor elements without Popper.js or Floating UI.
- **Perceptually Uniform Color Spaces (`oklch()`) and Relative Color Syntax (RCS)** enable mathematical, dynamic design systems and contrast palettes.
- **Typed Custom Properties (`@property`)** bring strict types, inheritance control, and smooth gradient/color transitions to CSS variables.
- **`field-sizing: content`** allows textareas and inputs to expand natively with their text content.
- **Emerging Specifications (`@function`, CSS conditionals `inline-if()`, and CSS Masonry)** complete the transition into a fully expressive, declarative system.

This module provides an exhaustive, specification-accurate breakdown of each modern feature with cross-browser Baseline availability matrices, internal browser execution lifecycles, and production-tested patterns.

---

## Module Syllabus

| # | Note | Core Capabilities & Key Standards | Baseline Status |
|:---|:---|:---|:---|
| 01 | [01-nesting.md](file:///e:/Learning/engineering-notes/01-languages/css/09-modern-css/01-nesting.md) | Native CSS Nesting syntax, `&` selector rules, relaxation of tag selector requirements, preprocessor migration. | Baseline 2023 |
| 02 | [02-has-selector-and-relational-styling.md](file:///e:/Learning/engineering-notes/01-languages/css/09-modern-css/02-has-selector-and-relational-styling.md) | Relational pseudo-class `:has()`, parent styling, previous-sibling emulation, form validation states. | Baseline 2023 |
| 03 | [03-is-where-and-not.md](file:///e:/Learning/engineering-notes/01-languages/css/09-modern-css/03-is-where-and-not.md) | Specificity zeroing with `:where()`, specificity aggregation with `:is()`, negation logic with `:not()`. | Widely Available |
| 04 | [04-scope-rule.md](file:///e:/Learning/engineering-notes/01-languages/css/09-modern-css/04-scope-rule.md) | `@scope` at-rule, scoping roots, scoping limits (donut scoping), component encapsulation without Shadow DOM. | Baseline 2024 |
| 05 | [05-container-style-queries.md](file:///e:/Learning/engineering-notes/01-languages/css/09-modern-css/05-container-style-queries.md) | `@container style(...)` queries, custom property querying, context-aware component variants. | Limited / In Progress |
| 06 | [06-anchor-positioning.md](file:///e:/Learning/engineering-notes/01-languages/css/09-modern-css/06-anchor-positioning.md) | `anchor-name`, `position-anchor`, `anchor()`, `position-try-fallbacks`, popovers without JS libraries. | Baseline 2024 / Newly Available |
| 07 | [07-popover-and-dialog-styling.md](file:///e:/Learning/engineering-notes/01-languages/css/09-modern-css/07-popover-and-dialog-styling.md) | Popover API styling, Top Layer mechanics, `::backdrop`, `popover="auto"` vs `popover="manual"`. | Baseline 2024 |
| 08 | [08-color-mix-oklch-and-relative-colors.md](file:///e:/Learning/engineering-notes/01-languages/css/09-modern-css/08-color-mix-oklch-and-relative-colors.md) | OKLCH color space, `color-mix()`, Relative Color Syntax (RCS) channels, dynamic contrast palettes. | Baseline 2024 |
| 09 | [09-property-typed-custom-properties.md](file:///e:/Learning/engineering-notes/01-languages/css/09-modern-css/09-property-typed-custom-properties.md) | `@property` at-rule, `syntax`, `inherits`, `initial-value`, animating CSS gradients and custom angles. | Baseline 2024 |
| 10 | [10-light-dark-and-theming.md](file:///e:/Learning/engineering-notes/01-languages/css/09-modern-css/10-light-dark-and-theming.md) | `light-dark()` function, `color-scheme` integration, zero-overhead theme switching. | Baseline 2024 |
| 11 | [11-field-sizing-and-form-controls.md](file:///e:/Learning/engineering-notes/01-languages/css/09-modern-css/11-field-sizing-and-form-controls.md) | `field-sizing: content`, auto-expanding `<textarea>` and `<input>`, custom form control styling. | Baseline 2024 (Chromium/Firefox) |
| 12 | [12-scroll-snap.md](file:///e:/Learning/engineering-notes/01-languages/css/09-modern-css/12-scroll-snap.md) | CSS Scroll Snap Module L1, `scroll-snap-type`, `scroll-snap-align`, `scroll-snap-stop`, carousels. | Widely Available |
| 13 | [13-content-visibility-and-containment.md](file:///e:/Learning/engineering-notes/01-languages/css/09-modern-css/13-content-visibility-and-containment.md) | `content-visibility: auto`, `contain-intrinsic-size`, CSS containment (`contain: paint layout size`). | Baseline 2023 |
| 14 | [14-css-if-and-custom-functions.md](file:///e:/Learning/engineering-notes/01-languages/css/09-modern-css/14-css-if-and-custom-functions.md) | Proposed CSS Functions and Mixins (`@function`), inline conditional `if()`, algorithmic styling future. | Experimental / Working Draft |
| 15 | [15-masonry-and-grid-lanes.md](file:///e:/Learning/engineering-notes/01-languages/css/09-modern-css/15-masonry-and-grid-lanes.md) | CSS Grid Masonry layout (`grid-template-rows: masonry`), Pinterest-style layouts without JavaScript. | Experimental (Firefox/Chrome flag) |
| 16 | [16-supports-and-progressive-enhancement.md](file:///e:/Learning/engineering-notes/01-languages/css/09-modern-css/16-supports-and-progressive-enhancement.md) | Feature detection via `@supports`, `@supports selector()`, `@supports font-tech()`, fallback layering. | Widely Available |
| 17 | [17-sibling-index-and-count.md](file:///e:/Learning/engineering-notes/01-languages/css/09-modern-css/17-sibling-index-and-count.md) | CSS Values & Units L5: `sibling-index()`, `sibling-count()`, dynamic indexed staggered animations. | Working Draft / Early Preview |

---

## Baseline Quick Reference (2024–2026)

```
Modern Platform Feature Availability Snapshot:
┌───────────────────────────────┬─────────────────────────────┬──────────────────────────┐
│ Feature                       │ Baseline Status             │ Browser Engines          │
├───────────────────────────────┼─────────────────────────────┼──────────────────────────┤
│ CSS Nesting                   │ Baseline 2023               │ All engines (Full)       │
│ :has() Relational Selector    │ Baseline 2023               │ All engines (Full)       │
│ Subgrid                       │ Baseline 2023               │ All engines (Full)       │
│ Cascade Layers (@layer)       │ Baseline 2022               │ All engines (Full)       │
│ OKLCH & Relative Colors       │ Baseline 2024               │ All engines (Full)       │
│ @property (Typed CSS)         │ Baseline 2024               │ All engines (Full)       │
│ @scope                        │ Baseline 2024               │ Blink, WebKit (Gecko WIP)│
│ Anchor Positioning            │ Baseline 2024 (Newly Avail) │ Blink 125+, WebKit/Gecko │
│ light-dark() Function         │ Baseline 2024               │ All engines (Full)       │
│ field-sizing: content         │ Baseline 2024 (Chrome 123+) │ Blink, Firefox 131+      │
└───────────────────────────────┴─────────────────────────────┴──────────────────────────┘
```

---

## Suggested Study Path

1. **Foundations of Modern Declarative Logic**:
   - Begin with `01-nesting.md` and `03-is-where-and-not.md` to streamline your selector stylesheets.
   - Master `02-has-selector-and-relational-styling.md` to eliminate DOM-state JavaScript listeners.
2. **Component Architecture & Encapsulation**:
   - Study `04-scope-rule.md` to understand donut scoping and modular styling boundaries.
   - Deepen your layout encapsulation with `05-container-style-queries.md`.
3. **Advanced Positioning & Modern Interactive Primitives**:
   - Master native overlay architectures with `06-anchor-positioning.md` and `07-popover-and-dialog-styling.md`.
   - Learn zero-JavaScript form resizing with `11-field-sizing-and-form-controls.md`.
4. **Design Systems & Color Science**:
   - Transform color manipulation using `08-color-mix-oklch-and-relative-colors.md` and `10-light-dark-and-theming.md`.
   - Implement type-safe custom properties using `09-property-typed-custom-properties.md`.
5. **Emerging Specs & Architectural Futures**:
   - Explore `13-content-visibility-and-containment.md` through `17-sibling-index-and-count.md` to prepare for upcoming platform standards.

---

### Navigation
- **Previous Module**: [../08-motion/README.md](file:///e:/Learning/engineering-notes/01-languages/css/08-motion/README.md)
- **Next Note**: [01-nesting.md](file:///e:/Learning/engineering-notes/01-languages/css/09-modern-css/01-nesting.md)
