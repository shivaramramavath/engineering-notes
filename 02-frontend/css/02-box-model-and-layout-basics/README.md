# Module 02: Box Model & Layout Basics

Welcome to **Module 02: Box Model & Layout Basics**. In CSS, everything is a box. Whether rendering a tiny text glyph, an interactive button, or a multi-column application grid, browser rendering engines model every DOM element as a nested rectangular box consisting of **Content**, **Padding**, **Border**, and **Margin**.

This module deconstructs how browser layout engines calculate geometric boundaries, why `box-sizing: border-box` is an essential universal reset, how margin collapse works algorithmically, how Block Formatting Contexts (BFC) isolate floating and clearing, and how modern CSS Positioning (`sticky`, `fixed`, `absolute`) and Stacking Contexts govern spatial 3D rendering.

---

## Module Roadmap & Navigation

| # | Note | Focus Area | Core Concepts & Capabilities |
|:---:|:---|:---|:---|
| **01** | [The Box Model: Content, Padding, Border, Margin](01-the-box-model.md) | Geometric Anatomy | The 4 concentric boxes, edge boundaries, dimension calculation, DevTools box inspection |
| **02** | [Box Sizing: `content-box` vs `border-box`](02-box-sizing-content-vs-border.md) | Sizing Models | W3C default vs modern `border-box` reset, padding expansion traps, fluid responsiveness |
| **03** | [Margin & Padding In-Depth](03-margin-and-padding.md) | Spacing Systems | Spacing systems, negative margins, `margin: auto` centering, percentage calculation context |
| **04** | [Margin Collapsing: Rules & Workarounds](04-margin-collapsing.md) | Layout Algorithm | Sibling collapse, parent-child collapse, empty block collapse, prevention via BFC/padding |
| **05** | [The `display` Property](05-display-property.md) | Display Model | `none`, `block`, `inline`, `inline-block`, modern two-value display syntax (`display: block flex`) |
| **06** | [Block vs Inline Formatting Contexts (BFC / IFC)](06-inline-vs-block-formatting-contexts.md) | Formatting Contexts | BFC establishment (`display: flow-root`, `overflow: hidden`), margin containment, float containment |
| **07** | [Normal Flow & Document Order](07-normal-flow.md) | Flow Layout | Block-level flow, inline-level flow, source order vs visual order, DOM tree synchronization |
| **08** | [Positioning: `static`, `relative`, `absolute`, `fixed`](08-positioning-static-relative-absolute-fixed.md) | Coordinate Systems | Containing blocks, offset properties (`top`, `right`, `bottom`, `left`, `inset`), removal from flow |
| **09** | [Sticky Positioning (`position: sticky`)](09-sticky-positioning.md) | Scroll Mechanics | Hybrid relative/fixed scrolling, scrolling containers, `overflow` containment trap, threshold offsets |
| **10** | [Stacking Contexts & `z-index`](10-stacking-contexts-and-z-index.md) | 3D Rendering | Painting order, stacking context creation triggers (`opacity`, `transform`, `isolation`), z-index wars |
| **11** | [Overflow & Clipping](11-overflow-and-clipping.md) | Content Bounds | `visible`, `hidden`, `clip`, `scroll`, `auto`, modern `overflow-x` / `overflow-y`, scroll chaining |
| **12** | [Logical Properties & Writing Modes](12-logical-properties-and-writing-modes.md) | Internationalization | `inline` vs `block` axes, `margin-inline`, `padding-block`, RTL layouts, vertical writing modes |
| **13** | [Box Alignment Basics](13-box-alignment-basics.md) | Universal Alignment | W3C Box Alignment Level 3, `align-content`, `justify-content`, centering in block containers |

---

## Key Learning Outcomes

1. **Master the Box Model Algorithm**: Understand why `box-sizing: border-box` prevents horizontal scrollbars and layout breaking.
2. **Predict and Control Margin Collapse**: Recognize the three conditions under which vertical margins collapse and tame them using modern `display: flow-root`.
3. **Architect Robust Sticky Headers**: Implement `position: sticky` headers that never detach unexpectedly due to rogue parent `overflow` properties.
4. **Eliminate `z-index` Arms Races**: Grasp how stacking contexts encapsulate child `z-index` numbers and use `isolation: isolate` to prevent modal leaks.
5. **Author Future-Proof Logical Layouts**: Replace physical directional properties (`left`, `right`) with writing-mode-aware logical properties (`inline-start`, `inline-end`) for effortless internationalization.

---

[Previous Module: 01 - Fundamentals & Core Syntax](../01-fundamentals/README.md) | [Next: 01 - The Box Model](01-the-box-model.md)
