# Project 10: Mini CSS Design System & UI Toolkit

## Metadata
| Attribute | Details |
|:---|:---|
| **Tier** | Capstone Project |
| **Prerequisites** | Modules 00–13 complete |
| **Estimated Completion Time** | 6 Hours |
| **Target Baseline** | Modern Web Baseline (2024–2026) |
| **Core Layout Engine** | CSS Cascade Layers (`@layer`), Custom Properties, Modern Reset |
| **Tested With** | Playwright `css_runner.js`, axe-core |

---

## 1. Project Overview & Architecture
This capstone project implements **Apex UI**—a production-grade, framework-agnostic CSS Design System and UI Component Library structured entirely with CSS Cascade Layers (`@layer`).

The design system architecture comprises 5 cascade layers:
1. `@layer reset`: Modern reset eliminating browser quirks while preserving focus visibility.
2. `@layer tokens`: Design primitives (OKLCH colors, fluid typographic clamps, elevations, spacing).
3. `@layer base`: Semantic HTML styling (headings, paragraphs, blockquotes, lists).
4. `@layer components`: Atomic and compound UI components (buttons, badges, inputs, cards, dialogs, accordions).
5. `@layer utilities`: High-priority layout helpers (`.flex`, `.grid`, `.visually-hidden`, `.text-center`).

Because all styling resides inside ordered `@layer` statements, consumers can override any component style effortlessly without specificity battles or `!important`.

---

## 2. File Organization
1. [`01-design.md`](./01-design.md): Layer architecture specification, token taxonomy, and component matrix.
2. [`02-html-and-css.md`](./02-html-and-css.md): Complete production implementation and living component showcase page.
3. [`03-measured-results.md`](./03-measured-results.md): Layer resolution verification, specificity metrics, and accessibility test results.
4. [`04-limitations-and-next-steps.md`](./04-limitations-and-next-steps.md): Distribution strategies, npm package compilation, and TypeScript typing integrations.
