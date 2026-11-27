# CSS Track Master Completion Plan (CSS_COMPLETION_PLAN.md)

**Curriculum Scope:** `01-languages/css/`  
**Governing Standard:** W3C Cascading Style Sheets (CSSWG) & Web Platform Baseline (2024–2026)  
**Total Target Modules:** 17 (00 through 16) + Track Root Governance Files

---

## 1. Complete Syllabus & Module Index

| Module | Title | Target Notes / Files | Core Capabilities Covered |
|:---|:---|:---:|:---|
| **Track Root** | Track Governance & Index | 12 files | README, INDEX, ROADMAP, GLOSSARY, CONTRIBUTING, FACTS, CONVENTIONS, AUDIT, DECISIONS, PLAN, PROGRESS, CHANGELOG |
| **`00-setup/`** | Setup, DevTools & Browser Engine Architecture | 6 notes + README | The cascade concept, Style/Layout/Paint/Composite pipeline, linking CSS, DevTools inspection, tooling (stylelint, Lightning CSS), CSS levels & Baseline |
| **`01-fundamentals/`** | Core Syntax, Selectors, Specificity & Cascade | 15 notes + README | Syntax, selectors, pseudo-classes/elements, specificity (A,B,C), cascade origins & layers (@layer), inheritance, units (px, rem, dvh, cq*), colors (oklch, color-mix, relative colors, light-dark), custom properties (@property), math functions (calc, clamp) |
| **`02-box-model-and-layout-basics/`** | Box Model, Formatting Contexts & Positioning | 13 notes + README | Box model & box-sizing, margin collapsing, display types & formatting contexts (BFC, IFC), normal flow, overflow & scrolling, positioning (relative, absolute, fixed, sticky), stacking contexts & z-index, floats & legacy, sizing, aspect-ratio, logical properties |
| **`03-flexbox/`** | Flexbox Layout in Depth | 8 notes + README | Flex mental model, container properties, item properties (grow, shrink, basis), alignment & gap, wrapping & ordering, flex sizing algorithm deep dive, flexbox patterns, common pitfalls |
| **`04-grid/`** | CSS Grid & Subgrid Layout | 11 notes + README | Grid mental model, tracks & fr units, item placement, grid areas, auto-placement & implicit grid, minmax & auto-fit/fill, alignment, subgrid, grid vs flexbox, grid patterns, grid pitfalls |
| **`05-responsive-design/`** | Responsive & Adaptive Design | 10 notes + README | Responsive principles, viewport meta & units, media queries, container queries (@container) & container units (cqw, cqi), fluid typography (clamp), responsive images, intrinsic design, responsive nav & tables, print styles, testing |
| **`06-typography-and-text/`** | Modern Typography, Web Fonts & Internationalization | 10 notes + README | Font properties, web fonts & font-display, variable fonts, text properties, line-height & vertical rhythm, text-wrap (balance/pretty), writing modes & RTL/bidi, text overflow & line clamping, icon fonts vs SVG, font performance |
| **`07-visual-styling/`** | Visual Effects, Borders, Shadows & Shapes | 12 notes + README | Backgrounds & gradients, borders/radius/outlines, box-shadow & text-shadow, filters & backdrop-filter, blend modes & isolation, clip-path & masks, transforms 2D/3D, lists & counters, form controls (appearance, accent-color), scrollbars, cursors, SVG in CSS |
| **`08-motion/`** | Transitions, Keyframes & Modern Motion | 9 notes + README | Transitions, animations & keyframes, linear() easing & timing, animation performance (compositor-only, will-change), scroll-driven animations, view transitions (SPA), @starting-style & discrete animation, reduced motion, spinners & micro-interactions |
| **`09-modern-css/`** | Modern CSS Features & Emerging Capabilities | 17 notes + README | Nesting, :has() relational styling, :is/:where/:not, @scope, container style queries, anchor positioning, popover/dialog styling, color-mix & oklch & RCS, @property typed variables, light-dark(), field-sizing, scroll snap, content-visibility, if(), masonry/grid-lanes, @supports |
| **`10-architecture-and-methodologies/`** | CSS Architecture, Methodologies & Scale | 15 notes + README | Architecture principles, BEM/OOCSS/SMACSS/ITCSS, Tailwind & utility-first, CUBE CSS, naming & file org, design tokens & theming, dark mode, resets & normalizations, CSS Modules, CSS-in-JS overview, Sass vs native CSS, UI frameworks compared, specificity at scale, dead CSS, component patterns |
| **`11-accessibility-and-ux-in-css/`** | Accessibility (a11y) & UX in CSS | 10 notes + README | :focus-visible & focus rings, color contrast & independence, prefers-color-scheme/contrast/reduced-data, forced-colors & High Contrast Mode, zoom & reflow (WCAG 1.4.10), touch targets (24x24px), visually-hidden pattern, line length & readability, print & alternative media, CSS & screen readers |
| **`12-performance-and-debugging/`** | Performance Optimization & DevTools Debugging | 10 notes + README | CSS performance myths vs realities, render blocking & critical CSS, layout/paint/composite costs, containment & content-visibility, file size & delivery, DevTools workflow, common layout bugs, cross-browser strategy, CSS testing & stylelint, Core Web Vitals |
| **`13-layout-and-component-recipes/`** | Layout & Component Recipes | 14 notes + README | Centering everything, holy grail & sidebar layouts, sticky header & footer, card grids & galleries, dashboards, nav bars & menus, modals & tooltips & popovers, hero & landing sections, forms layouts, responsive tables, tabs & accordions & toggles, buttons & alerts, skeleton loaders, CSS-only limits |
| **`14-projects/`** | Production Reference Architectures | 10 projects (40 files) + README | Landing page, Portfolio, Dashboard, Design Tokens Switcher, Pricing Page with Grid, Magazine Layout, Animated Components Set, CSS-only Accordion & Tabs, Dark Mode Blog, Mini Design System |
| **`15-interview/`** | Technical Interview Q&A | 10 notes + README | CSS basics, Cascade/specificity/inheritance, Box model & positioning, Flexbox & grid, Responsive design, Animations & performance, Modern CSS, Output & debugging questions, Practical layout challenges, Architectural discussion |
| **`16-cheatsheets/`** | Operational Cheatsheets | 12 cheatsheets + README | Selectors & specificity, Units & values, Box model & positioning, Flexbox, Grid, Responsive & media queries, Typography, Animations & transitions, Modern CSS Baseline, Centering recipes, Color functions, Common bugs & fixes |

