# Module 13: Layout & Component Recipes

Welcome to **Module 13: Layout & Component Recipes** of the authoritative CSS curriculum. This module serves as the production cookbook for modern web development, compiling proven, battle-tested layout algorithms and UI component patterns engineered entirely with modern Web Platform standards (Flexbox, CSS Grid, Subgrid, Container Queries, and Top Layer APIs).

---

## 1. Module Overview

Front-end engineers spend a significant portion of their professional careers implementing recurring layout structures and interactive interface widgets:
- Centering content horizontally and vertically.
- Building resilient Holy Grail and collapsible dashboard app shells.
- Designing responsive, auto-wrapping card grids and media galleries without media queries.
- Crafting accessible navigation headers, flyout menus, and mobile bottom tab bars.
- Engineering accessible dialogs, popovers, and anchor-positioned tooltips.
- Assembling bulletproof form layouts, responsive data tables, tabs, accordions, and skeleton loaders.

Rather than relying on bloated third-party component libraries or fragile legacy hacks, this module provides clean, modern, zero-dependency recipes that are robust, accessible, and performant by default.

---

## 2. Directory Structure & Notes

| Note | Title | Primary Focus |
| :--- | :--- | :--- |
| **[01-centering-everything.md](01-centering-everything.md)** | Centering Everything in CSS | The definitive guide to horizontal and vertical centering: Grid `place-content: center`, Flexbox, margin auto, and positioning. |
| **[02-holy-grail-and-sidebar-layouts.md](02-holy-grail-and-sidebar-layouts.md)** | Holy Grail & Sidebar Layouts | Modern 3-column Holy Grail layout, sticky sidebar patterns, RAM (Repeat, Auto, Minmax) grid shells. |
| **[03-sticky-header-and-footer-patterns.md](03-sticky-header-and-footer-patterns.md)** | Sticky Headers & Footers | Sticky navigation with scroll shadows, footer pinned to bottom via flex/grid, `min-height: 100dvh`. |
| **[04-card-grids-and-galleries.md](04-card-grids-and-galleries.md)** | Card Grids & Media Galleries | Responsive auto-fit grids (`minmax()`), Subgrid alignment for titles/actions, aspect-ratio image cards. |
| **[05-dashboard-layouts.md](05-dashboard-layouts.md)** | Enterprise Dashboard Layouts | Collapsible side navigation, top app bar, multi-panel widget grids, scrollable data viewports. |
| **[06-navigation-bars-and-menus.md](06-navigation-bars-and-menus.md)** | Navigation Bars & Responsive Menus | Header navbars, mobile hamburger flyouts without JavaScript, accessible dropdown menus. |
| **[07-modals-tooltips-and-popovers.md](07-modals-tooltips-and-popovers.md)** | Modals, Tooltips & Popovers | Native `<dialog>` modal styling, HTML Popover API, CSS Anchor Positioning, `::backdrop` transitions. |
| **[08-hero-sections-and-landing-layouts.md](08-hero-sections-and-landing-layouts.md)** | Hero Sections & Landing Layouts | Split-screen hero, full-viewport backgrounds, fluid typography clamps, floating badge accents. |
| **[09-forms-layouts.md](09-forms-layouts.md)** | Form Layouts & Floating Labels | Responsive grid forms, accessible custom checkboxes/radios, floating label animations, error states. |
| **[10-responsive-tables-recipes.md](10-responsive-tables-recipes.md)** | Responsive Table Recipes | Overflow scrolling with shadows, card reflow on mobile (`data-label`), sticky header & column tables. |
| **[11-tabs-accordions-and-toggles.md](11-tabs-accordions-and-toggles.md)** | Tabs, Accordions & Toggles | Semantic HTML `<details>` accordions, animated height transitions via grid, ARIA-driven tab styling. |
| **[12-buttons-badges-and-alerts.md](12-buttons-badges-and-alerts.md)** | Buttons, Badges & Alerts | Primary, secondary, ghost, destructive button states, pill badges, accessible notification banners. |
| **[13-skeleton-loaders-and-empty-states.md](13-skeleton-loaders-and-empty-states.md)** | Skeleton Loaders & Empty States | Shimmer animation skeletons without layout shifts, accessible illustrated empty states. |
| **[14-css-only-components-limits.md](14-css-only-components-limits.md)** | CSS-Only Components & Limits | Where CSS-only techniques shine (tooltips, toggles) vs where JavaScript is strictly required for accessibility. |

---

## 3. Recipe Architecture Philosophy

Every recipe in this module adheres to the **Four Pillars of Modern CSS Architecture**:
1. **Defensive Layout**: No hardcoded pixel widths, proper `min-width: 0` on flex/grid items, and container query adaptation.
2. **Accessible by Construction**: Every recipe includes keyboard focus indicators (`:focus-visible`), WCAG AA/AAA color contrast, screen reader helper text, and proper semantic HTML tags.
3. **Hardware-Accelerated Performance**: Animations and micro-interactions operate strictly on compositor-only properties (`transform`, `opacity`).
4. **Zero Legacy Hacks**: No float clearfixes, no `table-cell` vertical centering, no `!important` overrides, and no vendor prefixes.

---

| Previous Module | Next Module |
| :--- | :--- |
| [Module 12: Performance & Debugging](../12-performance-and-debugging/README.md) | [Module 14: Production Reference Projects](../14-projects/README.md) |
