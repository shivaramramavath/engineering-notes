# Animation Performance & 60/120fps

| Metadata | Details |
| :--- | :--- |
| **Topic** | Frame Budgets (16.6ms / 8.3ms), The Rendering Pipeline (Layout/Paint/Composite), GPU Layer Promotion, `will-change` Lifecycle, Layer Explosions |
| **Spec Level** | W3C CSS Will Change Module Level 1 / HTML Living Standard Rendering Engine |
| **Baseline Status** | Widely Available (`will-change` Baseline 2015 across all major desktop and mobile browsers) |
| **Browsers Tested** | Chrome 120+, Firefox 122+, Safari 17.2+, Edge 120+ |
| **Tested With** | Chrome DevTools Performance Profiler, Paint Flashing Tool, Safari Web Inspector Layers |
| **Module** | [08-motion](README.md) |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Usage](#4-syntax--usage)
5. [Practical Examples](#5-practical-examples)
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

On modern displays, fluid motion requires consistent frame rates:
- **60 Hz Displays**: 60 frames per second = **16.6 milliseconds** total processing budget per frame.
- **120 Hz Displays (ProMotion / High Refresh Phones)**: 120 frames per second = **8.3 milliseconds** per frame.

If the browser takes even 9ms to compute a frame on a 120Hz display, the compositor misses its vertical synchronization deadline (V-Sync). The screen drops a frame, producing visible stutter, lag, and jitter—commonly referred to in web engineering as **jank**.

High-performance animation engineering is not about writing faster JavaScript; it is about writing CSS that stays off the browser's main CPU thread and executes strictly on the **GPU Compositor Thread**:
- Understanding which CSS properties trigger expensive **Layout Reflows** vs **Repaints** vs cheap **Compositing**.
- Promoting elements to isolated GPU memory textures via `will-change` without causing catastrophic **Layer Explosions**.
- Profiling animations in browser developer tools using Paint Flashing and the Performance timeline.

---

## 2. Core Concepts

### The Browser Rendering Pipeline
Whenever an animated property changes, the browser executes up to three consecutive rendering stages:

1. **Layout (Reflow)**: The CPU calculates the physical geometry (position, width, height) of every affected element on the page. Changing one element's layout often causes a cascading reflow across all sibling and parent elements.
   - *Triggered by*: `width`, `height`, `top`, `left`, `right`, `bottom`, `margin`, `padding`, `font-size`, `display`, `border-width`.
2. **Paint (Repaint)**: The CPU fills in pixels (rasterization) for backgrounds, text, borders, and shadows into bitmap tiles.
   - *Triggered by*: `background-color`, `color`, `box-shadow`, `border-color`, `visibility`.
3. **Composite**: The GPU takes the pre-painted layer tiles from memory and combines them onto the screen, applying translation offsets, rotations, scales, and opacity blends.
   - *Triggered by*: `transform` (and `translate`/`rotate`/`scale`), `opacity`.

### The "Cheap Two" (Compositor-Only Properties)
In production CSS animations, **only two properties** are guaranteed to run 100% on the GPU compositor thread without triggering Layout or Paint:
1. **`transform`** (including independent `translate`, `rotate`, `scale`)
2. **`opacity`**

Animating anything else during continuous 60fps/120fps motion risks frame drops and battery exhaustion.

### The Role of `will-change`
`will-change` informs the browser's rendering engine that an element is expected to mutate in the near future:
```css
.modal-layer {
  will-change: transform, opacity;
}
```
- **What it does**: The browser pre-emptively allocates a dedicated GPU memory texture (compositor layer) for that element, decoupling it from the rest of the page. When the animation starts, the element is already uploaded to the GPU, eliminating the initial frame-drop hitch.
- **The Danger (Layer Explosion)**: GPU memory (VRAM) is finite. Declaring `will-change: transform` on hundreds of elements forces the device to allocate gigabytes of texture memory, crashing the browser tab on mobile devices.

---

## 3. How It Works Internally

```
Pipeline Stage Comparison: Layout vs Paint vs Composite

Mutating `margin-top: 10px` (Layout):
+------------+       +-------+       +-----------+
| Layout CPU | ====> | Paint | ====> | Composite | ===> Total: ~24ms (JANK!)
+------------+       +-------+       +-----------+

Mutating `background-color: blue` (Paint):
                     +-------+       +-----------+
      (Skipped)      | Paint | ====> | Composite | ===> Total: ~12ms (Risky)
                     +-------+       +-----------+

Mutating `translate: 0 10px` (Composite Only):
                                     +-----------+
      (Skipped)      (Skipped)       | Composite | ===> Total: ~2ms (120fps SILK!)
                                     +-----------+
```

### The `will-change` Lifecycle Management
```
User hovers parent container:
   JS or CSS: sets `will-change: transform` (Promotes to GPU layer in advance)
         |
         v
User clicks button:
   Animation executes smoothly on GPU (Zero allocation lag!)
         |
         v
Animation finishes:
   JS or CSS: sets `will-change: auto` (Releases GPU VRAM texture back to OS!)
```

---

## 4. Syntax & Usage

### Baseline & Feature Support
- **GPU Composited Transforms & Opacity**: Baseline Widely Available.
- **`will-change`**: Baseline Widely Available (Chrome 36+, Safari 9.1+, Firefox 36+, Edge 79+).

```css
/* High-Performance 120fps Card Hover Lift */
.perf-card {
  /* Rest state */
  translate: 0 0;
  opacity: 0.95;
  transition: translate 0.2s cubic-bezier(0.16, 1, 0.3, 1), opacity 0.2s ease;
}

/* Promote to layer only when user shows intent (hover over parent section) */
.card-section:hover .perf-card {
  will-change: translate;
}

.perf-card:hover {
  translate: 0 -6px;
  opacity: 1;
}

/* Clear will-change when active state finishes */
.perf-card:active {
  will-change: auto;
}
```

---

## 5. Practical Examples

### Example 1: Refactoring a Janky Layout Animation to a Pure GPU Animation
```css
/* ANTI-PATTERN: Triggers CPU Layout reflow on every frame (Causes severe stutter) */
.janky-drawer {
  position: fixed;
  top: 0;
  left: -300px;
  width: 300px;
  height: 100vh;
  transition: left 0.3s ease; /* BAD: Mutates layout geometry! */
}
.janky-drawer.open {
  left: 0;
}

/* PRODUCTION PATTERN: 100% GPU Compositor Animation (Locked 120fps) */
.smooth-drawer {
  position: fixed;
  top: 0;
  left: 0;
  width: 300px;
  height: 100vh;
  translate: -100% 0; /* GOOD: Pre-positioned, animated via GPU translation! */
  transition: translate 0.3s cubic-bezier(0.16, 1, 0.3, 1);
  will-change: translate;
}
.smooth-drawer.open {
  translate: 0 0;
}
```

### Example 2: Animating Height via `transform: scaleY()` Without Reflow
```html
<div class="accordion-item">
  <button class="accordion-trigger">Expand Server Logs</button>
  <div class="accordion-drawer">
    <div class="drawer-content">
      <p>Log line 1: Service worker registered</p>
      <p>Log line 2: Database connection pool warmed</p>
    </div>
  </div>
</div>
```

```css
/* Transforming scaleY avoids expensive height calculations */
.accordion-drawer {
  transform-origin: top center;
  scale: 1 0; /* Collapsed height */
  opacity: 0;
  transition: scale 0.25s cubic-bezier(0.16, 1, 0.3, 1), opacity 0.2s ease;
}

.accordion-item.is-open .accordion-drawer {
  scale: 1 1; /* Fully expanded */
  opacity: 1;
}
```

### Example 3: Simulating Smooth Box-Shadow Transitions Without Repaints
```css
/* Animating box-shadow directly forces expensive CPU repaints on every frame.
   Instead, cross-fade an opacity-transitioned pseudo-element! */
.fast-shadow-button {
  position: relative;
  padding: 0.75rem 1.5rem;
  background-color: #2563eb;
  color: #ffffff;
  border-radius: 6px;
  border: none;
  cursor: pointer;
  box-shadow: 0 2px 4px rgba(0, 0, 0, 0.1);
  transition: translate 0.2s ease;
}

.fast-shadow-button::after {
  content: "";
  position: absolute;
  inset: 0;
  border-radius: inherit;
  box-shadow: 0 12px 24px rgba(37, 99, 235, 0.4);
  opacity: 0;
  transition: opacity 0.2s ease;
  pointer-events: none;
}

.fast-shadow-button:hover {
  translate: 0 -2px;
}

.fast-shadow-button:hover::after {
  opacity: 1; /* GPU compositor opacity cross-fade! ZERO repaints! */
}

.fast-shadow-button:focus-visible {
  outline: 2px solid #2563eb;
  outline-offset: 2px;
}
```

### Example 4: Dynamic JavaScript `will-change` Promotion & Cleanup
```javascript
const modal = document.querySelector('.dialog-layer');
const openButton = document.querySelector('.open-modal-trigger');

// Promote to GPU layer on hover in anticipation of click!
openButton.addEventListener('mouseenter', () => {
  modal.style.willChange = 'transform, opacity';
});

openButton.addEventListener('click', () => {
  modal.classList.add('is-visible');
});

// Release GPU layer memory immediately after animation ends
modal.addEventListener('transitionend', (e) => {
  if (e.propertyName === 'transform') {
    modal.style.willChange = 'auto'; // Free up VRAM!
  }
});
```

### Example 5: DevTools Performance Verification Matrix
```css
/* Diagnostic Utility: Visualizes GPU-composited layers in DevTools */
.debug-gpu-layer {
  /* Forces layer creation if DevTools "Rendering -> Layer Borders" is active */
  transform: translateZ(0);
}
```

---

## 6. Best Practices

1. **Stick Strictly to `transform` and `opacity` for Animations**: Never animate properties that trigger Layout (`width`, `height`, `margin`, `top`, `left`).
2. **Apply `will-change` Sparingly and Temporarily**: Promote elements to GPU layers only when user intent is detected (`:hover` on parent or `mouseenter`), and release memory (`will-change: auto`) once the animation finishes.
3. **Never Put `will-change` on Universal Selectors**: Writing `* { will-change: transform; }` allocates hundreds of textures and will crash mobile browsers immediately.
4. **Use DevTools "Paint Flashing"**: In Chrome/Edge DevTools (Rendering tab), enable "Paint Flashing". If green boxes flash across the screen during your animation, you are triggering CPU repaints! Smooth animations produce zero green paint flashes.
5. **Use Pseudo-Elements for Shadow Transitions**: Cross-fade `opacity` on a `::after` shadow layer rather than transitioning `box-shadow` directly.
6. **Prefer CSS Over JavaScript RAF Loops**: CSS transitions and `@keyframes` run directly on the compositor thread and continue running at 60fps even if the main JavaScript thread is busy parsing data.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Correction |
| :--- | :--- | :--- |
| **Animating `top` / `left`** | Forces browser into full Layout reflow on every frame (16.6ms budget exceeded). | Use `translate: X Y;` or `transform: translate(...)`. |
| **Permanent `will-change: transform` on dozens of cards** | Causes GPU memory exhaustion and crashes mobile browser tabs. | Apply `will-change` dynamically on interaction or hover only. |
| **Directly transitioning `box-shadow`** | Forces CPU rasterizer to recalculate Gaussian blur filters 60 times a second. | Cross-fade `opacity` on a `::after` pseudo-element. |
| **Using `width: auto` to fixed width transitions** | Browser layout engine cannot interpolate `auto`; snaps abruptly. | Animate `max-width` or use CSS grid `grid-template-columns`. |
| **Assuming `opacity` is always free** | If an element with opacity has text that is not on its own layer, it may repaint. | Ensure layer promotion with `will-change: opacity;`. |
| **Ignoring 120Hz display budgets** | Animations running at 12ms drop frames on 120Hz ProMotion displays. | Audit animations against an 8.3ms frame budget in DevTools. |

---

## 8. Performance, Accessibility & Browser Support

### Chrome DevTools Profiling Workflow
1. Open Chrome DevTools -> Press `Ctrl+Shift+P` (or `Cmd+Shift+P`) -> Type `Show Rendering`.
2. Check **Paint flashing**: Verifies that no green rectangles flash during motion.
3. Check **Layer borders**: Cyan borders indicate GPU compositor layer boundaries.
4. Open the **Performance panel**: Record a 3-second trace while interacting with the animation. Ensure the "Main" thread is completely flat (idle) while the "GPU" or "Compositor" thread handles the motion.

---

## 9. Comparison / When to Use What

| Animated Property | Pipeline Cost | Main Thread Work | Safe for Infinite Loops | 120fps Achievable |
| :--- | :--- | :--- | :--- | :--- |
| **`translate` / `scale` / `rotate`** | **Composite Only** | **0% (GPU Thread)** | **Yes** | **Yes (100%)** |
| **`opacity`** | **Composite Only** | **0% (GPU Thread)** | **Yes** | **Yes (100%)** |
| **`box-shadow`** | Paint + Composite | ~30% (Repaints) | No (Battery drain) | No |
| **`top` / `left` / `height`** | **Layout + Paint + Composite**| **100% (Reflows)** | **Dangerous** | **No (Heavy jank)** |

---

## 10. Interview Questions

<details>
<summary>1. Describe the four phases of the browser rendering engine pipeline and explain why animating <code>transform</code> is cheaper than animating <code>left</code>.</summary>

The browser rendering pipeline consists of:
1. **JavaScript / Style**: Computes which CSS rules match elements.
2. **Layout (Reflow)**: Calculates geometric coordinates and sizes for every box on the page ($X, Y, W, H$).
3. **Paint (Repaint)**: Fills in pixels into bitmap tile textures on the CPU.
4. **Composite**: Uploads tiles to the GPU, combines them, and displays the final image.

- Animating `left` mutates geometry, forcing the browser to repeat **Layout -> Paint -> Composite** on every single frame. Recalculating page geometry across thousands of DOM nodes easily exceeds the 16.6ms (or 8.3ms) frame budget.
- Animating `transform` skips Layout and Paint entirely. The element is already cached on the GPU as a bitmap texture. The GPU simply executes a fast matrix multiplication to move the texture during the **Composite** phase, consuming near-zero CPU time.
</details>

<details>
<summary>2. What is a "layer explosion" and how does misuse of <code>will-change</code> cause it?</summary>

When `will-change: transform` is declared on an element, the browser promotes that element into an independent GPU composited layer (allocating dedicated VRAM for its bitmap texture). 

If a developer applies `will-change` globally (e.g., `.card { will-change: transform; }` across a list of 500 items), the browser is forced to allocate 500 distinct GPU memory buffers. On mobile devices with integrated graphics and limited RAM, this exhausts video memory, leading to browser crashes, tab restarts, and severe scrolling stutter.
</details>

<details>
<summary>3. How can you animate a change in <code>box-shadow</code> without triggering CPU repaints?</summary>

`box-shadow` requires Gaussian blur rasterization, which runs on the CPU paint pipeline. 

To animate a shadow at 60fps/120fps:
1. Render the resting shadow on the element itself.
2. Render the elevated hover shadow on an absolute `::after` pseudo-element with `opacity: 0`.
3. On hover, transition `opacity: 1` on the `::after` pseudo-element.
Because `opacity` runs on the GPU compositor, the browser simply cross-fades between the two pre-rendered bitmap textures, achieving a smooth shadow transition with zero CPU repaints.
</details>

<details>
<summary>4. What are the frame time budgets for 60Hz and 120Hz displays?</summary>

- **60Hz**: $\frac{1000\text{ms}}{60} \approx 16.66\text{ms}$ per frame. However, because the browser engine overhead takes ~6ms, developers realistically have about **10ms** to execute style, layout, and JavaScript.
- **120Hz (ProMotion, High-Refresh Gaming Displays)**: $\frac{1000\text{ms}}{120} \approx 8.33\text{ms}$ per frame. With browser overhead, developers have less than **4ms–5ms** of execution window.
</details>

<details>
<summary>5. What is "subpixel antialiasing loss" during layer promotion?</summary>

When an element is promoted to its own GPU compositor layer, the browser frequently disables subpixel font antialiasing (ClearType on Windows, subpixel text on macOS) and falls back to grayscale antialiasing for that layer. As a result, text inside the animated element can appear slightly thinner or shift weight the moment the animation starts or finishes.
</details>

<details>
<summary>6. How does DevTools "Paint Flashing" help diagnose animation bottlenecks?</summary>

Paint Flashing highlights any rectangular area of the screen in bright green whenever the browser performs a CPU repaint. 
During a well-engineered animation (using `transform` and `opacity`), the screen should show **zero green flashes**. If an element or surrounding container flashes green while moving, it indicates that a non-composited property (such as `margin`, `color`, or `box-shadow`) is triggering unnecessary repaints.
</details>

<details>
<summary>7. Why is a CSS transition preferred over <code>setInterval()</code> or <code>requestAnimationFrame()</code> for UI animations?</summary>

JavaScript execution (`setInterval`, `setTimeout`, and even `requestAnimationFrame`) runs on the browser's single **Main Thread**. If the main thread is blocked by heavy JSON parsing, DOM updates, or React reconciliation, the JavaScript animation loop is starved of CPU time and drops frames. 

CSS animations on compositor-only properties run on a separate **Compositor Thread** that is completely insulated from main-thread JavaScript bottlenecks, maintaining smooth 60fps/120fps motion even if JavaScript is temporarily frozen.
</details>

<details>
<summary>8. How should <code>will-change</code> be managed dynamically in production JavaScript applications?</summary>

Promote the element immediately *before* the animation is expected to start (e.g., when the user hovers over the parent container or starts a gesture), and remove it (`element.style.willChange = 'auto'`) inside a `transitionend` or `animationend` listener once the animation completes.
</details>

---

## 11. Practice Exercises

### Exercise 1: Refactor Janky Dropdown Navigation
**Goal**: Convert a dropdown menu that animates `height` and `margin-top` into a compositor-only GPU animation.

<details>
<summary>View Solution</summary>

```css
/* JANKY (Reflow):
.dropdown { height: 0; margin-top: 10px; transition: height 0.3s; }
.dropdown.open { height: 200px; margin-top: 0; }
*/

/* COMPOSITOR OPTIMIZED (120fps): */
.dropdown {
  position: absolute;
  top: 100%;
  left: 0;
  width: 240px;
  background-color: #ffffff;
  border-radius: 8px;
  box-shadow: 0 10px 15px -3px rgba(0, 0, 0, 0.1);
  
  /* GPU Compositor Properties */
  transform-origin: top center;
  scale: 1 0.95;
  opacity: 0;
  pointer-events: none;
  transition: scale 0.2s cubic-bezier(0.16, 1, 0.3, 1), opacity 0.2s ease;
}

.dropdown.open {
  scale: 1 1;
  opacity: 1;
  pointer-events: auto;
}
```
</details>

### Exercise 2: Zero-Repaint Floating Action Button
**Goal**: Build a floating action button that elevates on hover without triggering CPU box-shadow repaints.

<details>
<summary>View Solution</summary>

```css
.fab-fast {
  position: relative;
  width: 56px;
  height: 56px;
  border-radius: 50%;
  background-color: #2563eb;
  color: #ffffff;
  border: none;
  cursor: pointer;
  box-shadow: 0 4px 6px rgba(0, 0, 0, 0.1);
  translate: 0 0;
  transition: translate 0.2s cubic-bezier(0.16, 1, 0.3, 1);
}

.fab-fast::after {
  content: "";
  position: absolute;
  inset: 0;
  border-radius: inherit;
  box-shadow: 0 16px 24px rgba(37, 99, 235, 0.4);
  opacity: 0;
  transition: opacity 0.2s ease;
  pointer-events: none;
}

.fab-fast:hover {
  translate: 0 -4px;
}

.fab-fast:hover::after {
  opacity: 1;
}

.fab-fast:focus-visible {
  outline: 2px solid #2563eb;
  outline-offset: 3px;
}
```
</details>

### Exercise 3: Dynamic `will-change` Hover Promotion
**Goal**: Promote a heavy gallery card to a GPU layer on parent container hover.

<details>
<summary>View Solution</summary>

```css
.gallery-grid:hover .gallery-card {
  will-change: translate, scale;
}

.gallery-card {
  translate: 0 0;
  scale: 1;
  transition: translate 0.25s ease, scale 0.25s ease;
}

.gallery-card:hover {
  translate: 0 -8px;
  scale: 1.02;
}
```
</details>

### Exercise 4: Off-Screen Slide-In Drawer
**Goal**: Slide a mobile drawer in from the left edge using `translate` rather than `left`.

<details>
<summary>View Solution</summary>

```css
.mobile-nav-drawer {
  position: fixed;
  inset-block: 0;
  inset-inline-start: 0;
  width: 300px;
  background-color: #0f172a;
  color: #ffffff;
  translate: -100% 0;
  transition: translate 0.3s cubic-bezier(0.16, 1, 0.3, 1);
  z-index: 1000;
}

.mobile-nav-drawer.active {
  translate: 0 0;
}
```
</details>

### Exercise 5: Paint Flashing Elimination Verification
**Goal**: Author a CSS rule set for an animated badge that runs with zero green flashes in DevTools Paint Flashing.

<details>
<summary>View Solution</summary>

```css
@keyframes pulseIndicator {
  0% {
    scale: 1;
    opacity: 0.8;
  }
  50% {
    scale: 1.25;
    opacity: 1;
  }
  100% {
    scale: 1;
    opacity: 0.8;
  }
}

.performance-badge {
  width: 12px;
  height: 12px;
  border-radius: 50%;
  background-color: #10b981;
  /* Compositor properties only! */
  animation: pulseIndicator 2s ease-in-out infinite;
}
```
</details>

### Exercise 6: Frame Rate Throttling Guard
**Goal**: Automatically clamp animations to safe fallback styles when hardware is under low-power mode or user prefers reduced motion.

<details>
<summary>View Solution</summary>

```css
@media (prefers-reduced-motion: reduce) {
  .dropdown,
  .fab-fast,
  .gallery-card,
  .mobile-nav-drawer,
  .performance-badge {
    transition-duration: 0.01ms !important;
    animation-duration: 0.01ms !important;
    will-change: auto !important;
  }
}
```
</details>

---

## 12. Summary / Cheat Sheet

| Rendering Phase | Executing Hardware | Trigger Properties | Performance Cost |
| :--- | :--- | :--- | :--- |
| **Layout (Reflow)** | CPU Main Thread | `width`, `height`, `top`, `left`, `margin`, `padding` | **Catastrophic (Avoid!)** |
| **Paint (Repaint)** | CPU Main Thread | `color`, `background-color`, `box-shadow` | Moderate |
| **Composite** | **GPU Compositor Thread** | **`transform`, `translate`, `scale`, `rotate`, `opacity`** | **Optimal (Locked 120fps)** |
| `will-change` | Browser Engine | `will-change: transform;` | Promotes to GPU layer; must clean up |

---

## 13. Further Reading
- [W3C CSS Will Change Module Level 1](https://www.w3.org/TR/css-will-change-1/)
- [web.dev: Stick to Compositor-Only Properties and Manage Layer Count](https://web.dev/articles/animations-guide)
- [MDN Web Docs: will-change](https://developer.mozilla.org/en-US/docs/Web/CSS/will-change)
- [Paul Lewis: High Performance Animations](https://www.html5rocks.com/en/tutorials/speed/high-performance-animations/)
- [CSS-Tricks: Everything You Need to Know About will-change](https://css-tricks.com/almanac/properties/w/will-change/)

---

[Previous: 03-timing-functions-and-linear-easing.md](03-timing-functions-and-linear-easing.md) | [Module README](README.md) | [Next: 05-scroll-driven-animations.md](05-scroll-driven-animations.md)
