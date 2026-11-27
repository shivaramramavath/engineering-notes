# 03. Layout, Paint and Composite Costs

| Metadata | Details |
| :--- | :--- |
| **Spec Level & Status** | Browser Engine Architecture / Web Performance Standards |
| **Browsers Tested** | Chrome 120+, Firefox 125+, Safari 17.4+ |
| **Tested With** | Chrome DevTools Layers Panel, Performance Flamechart, Rendering Flags |
| **Prerequisites** | `00-setup/02-how-the-browser-renders-css.md`, `12-performance-and-debugging/01-css-performance-myths-and-realities.md` |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Usage](#4-syntax--usage)
5. Practical Examples
6. [Best Practices](#6-best-practices)
7. [Common Mistakes & Pitfalls](#7-common-mistakes--pitfalls)
8. [Performance, Accessibility & Browser Support](#8-performance-accessibility--browser-support)
9. [Comparison / When to Use What](#9-comparison--when-to-use-what)
10. [Interview Questions](#10-interview-questions)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary / Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

### The Problem
Modern displays refresh at **60 Hz (60 frames per second)**, while mobile devices and high-end monitors refresh at **120 Hz (ProMotion / 120fps)**. This establishes rigid execution budgets for smooth interaction:
- At **60 FPS**, the browser has exactly **16.6 milliseconds** per frame to execute JavaScript, recalculate styles, perform layout, rasterize pixels, and composite the screen.
- At **120 FPS**, that budget shrinks to a razor-thin **8.3 milliseconds**.

When animations or user interactions trigger CSS properties that force the CPU to recalculate the geometric layout of the entire page or repaint vast areas of pixels on every frame, execution exceeds the 16.6ms / 8.3ms window. The browser drops frames, resulting in visual stutter, jerky scrolling, high battery consumption, and severe degradation in **Interaction to Next Paint (INP)**.

### The Solution
Understanding the browser's three-stage rendering pipeline (**Layout $\rightarrow$ Paint $\rightarrow$ Composite**) allows developers to classify every CSS property into its architectural cost tier.
By confining animations, hover states, and dynamic transitions strictly to **Compositor-Only Properties** (`transform` and `opacity`), styling calculations completely bypass the CPU main thread and execute directly on dedicated GPU texture layers, guaranteeing flawless 60/120 FPS performance.

---

## 2. Core Concepts

### 1. The Three Rendering Pipeline Stages

```
┌─────────────────────────────────────────────────────────────┐
│                 BROWSER RENDERING PIPELINE                  │
└─────────────────────────────────────────────────────────────┘
  1. LAYOUT / REFLOW (Most Expensive! CPU Main Thread):
     Computes exact geometry: X, Y coordinates, Width, and Height.
     Affects the target element AND can invalidate all surrounding siblings.
     Examples: width, height, margin, padding, top, left, font-size.
                    │
                    ▼
  2. PAINT / REPAINT (Expensive! CPU Main Thread):
     Fills in visual pixels: text glyphs, colors, images, borders, shadows.
     Draws elements into one or more rasterized bitmap layers.
     Examples: color, background-color, box-shadow, border-radius, outline.
                    │
                    ▼
  3. COMPOSITE (Fastest! GPU Hardware Dedicated Thread):
     Combines individual layer bitmaps and uploads them to the screen.
     Executes on GPU without recalculating layout or repainting pixels.
     Examples: transform, opacity, filter (on promoted layers).
```

### 2. Property Pipeline Cost Matrix

| CSS Property | Triggers Layout? | Triggers Paint? | Triggers Composite? | Engine Performance |
| :--- | :--- | :--- | :--- | :--- |
| **`transform`** | ❌ NO | ❌ NO | ✅ YES | **Instant (GPU 120 FPS)** |
| **`opacity`** | ❌ NO | ❌ NO | ✅ YES | **Instant (GPU 120 FPS)** |
| **`color` / `background-color`** | ❌ NO | ✅ YES | ✅ YES | Medium (CPU Paint) |
| **`box-shadow`** | ❌ NO | ✅ YES | ✅ YES | Heavy Paint (Blur math) |
| **`top` / `left` / `bottom` / `right`** | ✅ YES | ✅ YES | ✅ YES | **Extremely Slow (CPU Reflow)** |
| **`width` / `height`** | ✅ YES | ✅ YES | ✅ YES | **Extremely Slow (CPU Reflow)** |
| **`margin` / `padding`** | ✅ YES | ✅ YES | ✅ YES | **Extremely Slow (CPU Reflow)** |
| **`font-size`** | ✅ YES | ✅ YES | ✅ YES | **Extremely Slow (CPU Reflow)** |

---

## 3. How It Works Internally

### Stacking Contexts vs. Compositor Layers
Developers frequently confuse **Stacking Contexts** with **Compositor Layers**:
1. **Stacking Context (CSS Spec concept)**:
   - Dictates paint order along the Z-axis (which element renders in front of another).
   - Created by `opacity < 1`, `z-index` on positioned elements, `transform`, `mix-blend-mode`, etc.
   - Handled completely on the CPU main thread during paint tree construction.
2. **Compositor Layer / GraphicsLayer (Browser Engine implementation concept)**:
   - A distinct physical bitmap texture allocated in GPU Video RAM (VRAM).
   - The GPU compositor can move, rotate, scale, or fade this bitmap independently without re-rasterizing the underlying HTML elements.

```
DOM TREE (CPU Main Thread)
  <div class="card"> (Normal DOM element)
  <div class="sidebar" style="will-change: transform"> (PROMOTED!)
       │
       ▼
GPU COMPOSITOR LAYERS (Hardware VRAM)
  Layer 1 (Root Canvas): [Header, Background, Cards, Footer]
  Layer 2 (Sidebar Texture): [Isolated Sidebar Bitmap]
       │
       ▼ (During transform: translateX(100px))
  GPU moves Layer 2 texture directly. Layer 1 is untouched! ZERO REPAINT!
```

---

## 4. Syntax & Usage

### 1. Compositor-Only Sliding Drawer Pattern
Never animate `left` or `margin-left`; animate `transform: translateX()`:

```css
/* ANTI-PATTERN: Forces CPU Layout & Paint on every frame */
.drawer-slow {
  position: fixed;
  left: -320px;
  width: 320px;
  transition: left 0.3s ease; /* JANKY */
}
.drawer-slow.is-open {
  left: 0;
}

/* PRODUCTION PATTERN: 100% GPU Hardware Composited */
.drawer-fast {
  position: fixed;
  left: 0;
  width: 320px;
  transform: translateX(-100%);
  transition: transform 0.3s cubic-bezier(0.16, 1, 0.3, 1);
  will-change: transform; /* Informs browser to promote layer */
}
.drawer-fast.is-open {
  transform: translateX(0);
}
```

### 2. Compositor-Only Fade & Scale Modal
```css
.modal-overlay {
  opacity: 0;
  visibility: hidden;
  transition: opacity 0.2s ease, visibility 0.2s ease;
}

.modal-dialog {
  transform: scale(0.95);
  transition: transform 0.2s cubic-bezier(0.16, 1, 0.3, 1);
  will-change: transform;
}

.modal-overlay.is-active {
  opacity: 1;
  visibility: visible;
}

.modal-overlay.is-active .modal-dialog {
  transform: scale(1);
}
```

---

## 5. Practical Examples

### Example 1: Profiling Paint Storms with Chrome DevTools
When hover states trigger expensive box shadows or borders, large areas of the screen repaint unnecessarily:

```html
<div class="card-grid">
  <div class="heavy-card">Product A</div>
  <div class="heavy-card">Product B</div>
</div>
```

```css
/* EXPENSIVE HOVER: Repaints on mouse enter and mouse leave */
.heavy-card {
  padding: 1.5rem;
  background: #ffffff;
  border-radius: 8px;
  transition: box-shadow 0.2s ease;
}
.heavy-card:hover {
  /* Forces CPU rasterization pass across the entire bounding box! */
  box-shadow: 0 20px 25px -5px rgba(0, 0, 0, 0.2);
}

/* REFACTORED COMPOSITOR HOVER: Pre-renders shadow on pseudo-element, animates opacity */
.optimized-card {
  position: relative;
  padding: 1.5rem;
  background: #ffffff;
  border-radius: 8px;
  /* Base subtle shadow */
  box-shadow: 0 1px 3px rgba(0, 0, 0, 0.1);
}

.optimized-card::after {
  content: "";
  position: absolute;
  inset: 0;
  border-radius: inherit;
  /* Heavy shadow pre-painted ONCE in advance */
  box-shadow: 0 20px 25px -5px rgba(0, 0, 0, 0.2);
  opacity: 0;
  transition: opacity 0.2s ease;
  pointer-events: none;
}

.optimized-card:hover::after {
  /* GPU compositor animates opacity with ZERO repaint! */
  opacity: 1;
}
```

### Example 2: Animating Expansions with `transform: scaleY()` instead of `height`
```css
/* ANTI-PATTERN: Animating height reflows all sibling content below */
.slow-bar {
  height: 20px;
  transition: height 0.3s ease;
}
.slow-bar.is-expanded {
  height: 200px; /* Reflows the entire page! */
}

/* PRODUCTION PATTERN: Transform scale retains fixed geometry, zero reflow */
.fast-bar {
  height: 200px;
  transform-origin: top;
  transform: scaleY(0.1);
  transition: transform 0.3s cubic-bezier(0.16, 1, 0.3, 1);
  will-change: transform;
}
.fast-bar.is-expanded {
  transform: scaleY(1);
}
```

### Example 3: The Dangerous "Layer Explosion" Problem
Promoting too many elements to GPU layers wastes hundreds of megabytes of device memory:

```css
/* DANGEROUS CODE: Spawns 1,000 separate GPU textures! */
.feed-item {
  transform: translateZ(0); /* Legacy hack to force hardware acceleration */
}
```
**Consequence**: If a list has 1,000 items, the browser allocates 1,000 textures in GPU VRAM. On mobile devices with shared memory, the browser tab runs out of memory and crashes.
**Remedy**: Promote only the scrollable container or the single active element being animated.

### Example 4: Fixed Headers and Compositor Scrolling
Ensure sticky and fixed headers do not trigger paint invalidation during high-speed scrolling:

```css
.site-header {
  position: sticky;
  top: 0;
  z-index: 100;
  background-color: rgba(255, 255, 255, 0.9);
  /* Promotes header to its own layer so page content scrolls beneath it without repainting the header */
  transform: translateZ(0);
}
```

### Example 5: High-Performance Loading Spinner
Spinners run indefinitely. If implemented poorly, they continuously keep the CPU at 100% load:

```css
/* 100% GPU-Executed Loading Spinner: 0% CPU Load */
.spinner {
  width: 2rem;
  height: 2rem;
  border: 3px solid #e2e8f0;
  border-top-color: #2563eb;
  border-radius: 50%;
  animation: spin 0.8s linear infinite;
  will-change: transform;
}

@keyframes spin {
  to {
    transform: rotate(360deg);
  }
}
```

---

## 6. Best Practices

1. **Stick to the Fast Properties for Animation**: Only animate `transform` and `opacity` in production micro-interactions, sliders, and drawers.
2. **Avoid Layout-Triggering Properties in Transitions**: Never transition `top`, `left`, `right`, `bottom`, `width`, `height`, `margin`, or `padding`.
3. **Animate Box Shadows via Pseudo-Element Opacity**: Instead of transitioning `box-shadow` directly (which forces repainting), render the shadow on a `::after` pseudo-element and transition `opacity`.
4. **Audit Layers in Chrome DevTools**: Open the **Layers** panel to verify that your page maintains a lean layer tree (<15 layers total); avoid unintended layer explosions.
5. **Remove `will-change` After Animations Finish**: Do not leave `will-change` permanently active on idle elements; toggle it on during active interaction and remove it when idle.
6. **Use Paint Flashing in DevTools**: Enable "Paint Flashing" under the Rendering tab to confirm that scrolling or hovering does not cause unnecessary green paint flashes across static content.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Consequence | Correct Pattern |
| :--- | :--- | :--- |
| **Animating `top` or `left` for mobile menus** | Drops frames, stutters, and causes high battery drain. | Animate `transform: translateX()` or `translateY()`. |
| **Using `translateZ(0)` on all list items** | Causes GPU memory exhaustion and crashes mobile browser tabs. | Apply layer promotion only to the actively animating element. |
| **Directly transitioning `box-shadow`** | Forces CPU rasterization passes across every single frame. | Pre-render shadow on `::after` and transition `opacity`. |
| **Animating `max-height` for collapsibles** | Reflows all subsequent document content on every animation frame. | Animate using CSS Grid tracks (`grid-template-rows: 0fr` to `1fr`) or transform scaling. |
| **Assuming stacking context is a GPU layer** | Elements with `z-index` still repaint on CPU unless promoted to a Compositor Layer. | Use `transform` or `will-change` to promote to a true compositor layer. |
| **Leaving `will-change` active on 500 cards** | Locks massive chunks of GPU memory into permanent allocations. | Apply `will-change` only on active hover or transition classes. |

---

## 8. Performance, Accessibility & Browser Support

### Inspecting Layers in Chrome DevTools
1. Press `Cmd+Shift+P` (Mac) or `Ctrl+Shift+P` (Windows) in DevTools.
2. Type "Layers" and select **Show Layers**.
3. DevTools renders an interactive 3D model of all GPU Compositor Layers:
   - Rotate the 3D model to see physical separation between layers.
   - Inspect the **Paints** count: well-optimized animations show **0 paints** during execution.
   - View the exact reason why each layer was promoted (e.g. `will-change: transform`, `composite after 3D transform`).

---

## 9. Comparison / When to Use What

| Technique | Pipeline Stage | CPU Cost | GPU Cost | Best Suited For |
| :--- | :--- | :--- | :--- | :--- |
| **`transform: translate()`** | Composite | **0%** | Minimal | Sliders, drawers, tooltips, dialogs |
| **`opacity`** | Composite | **0%** | Minimal | Fades, modals, dropdown reveals |
| **`color` / `background`** | Paint | Moderate | None | Button hover color swaps, tab state changes |
| **`top` / `left`** | Layout + Paint + Composite | **Extreme** | High | Static layout placement ONLY (Never animate!) |
| **`width` / `height`** | Layout + Paint + Composite | **Extreme** | High | Initial document layout (Never animate!) |

---

## 10. Interview Questions

### Q1: Detail the three primary stages of the browser rendering pipeline and explain their execution hierarchy.
<details>
<summary>Answer</summary>
1. **Layout (Reflow)**: The browser calculates the physical geometric box model dimensions ($X, Y$ positions, width, height) for each element in the Render Tree. Because elements in standard document flow affect one another, altering the geometry of one element can force the browser to recalculate the layout of its ancestors, siblings, and descendants.
2. **Paint (Rasterization)**: The browser takes the layout geometry and fills in pixels on memory bitmaps—drawing text glyphs, background colors, borders, image pixels, and shadows.
3. **Composite**: The browser takes individual bitmap layers, uploads them as textures to the GPU, and composites them together into the final image rendered onto the physical screen.

**Hierarchy**: Changes that trigger Layout automatically force a subsequent Paint and Composite pass. Changes that trigger Paint force a subsequent Composite pass. Changes that trigger Composite bypass both Layout and Paint entirely.
</details>

### Q2: Why does animating `transform` bypass the Layout and Paint stages completely?
<details>
<summary>Answer</summary>
`transform` does not affect the element's geometry within standard document flow; its bounding box in the layout tree remains unchanged, so surrounding sibling elements do not need to reflow.
Furthermore, modern browser engines promote transformed elements to their own dedicated GPU Compositor Layer. The element's visual pixels are rasterized once into a GPU texture. During the animation, the browser simply passes transformation matrices (translation, rotation, scale) directly to the GPU hardware, which renders the texture at new screen coordinates without executing CPU layout reflows or repainting pixels.
</details>

### Q3: What is "Layer Explosion," and how can legacy performance hacks cause mobile crashes?
<details>
<summary>Answer</summary>
In the early days of mobile web performance, developers discovered that adding `transform: translateZ(0)` or `will-change: transform` promoted elements to GPU layers, eliminating scroll stutter.
However, developers began applying this hack indiscriminately (e.g. `.card { transform: translateZ(0); }`).

**Layer Explosion** occurs when hundreds or thousands of elements are promoted to individual GPU layers. Each layer requires physical Video RAM (VRAM) to store its texture bitmap. On mobile devices with constrained, shared system memory, thousands of allocated textures exhaust memory, causing frame drops, battery depletion, and out-of-memory browser tab crashes.
</details>

### Q4: How does the "Pseudo-Element Opacity Technique" solve the performance penalty of animating `box-shadow`?
<details>
<summary>Answer</summary>
Directly animating `box-shadow` (e.g. `box-shadow 0.2s ease` on `:hover`) forces the CPU to recalculate expensive Gaussian blur convolution matrices across every frame of the animation, triggering intense Paint invalidations.

The technique:
1. Renders the elevated, heavy box-shadow on a `::after` pseudo-element positioned identically over the card.
2. Sets `opacity: 0` on `::after` by default. The shadow is rasterized once by the browser.
3. On `:hover`, the CSS transitions `opacity: 1` on `::after`.
Because `opacity` is a compositor-only property, the GPU fades the pre-rasterized shadow layer in and out with zero CPU paint calculations, maintaining solid 60/120 FPS.
</details>

### Q5: What is the difference between a Stacking Context and a Compositor Layer?
<details>
<summary>Answer</summary>
- **Stacking Context**: A conceptual rule in the CSS specification governing the order in which elements are painted along the Z-axis (front-to-back ordering). It is created by properties like `z-index`, `opacity < 1`, `mix-blend-mode`, or `transform`. It exists on the CPU main thread during paint tree construction and does not imply GPU hardware acceleration.
- **Compositor Layer**: A concrete architectural implementation inside the browser engine (Blink/WebKit/Gecko). It is a distinct, physical bitmap surface allocated in GPU memory. Stacking contexts can exist within a single compositor layer; an element only becomes a compositor layer if it is promoted via `will-change`, 3D transforms, or video/canvas tags.
</details>

### Q6: Why should `will-change` be applied right before an animation and removed immediately after?
<details>
<summary>Answer</summary>
`will-change: transform` instructs the browser: *"This element is about to animate, so allocate GPU memory and promote it to a compositor layer immediately."*
If `will-change` is declared permanently on hundreds of static elements in global stylesheets, the browser permanently locks up GPU VRAM and compositor resources for elements that are not doing anything.
Best practice is to apply `will-change` conditionally—such as on `:hover`, on pointer down, or by adding a temporary class in JavaScript right before an animation triggers, and removing it via `animationend` or `transitionend`.
</details>

### Q7: How can you detect whether an animation is running on the GPU Compositor thread or the CPU Main thread in Chrome DevTools?
<details>
<summary>Answer</summary>
In Chrome DevTools:
1. Open the **Performance** panel and record a trace during the animation.
2. In the flame chart, inspect the **GPU** track and the **Main** thread track.
3. If the animation is compositor-driven:
   - The Main thread shows almost zero activity.
   - The **Chrome Compositor** thread shows smooth, periodic execution blocks.
4. If the animation is CPU-driven (e.g. animating `top`):
   - The Main thread shows continuous purple slices labeled **Layout** and green slices labeled **Paint** on every frame.
</details>

### Q8: What is the frame budget in milliseconds for a 120Hz display, and what happens if a frame takes 10ms to execute?
<details>
<summary>Answer</summary>
On a 120Hz display:
$$\text{Frame Budget} = \frac{1000\text{ms}}{120\text{ frames}} \approx 8.33\text{ms}$$
If frame execution takes **10ms**:
The browser misses the hardware refresh deadline for that frame. The display hardware is forced to re-display the previous frame while waiting for the next buffer swap (a **dropped frame** or **jank**). The user perceives this as a micro-stutter or hitch in animation continuity.
</details>

---

## 11. Practice Exercises

### Exercise 1: Convert a Slow Slide-Out Menu into a 120 FPS Compositor Animation
**Objective**: Refactor the following slow, layout-triggering navigation drawer into an optimized compositor-only transition.

```css
/* BRITTLE CODE TO REFACTOR */
.side-drawer {
  position: fixed;
  top: 0;
  right: -300px;
  width: 300px;
  height: 100vh;
  transition: right 0.25s ease;
}
.side-drawer.open {
  right: 0;
}
```

<details>
<summary>Solution</summary>

```css
/* REFACTORED COMPOSITOR-ONLY PATTERN */
.side-drawer {
  position: fixed;
  top: 0;
  right: 0;
  width: 300px;
  height: 100vh;
  /* Positioned off-screen via GPU hardware translation */
  transform: translateX(100%);
  transition: transform 0.25s cubic-bezier(0.16, 1, 0.3, 1);
  will-change: transform;
  z-index: 1000;
}

.side-drawer.open {
  /* Smoothly glides into view with zero Layout and zero Paint */
  transform: translateX(0);
}
```
</details>

### Exercise 2: Zero-Paint Hover Elevation Card
**Objective**: Build a product card that elevates smoothly with an intense drop-shadow on hover, using pseudo-element opacity to ensure zero CPU paint recalculations during the hover transition.

<details>
<summary>Solution</summary>

```html
<article class="elevation-card">
  <h3>Scalable Cache Clusters</h3>
  <p>In-memory data grid with automated failover replication.</p>
</article>
```

```css
.elevation-card {
  position: relative;
  background: #ffffff;
  border-radius: 0.75rem;
  padding: 1.5rem;
  border: 1px solid #e2e8f0;
  cursor: pointer;
  /* Base subtle shadow */
  box-shadow: 0 1px 3px rgba(0, 0, 0, 0.05);
  transform: translateY(0);
  transition: transform 0.2s cubic-bezier(0.16, 1, 0.3, 1);
  will-change: transform;
}

/* Pseudo-element pre-renders heavy shadow once */
.elevation-card::after {
  content: "";
  position: absolute;
  inset: 0;
  border-radius: inherit;
  box-shadow: 0 20px 25px -5px rgba(0, 0, 0, 0.15), 0 8px 10px -6px rgba(0, 0, 0, 0.1);
  opacity: 0;
  transition: opacity 0.2s cubic-bezier(0.16, 1, 0.3, 1);
  pointer-events: none;
}

.elevation-card:hover {
  /* Lift card slightly on GPU */
  transform: translateY(-4px);
}

.elevation-card:hover::after {
  /* Fade pre-rendered shadow in on GPU */
  opacity: 1;
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **Layout (Reflow)**: Calculates geometric boundaries ($X, Y, W, H$). Most expensive CPU operation. Triggered by `top`, `left`, `width`, `height`, `margin`.
- **Paint (Repaint)**: Fills in pixels on bitmaps. Expensive CPU operation. Triggered by `background`, `color`, `box-shadow`.
- **Composite**: Assembles bitmap textures on GPU. Blazing fast (0ms CPU). Triggered by `transform` and `opacity`.
- **Frame Budgets**: 60 FPS = **16.6ms**; 120 FPS = **8.3ms**.
- **Shadow Trick**: Pre-render heavy `box-shadow` on `::after` and transition `opacity` to avoid repaints.
- **Layer Explosion**: Never apply `translateZ(0)` or `will-change` globally; promote only active elements.
- **DevTools**: Use the **Layers** panel to inspect GPU textures and the **Rendering** panel $\rightarrow$ "Paint Flashing" to audit repaints.

---

## 13. Further Reading
- [web.dev: Stick to Compositor-Only Properties and Manage Layer Count](https://web.dev/stick-to-compositor-only-properties-and-manage-layer-count/)
- [CSS Triggers: Complete Guide to What Properties Trigger Layout, Paint, or Composite](https://csstriggers.com/)
- [Google Developers: Rendering Performance](https://developer.chrome.com/docs/devtools/performance/)
- [Smashing Magazine: The Mystery of CSS will-change](https://www.smashingmagazine.com/2016/12/gpu-animation-doing-it-right/)
- [MDN Web Docs: CSS Painting API & Compositing](https://developer.mozilla.org/en-US/docs/Web/Performance/Animation_performance_and_frame_rate)

---

| Previous Note | Next Note |
| :--- | :--- |
| [02. Render Blocking and Critical CSS](02-render-blocking-and-critical-css.md) | [04. Containment and Content Visibility](04-containment-and-content-visibility.md) |
