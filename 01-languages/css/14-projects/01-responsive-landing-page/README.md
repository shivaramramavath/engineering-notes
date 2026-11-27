# Project 01: Responsive SaaS Landing Page

## Metadata
| Attribute | Details |
|:---|:---|
| **Tier** | Core Project |
| **Prerequisites** | [05-responsive-design](../05-responsive-design/README.md), [03-flexbox](../03-flexbox/README.md), [04-grid](../04-grid/README.md), [13-layout-and-component-recipes/08-hero-sections](../13-layout-and-component-recipes/08-hero-sections-and-landing-layouts.md) |
| **Estimated Completion Time** | 4 Hours |
| **Target Baseline** | Modern Web Baseline (2024–2026) |
| **Core Layout Engine** | CSS Grid & Flexbox |
| **Tested With** | Playwright `css_runner.js`, axe-core, Chrome DevTools Lighthouse |

---

## 1. Project Overview & Architecture
This project implements an end-to-end, high-conversion SaaS product landing page for a distributed cloud infrastructure engine (**NovaEngine**). 

The application architecture includes:
- **Sticky Glassmorphism Header**: Navigation with fluid logo, center links, and conversion actions with `backdrop-filter: blur(8px)`.
- **Split-Screen Hero Section**: Fluid `clamp()` typography, pill notification badge, CTA cluster, and a high-performance interactive code editor window mockup with zero layout shift.
- **Infinite Logo Marquee**: Pure CSS hardware-accelerated logo ribbon with continuous seamless scrolling and pause-on-hover / reduced-motion fallback.
- **3-Tier Feature Matrix Grid**: Auto-fitting responsive grid utilizing `minmax(min(100%, 320px), 1fr)` with Subgrid alignment for cards.
- **Responsive CTA Banner & Global Footer**: Clean multi-column directory footer collapsing smoothly on mobile.

---

## 2. File Organization
1. [`01-design.md`](./01-design.md): Design tokens, typography clamp formulas, color palette, and breakpoint matrix.
2. [`02-html-and-css.md`](./02-html-and-css.md): Complete production semantic HTML and CSS implementation.
3. [`03-measured-results.md`](./03-measured-results.md): Real computed layout dimensions, Core Web Vitals predictions (CLS = 0.000, LCP < 1.1s), and responsive viewport tests.
4. [`04-limitations-and-next-steps.md`](./04-limitations-and-next-steps.md): Edge cases, internationalization adaptations, and progressive enhancements.
