# Module 12: Performance & Debugging

Welcome to **Module 12: Performance & Debugging** of the authoritative CSS curriculum. This module covers the physics and runtime engineering of CSS inside modern browser rendering engines (Chromium Blink, Mozilla Gecko, Apple WebKit), performance optimization strategies, Core Web Vitals optimization, and professional DevTools debugging workflows.

---

## 1. Module Overview

CSS is not merely an aesthetic layout language; it is an active participant in browser main-thread scheduling, frame budgets (16.6ms for 60fps, 8.3ms for 120fps), network delivery bottlenecks, and interaction responsiveness.

Suboptimal CSS choices cause severe user-perceived performance degradations:
- Heavy render-blocking stylesheets delay **First Contentful Paint (FCP)** and **Largest Contentful Paint (LCP)**.
- Unconstrained DOM reflows and layout thrashing spike **Interaction to Next Paint (INP)**.
- Missing container aspect ratios or font metric mismatches cause violent layout jumps, penalizing **Cumulative Layout Shift (CLS)**.
- Expensive selector chains and uncontained paint trees overwhelm low-power mobile CPUs and drain device batteries.

This module equips you with the mental models, profiling tools, and production techniques required to build blazing fast, 120fps web applications.

---

## 2. Directory Structure & Notes

| Note | Title | Primary Focus |
| :--- | :--- | :--- |
| **[01-css-performance-myths-and-realities.md](01-css-performance-myths-and-realities.md)** | CSS Performance: Myths vs Realities | Selector performance in 2026 vs layout thrashing; what actually moves the needle in real-world applications. |
| **[02-render-blocking-and-critical-css.md](02-render-blocking-and-critical-css.md)** | Render Blocking & Critical CSS | How browsers parse stylesheets, the 14KB TCP initial congestion window, inlining critical CSS, async CSS loading. |
| **[03-layout-paint-and-composite-costs.md](03-layout-paint-and-composite-costs.md)** | Layout, Paint & Composite Costs | The 3-stage rendering pipeline, reflow triggers, repaint triggers, GPU composited properties (`transform`, `opacity`). |
| **[04-containment-and-content-visibility.md](04-containment-and-content-visibility.md)** | Containment & Content Visibility | `contain: layout paint size style`, `content-visibility: auto`, `contain-intrinsic-size`, zero-JS DOM virtualization. |
| **[05-file-size-and-delivery.md](05-file-size-and-delivery.md)** | File Size, Minification & Delivery | Compression algorithms (Brotli vs Gzip), LightningCSS, HTTP/2 & HTTP/3 multiplexing, CDN caching, asset hashing. |
| **[06-debugging-workflow-in-devtools.md](06-debugging-workflow-in-devtools.md)** | Professional DevTools Workflows | Elements style pane, computed properties, forced pseudo-states, Performance profiler, Rendering panel, CSS Coverage. |
| **[07-common-layout-bugs-and-fixes.md](07-common-layout-bugs-and-fixes.md)** | Common Layout Bugs & Fixes | Margin collapse, unexpected horizontal scrollbars, z-index stacking failures, flexbox minimum content sizing. |
| **[08-cross-browser-strategy.md](08-cross-browser-strategy.md)** | Cross-Browser Strategy & Baseline | Browser engines (Blink, Gecko, WebKit), caniuse/Baseline data, `@supports`, graceful degradation, progressive enhancement. |
| **[09-css-testing-and-stylelint.md](09-css-testing-and-stylelint.md)** | CSS Testing & Automated Quality | Stylelint configuration, visual regression testing (Playwright), AST validation, CI pipeline enforcement. |
| **[10-css-metrics-and-core-web-vitals.md](10-css-metrics-and-core-web-vitals.md)** | CSS Metrics & Core Web Vitals | Optimizing FCP, LCP, INP, and CLS; layout shift debugging, font fallback metric overrides (`size-adjust`), performance budgets. |

---

## 3. The Browser Rendering Pipeline

```
┌─────────────────────────────────────────────────────────────┐
│                 BROWSER RENDERING ENGINE                    │
└─────────────────────────────────────────────────────────────┘
  1. PARSE & CONSTRUCT:
     HTML ──► DOM Tree   ┐
                         ├─► Render Tree (Matched Styles)
     CSS  ──► CSSOM Tree ┘
               │
               ▼
  2. LAYOUT / REFLOW (Most Expensive! Computes box geometries: X, Y, W, H):
     Triggered by: width, height, margin, padding, font-size, top, left, etc.
               │
               ▼
  3. PAINT / REPAINT (Paints pixels to bitmap layers):
     Triggered by: color, background-color, box-shadow, border-radius, outline.
               │
               ▼
  4. COMPOSITE (Zero CPU layout cost! Direct GPU hardware execution):
     Triggered by: transform, opacity, filter (on dedicated compositor layers).
```

---

## 4. Learning Objectives

By completing this module, you will be able to:
1. Distinguish between theoretical micro-optimizations (selector matching) and macro-optimizations (layout reflows, critical CSS, and bundle delivery).
2. Eliminate render-blocking stylesheet latency using critical CSS inlining and non-blocking asynchronous links.
3. Architect animations and micro-interactions that strictly operate in the **Composite** pipeline stage, avoiding layout thrashing.
4. Accelerate initial rendering of large pages by up to 700% using `content-visibility: auto` and CSS containment.
5. Profile, diagnose, and resolve subtle layout bugs, z-index stacking context confusion, and unwanted scrollbars in Chrome/Firefox DevTools.
6. Achieve green Core Web Vitals scores (LCP < 2.5s, INP < 200ms, CLS < 0.1) through targeted CSS optimizations.

---

| Previous Module | Next Module |
| :--- | :--- |
| [Module 11: Accessibility & UX in CSS](../11-accessibility-and-ux-in-css/README.md) | [Module 13: Layout & Component Recipes](../13-layout-and-component-recipes/README.md) |
