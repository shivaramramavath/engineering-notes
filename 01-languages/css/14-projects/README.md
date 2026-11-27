# Module 14: Production Reference Projects

| Topic | Module | Level | Baseline Status | Browsers Tested | Date | Prerequisites |
|---|---|---|---|---|---|---|
| Reference Projects | 14: Production Projects | Advanced / Capstone | Modern Web Baseline (2024–2026) | Chrome 120+, Firefox 120+, Safari 17.4+ | 2026-03-31 | Modules 00–13 |

---

## Overview

The **Production Reference Projects** module transitions from theoretical concepts, syntactical mechanics, and layout recipes into end-to-end, production-grade applications. Each project represents a complete real-world software interface—crafted in strict conformance with modern Web Platform standards (Baseline 2024–2026), zero external framework dependencies, and zero raw external `.html` or `.css` files (Markdown-Only policy).

Every project is rigorously documented across a standardized 5-file technical architecture:
1. `README.md`: System requirements, architectural blueprint, user journeys, and component inventory.
2. `01-design.md`: Design system foundations, OKLCH token scales, typography clamps, elevation elevations, and responsive breakpoint matrix.
3. `02-html-and-css.md`: Complete production implementation inside fenced markdown blocks, including semantic HTML and modern CSS (`@layer`, Subgrid, Container Queries, `:has()`, discrete transitions).
4. `03-measured-results.md`: Empirical verification including computed layout dimensions, Core Web Vitals predictions (CLS = 0.000, LCP < 1.2s), responsive viewport measurements, and accessibility audits.
5. `04-limitations-and-next-steps.md`: Production trade-offs, browser quirks, performance bottlenecks under heavy data, and future CSS specification enhancements.

---

## Project Directory

| Project | Name | Core Layout Engine | Key Platform Features |
|---|---|---|---|
| **[01-responsive-landing-page](./01-responsive-landing-page/README.md)** | SaaS Product Landing Page | CSS Grid & Flexbox | Fluid Typography, Photometric Video Scrim, Infinite Marquee, Sticky Header |
| **[02-personal-portfolio](./02-personal-portfolio/README.md)** | Minimalist Engineer Portfolio | Asymmetric Bento Grid | Container Queries, Dark/Light Mode, Card Hover Physics, Print Resume Stylesheet |
| **[03-dashboard-layout](./03-dashboard-layout/README.md)** | Cloud Telemetry Operations Console | Multi-Tier CSS Grid Shell | `100dvh` Zero-Scroll Root, Collapsible Rail, Subgrid Card Alignment, Off-Canvas Drawer |
| **[04-design-tokens-and-theme-switcher](./04-design-tokens-and-theme-switcher/README.md)** | Enterprise Token Architecture | CSS Custom Properties & `@property` | OKLCH Color Palettes, RCS (`color-mix()`), High Contrast Mode, Multi-Brand Themes |
| **[05-pricing-page-with-grid](./05-pricing-page-with-grid/README.md)** | High-Conversion SaaS Pricing Matrix | CSS Subgrid & Grid Areas | Aligned Feature Checkmarks via Subgrid, Billing Frequency Toggle, Badge Overlays |
| **[06-magazine-layout](./06-magazine-layout/README.md)** | Editorial Publishing Platform | CSS Grid & Fragmentation | Multi-Column Columns (`columns`), Pull Quotes, Drop Caps (`::first-letter`), Asymmetric Spans |
| **[07-animated-components-set](./07-animated-components-set/README.md)** | Micro-Interaction Design System | GPU Transitions & `@keyframes` | Tactile Button Physics, Zero-CLS Spinners, Popover Dialog Fades, Floating Labels |
| **[08-css-only-accordion-and-tabs](./08-css-only-accordion-and-tabs/README.md)** | Native Disclosure Suite | HTML `<details name>` & CSS Grid | Native Mutex Accordion, `0fr` to `1fr` Height Transitions, Segmented Pill Controls |
| **[09-dark-mode-blog](./09-dark-mode-blog/README.md)** | High-Performance Typography Blog | Fluid Sizing & `light-dark()` | Typographic Measure (`65ch`), Text Clamping, Zero-JS System Theme Switching |
| **[10-mini-css-design-system](./10-mini-css-design-system/README.md)** | Modular Enterprise UI Toolkit | CSS Cascade Layers (`@layer`) | Reset, Tokens, Elements, Components, Utilities, Full WCAG AAA Accessibility |

---

## Architectural Principles Across All Projects

1. **Modern Baseline Conformity**: Code leverages standard, widely available features across Chrome, Firefox, and Safari.
2. **Zero Layout Shift (CLS = 0.000)**: All visual assets, skeleton containers, and dynamic buttons allocate immutable layout volume prior to data loading.
3. **Accessibility First (WCAG 2.2 AA/AAA)**: Minimum 4.5:1 text contrast (7:1 for headers), visible non-zero `:focus-visible` outlines, explicit touch target expansion to 48x48px, and complete keyboard operable flows.
4. **Markdown-Only Execution**: All implementation code is self-contained in markdown files, ensuring complete version control and repository portability.
