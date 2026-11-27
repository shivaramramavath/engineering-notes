# Project 04: Enterprise Design Tokens and Theme Switcher

## Metadata
| Attribute | Details |
|:---|:---|
| **Tier** | Core Project |
| **Prerequisites** | [01-fundamentals/10-colours](../01-fundamentals/10-colours.md), [01-fundamentals/11-custom-properties](../01-fundamentals/11-custom-properties.md), [09-modern-css/07-light-dark](../09-modern-css/07-light-dark-and-system-colors.md) |
| **Estimated Completion Time** | 4 Hours |
| **Target Baseline** | Modern Web Baseline (2024–2026) |
| **Core Layout Engine** | CSS Custom Properties, `@property`, OKLCH, Relative Color Syntax |
| **Tested With** | Playwright `css_runner.js`, axe-core |

---

## 1. Project Overview & Architecture
This project implements an enterprise-grade multi-brand, multi-mode Design Token Architecture.

Key features:
- **3-Tier Token Hierarchy**: Primitive Tokens (raw OKLCH color palettes, spacing units) → Semantic Tokens (purpose-driven variables like `--color-bg-canvas`, `--color-text-subtle`) → Component Tokens (scoped component variables).
- **Multi-Brand Theming**: Switching between Brand A ("FinTech Secure"), Brand B ("NeoCyberpunk"), and Brand C ("Warm Editorial") with a single `data-brand` attribute.
- **Light, Dark, and High-Contrast Modes**: Native zero-JS switching via `light-dark()` and `@media (forced-colors: active)`.
- **Typed CSS Properties with `@property`**: Animatable CSS variables with explicit types (`<color>`, `<percentage>`).

---

## 2. File Organization
1. [`01-design.md`](./01-design.md): Three-tier token schema, OKLCH scales, and brand identity matrix.
2. [`02-html-and-css.md`](./02-html-and-css.md): Complete production implementation and interactive theme previewer.
3. [`03-measured-results.md`](./03-measured-results.md): Color contrast calculations across all brand modes, token override latency, and transition smoothness.
4. [`04-limitations-and-next-steps.md`](./04-limitations-and-next-steps.md): Typed property inheritance caveats, style dictionary integration, and Figma token syncing.
