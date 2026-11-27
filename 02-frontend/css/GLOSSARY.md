# CSS Track Master Glossary (GLOSSARY.md)

This glossary provides formal, standards-compliant definitions for core terms and concepts used across modern CSS specifications.

---

### Anchor Positioning
A CSS layout mechanism that tethers absolute or fixed elements (popovers, tooltips, dropdowns) to anchor elements in the DOM without requiring JavaScript positioning libraries.

### Block Formatting Context (BFC)
A region of the visual CSS layout where block boxes are laid out. Margin collapsing occurs only between block boxes in the same BFC. A BFC is established by the root element, floats, absolute elements, flex/grid items, and elements with `overflow` other than `visible`.

### Box-Sizing
The CSS property determining how element dimensions are computed. `content-box` calculates width as content area only (padding and border add extra width). `border-box` includes padding and border within the declared width and height.

### Cascade Layers (`@layer`)
An explicit CSS at-rule used to define precedence buckets in the cascade. Normal declarations in later layers override earlier layers regardless of selector specificity, while unlayered styles override all layered styles.

### Container Queries
A responsive design model allowing elements to adapt styling based on the size (`@container (min-width: ...)`) or style (`@container style(...)`) of an ancestor container rather than the viewport.

### Critical Rendering Path (CRP)
The sequence of steps a browser engine executes to convert HTML, CSS, and JavaScript into pixels on the display: DOM construction &rarr; CSSOM construction &rarr; Render Tree &rarr; Layout (Reflow) &rarr; Paint &rarr; GPU Compositing.

### Dynamic Viewport Units (`dvh`, `svh`, `lvh`)
Modern viewport units that resolve the 100vh mobile clipping bug. `svh` calculates height with UI bars visible; `lvh` calculates height with UI bars hidden; `dvh` recalculates dynamically in real time as bars collapse.

### Flexbox Formatting Context (FFC)
A formatting context established by declaring `display: flex` or `display: inline-flex`. Children become flex items laid out along a main axis and cross axis.

### Grid Formatting Context (GFC)
A formatting context established by declaring `display: grid` or `display: inline-grid`. Children become grid items placed along two-dimensional rows and columns.

### Initial / Inherit / Unset / Revert
Global CSS property keywords:
- `initial`: Sets property to its official CSS specification default.
- `inherit`: Forces element to take the computed value of its parent.
- `unset`: Resets property to `inherit` if naturally inherited, otherwise `initial`.
- `revert`: Rolls back the cascade to the value from the previous cascade origin (User Agent or User).

### Margin Collapsing
The behavior where adjacent vertical margins of in-flow block boxes combine into a single margin equal to the maximum of the positive margins.

### OKLCH
A modern color function representing Lightness ($0$ to $1$ or $0\%$ to $100\%$), Chroma ($0$ to $0.4+$), and Hue ($0$ to $360$ degrees) in the OKLab color space. Unlike HSL, OKLCH is perceptually uniform.

### Relative Color Syntax (RCS)
CSS syntax (`oklch(from var(--brand) l c h)`) allowing authors to destructure an existing color into individual channels and modify them dynamically.

### Stacking Context
A three-dimensional conceptualization of HTML elements along the z-axis relative to the user. Elements within a stacking context are rendered together in back-to-front order.

### Subgrid
A feature of CSS Grid Level 2 (`grid-template-columns: subgrid`) where a nested grid item inherits and aligns with the tracks and gaps of its parent grid container.
