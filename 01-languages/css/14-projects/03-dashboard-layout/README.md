# Project 03: Cloud Operations Dashboard Shell

## Metadata
| Attribute | Details |
|:---|:---|
| **Tier** | Core Project |
| **Prerequisites** | [04-grid](../04-grid/README.md), [13-layout-and-component-recipes/05-dashboard-layouts](../13-layout-and-component-recipes/05-dashboard-layouts.md), [13-layout-and-component-recipes/10-responsive-tables](../13-layout-and-component-recipes/10-responsive-tables-recipes.md) |
| **Estimated Completion Time** | 5 Hours |
| **Target Baseline** | Modern Web Baseline (2024–2026) |
| **Core Layout Engine** | Multi-Tier CSS Grid Shell & Subgrid |
| **Tested With** | Playwright `css_runner.js`, axe-core |

---

## 1. Project Overview & Architecture
This project implements an enterprise-grade cloud operations dashboard shell for monitoring distributed Kubernetes clusters and network telemetries.

Key architectural features:
- **`100dvh` Zero-Document Scroll Root**: Fixed application shell pinning the Top Application Bar (64px) and Primary Navigation Rail (260px desktop, 72px collapsed, off-canvas mobile).
- **Subgrid Synchronized Metric Cards**: 4-column metric ribbon where card headers, key performance indicators, and trend badges maintain synchronized baselines.
- **Scrollable Data Table with Freeze Panes**: Multi-column cluster telemetry matrix with sticky `<thead>` and sticky primary cluster ID column.
- **Contextual Side Drawer**: Off-canvas filter and alert panel triggered via the native HTML Popover API with zero JavaScript.

---

## 2. File Organization
1. [`01-design.md`](./01-design.md): System layout geometry, track definitions, color tokens, and elevation levels.
2. [`02-html-and-css.md`](./02-html-and-css.md): Complete semantic production implementation.
3. [`03-measured-results.md`](./03-measured-results.md): Real computed dimensions, independent scroll verification, and zero layout shift checks.
4. [`04-limitations-and-next-steps.md`](./04-limitations-and-next-steps.md): Performance under large DOM datasets, virtual scrolling boundaries, and container query upgrades.
