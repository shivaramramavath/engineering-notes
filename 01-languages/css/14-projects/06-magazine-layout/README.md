# Project 06: High-End Editorial Magazine Layout

## Metadata
| Attribute | Details |
|:---|:---|
| **Tier** | Core Project |
| **Prerequisites** | [06-typography-and-text](../06-typography-and-text/README.md), [04-grid](../04-grid/README.md), [07-visual-styling/06-clip-path-and-masks](../07-visual-styling/06-clip-path-and-masks.md) |
| **Estimated Completion Time** | 4 Hours |
| **Target Baseline** | Modern Web Baseline (2024–2026) |
| **Core Layout Engine** | CSS Multi-Column Layout (`columns`), CSS Grid & Drop Caps |
| **Tested With** | Playwright `css_runner.js`, axe-core |

---

## 1. Project Overview & Architecture
This project implements an editorial publication and architectural review feature article layout inspired by *The New Yorker*, *Wired*, and *Monocle*.

Key architectural features:
- **Full-Bleed Cover Header**: Large-scale typographic masthead with fluid editorial clamp headers and author bylines.
- **Drop Caps & Stylized Lead Paragraph**: Decorated initial letter using `::first-letter` and `initial-letter: 3 2` (or float fallback).
- **Multi-Column Flow with CSS Columns**: Flowing continuous text across responsive column tracks (`columns: 2 320px; column-gap: 3rem; column-rule: 1px solid var(--border);`).
- **Asymmetric Full-Width Pull Quotes**: Quotes spanning across multi-column boundaries (`column-span: all`) with quotation marks and italicized citations.
- **Editorial Figures & Inline Callouts**: Figures with captioned architectural blueprints and contextual sidebars.

---

## 2. File Organization
1. [`01-design.md`](./01-design.md): Editorial typographic scale, baseline grids, serif pairings, and column measures.
2. [`02-html-and-css.md`](./02-html-and-css.md): Complete semantic production implementation.
3. [`03-measured-results.md`](./03-measured-results.md): Typographic line length measurements (measure), column balances, and viewport reflow tests.
4. [`04-limitations-and-next-steps.md`](./04-limitations-and-next-steps.md): Column balancing quirks, fragmentation breaks (`break-inside: avoid`), and browser support for `initial-letter`.
