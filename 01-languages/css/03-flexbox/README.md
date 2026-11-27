# Module 03: Flexbox Deep Dive (`03-flexbox/`)

| Metadata | Details |
| :--- | :--- |
| **Level** | Core / Professional |
| **Prerequisites** | `02-box-model-and-layout-basics/` |
| **Estimated Completion Time** | 3.5 hours |
| **Status** | Complete |
| **Baseline Status** | Widely Available across all modern browsers (Flexbox Baseline 2015, Flex Gap Baseline 2021) |
| **Module Focus** | 1-Dimensional Flow, Dynamic Space Distribution, Alignment, and Production UI Patterns |

---

## Overview

The **CSS Flexible Box Layout Module (Flexbox)** revolutionized component-level layout on the web. Before Flexbox, distributing space evenly, vertically centering elements, and building dynamic toolbars required fragile float clearances, inline-block whitespace hacks, or rigid table display hacks.

Flexbox provides a one-dimensional layout model designed specifically for component UI architecture:
- Managing content along a **Main Axis** and a **Cross Axis**.
- Intelligently growing and shrinking items based on available container space.
- Distributing spare space cleanly using modern Box Alignment and `gap`.
- Auto-margin space absorption for intuitive split toolbars.

This module provides exhaustive, production-grade mastery of Flexbox, from fundamental mental models to the browser engine's internal mathematical sizing algorithms.

---

## Module Roadmap

| Note | Title | Focus & Core Mechanics | Est. Time |
| :--- | :--- | :--- | :--- |
| **[01](01-flexbox-mental-model.md)** | **Flexbox Mental Model** | 1D layout paradigm, main vs cross axis, establishing a Flex Formatting Context (FFC), container vs item separation. | 22 min |
| **[02](02-flex-container-properties.md)** | **Flex Container Properties** | `flex-direction` (`row`, `column`, `-reverse`), `flex-wrap`, `flex-flow` shorthand, writing mode and direction effects. | 20 min |
| **[03](03-flex-item-properties.md)** | **Flex Item Properties** | `order`, `align-self`, the `flex` shorthand rules, default values (`0 1 auto` vs `1 1 0%`). | 24 min |
| **[04](04-alignment-and-gap.md)** | **Alignment and Gap** | `justify-content`, `align-items`, `align-content`, `gap`, `row-gap`, `column-gap`, and `margin: auto` space absorption. | 22 min |
| **[05](05-wrapping-and-ordering.md)** | **Wrapping and Ordering** | Multi-line flex layout, line breaking mechanics, independent cross-axis lines, DOM order vs visual order accessibility. | 20 min |
| **[06](06-flexbox-sizing-algorithm-deep-dive.md)** | **Flexbox Sizing Algorithm Deep Dive** | Base size, hypothetical main size, positive free space distribution (`flex-grow`), negative free space shrinkage (`flex-shrink`), flex factor math. | 28 min |
| **[07](07-flexbox-patterns.md)** | **Common Flexbox UI Patterns** | Media object, split navbar, sticky footer layout, centered cards, wrapping chip clouds, dynamic input button groups. | 25 min |
| **[08](08-flexbox-pitfalls.md)** | **Flexbox Pitfalls & Edge Cases** | The `min-width: auto` overflow trap, collapsed image aspect ratios, nested flex scroll containers, button flex bugs. | 25 min |

---

## Learning Objectives

By completing this module, you will:
1. **Master the Dual-Axis Model**: Effortlessly switch between `row` and `column` flows while predicting how `justify-*` and `align-*` swap directions.
2. **Calculate Space Distribution Mathematically**: Know precisely how the browser computes `flex-grow` proportional expansion and `flex-shrink * flex-basis` scaled shrinkage.
3. **Eradicate Flex Layout Bugs**: Instantly identify and fix the infamous `min-width: auto` child blowout that causes horizontal overflow in responsive flex layouts.
4. **Build Accessible Component Systems**: Understand why visual reordering via `order` or `flex-direction: row-reverse` can severely impair keyboard and screen-reader accessibility if DOM ordering is neglected.

---

[Previous Module: 02-box-model-and-layout-basics](file:///e:/Learning/engineering-notes/01-languages/css/02-box-model-and-layout-basics/README.md) | [Track Index](file:///e:/Learning/engineering-notes/01-languages/css/INDEX.md) | [Next Module: 04-grid](file:///e:/Learning/engineering-notes/01-languages/css/04-grid/README.md)
