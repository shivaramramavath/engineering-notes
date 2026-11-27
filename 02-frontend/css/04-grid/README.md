# Module 04: Grid & Subgrid Deep Dive (`04-grid/`)

| Metadata | Details |
| :--- | :--- |
| **Level** | Core / Professional |
| **Prerequisites** | `02-box-model-and-layout-basics/`, `03-flexbox/` |
| **Estimated Completion Time** | 4.5 hours |
| **Status** | Complete |
| **Baseline Status** | Widely Available (CSS Grid Baseline 2017; CSS Subgrid Baseline 2023) |
| **Module Focus** | 2-Dimensional Layout, Explicit & Implicit Tracks, Named Areas, Subgrid Track Inheritance, Fluid Patterns |

---

## Overview

While Flexbox excels at one-dimensional component layouts (rows *or* columns), **CSS Grid Layout** is the web platform's premier **two-dimensional layout system**. It gives authors full, coordinated control over both rows and columns simultaneously.

Furthermore, **CSS Subgrid** (Baseline 2023 across Chrome 117+, Safari 16+, Firefox 71+) resolves the longstanding problem of nested component alignment, allowing children of grid items to inherit and participate directly in the parent grid's tracks!

This module provides exhaustive, production-grade mastery of CSS Grid and Subgrid, covering everything from line-based placement and ASCII-like template areas to the mathematical fractional unit (`fr`), the `minmax()` track sizing algorithm, and full subgrid card architectures.

---

## Module Roadmap

| Note | Title | Focus & Core Mechanics | Est. Time |
| :--- | :--- | :--- | :--- |
| **[01](01-grid-mental-model.md)** | **Grid Mental Model** | 2D layout paradigm, Grid Formatting Context (GFC), container vs items, tracks, lines, cells, and areas. | 22 min |
| **[02](02-defining-tracks-and-fr-units.md)** | **Defining Tracks and `fr` Units** | `grid-template-columns`, `grid-template-rows`, `repeat()` notation, fractional unit (`fr`) math, fixed vs fluid tracks. | 24 min |
| **[03](03-placing-items-and-lines.md)** | **Placing Items and Grid Lines** | Positive and negative line numbers, spanning (`span N`), line naming syntax (`[col-start]`), `grid-column` and `grid-row` shorthands. | 22 min |
| **[04](04-grid-areas-and-template-areas.md)** | **Grid Areas and Template Areas** | ASCII art layout with `grid-template-areas`, the dot (`.`) null cell keyword, named area placement, responsive layout shifting without DOM changes. | 22 min |
| **[05](05-auto-placement-and-implicit-grid.md)** | **Auto-Placement and Implicit Grid** | `grid-auto-flow` (`row`, `column`, `dense`), implicit track sizing via `grid-auto-columns`/`rows`, packing algorithms. | 24 min |
| **[06](06-minmax-auto-fit-and-auto-fill.md)** | **`minmax()`, `auto-fill`, and `auto-fit`** | The Holy Grail fluid grid pattern: `repeat(auto-fit, minmax(280px, 1fr))`, auto-fill vs auto-fit empty track mechanics. | 25 min |
| **[07](07-alignment-in-grid.md)** | **Alignment in CSS Grid** | Two-axis alignment: `justify-items`, `align-items`, `place-items`, `justify-content`, `align-content`, `place-content`, item self-overrides. | 22 min |
| **[08](08-subgrid.md)** | **CSS Subgrid Deep Dive** | Baseline 2023 milestone, nested track inheritance (`grid-template-rows: subgrid`), card alignment across siblings, named subgrid lines. | 26 min |
| **[09](09-grid-vs-flexbox.md)** | **Grid vs. Flexbox Decision Framework** | 1D vs 2D, Content-out (Flexbox) vs Layout-in (Grid), combining Grid containers with Flexbox components, architectural guidelines. | 20 min |
| **[10](10-grid-patterns.md)** | **Production Grid Layout Patterns** | The 12-column editorial grid, Holy Grail app layout, full-bleed breakout layout (`[full-start]`), responsive dashboards, overlapping collage cards. | 26 min |
| **[11](11-grid-pitfalls.md)** | **Grid Pitfalls & Edge Cases** | The `1fr` minimum size blowout (`minmax(0, 1fr)`), grid auto-placement collisions, subgrid fallback strategies, accessibility of visual reordering. | 25 min |

---

## Learning Objectives

By completing this module, you will:
1. **Master True Two-Dimensional Geometry**: Effortlessly structure complex multi-column, multi-row page architectures with zero math hacks.
2. **Build Zero-Media-Query Responsive Layouts**: Utilize `repeat(auto-fit, minmax(...))` to create fluid card grids that seamlessly adapt from mobile phones to 4K monitors without a single `@media` rule.
3. **Align Nested Components with Subgrid**: Eradicate jagged, mismatched card titles and footers across rows by having children inherit parent grid tracks directly.
4. **Distinguish Grid from Flexbox Intuitively**: Know precisely when to use Grid (macro page layout, rigid 2D alignment) versus Flexbox (micro component flow, toolbars, tags).

---

[Previous Module: 03-flexbox](file:///e:/Learning/engineering-notes/01-languages/css/03-flexbox/README.md) | [Track Index](file:///e:/Learning/engineering-notes/01-languages/css/INDEX.md) | [Next Module: 05-responsive-design](file:///e:/Learning/engineering-notes/01-languages/css/05-responsive-design/README.md)
