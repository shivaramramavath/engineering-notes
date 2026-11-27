# CSS Learning Track Roadmap (ROADMAP.md)

This roadmap outlines five tailored learning paths through the CSS curriculum depending on your professional engineering focus and background.

---

## 1. The Beginner Track: From First Selectors to Confident Layouts
*Target Audience:* New web developers, backend engineers, or students learning web styling from scratch.  
*Total Estimated Time:* **22–28 Hours**

```
[00-setup] ──► [01-fundamentals (01-08)] ──► [02-box-model (01-07)] ──► [03-flexbox] ──► [04-grid (01-07)]
   │                                                                                         │
   ▼                                                                                         ▼
Setup & Engine DevTools                                                              Responsive Design (05)
```

1. **Setup & Cascade Concept**: `00-setup/` (All 6 notes) — 3 Hours
2. **Core Fundamentals & Specificity**: `01-fundamentals/` (Notes 01–08: syntax, selectors, specificity, cascade, inheritance) — 5 Hours
3. **Box Model & Positioning**: `02-box-model-and-layout-basics/` (Notes 01–07: box-sizing, margins, flow, positioning, z-index) — 5 Hours
4. **Modern Flexbox**: `03-flexbox/` (Notes 01–08: container, item, sizing, alignment) — 4 Hours
5. **Modern Grid Basics**: `04-grid/` (Notes 01–07: fr units, lines, areas, minmax) — 5 Hours

---

## 2. The Frontend Specialist Track: Production Component Engineering
*Target Audience:* Fullstack and frontend developers building scalable, high-performance web applications.  
*Total Estimated Time:* **30–38 Hours**

1. **Deep Fundamentals**: `01-fundamentals/` (Notes 07–15: `@layer`, units, `oklch`, custom properties, math) — 6 Hours
2. **Advanced Layout**: `03-flexbox/` & `04-grid/` (including `subgrid`) — 8 Hours
3. **Responsive Architecture**: `05-responsive-design/` (Container Queries, fluid clamp, responsive tables) — 6 Hours
4. **Modern CSS Features**: `09-modern-css/` (`:has()`, nesting, `@scope`, `@property`, `light-dark()`, anchor positioning) — 10 Hours
5. **Performance & Debugging**: `12-performance-and-debugging/` (containment, layout thrashing, DevTools) — 5 Hours

---

## 3. The Layout & Responsive Specialist Track: Fluid, Intrinsic Design
*Target Audience:* UI engineers, designers who code, and developers specializing in fluid layouts.  
*Total Estimated Time:* **24–30 Hours**

1. **Box Model & Formatting Contexts**: `02-box-model-and-layout-basics/` (All 13 notes) — 7 Hours
2. **Mastering Flexbox & Grid**: `03-flexbox/` & `04-grid/` (All notes + subgrid) — 9 Hours
3. **Modern Responsive Design**: `05-responsive-design/` (Container queries, container units, RAM pattern) — 6 Hours
4. **Layout Recipes**: `13-layout-and-component-recipes/` (Centering, holy grail, dashboards, card grids) — 6 Hours

---

## 4. The Design Systems & Architecture Track: Enterprise Scale
*Target Audience:* Design system engineers, tech leads, and architects managing multi-brand component libraries.  
*Total Estimated Time:* **28–35 Hours**

1. **Cascade Layers & Custom Properties**: `01-fundamentals/` (Notes 07, 10, 11) & `09-modern-css/` (Notes 08, 09, 10) — 6 Hours
2. **Color Science & Modern Spaces**: `oklch()`, `color-mix()`, Relative Color Syntax, `light-dark()` — 5 Hours
3. **Architecture & Methodologies**: `10-architecture-and-methodologies/` (All 15 notes: BEM, CUBE CSS, Design Tokens, CSS Modules) — 10 Hours
4. **Accessibility (a11y) in CSS**: `11-accessibility-and-ux-in-css/` (Contrast, `:focus-visible`, forced-colors, reduced motion) — 6 Hours
5. **Reference Capstone Project**: `14-projects/10-mini-css-design-system/` — 6 Hours

---

## 5. The Technical Interview & Review Track: Fast-Track Revision
*Target Audience:* Engineers preparing for senior frontend technical interviews or architecture reviews.  
*Total Estimated Time:* **12–16 Hours**

1. **Core Mechanics Refresher**:
   - Specificity math & cascade order: `01-fundamentals/05-specificity.md`, `06-cascade-origins...md`, `07-cascade-layers.md`
   - Stacking contexts & z-index: `02-box-model-and-layout-basics/07-stacking-contexts-and-z-index.md`
   - Flexbox vs Grid sizing algorithms: `03-flexbox/06-flexbox-sizing...md`, `04-grid/08-subgrid.md`
2. **Modern Baseline Trivia**:
   - `09-modern-css/` (`:has()`, `@property`, `@starting-style`, anchor positioning)
3. **Interview Question Sets**:
   - `15-interview/` (All 10 Q&A notes: output questions, debugging challenges, architectural discussion) — 8 Hours
4. **Cheatsheet Drills**:
   - `16-cheatsheets/` (Quick revision across all 12 operational sheets) — 2 Hours
