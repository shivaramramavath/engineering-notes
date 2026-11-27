# Module 08: Motion & Animations (`08-motion/`)

| Metadata | Details |
| :--- | :--- |
| **Level** | Core / Professional |
| **Prerequisites** | `01-fundamentals/`, `02-box-model-and-layout-basics/`, `07-visual-styling/` |
| **Estimated Completion Time** | 5.5 hours |
| **Status** | Complete |
| **Baseline Status** | Widely Available (`transition`, `animation` Baseline 2015; `linear()` easing Baseline 2023; Scroll-Driven Animations Baseline 2023; `@starting-style` Baseline 2024) |
| **Module Focus** | GPU Compositor Animations, Non-Linear Cubic & Spring Physics, Scroll-Driven Progress & View Timelines, Discrete Entry/Exit Transitions, View Transitions API |

---

## Overview

Motion on the web is not mere visual ornamentation; it is functional communication. Purposeful animation guides spatial attention, clarifies relationship hierarchies between screens, provides tactile feedback for user inputs, and maintains cognitive continuity during state transitions.

In modern web engineering, CSS motion encompasses cutting-edge browser graphics capabilities:
- **Compositor-Thread Execution**: Understanding the difference between cheap GPU composited transforms/opacity and catastrophic CPU layout thrashing.
- **Advanced Timing & Physics**: Moving beyond default `ease-in-out` into custom cubic-beziers and mathematical spring approximations via `linear()` easing (Baseline 2023).
- **Scroll-Driven Animations (Baseline 2023)**: Driving animations directly from scroll position (`scroll()` and `view()`) on the compositor thread without a single line of JavaScript scroll listener code.
- **Animating to and from `display: none` (Baseline 2024)**: Utilizing `@starting-style` and `transition-behavior: allow-discrete` to animate modal dialogs, menus, and popovers entering and exiting the DOM.
- **View Transitions**: Seamless cross-page and single-page application morphing via `@view-transition` and pseudo-element snapshots.
- **Universal Accessibility (Vestibular Safety)**: Honoring `@media (prefers-reduced-motion: reduce)` while preserving JavaScript `transitionend` event execution.

---

## Module Roadmap

| Note | Title | Focus & Core Mechanics | Est. Time |
| :--- | :--- | :--- | :--- |
| **[01](01-transitions.md)** | **CSS Transitions** | `transition` shorthand, sub-properties (`property`, `duration`, `timing-function`, `delay`), multiple transitions, state toggling. | 25 min |
| **[02](02-animations-and-keyframes.md)** | **Keyframes & Complex Animations** | `@keyframes`, animation shorthand, `iteration-count`, `direction`, `fill-mode` (`forwards`, `backwards`), play states. | 26 min |
| **[03](03-timing-functions-and-linear-easing.md)** | **Timing Functions & Mathematical Easing** | `cubic-bezier()`, `steps()`, `linear()` function for complex spring and bounce curves, physical momentum. | 28 min |
| **[04](04-animation-performance.md)** | **Animation Performance & 60/120fps** | GPU compositor layers, pipeline bottlenecks (Layout/Paint vs Composite), `will-change` lifecycle, DevTools Performance profiler. | 28 min |
| **[05](05-scroll-driven-animations.md)** | **Scroll-Driven Animations** | Modern standard: `animation-timeline: scroll()`, `view()`, scroll progress bars, entrance/exit reveal timelines. | 30 min |
| **[06](06-view-transitions-css.md)** | **CSS View Transitions** | Cross-document & SPA morphing, `@view-transition`, `::view-transition-old/new`, shared element transitions. | 28 min |
| **[07](07-starting-style-and-discrete-animation.md)** | **`@starting-style` & Discrete Animations** | Modern milestone (Baseline 2024): Animating `display: none` to `display: block`, `transition-behavior: allow-discrete`, top-layer entry/exit. | 28 min |
| **[08](08-reduced-motion-and-accessible-animation.md)** | **Accessible Motion & `prefers-reduced-motion`**| Vestibular disorders, safe motion alternatives (opacity vs spatial), 0.01ms duration pattern for `transitionend` preservation. | 24 min |
| **[09](09-loading-spinners-and-micro-interactions.md)** | **Loading Spinners & Micro-Interactions** | Pure CSS loaders, pulsating skeleton screens, button success morphs, ripple physics. | 25 min |

---

## Learning Objectives

By completing this module, you will:
1. **Architect High-Performance 60/120fps Motion**: Animate strictly across GPU-promoted compositor properties (`transform`, `opacity`) without layout reflows.
2. **Deploy Pure CSS Scroll-Driven Timelines**: Build scroll progress bars and element reveal animations using native `scroll()` and `view()` timelines.
3. **Animate `display: none` Elements**: Master Baseline 2024 `@starting-style` and discrete transitions to animate popovers and dialogs into the DOM.
4. **Author Natural Physics with `linear()`**: Construct realistic elastic, spring, and bounce easing curves without JavaScript physics engines.
5. **Guarantee Vestibular Accessibility**: Protect users with motion sensitivities while maintaining robust JavaScript event lifecycle integration.

---

[Previous Module: 07-visual-styling](file:///e:/Learning/engineering-notes/01-languages/css/07-visual-styling/README.md) | [Track Index](file:///e:/Learning/engineering-notes/01-languages/css/INDEX.md) | [Next Module: 09-modern-css](file:///e:/Learning/engineering-notes/01-languages/css/09-modern-css/README.md)
