# Project 02: Minimalist Engineer Bento Portfolio

## Metadata
| Attribute | Details |
|:---|:---|
| **Tier** | Core Project |
| **Prerequisites** | [04-grid](../04-grid/README.md), [05-responsive-design](../05-responsive-design/README.md), [07-visual-styling](../07-visual-styling/README.md), [11-accessibility-and-ux-in-css/09-print](../11-accessibility-and-ux-in-css/09-print-and-alternative-media.md) |
| **Estimated Completion Time** | 4 Hours |
| **Target Baseline** | Modern Web Baseline (2024–2026) |
| **Core Layout Engine** | CSS Bento Grid (`grid-template-areas`) & Container Queries |
| **Tested With** | Playwright `css_runner.js`, axe-core, Chromium Print Emulator |

---

## 1. Project Overview & Architecture
This project implements a high-aesthetic, high-performance personal engineering portfolio and interactive resume for a Senior Staff Systems Architect.

Key features:
- **Asymmetric Bento Grid Layout**: Multi-span interactive tiles organizing Bio, Featured Architecture Project, GitHub Activity Streak, Tech Stack pills, Writing articles, and Contact actions into an ergonomic matrix.
- **Micro-Hover Physics**: Smooth GPU-accelerated card translations, subtle radial gradient cursor illumination (`background: radial-gradient`), and high-contrast `:focus-visible` styling.
- **System Theme Integration**: Fully responsive dark mode and light mode adaptation via `light-dark()` and CSS custom properties.
- **Dedicated Print Stylesheet**: Native `@media print` rules transforming the interactive bento grid into a clean, paginated, two-page executive resume without ink waste.

---

## 2. File Organization
1. [`01-design.md`](./01-design.md): Design tokens, bento grid coordinate mapping, color palettes, and typographic rhythm.
2. [`02-html-and-css.md`](./02-html-and-css.md): Complete production implementation in semantic HTML and CSS.
3. [`03-measured-results.md`](./03-measured-results.md): Real computed bounding box dimensions, print layout simulation measurements, and contrast compliance.
4. [`04-limitations-and-next-steps.md`](./04-limitations-and-next-steps.md): Bento grid wrapping limitations on small tablets, print engine differences, and future subgrid extensions.
