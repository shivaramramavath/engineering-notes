# Project 09: High-Performance Technical Blog with Native Dark Mode

## Metadata
| Attribute | Details |
|:---|:---|
| **Tier** | Core Project |
| **Prerequisites** | [06-typography-and-text](../06-typography-and-text/README.md), [09-modern-css/07-light-dark](../09-modern-css/07-light-dark-and-system-colors.md), [11-accessibility-and-ux-in-css/08-readable-line-length](../11-accessibility-and-ux-in-css/08-readable-line-length-and-spacing.md) |
| **Estimated Completion Time** | 4 Hours |
| **Target Baseline** | Modern Web Baseline (2024–2026) |
| **Core Layout Engine** | Fluid Typographic Measure (`65ch`), CSS `light-dark()` & System Colors |
| **Tested With** | Playwright `css_runner.js`, axe-core |

---

## 1. Project Overview & Architecture
This project implements an authoritative, publication-grade technical blog for software architects.

Key architectural features:
- **Zero-JS Theme Adaptation via `light-dark()`**: The entire color scheme synchronizes with the user's OS preference (`color-scheme: light dark`) without class toggles or flash of unstyled theme (FOUC).
- **Golden Measure Typographic Layout**: Caps article reading width at `65ch` with centered margins, proportional vertical rhythm (`margin-trim: block`), and unitless line heights.
- **Syntactic Code Highlighting Blocks**: Preformatted code fences with dual-theme color coordination, line numbering, and mobile horizontal scroll defense.
- **Balanced Headlines**: Headers styled with `text-wrap: balance` to eliminate single-word widows automatically across all device widths.

---

## 2. File Organization
1. [`01-design.md`](./01-design.md): Typographic scale, line length calculations, and dual-mode color tokens.
2. [`02-html-and-css.md`](./02-html-and-css.md): Complete semantic production implementation.
3. [`03-measured-results.md`](./03-measured-results.md): Character count per line measurements, contrast audits in light and dark modes, and FOUC prevention checks.
4. [`04-limitations-and-next-steps.md`](./04-limitations-and-next-steps.md): Font loading optimization, reading progress indicators, and table of contents sticky positioning.
