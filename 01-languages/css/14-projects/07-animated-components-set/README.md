# Project 07: Micro-Interaction & Animated Components Set

## Metadata
| Attribute | Details |
|:---|:---|
| **Tier** | Specialized Project |
| **Prerequisites** | [08-motion](../08-motion/README.md), [13-layout-and-component-recipes/12-buttons](../13-layout-and-component-recipes/12-buttons-badges-and-alerts.md), [13-layout-and-component-recipes/13-skeleton](../13-layout-and-component-recipes/13-skeleton-loaders-and-empty-states.md) |
| **Estimated Completion Time** | 4 Hours |
| **Target Baseline** | Modern Web Baseline (2024–2026) |
| **Core Layout Engine** | GPU Accelerated Transitions, `@keyframes`, `@starting-style` |
| **Tested With** | Playwright `css_runner.js`, axe-core |

---

## 1. Project Overview & Architecture
This project implements a production micro-interaction UI suite comprising five tactile, hardware-accelerated animated components:
1. **The Tactile Button**: Interactive spring-physics button with press downscaling (`active: scale(0.97)`), ripple sheen overlay, and keyboard focus expansion.
2. **The Zero-CLS Loading Button**: Grid-stacked button that replaces text with an SVG circular spinner without changing width or height.
3. **The Animated Floating Label Input**: Pure CSS floating label utilizing `:placeholder-shown` and sibling combinators.
4. **The Discrete Transition Modal**: Native `<dialog>` modal with `@starting-style` and `allow-discrete` zoom and fade transitions.
5. **The Shimmer Skeleton Card**: Composited `transform: translateX()` shimmer wireframe with reduced-motion static pulse fallback.

---

## 2. File Organization
1. [`01-design.md`](./01-design.md): Cubic-bezier timing tokens, spring physics curves, duration scales, and motion budgets.
2. [`02-html-and-css.md`](./02-html-and-css.md): Complete production implementation in semantic HTML and CSS.
3. [`03-measured-results.md`](./03-measured-results.md): Frame rate profiling (60fps/120fps lock), layout thrashing verification (0 reflows), and reduced-motion audit.
4. [`04-limitations-and-next-steps.md`](./04-limitations-and-next-steps.md): Compositor memory limits, discrete property inheritance, and Web Animations API integration.
