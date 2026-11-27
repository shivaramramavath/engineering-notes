# Module 06: Typography & Text (`06-typography-and-text/`)

| Metadata | Details |
| :--- | :--- |
| **Level** | Core / Professional |
| **Prerequisites** | `01-fundamentals/`, `02-box-model-and-layout-basics/`, `05-responsive-design/` |
| **Estimated Completion Time** | 5.5 hours |
| **Status** | Complete |
| **Baseline Status** | Widely Available (`@font-face`, `font-*`, `line-height`, `letter-spacing` Baseline 2015; Variable Fonts Baseline 2020; `text-wrap: balance/pretty` Baseline 2023) |
| **Module Focus** | Font Engine Mechanics, Variable Fonts & OpenType Features, Vertical Rhythm, Modern Wrapping Algorithms, Internationalization & Font Performance |

---

## Overview

Typography is the foundational medium of the web. Over 90% of user interface interactions involve reading and processing text. In modern CSS engineering, typography extends far beyond picking a system font and setting `font-size: 16px`:

- **Font Engine & Metrics Mechanics**: Understanding how browsers rasterize glyphs, compute em-boxes, align baselines, and calculate the actual line box geometry.
- **Modern Web Font Loading**: Eliminating Cumulative Layout Shift (CLS) and Flash of Invisible/Unstyled Text (FOIT/FOUT) through `@font-face`, `font-display`, `size-adjust`, and `ascent-override`.
- **Variable Fonts**: Consolidating dozens of individual font files into a single multi-axis binary with dynamic control over weight (`wght`), width (`wdth`), slant (`slnt`), and custom parametric axes.
- **Vertical Rhythm & Leading**: Mastering unitless `line-height`, baseline grids, cap-height alignment, and mathematical scale systems.
- **Modern Text Wrapping Algorithms**: Eliminating typographic "widows" and awkward breaks using `text-wrap: balance` (Baseline 2023) and `text-wrap: pretty` (Baseline 2024).
- **Global Internationalization**: Writing modes (`writing-mode: vertical-rl`), bidirectional text (`direction: rtl`), and script-specific font fallbacks.
- **Font Delivery & Core Web Vitals**: Zero-CLS font matching, WOFF2 compression, glyph subsetting, and preload strategies.

---

## Module Roadmap

| Note | Title | Focus & Core Mechanics | Est. Time |
| :--- | :--- | :--- | :--- |
| **[01](01-font-properties.md)** | **Font Properties & Shorthand Engine** | `font-family`, `font-size`, `font-weight`, `font-style`, `font-stretch`, `font-variant`, and parsing mechanics of the `font` shorthand. | 25 min |
| **[02](02-web-fonts-and-font-display.md)** | **Web Fonts & Font Display** | `@font-face` syntax, WOFF2 vs WOFF, `font-display` states (`swap`, `optional`, `block`, `fallback`), preventing FOIT/FOUT. | 25 min |
| **[03](03-variable-fonts.md)** | **Variable Fonts & OpenType Axes** | Standard axes (`wght`, `wdth`, `slnt`, `ital`, `opsz`), custom axes, `font-variation-settings`, fluid typography animations. | 28 min |
| **[04](04-text-properties-and-spacing.md)** | **Text Properties & Spacing** | `text-align`, `text-decoration`, `text-transform`, `letter-spacing`, `word-spacing`, `text-indent`, OpenType ligatures and kerning. | 24 min |
| **[05](05-line-height-and-readability.md)** | **Line Height & Vertical Rhythm** | Unitless `line-height` calculation, half-leading distribution, em-box anatomy, baseline alignment, vertical cadence. | 25 min |
| **[06](06-text-wrap-and-hyphenation.md)** | **Text Wrapping & Hyphenation** | `text-wrap: balance` vs `pretty` vs `wrap`, `hyphens: auto`, `overflow-wrap: anywhere`, `word-break: break-word`. | 24 min |
| **[07](07-writing-modes-and-internationalisation.md)** | **Writing Modes & Internationalization** | `writing-mode` (`horizontal-tb`, `vertical-rl`, `vertical-lr`), `direction: rtl`, `unicode-bidi`, logical typographic properties. | 26 min |
| **[08](08-text-overflow-and-clamping.md)** | **Text Overflow & Multi-Line Clamping** | Single-line `text-overflow: ellipsis`, modern multi-line line-clamp (`-webkit-line-clamp` & standard `line-clamp`), scroll indicators. | 22 min |
| **[09](09-icon-fonts-vs-svg.md)** | **Icon Fonts vs Inline & External SVG** | Why icon fonts are deprecated in modern systems, visual blurring, screen reader failure modes, accessible inline SVG patterns. | 22 min |
| **[10](10-performance-of-fonts.md)** | **Font Performance & Zero-CLS Metrics** | Zero-CLS font matching with `size-adjust`, `ascent-override`, `descent-override`, glyph subsetting with `pyftsubset`, preload headers. | 28 min |

---

## Learning Objectives

By completing this module, you will:
1. **Master Font Metrics**: Understand the internal anatomy of glyphs (ascent, descent, cap height, x-height, baseline) and how browsers assemble inline line boxes.
2. **Eliminate Layout Shifts (Zero CLS)**: Tune fallback fonts using CSS Font Metrics Overrides so local system fonts occupy the exact footprint of web fonts during download.
3. **Deploy Variable Fonts**: Harness multi-axis variable fonts to reduce network payloads by up to 70% while enabling fine-grained typography animations.
4. **Implement Balanced Wrapping**: Eliminate typographical orphans and ragged headings using modern standard CSS wrapping properties.
5. **Architect Global Typographic Systems**: Build resilient multilingual interfaces supporting LTR, RTL, and vertical writing modes.

---

[Previous Module: 05-responsive-design](file:///e:/Learning/engineering-notes/01-languages/css/05-responsive-design/README.md) | [Track Index](file:///e:/Learning/engineering-notes/01-languages/css/INDEX.md) | [Next Module: 07-visual-styling](file:///e:/Learning/engineering-notes/01-languages/css/07-visual-styling/README.md)
