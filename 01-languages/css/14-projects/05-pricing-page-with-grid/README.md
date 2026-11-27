# Project 05: High-Conversion SaaS Pricing Page with Subgrid

## Metadata
| Attribute | Details |
|:---|:---|
| **Tier** | Core Project |
| **Prerequisites** | [04-grid/08-subgrid](../04-grid/08-subgrid.md), [13-layout-and-component-recipes/04-card-grids](../13-layout-and-component-recipes/04-card-grids-and-galleries.md), [13-layout-and-component-recipes/11-tabs-accordions](../13-layout-and-component-recipes/11-tabs-accordions-and-toggles.md) |
| **Estimated Completion Time** | 4 Hours |
| **Target Baseline** | Modern Web Baseline (2024–2026) |
| **Core Layout Engine** | CSS Grid & Subgrid |
| **Tested With** | Playwright `css_runner.js`, axe-core |

---

## 1. Project Overview & Architecture
This project implements an enterprise 3-tier SaaS pricing comparison matrix (**Starter**, **Pro [Popular]**, and **Enterprise**).

Key architectural features:
- **Subgrid Baseline Alignment**: Eliminates uneven card alignments across disparate plan descriptions. Card Title, Tier Badge, Price Display, Feature List, and Primary CTA button all bind to parent grid row tracks via `grid-template-rows: subgrid`.
- **Annual vs Monthly Billing Frequency Switcher**: CSS `:has()` toggle that recalculates displayed pricing numbers smoothly.
- **Popular Tier Elevation**: Visual elevation, prominent highlight badge, and distinct accent border without causing layout shift.
- **Responsive Stacking**: Seamlessly transitions from 3-column synchronized Subgrid rows to independent mobile cards on smaller viewports.

---

## 2. File Organization
1. [`01-design.md`](./01-design.md): Subgrid row track mapping, pricing matrix specs, and typography.
2. [`02-html-and-css.md`](./02-html-and-css.md): Complete production semantic implementation.
3. [`03-measured-results.md`](./03-measured-results.md): Subgrid row height measurements, baseline alignment verification, and viewport tests.
4. [`04-limitations-and-next-steps.md`](./04-limitations-and-next-steps.md): Subgrid fallback strategies and currency formatting enhancements.
