# Module 07: Visual Styling (`07-visual-styling/`)

| Metadata | Details |
| :--- | :--- |
| **Level** | Core / Professional |
| **Prerequisites** | `01-fundamentals/`, `02-box-model-and-layout-basics/`, `06-typography-and-text/` |
| **Estimated Completion Time** | 6.5 hours |
| **Status** | Complete |
| **Baseline Status** | Widely Available (`background-*`, `box-shadow`, `border-radius`, `transform` Baseline 2015; `backdrop-filter` Baseline 2020; `scrollbar-color/width` Baseline 2024) |
| **Module Focus** | Advanced CSS Painting, Gradients & Color Interpolation, Glassmorphism & Blend Modes, Clipping & Masking Geometry, Hardware-Accelerated 3D Transforms |

---

## Overview

Visual styling defines the sensory depth, texture, elevation, and tactile polish of modern web applications. Moving far beyond flat background colors and simple 1px borders, modern visual engineering bridges raw mathematical geometry with GPU compositing hardware:

- **Complex Gradients & Color Spaces**: Conic, radial, linear, and mesh gradients evaluated across wide-gamut color spaces (`oklch`, `display-p3`) with non-linear interpolation easing.
- **Elevation & Volumetric Shadows**: Multi-layered ambient and directional box shadows, inset bevels, and atmospheric depth without rasterized imagery.
- **Glassmorphism & Optical Filters**: Hardware-accelerated backdrop blur (`backdrop-filter: blur()`), contrast boosting, saturation shifts, and saturation adjustments with GPU compositor isolation.
- **Vector Clipping & Masking**: Non-rectangular layouts, organic hero dividers, and dynamic geometric masks via `clip-path` and `mask-image`.
- **Hardware-Accelerated 3D Transforms**: Spatial perspective, 3D card flips, isometric interfaces, and matrix transformations that execute on dedicated GPU render layers.
- **Polished Micro-Surfaces**: Cross-browser custom scrollbars (`scrollbar-gutter`, `scrollbar-width`), custom interactive form controls (`:focus-visible`, `accent-color`), and accessible SVG styling.

---

## Module Roadmap

| Note | Title | Focus & Core Mechanics | Est. Time |
| :--- | :--- | :--- | :--- |
| **[01](01-backgrounds-and-gradients.md)** | **Backgrounds & Modern Gradients** | Multi-background stacking, `background-attachment: local`, linear/radial/conic gradients, color stops, color interpolation spaces. | 28 min |
| **[02](02-borders-radius-and-outlines.md)** | **Borders, Corner Radii & Outlines** | Elliptical corner radii, `border-image`, outline offsets, accessible `:focus-visible` rings, WHCM border defense. | 25 min |
| **[03](03-box-shadows-and-text-shadows.md)** | **Box Shadows & Text Shadows** | Physics of elevation, multi-layer ambient + key light shadows, inset bevels, neon glows, performance impacts of large blur radii. | 25 min |
| **[04](04-filters-and-backdrop-filter.md)** | **Filters & Glassmorphism** | `filter` vs `backdrop-filter`, blur, brightness, contrast, drop-shadow vs box-shadow, GPU compositing layer overhead. | 26 min |
| **[05](05-blend-modes-and-isolation.md)** | **Blend Modes & Compositor Isolation** | `mix-blend-mode`, `background-blend-mode`, mathematical blending algorithms (multiply, screen, overlay), `isolation: isolate`. | 24 min |
| **[06](06-clip-path-and-masks.md)** | **Clip Path & CSS Masks** | Vector clipping (`polygon`, `circle`, `path()`), alpha & luminance masking (`mask-image`), non-rectangular layouts. | 26 min |
| **[07](07-transforms-2d-3d.md)** | **Hardware Transforms (2D & 3D)** | `transform`, individual transform properties (`translate`, `rotate`, `scale`), `perspective`, `transform-style: preserve-3d`, backface visibility. | 30 min |
| **[08](08-lists-and-counters.md)** | **Lists, Markers & CSS Counters** | `::marker` styling, multi-level nested counters (`counter-reset`, `counter-increment`), accessible roman/hierarchical numbering. | 22 min |
| **[09](09-styling-forms-and-inputs.md)** | **Form Controls & Input Styling** | Custom checkboxes, radio toggles, range sliders, `accent-color`, floating labels, `:user-invalid` validation states. | 28 min |
| **[10](10-scrollbars-and-scroll-behaviour.md)** | **Scrollbars & Scroll Behavior** | Standard CSS scrollbars (`scrollbar-color`, `scrollbar-width`), `scrollbar-gutter: stable`, smooth scroll behavior. | 22 min |
| **[11](11-cursors-and-pointer-events.md)** | **Cursors & Pointer Events** | Custom SVG cursors, grab/grabbing physics, `pointer-events: none` hit-testing pass-throughs, touch action ergonomics. | 20 min |
| **[12](12-images-svg-and-css.md)** | **Image Processing & SVG in CSS** | `object-fit`, `object-position`, SVG data-URIs, coloring SVG masks with CSS variables, image rendering algorithms. | 24 min |

---

## Learning Objectives

By completing this module, you will:
1. **Architect Multi-Layered Elevation Systems**: Construct realistic physical depth using layered ambient and directional shadows rather than harsh single-blur drops.
2. **Implement Performance-Safe Glassmorphism**: Deploy frosted glass interfaces using `backdrop-filter` while isolating stacking contexts to avoid layout repainting.
3. **Master Non-Rectangular Layouts**: Slice geometric card headers, angled section dividers, and polygon masks with mathematical `clip-path`.
4. **Construct 3D Spatial Transforms**: Build card flipping and isometric grid systems using 3D perspective, matrix math, and `preserve-3d`.
5. **Style Native Form Controls Accessibly**: Build custom accessible inputs, toggles, and range sliders that honor user focus rings and high-contrast settings.

---

[Previous Module: 06-typography-and-text](file:///e:/Learning/engineering-notes/01-languages/css/06-typography-and-text/README.md) | [Track Index](file:///e:/Learning/engineering-notes/01-languages/css/INDEX.md) | [Next Module: 08-motion](file:///e:/Learning/engineering-notes/01-languages/css/08-motion/README.md)
