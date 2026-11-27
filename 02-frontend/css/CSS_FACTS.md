# CSS Specification & Baseline Facts (CSS_FACTS.md)

**Generated:** 2026-11-26  
**Governing Authority:** W3C Cascading Style Sheets Working Group (CSSWG) & Web Platform Baseline (W3C WebDX / MDN / web.dev)  
**Standard Environment Baseline:** Modern Web Baseline (2024–2026)

---

## 1. Verified Web Platform Baseline Status Matrix

| Feature / Specification | Baseline Status | Interoperable Since | Primary Browser Support | Core Specification & MDN Reference |
|:---|:---:|:---:|:---|:---|
| **Cascade Layers (`@layer`)** | **Widely Available** | March 2022 | Chrome 99+, Safari 15.4+, Firefox 97+ | [CSS Cascading & Inheritance Level 5](https://www.w3.org/TR/css-cascade-5/) |
| **Relational Pseudo-Class (`:has()`)** | **Widely Available** | December 2023 | Chrome 105+, Safari 15.4+, Firefox 121+ | [CSS Selectors Level 4](https://www.w3.org/TR/selectors-4/) |
| **Native CSS Nesting** | **Widely Available** | December 2023 | Chrome 120+, Safari 17.2+, Firefox 117+ | [CSS Nesting Module](https://www.w3.org/TR/css-nesting-1/) |
| **Container Queries (Size & `cq*` units)** | **Widely Available** | February 2023 | Chrome 105+, Safari 16.0+, Firefox 110+ | [CSS Containment Level 3](https://www.w3.org/TR/css-contain-3/) |
| **CSS Subgrid (`subgrid`)** | **Widely Available** | September 2023 | Chrome 117+, Safari 16.0+, Firefox 71+ | [CSS Grid Layout Level 2](https://www.w3.org/TR/css-grid-2/) |
| **Color Spaces & Functions (`oklch()`, `color-mix()`)** | **Widely Available** | Late 2023 | Chrome 111+, Safari 16.2+, Firefox 113+ | [CSS Color Module Level 4 & 5](https://www.w3.org/TR/css-color-5/) |
| **Relative Color Syntax (`from <color>`)** | **Newly Available (2024)** | July 2024 | Chrome 119+, Safari 18.0+, Firefox 128+ | [CSS Color Module Level 5](https://www.w3.org/TR/css-color-5/) |
| **Color Scheme Mapping (`light-dark()`)** | **Widely Available** | May 2024 | Chrome 123+, Safari 17.5+, Firefox 120+ | [CSS Color Module Level 5](https://www.w3.org/TR/css-color-5/) |
| **Typed Custom Properties (`@property`)** | **Newly Available (2024)** | July 2024 | Chrome 85+, Safari 16.4+, Firefox 128+ | [CSS Properties & Values API Level 1](https://www.w3.org/TR/css-properties-values-api-1/) |
| **Dynamic Viewport Units (`dvh`, `svh`, `lvh`)** | **Widely Available** | March 2023 | Chrome 108+, Safari 16.4+, Firefox 101+ | [CSS Values & Units Level 4](https://www.w3.org/TR/css-values-4/) |
| **Linear Easing (`linear()`)** | **Widely Available** | December 2023 | Chrome 113+, Safari 17.2+, Firefox 112+ | [CSS Easing Functions Level 2](https://www.w3.org/TR/css-easing-2/) |
| **Scroll-Driven Animations (`animation-timeline`)** | **Baseline 2026** | Early 2026 | Chrome 115+, Safari 26+, Firefox 159+ | [CSS Scroll-Driven Animations](https://www.w3.org/TR/scroll-animations-1/) |
| **View Transitions API (Single-Doc / SPA)** | **Newly Available (2024-2025)** | Late 2024 | Chrome 111+, Safari 18.0+, Firefox 144+ | [CSS View Transitions Level 1](https://www.w3.org/TR/css-view-transitions-1/) |
| **Starting Style (`@starting-style`)** | **Newly Available (2024)** | August 2024 | Chrome 117+, Safari 17.5+, Firefox 129+ | [CSS Transitions Level 2](https://www.w3.org/TR/css-transitions-2/) |
| **Discrete Transitions (`transition-behavior: allow-discrete`)** | **Newly Available (2024)** | August 2024 | Chrome 117+, Safari 17.4+, Firefox 129+ | [CSS Transitions Level 2](https://www.w3.org/TR/css-transitions-2/) |
| **Scoping Rule (`@scope`)** | **Widely Available** | Late 2024 | Chrome 118+, Safari 17.4+, Firefox 146+ | [CSS Cascading & Inheritance Level 6](https://www.w3.org/TR/css-cascade-6/) |
| **Anchor Positioning (`anchor()`, `position-anchor`)** | **Newly Available (2025-2026)**| Early 2026 | Chrome 125+, Safari 18.2+, Firefox 147+ | [CSS Anchor Positioning](https://www.w3.org/TR/css-anchor-position-1/) |
| **Form Control Auto-Sizing (`field-sizing: content`)** | **Newly Available (2026)** | Mid 2026 | Chrome 123+, Safari 26.2+, Firefox 152+ | [CSS Basic User Interface Module Level 4](https://www.w3.org/TR/css-ui-4/) |
| **Text Wrap Balancing (`text-wrap: balance`)** | **Widely Available** | December 2023 | Chrome 114+, Safari 17.4+, Firefox 121+ | [CSS Text Module Level 4](https://www.w3.org/TR/css-text-4/) |
| **Text Wrap Pretty (`text-wrap: pretty`)** | **Limited / Progressive** | Progressive | Chrome 117+, Safari 26+; Gecko partial | [CSS Text Module Level 4](https://www.w3.org/TR/css-text-4/) |
| **Tree-Counting Functions (`sibling-index()`, `sibling-count()`)** | **Baseline 2026** | Mid 2026 | Chrome 138+, Safari 26.2+, Firefox 154+ | [CSS Values & Units Level 5](https://www.w3.org/TR/css-values-5/) |
| **Inline Conditional (`if()`)** | **Limited Availability** | Experimental | Chrome 137+; Gecko/WebKit in discussion | [CSS Values & Units Level 5](https://www.w3.org/TR/css-values-5/) |
| **Masonry / Grid Lanes (`display: grid-lanes`)** | **Limited Availability** | Standardizing | Safari TP / Gecko flags; Blink prototyping | [CSS Grid Layout Level 3](https://www.w3.org/TR/css-grid-3/) |
| **Rendering Optimization (`content-visibility: auto`)** | **Newly Available (2024)** | Mid 2024 | Chrome 85+, Safari 18.0+, Firefox 125+ | [CSS Containment Level 2](https://www.w3.org/TR/css-contain-2/) |

---

## 2. Fundamental Architectural Facts

### 1. The Cascade Order of Precedence (Origins & Layers)
Under CSS Cascading & Inheritance Level 5, declarations are prioritized in the following strict order (from lowest to highest precedence):
1. User Agent normal declarations
2. User normal declarations
3. Author unlayered normal declarations (Normal styles in `@layer` lose to unlayered styles!)
4. Author layered normal declarations (First declared layer loses to subsequent layers)
5. CSS Animation keyframe declarations
6. Author layered `!important` declarations (Inverted order: First declared layer WINS over subsequent layers!)
7. Author unlayered `!important` declarations
8. User `!important` declarations
9. User Agent `!important` declarations
10. CSS Transition active declarations (Transitions always beat everything!)

### 2. Specificity Calculation Mechanics
Selectors are evaluated using a 3-component tuple `(A, B, C)`:
- `A` = ID selectors (`#header`)
- `B` = Class selectors (`.nav`), attribute selectors (`[type="text"]`), and pseudo-classes (`:hover`, `:has()`, `:is()`, `:nth-child()`)
- `C` = Type selectors (`div`, `h1`) and pseudo-elements (`::before`, `::after`)
*Note*: The `:where()` pseudo-class adds `(0, 0, 0)` specificity. The `:is()` and `:has()` pseudo-classes take the specificity of their most specific argument inside the list. Inline styles override selector specificity via the style attribute origin.

### 3. The 100vh Mobile Viewport Bug
`100vh` in mobile browsers is calculated based on the maximum possible screen height assuming the browser address bar and navigation bar are completely retracted. As a result, bottom-pinned elements get clipped beneath mobile browser UI chrome.
- `100svh` (Small Viewport Height): Calculated when browser UI chrome is fully expanded (safe area).
- `100lvh` (Large Viewport Height): Calculated when browser UI chrome is fully collapsed.
- `100dvh` (Dynamic Viewport Height): Dynamically adjusts in real time as the user scrolls and the UI expands/collapses.

### 4. Box Sizing & Margin Collapsing Rules
- `box-sizing: border-box` includes `padding` and `border` inside the specified `width` and `height`, but excludes `margin`.
- Margin collapsing occurs **only** on vertical (block-axis) margins of in-flow block-level boxes in the same block formatting context.
- Margins do **not** collapse on:
  1. Elements in Flex or Grid formatting contexts.
  2. Floating or absolutely positioned elements.
  3. Elements with `overflow` other than `visible` (`overflow: clip | auto | hidden`).
  4. Between parent and children if separated by a `border`, `padding`, or clearance.

### 5. Stacking Context Triggers
A stacking context is formed by:
1. Root element of document (`<html>`).
2. Element with `position: relative | absolute` and `z-index` other than `auto`.
3. Element with `position: fixed | sticky`.
4. Flex or Grid item with `z-index` other than `auto`.
5. Element with `opacity < 1`.
6. Element with `mix-blend-mode` other than `normal`.
7. Element with `transform`, `filter`, `perspective`, `clip-path`, or `mask` other than `none`.
8. Element with `isolation: isolate`.
9. Element with `contain: paint | layout | strict | content`.
10. Element with `will-change` specifying any property that creates a stacking context.
11. Element in the browser's **Top Layer** (`<dialog showModal()>`, `popover="auto"`).

---

## 3. Dangerous Legacy Anti-Patterns to Flag

1. **Float-based grid systems**: Using `float: left` and clearfix hacks (`.clearfix::after { content: ""; display: table; clear: both; }`) as primary page layout.
2. **`px`-only sizing**: Defining all typography and container dimensions in pixels, destroying user font-scaling preferences.
3. **`* { margin: 0; }` without `box-sizing: border-box`**: Creating layout distortion.
4. **`!important` abuse**: Using `!important` to solve specificity conflicts instead of `@layer` or refactoring selectors.
5. **ID selectors for styling (`#nav a`)**: Creating artificially high specificity `(1, 0, 1)` that cannot be overridden by component classes.
6. **Removing focus outlines (`outline: none`)**: Violating WCAG 2.4.7 (Focus Visible) without a `:focus-visible` replacement.
7. **Animating layout properties (`width`, `height`, `top`, `margin`)**: Triggering main-thread layout reflows and paint jank instead of compositor-only properties (`transform`, `opacity`).
8. **Outdated vendor prefixes**: Retaining `-webkit-box-shadow`, `-moz-border-radius`, `-ms-flexbox` in modern stylesheets.
9. **Color contrast failures**: Using light grey text (`#999999` on `#ffffff`) that fails the WCAG 4.5:1 ratio.
