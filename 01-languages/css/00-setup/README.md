# Module 00: Setup & Browser Engine Architecture

Welcome to **Module 00: Setup & Browser Engine Architecture**. Cascading Style Sheets (CSS) is not merely a collection of cosmetic visual properties; it is a declarative, rule-based layout language operating inside a high-performance browser rendering pipeline.

This foundational module deconstructs how browser engines (Blink in Chrome/Edge, Gecko in Firefox, WebKit in Safari) parse stylesheet text into the **CSSOM (CSS Object Model)**, resolve the **Cascade**, calculate geometry during **Layout (Reflow)**, rasterize pixels during **Paint**, and hand off layers to the GPU during **Compositing**.

---

## Module Roadmap & Navigation

| # | Note | Focus Area | Core Concepts & Tools |
|:---:|:---|:---|:---|
| **01** | [What is CSS & The Cascade Idea](01-what-is-css-and-the-cascade-idea.md) | Philosophy & Architecture | The cascade mental model, Håkon Wium Lie proposal, separation of concerns, declarative rules |
| **02** | [How the Browser Renders CSS](02-how-the-browser-renders-css.md) | Browser Internals | Critical Rendering Path (CRP), CSSOM construction, Render Tree, Layout, Paint, Compositing |
| **03** | [Ways to Add CSS](03-ways-to-add-css.md) | Integration & Loading | `<link rel="stylesheet">`, `<style>`, inline styles, `@import` performance trap, HTTP/2 multiplexing |
| **04** | [DevTools for CSS](04-devtools-for-css.md) | Diagnostics & Profiling | Elements panel, Styles vs Computed pane, Box Model viewer, Layout overlay, Layers & Rendering panel |
| **05** | [Tooling: Stylelint, Prettier & Lightning CSS](05-tooling-stylelint-prettier-lightningcss.md) | Modern Toolchain | Linting rules (stylelint-config-standard), formatting, PostCSS, Lightning CSS (Rust-based bundling) |
| **06** | [CSS Modules, Spec Levels & Baseline](06-css-modules-levels-and-baseline.md) | Standards & Evolution | W3C CSSWG process, Rec levels, CSS Snapshot, Web Platform Baseline (Widely vs Newly Available) |

---

## Key Learning Outcomes

1. **Internalize the Browser Pipeline**: Understand why altering `transform` runs at 120 FPS on the compositor thread while modifying `top` or `width` forces expensive synchronous layout reflows on the main thread.
2. **Master the Cascade Concept**: Grasp why CSS was designed with user-agent, author, and user origins competing through an inheritance tree rather than a single procedural style script.
3. **Avoid the `@import` Bottleneck**: Understand why `@import` creates sequential waterfall download delays and learn the optimal resource loading strategies (`preload`, non-blocking print styles).
4. **Harness Modern DevTools**: Use Chrome, Firefox, and Safari DevTools to inspect computed properties, visualize grid/flex lines, debug stacking contexts, and emulate user preferences (`prefers-reduced-motion`, `forced-colors`).
5. **Navigate the Web Platform Baseline**: Distinguish between widely available, newly available, and experimental CSS capabilities.

---

[Next: 01 - What is CSS & The Cascade Idea ->](01-what-is-css-and-the-cascade-idea.md)
