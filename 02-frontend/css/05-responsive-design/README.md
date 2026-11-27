# Module 05: Responsive Design (`05-responsive-design/`)

| Metadata | Details |
| :--- | :--- |
| **Level** | Core / Professional |
| **Prerequisites** | `01-fundamentals/`, `02-box-model-and-layout-basics/`, `03-flexbox/`, `04-grid/` |
| **Estimated Completion Time** | 4.5 hours |
| **Status** | Complete |
| **Baseline Status** | Widely Available (Media Queries Baseline 2015; Container Queries Baseline 2023) |
| **Module Focus** | Mobile-First Architecture, Modern Viewports, Container Queries & CQ Units, Fluid Sizing, Accessibility Queries |

---

## Overview

Responsive Web Design (RWD) has evolved far beyond simply stacking desktop layouts on mobile phones. In modern web engineering, responsive design represents a holistic approach to user interface resilience across:
- **Device Diversity**: From smartwatches and handheld phones to dual-screen foldables, tablets, ultrawide monitors, and TV browsers.
- **Contextual Adaptation**: Adapting UI based on the parent component's width (**Container Queries**) rather than global viewport width alone.
- **Fluid Geometry**: Smooth scaling of typography, paddings, and gutters via mathematical `clamp()` without jarring step-function breakpoint jumps.
- **User Agency**: Respecting user hardware and operating system preferences: dark mode (`prefers-color-scheme`), reduced motion (`prefers-reduced-motion`), high contrast (`forced-colors`), data saving (`prefers-reduced-data`), and touch input capabilities (`hover`, `pointer`).

This module provides exhaustive, production-grade mastery of modern responsive engineering.

---

## Module Roadmap

| Note | Title | Focus & Core Mechanics | Est. Time |
| :--- | :--- | :--- | :--- |
| **[01](01-responsive-principles-and-mobile-first.md)** | **Responsive Principles & Mobile-First** | Progressive enhancement vs graceful degradation, mobile-first CSS architecture, min-width vs max-width cascading logic. | 22 min |
| **[02](02-viewport-meta-and-units.md)** | **Viewport Meta and Modern Units** | `<meta name="viewport">` breakdown, solving the mobile 100vh address bar bug with `svh`, `lvh`, and `dvh` viewport units. | 22 min |
| **[03](03-media-queries.md)** | **Modern Media Queries** | Media Query Level 4 range syntax (`@media (600px <= width <= 900px)`), boolean logic (`and`, `or`, `not`), standard breakpoint systems. | 24 min |
| **[04](04-container-queries-and-container-units.md)** | **Container Queries & Container Units** | Modern milestone (Baseline 2023), `container-type: inline-size`, `@container` queries, CQ units (`cqi`, `cqb`, `cqmin`). | 26 min |
| **[05](05-fluid-typography-and-spacing-clamp.md)** | **Fluid Typography & Spacing with `clamp()`** | Mathematical linear interpolation derivation, fluid scales between mobile and desktop viewports, accessibility safeguards. | 25 min |
| **[06](06-responsive-images-with-css.md)** | **Responsive Images with CSS** | `aspect-ratio` (Baseline 2021), `object-fit`, `object-position`, image orientation, pairing CSS with HTML `<picture>` and `srcset`. | 24 min |
| **[07](07-user-preference-media-features.md)** | **User Preference Media Features** | `@media (prefers-color-scheme)`, `@media (prefers-reduced-motion)`, `@media (forced-colors)`, `@media (prefers-contrast)`. | 26 min |
| **[08](08-interaction-media-features.md)** | **Interaction Media Features** | Distinguishing mice from touchscreens: `@media (hover: hover)` vs `(hover: none)`, `@media (pointer: fine)` vs `(pointer: coarse)`. | 20 min |
| **[09](09-responsive-navigation-patterns.md)** | **Responsive Navigation Patterns** | Pure-CSS & accessible hybrid navbars: responsive hamburger drawers, scrollable tab bars, priority+ navigation. | 25 min |
| **[10](10-responsive-tables-and-data.md)** | **Responsive Tables and Data** | Mobile data presentation: horizontal scrolling wrappers, responsive card transformation (`data-label`), sticky header grids. | 25 min |

---

## Learning Objectives

By completing this module, you will:
1. **Architect True Mobile-First Stylesheets**: Build lean, performant mobile baselines that scale upward cleanly without override bloat.
2. **Master Container Queries**: Build truly self-contained, modular UI components that adapt dynamically whether placed in a full-width banner or a 240px sidebar.
3. **Derive Fluid Typography Mathematically**: Write precise, accessible `clamp()` expressions that scale smoothly between viewports without abrupt layout jumps.
4. **Implement Accessibility and Preference Queries**: Honor user accessibility needs for reduced motion, high contrast modes, and touch target sizes.

---

[Previous Module: 04-grid](file:///e:/Learning/engineering-notes/01-languages/css/04-grid/README.md) | [Track Index](file:///e:/Learning/engineering-notes/01-languages/css/INDEX.md) | [Next Module: 06-typography-and-text](file:///e:/Learning/engineering-notes/01-languages/css/06-typography-and-text/README.md)
