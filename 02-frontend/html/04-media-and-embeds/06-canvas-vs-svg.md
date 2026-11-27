# Canvas vs. SVG: Architecture, Performance, and Accessibility

| Metadata | Details |
| :--- | :--- |
| **Specification** | WHATWG HTML Living Standard (§4.12.5 The `canvas` element) / W3C SVG 2 |
| **Prerequisites** | `04-media-and-embeds/05-embedded-svg.md`, `00-setup/02-how-browsers-turn-html-into-pixels.md` |
| **Estimated Reading Time** | 25 minutes |
| **Status** | Production Ready |
| **Baseline Status** | Widely Available (`<canvas>`, 2D Context, WebGL, SVG DOM) |
| **Browsers Tested** | Chrome 124+, Firefox 125+, Safari 17.4+, Edge 124+ |
| **Tested With** | W3C Nu HTML Validator, Chrome DevTools Performance Profiler, axe-core 4.9 |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Usage](#4-syntax--usage)
5. [Practical Examples](#5-practical-examples)
   - [Example 1: Crisp Retina-Scaled `<canvas>` Line Chart](#example-1-crisp-retina-scaled-canvas-line-chart)
   - [Example 2: Equivalent Scalable Vector SVG Line Chart with Tooltips](#example-2-equivalent-scalable-vector-svg-line-chart-with-tooltips)
   - [Example 3: Accessible Fallback Subtree Inside `<canvas>`](#example-3-accessible-fallback-subtree-inside-canvas)
   - [Example 4: High-Performance Particle Simulation (10,000 Nodes)](#example-4-high-performance-particle-simulation-10000-nodes)
   - [Example 5: Hybrid Canvas-SVG Architecture](#example-5-hybrid-canvas-svg-architecture)
6. [Best Practices](#6-best-practices)
7. [Common Mistakes & Pitfalls](#7-common-mistakes--pitfalls)
8. [Performance, Accessibility, SEO & Security Notes](#8-performance-accessibility-seo--security-notes)
9. [Comparison / When to Use What](#9-comparison--when-to-use-what)
10. [Interview Questions](#10-interview-questions)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary / Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

Modern web browsers feature two distinct, powerful graphic rendering technologies:
1. **SVG (Scalable Vector Graphics)**: A **retained-mode**, XML-based vector graphics format where every shape is a living, inspectable DOM element.
2. **`<canvas>`**: An **immediate-mode**, pixel-based bitmap drawing surface where graphics are rendered programmatically via JavaScript (using 2D Context, WebGL, or WebGPU).

Choosing between Canvas and SVG is one of the most critical architectural decisions in web frontend engineering. Choosing incorrectly can destroy application frame rates (e.g., trying to render 20,000 animated DOM nodes in SVG) or ruin accessibility and responsiveness (e.g., drawing simple UI charts on a fixed-resolution Canvas without keyboard focus or Retina scaling).

Understanding the fundamental trade-offs between retained-mode and immediate-mode graphics empowers developers to select the ideal engine based on object counts, interaction complexity, performance budgets, and accessibility mandates.

---

## 2. Core Concepts

### Retained Mode (SVG) vs. Immediate Mode (Canvas)

```
+-----------------------------------------------------------------------------------------+
|                  Retained Mode (SVG) vs. Immediate Mode (Canvas)                        |
+-----------------------------------------------------------------------------------------+

  RETAINED MODE: SVG
  +---------------------------------------------------------------+
  | DOM Tree:                                                     |
  | <svg> -> <g> -> <circle id="node-1">                          |
  |              -> <circle id="node-2">                          |
  |              -> <rect id="bar-1">                             |
  +---------------------------------------------------------------+
  - The browser retains knowledge of every individual shape in memory.
  - Automatic hit-testing: node.addEventListener('click', fn).
  - Memory cost scales linearly with object count (O(N)).

-------------------------------------------------------------------------------------------

  IMMEDIATE MODE: CANVAS
  +---------------------------------------------------------------+
  | Single DOM Node: <canvas width="800" height="600"></canvas>   |
  | Pixel Buffer in GPU Memory: [ R, G, B, A, R, G, B, A ... ]    |
  +---------------------------------------------------------------+
  - Browser forgets shapes immediately after drawing: ctx.fillRect(10, 10, 50, 50).
  - No DOM elements for shapes; developer must compute manual hit-testing via math.
  - Memory cost is fixed to resolution (Width * Height * 4 bytes), independent of object count!
```

### Canvas Resolution vs. Display Dimensions
The `<canvas>` element possesses two distinct sets of dimensions:
1. **Coordinate Drawing Buffer**: Set via HTML attributes `<canvas width="800" height="600">`. This defines the actual number of pixels in the memory drawing buffer.
2. **CSS Display Dimensions**: Set via CSS styles `style="width: 400px; height: 300px;"`. This defines how large the canvas appears in the browser layout.

If you omit the HTML attributes and set only CSS dimensions, the browser defaults the internal buffer to `300x150` pixels and stretches it to fill the CSS box, resulting in terribly blurry graphics.

---

## 3. How It Works Internally

### The Retina / HiDPR Scaling Algorithm
Modern mobile devices, laptops, and 4K monitors feature Device Pixel Ratios (DPR) of `2.0`, `3.0`, or higher. To make a `<canvas>` appear pixel-sharp:
1. Detect device DPR: `const dpr = window.devicePixelRatio || 1;`
2. Multiply the HTML buffer dimensions by DPR:
   `canvas.width = displayWidth * dpr;`
   `canvas.height = displayHeight * dpr;`
3. Constrain the CSS display size to layout dimensions:
   `canvas.style.width = displayWidth + 'px';`
   `canvas.style.height = displayHeight + 'px';`
4. Scale the 2D drawing context:
   `ctx.scale(dpr, dpr);`

Now, drawing at `ctx.arc(50, 50, 20)` renders with full subpixel Retina clarity!

---

## 4. Syntax & Usage

```html
<!-- Native Canvas with Accessible Fallback Table -->
<canvas id="sales-canvas" width="600" height="400" role="img" aria-label="Quarterly Revenue Trends">
  <!-- Fallback content exposed to screen readers and legacy browsers -->
  <table>
    <caption>Quarterly Revenue Summary (in thousands)</caption>
    <thead><tr><th>Quarter</th><th>Revenue</th></tr></thead>
    <tbody>
      <tr><td>Q1</td><td>$450k</td></tr>
      <tr><td>Q2</td><td>$620k</td></tr>
    </tbody>
  </table>
</canvas>
```

> **Cross-Browser & Baseline Status Box**
>
> - `<canvas>` and 2D Context (`getContext('2d')`): **Baseline Widely Available** across all desktop and mobile browsers since 2011.
> - WebGL 1.0 & 2.0: **Baseline Widely Available**.
> - WebGPU: Newly Available in modern Chromium and Safari 18+; limited in Firefox.

---

## 5. Practical Examples

### Example 1: Crisp Retina-Scaled `<canvas>` Line Chart
Properly scaling the canvas memory buffer to match device pixel density.

```html
<div class="chart-box">
  <canvas id="line-chart" role="img" aria-label="Stock price trend line"></canvas>
</div>

<style>
  .chart-box {
    max-width: 600px;
    width: 100%;
  }
  #line-chart {
    width: 100%;
    height: 300px;
    display: block;
  }
</style>

<script>
  const canvas = document.getElementById('line-chart');
  const ctx = canvas.getContext('2d');

  function renderChart() {
    const dpr = window.devicePixelRatio || 1;
    const rect = canvas.getBoundingClientRect();

    // 1. Set internal buffer resolution scaled by DPR
    canvas.width = rect.width * dpr;
    canvas.height = rect.height * dpr;

    // 2. Normalize coordinate system so drawing logic uses CSS pixels
    ctx.scale(dpr, dpr);

    // 3. Draw sharp Retina vector graphics
    ctx.clearRect(0, 0, rect.width, rect.height);
    
    // Draw grid
    ctx.strokeStyle = '#e2e8f0';
    ctx.lineWidth = 1;
    ctx.beginPath();
    ctx.moveTo(0, rect.height - 20);
    ctx.lineTo(rect.width, rect.height - 20);
    ctx.stroke();

    // Draw line trend
    ctx.strokeStyle = '#0284c7';
    ctx.lineWidth = 3;
    ctx.beginPath();
    ctx.moveTo(20, rect.height - 40);
    ctx.lineTo(rect.width * 0.3, rect.height - 120);
    ctx.lineTo(rect.width * 0.6, rect.height - 80);
    ctx.lineTo(rect.width - 20, rect.height - 220);
    ctx.stroke();
  }

  window.addEventListener('resize', renderChart);
  renderChart();
</script>
```

---

### Example 2: Equivalent Scalable Vector SVG Line Chart with Tooltips
The exact same chart rendered in retained-mode SVG: inherently sharp at any zoom level, styleable with CSS, and offering native DOM hover events on individual data points.

```html
<svg 
  viewBox="0 0 600 300" 
  width="100%" 
  height="300" 
  role="img" 
  aria-labelledby="svg-chart-title" 
  class="svg-chart"
>
  <title id="svg-chart-title">Stock Price Trend Line</title>

  <!-- Base Grid Line -->
  <line x1="0" y1="280" x2="600" y2="280" stroke="#e2e8f0" stroke-width="1" />

  <!-- Trend Polyline -->
  <polyline 
    fill="none" 
    stroke="#0284c7" 
    stroke-width="3" 
    points="20,260 180,180 360,220 580,80" 
  />

  <!-- Interactive Data Points (Full DOM nodes!) -->
  <g class="data-points">
    <circle cx="20" cy="260" r="5" class="point" tabindex="0" aria-label="Day 1: $260" />
    <circle cx="180" cy="180" r="5" class="point" tabindex="0" aria-label="Day 2: $180" />
    <circle cx="360" cy="220" r="5" class="point" tabindex="0" aria-label="Day 3: $220" />
    <circle cx="580" cy="80" r="5" class="point" tabindex="0" aria-label="Day 4: $80" />
  </g>
</svg>

<style>
  .svg-chart {
    display: block;
    max-width: 600px;
  }
  .point {
    fill: #0284c7;
    transition: r 0.2s ease, fill 0.2s ease;
    cursor: pointer;
  }
  .point:hover, .point:focus {
    r: 8;
    fill: #d97706;
    outline: none;
  }
</style>
```

---

### Example 3: Accessible Fallback Subtree Inside `<canvas>`
The contents placed between `<canvas>` and `</canvas>` are only rendered by assistive technologies and browsers that lack canvas support. This allows providing fully accessible interactive forms or tables behind visual canvas games or charts.

```html
<div class="interactive-canvas-wrapper">
  <canvas id="accessible-game-board" width="400" height="400" role="application" aria-label="Tic-Tac-Toe Game">
    <!-- Accessible DOM Fallback Tree -->
    <fieldset>
      <legend>Tic-Tac-Toe Interactive Grid</legend>
      <div class="grid-controls">
        <button type="button" id="cell-0-0">Row 1, Column 1: Empty</button>
        <button type="button" id="cell-0-1">Row 1, Column 2: X</button>
        <button type="button" id="cell-0-2">Row 1, Column 3: O</button>
        <!-- Additional buttons for remaining cells -->
      </div>
    </fieldset>
  </canvas>
</div>
```

---

### Example 4: High-Performance Particle Simulation (10,000 Nodes)
Demonstrating why Canvas shines: animating 10,000 independent entities at 60 FPS without overloading the browser's DOM tree.

```html
<canvas id="particle-canvas" width="800" height="400"></canvas>

<script>
  const canvas = document.getElementById('particle-canvas');
  const ctx = canvas.getContext('2d');
  const PARTICLE_COUNT = 5000;
  const particles = [];

  for (let i = 0; i < PARTICLE_COUNT; i++) {
    particles.push({
      x: Math.random() * canvas.width,
      y: Math.random() * canvas.height,
      vx: (Math.random() - 0.5) * 2,
      vy: (Math.random() - 0.5) * 2,
      size: Math.random() * 2 + 1
    });
  }

  function renderLoop() {
    ctx.fillStyle = 'rgba(15, 23, 42, 0.2)'; // Trailing fade effect
    ctx.fillRect(0, 0, canvas.width, canvas.height);

    ctx.fillStyle = '#38bdf8';
    for (let i = 0; i < PARTICLE_COUNT; i++) {
      const p = particles[i];
      p.x += p.vx;
      p.y += p.vy;

      if (p.x < 0 || p.x > canvas.width) p.vx *= -1;
      if (p.y < 0 || p.y > canvas.height) p.vy *= -1;

      ctx.fillRect(p.x, p.y, p.size, p.size);
    }

    requestAnimationFrame(renderLoop);
  }

  renderLoop();
</script>
```

---

### Example 5: Hybrid Canvas-SVG Architecture
The enterprise architectural pattern used by Google Maps and Figma: using high-performance `<canvas>` for rendering heavy background bitmaps or 100,000 geometry points, overlaid with SVG for razor-sharp interactive selection handles, tooltips, and accessible labels.

```html
<div class="hybrid-viewport">
  <!-- 1. Bottom Layer: High-speed Canvas rendering 50,000 scatter points -->
  <canvas id="data-layer" width="800" height="500" class="canvas-layer"></canvas>

  <!-- 2. Top Layer: SVG rendering accessible user cursor, crosshairs, and tooltips -->
  <svg class="overlay-layer" viewBox="0 0 800 500">
    <!-- User crosshair cursor -->
    <line x1="400" y1="0" x2="400" y2="500" stroke="#f43f5e" stroke-dasharray="4" />
    <line x1="0" y1="250" x2="800" y2="250" stroke="#f43f5e" stroke-dasharray="4" />
    
    <!-- Active selection indicator -->
    <circle cx="400" cy="250" r="10" fill="none" stroke="#f43f5e" stroke-width="2" />
    <text x="415" y="245" fill="#f43f5e" font-family="sans-serif" font-size="12">Target 99.4%</text>
  </svg>
</div>

<style>
  .hybrid-viewport {
    position: relative;
    width: 800px;
    height: 500px;
    border: 1px solid #334155;
    background: #0f172a;
  }
  .canvas-layer, .overlay-layer {
    position: absolute;
    top: 0;
    left: 0;
    width: 100%;
    height: 100%;
  }
  .overlay-layer {
    pointer-events: none; /* Let mouse clicks pass through to canvas if needed */
  }
</style>
```

---

## 6. Best Practices

- [ ] **Always scale Canvas buffer resolution to `devicePixelRatio`**: Match the hardware pixel density so lines and text do not appear blurry on Retina and 4K displays.
- [ ] **Choose SVG for documents, icons, and UI charts (< 1,000 elements)**: Retain DOM styling, CSS animations, and native screen reader accessibility.
- [ ] **Choose Canvas for games, video processing, and big data (> 10,000 elements)**: Avoid browser DOM memory leaks and layout reflow overhead.
- [ ] **Provide an accessible fallback subtree inside `<canvas>`**: Nest a semantic HTML table, form, or list between `<canvas>` and `</canvas>` to satisfy accessibility guidelines.
- [ ] **Always use `requestAnimationFrame()` for Canvas animation loops**: Syncs rendering with display refresh rate (60Hz / 120Hz) and pauses execution when the browser tab is hidden in the background.
- [ ] **Consider a hybrid architecture for complex tools**: Use Canvas for pixel-intensive rendering and SVG for overlay controls and accessible interactions.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Severity | Consequence | Correction |
| :--- | :--- | :--- | :--- |
| Setting canvas size only in CSS (`width: 400px; height: 300px;`) | **High** | Browser stretches the default `300x150` internal buffer, resulting in blurry, pixelated graphics. | Set explicit `width` and `height` attributes on the `<canvas>` tag. |
| Forgetting Retina scaling (`window.devicePixelRatio`) | **Moderate** | Canvas text and thin vector lines appear visibly blurry on high-DPR screens. | Scale buffer dimensions by DPR and call `ctx.scale(dpr, dpr)`. |
| Rendering tens of thousands of animated DOM elements in SVG | **Critical** | Browser DOM freezes, memory spikes, and frame rate drops below 5 FPS. | Switch to `<canvas>` (2D or WebGL) for mass particle/data rendering. |
| Neglecting `<canvas>` accessibility entirely | **Critical** | Screen readers perceive the entire canvas as an invisible, silent void. | Add fallback HTML inside `<canvas>` or use `aria-label` / `role="img"`. |
| Using `setInterval()` for canvas animation loops | **Moderate** | Causes frame stuttering (jank) and burns CPU/battery when the user switches tabs. | Use `requestAnimationFrame(callback)`. |
| Expecting CSS pseudo-classes (`:hover`) to work on Canvas shapes | **High** | Fails; shapes drawn on canvas are not DOM elements. | Implement manual mathematical hit-testing in JavaScript or use SVG. |

---

## 8. Performance, Accessibility, SEO & Security Notes

### Performance Comparison Matrix
- **Memory Consumption**:
  - SVG memory scales with the number of elements: $O(N)$. 10,000 complex SVG `<path>` elements consume hundreds of megabytes of RAM.
  - Canvas memory is fixed to resolution: $O(\text{Width} \times \text{Height} \times 4 \text{ bytes})$. Drawing 1 circle or 1,000,000 circles consumes the exact same memory buffer.
- **CPU vs. GPU**:
  - SVG relies heavily on CPU DOM tree management and layout recalculations.
  - Canvas (specifically WebGL/WebGPU) delegates raw draw calls directly to GPU vertex and fragment shaders.

### Accessibility (WCAG 2.2 AA)
- While SVG elements can be enriched with ARIA attributes directly on `<g>`, `<path>`, or `<circle>` nodes, Canvas is an opaque raster surface. To make Canvas accessible, engineers must populate the fallback DOM subtree inside `<canvas>...</canvas>` with focusable elements.

---

## 9. Comparison / When to Use What

| Feature | SVG (Retained Mode) | `<canvas>` (Immediate Mode) |
| :--- | :--- | :--- |
| **Rendering Model** | Vector DOM tree | Pixel Bitmap buffer |
| **Resolution Independence** | Infinite vector sharpness at any zoom | Pixelated on zoom unless re-drawn |
| **Object Limit (Smooth 60fps)** | Up to ~1,000 objects | 100,000+ objects (WebGL) |
| **CSS Styling & Transitions** | Full CSS support (`fill`, `hover`, animations) | None (All styling done in JS code) |
| **Event Handling** | Native DOM events (`click`, `mouseenter` on shapes) | Manual coordinate hit-testing |
| **Accessibility** | Built-in via AOM (`<title>`, ARIA, `role="img"`) | Requires internal fallback DOM subtree |
| **Ideal Use Cases** | UI icons, logos, simple charts, interactive maps | Games, particle simulations, video filters, charts > 10k points |

---

## 10. Interview Questions

<details>
<summary><strong>1. What is the fundamental difference between "Retained Mode" (SVG) and "Immediate Mode" (Canvas) rendering?</strong></summary>

- **Retained Mode (SVG)**: The browser retains a complete object model (DOM tree) of all drawn shapes in memory. If a shape moves, the developer modifies its DOM attribute (`cx="100"`), and the browser engine automatically recalculates layout, handles repainting, and manages mouse hit-testing.
- **Immediate Mode (Canvas)**: The browser does not store shapes; it merely exposes a drawing API that mutates an array of pixel bytes in memory. Once `ctx.fillRect()` executes, the browser completely forgets that a rectangle was drawn. If an object moves, the entire canvas buffer must be cleared and re-drawn by JavaScript.
</details>

<details>
<summary><strong>2. Why do graphics drawn on a <code>&lt;canvas&gt;</code> look blurry on Retina screens, and how do you fix it?</strong></summary>

Graphics appear blurry when the canvas coordinate buffer resolution is lower than the physical pixel density of the screen. For example, on a 2x Retina screen, 1 CSS pixel equals 4 physical hardware pixels. If the canvas buffer is not scaled, the browser upscales the bitmap, producing blurriness. You fix this by multiplying `canvas.width` and `canvas.height` by `window.devicePixelRatio`, restricting `canvas.style.width/height` to CSS pixels, and calling `ctx.scale(dpr, dpr)`.
</details>

<details>
<summary><strong>3. How does the browser handle content placed inside the <code>&lt;canvas&gt;...&lt;/canvas&gt;</code> tags?</strong></summary>

Content placed between `<canvas>` and `</canvas>` is called **fallback content**. It is invisible to sighted users in modern browsers, but is actively parsed into the DOM and exposed to assistive technologies (screen readers, braille displays). It is also rendered visually if the browser is too old to support Canvas or if the user has disabled JavaScript.
</details>

<details>
<summary><strong>4. Why does an SVG chart with 50,000 data points cause extreme browser lag, while a Canvas chart handles it effortlessly?</strong></summary>

Every SVG data point is an individual DOM element (`SVGCircleElement`). 50,000 SVG elements require massive memory allocations, and every interaction triggers expensive DOM tree traversals, style recalculations, and layout reflows. In contrast, Canvas renders all 50,000 points as raw drawing commands into a single raster buffer without creating any DOM nodes, executing at 60 FPS directly on the GPU.
</details>

<details>
<summary><strong>5. How do you implement click detection on a shape drawn inside a <code>&lt;canvas&gt;</code>?</strong></summary>

Because Canvas shapes are not DOM nodes, you cannot attach `.addEventListener('click')` directly to a drawn shape. Instead, you listen for clicks on the entire `<canvas>`, capture the mouse coordinates (`event.offsetX`, `event.offsetY`), and perform mathematical hit-testing (e.g., checking if the click coordinates fall within the radius of a circle, or using `ctx.isPointInPath(x, y)`).
</details>

<details>
<summary><strong>6. When would you choose a "Hybrid Canvas-SVG" architecture?</strong></summary>

In complex data-intensive applications (such as CAD tools, mapping applications like Google Maps, or visual design software like Figma). The heavy data layer (thousands of map tiles, road geometries, or complex vector layers) is rendered on a high-speed Canvas, while the interactive UI layer (selection bounding boxes, tooltips, drag handles) is rendered in SVG on top for precise CSS styling and accessibility.
</details>

<details>
<summary><strong>7. What happens if you modify the <code>width</code> or <code>height</code> attribute of a <code>&lt;canvas&gt;</code> via JavaScript?</strong></summary>

Modifying `canvas.width` or `canvas.height` immediately clears the entire drawing buffer, wipes all previously drawn pixels, and resets the drawing context state (transform matrix, fill styles, line widths, and stroke colors) back to their initial default values.
</details>

<details>
<summary><strong>8. Can search engine web crawlers index text rendered inside an SVG vs. text drawn on a Canvas?</strong></summary>

Yes for SVG; no for Canvas. Text inside an SVG is represented by native `<text>` DOM elements that web crawlers (Googlebot) easily index and parse. Text drawn onto a Canvas via `ctx.fillText()` is merely colored pixel data in a bitmap; search engines cannot index it unless identical text is mirrored in the canvas fallback subtree.
</details>

---

## 11. Practice Exercises

### Exercise 1: Canvas Dimension Configuration (Easy)
- **Task**: Author an HTML snippet with a `<canvas>` that has an internal buffer resolution of `800x600` pixels, but is styled via CSS to occupy a responsive `100%` width with a maximum layout width of `400px`. Include accessible fallback text.

<details>
<summary>View Solution</summary>

```html
<canvas 
  id="my-canvas" 
  width="800" 
  height="600" 
  style="width: 100%; max-width: 400px; height: auto; display: block;"
  role="img" 
  aria-label="Responsive diagram showcase"
>
  <p>Your browser does not support HTML5 Canvas. Please update your browser.</p>
</canvas>
```
</details>

---

### Exercise 2: Retina-Ready 2D Canvas Square (Medium)
- **Task**: Write HTML and JavaScript to render a `100x100` blue square on a canvas with full Retina (`devicePixelRatio`) scaling, so the edges are perfectly crisp on high-density displays.

<details>
<summary>View Solution</summary>

```html
<canvas id="retina-canvas" style="width: 300px; height: 300px; border: 1px solid #ccc;"></canvas>

<script>
  const canvas = document.getElementById('retina-canvas');
  const ctx = canvas.getContext('2d');
  const dpr = window.devicePixelRatio || 1;

  // Scale internal buffer
  canvas.width = 300 * dpr;
  canvas.height = 300 * dpr;

  // Scale context coordinates
  ctx.scale(dpr, dpr);

  // Draw 100x100 blue square in CSS pixels
  ctx.fillStyle = '#0284c7';
  ctx.fillRect(50, 50, 100, 100);
</script>
```
</details>

---

### Exercise 3: Architectural Decision Matrix (Hard)
- **Task**: For each of the following 4 web application scenarios, select whether **SVG**, **`<canvas>`**, or **Hybrid** is the architecturally correct choice and justify your answer based on performance and accessibility constraints:
  1. An interactive world map where users click 195 countries to view population statistics.
  2. A financial charting tool rendering 100,000 real-time cryptocurrency tick data points with zooming and panning.
  3. A responsive website navigation bar with 12 iconography buttons.
  4. A cloud architecture design editor where users connect components with lines, drag selection boxes, and inspect node properties.

<details>
<summary>View Solution</summary>

1. **Interactive World Map (195 countries)**: **SVG**.
   - *Reason*: 195 nodes is well within SVG's smooth rendering capacity (< 1,000 objects). SVG paths allow native CSS `:hover` states, keyboard tab focus, and screen reader labels (`aria-label="Brazil: 214 Million"`) on individual country paths without complex coordinate math.
2. **Financial Tick Chart (100,000 points)**: **`<canvas>` (or WebGL)**.
   - *Reason*: 100,000 SVG DOM elements would freeze the browser engine. Canvas maintains a fixed memory buffer and handles 100,000 draw operations at 60 FPS.
3. **Navbar with 12 Icon Buttons**: **SVG**.
   - *Reason*: Infinite vector sharpness, minuscule file size, seamless CSS `fill: currentColor;` styling across themes, and zero JavaScript rendering overhead.
4. **Cloud Architecture Editor**: **Hybrid (Canvas + SVG)**.
   - *Reason*: Canvas handles rendering complex background grids and hundreds of diagram blocks at high FPS; SVG overlays interactive selection boxes, drag handles, and connection endpoints with clean DOM event tracking.
</details>

---

## 12. Summary / Cheat Sheet

```
+-------------------------------------------------------------------------------------------------+
|                                  CANVAS VS. SVG CHEAT SHEET                                     |
+-------------------------------------------------------------------------------------------------+
| Feature / Criteria       | SVG                                    | <canvas>                    |
+-------------------------------------------------------------------------------------------------+
| Architectural Pattern    | Retained Mode (DOM Tree)               | Immediate Mode (Pixel Buffer)|
| Resolution Scaling       | Vector (Lossless at any scale)         | Bitmap (Needs DPR scaling)  |
| Event Binding            | Native DOM: element.addEventListener   | Manual coordinate math      |
| Styling                  | CSS classes, fill, stroke, hover       | Programmatic JS context only|
| Memory Footprint         | O(N) (Grows with object count)         | Fixed to resolution buffer  |
| Object Limit (60 FPS)    | ~1,000 objects                         | 100,000+ objects (WebGL)    |
| Accessibility (AOM)      | Native (role="img", <title>, ARIA)     | Fallback DOM subtree        |
| Best For                 | UI icons, simple charts, logos         | Games, massive data, video  |
+-------------------------------------------------------------------------------------------------+
```

---

## 13. Further Reading

- [WHATWG HTML Living Standard: The `canvas` element](https://html.spec.whatwg.org/multipage/canvas.html#the-canvas-element)
- [MDN Web Docs: Canvas tutorial](https://developer.mozilla.org/en-US/docs/Web/API/Canvas_API/Tutorial)
- [web.dev: Canvas vs SVG](https://web.dev/)
- [W3C HTML Accessibility API Mappings: Canvas Element](https://www.w3.org/TR/html-aam-1.0/#el-canvas)

---

[← Previous: Embedded SVG](05-embedded-svg.md) | [Section Index](README.md) | [Next: WebVTT & Subtitles →](07-webvtt-and-subtitles.md)