---

## 2. Extra Expert Topics Included

1. **Cascade Resolution Algorithm & Inverted Importance**: The exact mathematical steps browser engines use to resolve competing declarations across user agent, user, author, animation, transition, and layered origins.
2. **The Formatting Context Machine**: Block, Inline, Flex, Grid, and Subgrid formatting context establishment, margin collapsing boundaries, and float clearances.
3. **Compositor Thread & GPU Layer Promotion**: Why `transform` and `opacity` bypass main-thread layout and paint stages to execute at 60/120 FPS.
4. **Anchor Positioning & Top Layer Topology**: How `anchor()`, `position-anchor`, and `position-area` bind non-DOM ancestors across top layer boundaries.
5. **Color Science & Perceptual Uniformity**: The CIELAB / OKLab color spaces, Gamut P3 mapping, and Relative Color Syntax channel extraction.
6. **Container Query Mechanics & CQ Units**: How `container-type: inline-size` establishes a containment root and enables `cqw`, `cqh`, `cqi`, `cqmin`.
7. **Windows High Contrast Mode (Forced Colors) Mechanics**: How system color keywords (`Canvas`, `CanvasText`, `Highlight`) override author CSS and why `box-shadow` is stripped.
8. **Scroll-Driven Animation Timelines**: Declarative time mapping from scroll progress to `@keyframes` via `animation-timeline: scroll()` and `view()`.

---

## 3. Execution Batches

- **Batch 1 (Governance, Toolchain & Module 00)**: Root files, templates, `css_runner.js` scratch script, Module 00 README + 6 notes.
- **Batch 2 (Module 01 Fundamentals Part 1)**: Notes 01 to 08 (Syntax, selectors, pseudo-classes, pseudo-elements, specificity, cascade origins, cascade layers, inheritance).
- **Batch 3 (Module 01 Fundamentals Part 2)**: Notes 09 to 15 (Units, colors, custom properties, math functions, text basics, shorthands, comments).
- **Batch 4 (Module 02 Box Model & Layout Basics)**: Notes 01 to 07 (Box model, margins, display types, normal flow, overflow, positioning, stacking contexts).
- **Batch 5 (Module 02 Continued & Module 03 Flexbox)**: Module 02 notes 08 to 13 + Module 03 README + notes 01 to 08.
- **Batch 6 (Module 04 Grid & Subgrid)**: Module 04 README + notes 01 to 11.
- **Batch 7 (Module 05 Responsive & Module 06 Typography)**: Module 05 notes 01 to 10 + Module 06 notes 01 to 10.
- **Batch 8 (Module 07 Visual Styling & Module 08 Motion)**: Module 07 notes 01 to 12 + Module 08 notes 01 to 09.
- **Batch 9 (Module 09 Modern CSS)**: Module 09 README + notes 01 to 17.
- **Batch 10 (Module 10 Architecture & Module 11 A11y)**: Module 10 notes 01 to 15 + Module 11 notes 01 to 10.
- **Batch 11 (Module 12 Performance & Module 13 Recipes)**: Module 12 notes 01 to 10 + Module 13 notes 01 to 14.
- **Batch 12 (Module 14 Production Projects)**: 10 production projects (40 files).
- **Batch 13 (Module 15 Interview & Module 16 Cheatsheets)**: 10 interview notes + 12 cheatsheets.
- **Batch 14 (Whole-Track Verification & Final Report)**: Verification script, tree update, `CSS_FINAL_REPORT.md`.
