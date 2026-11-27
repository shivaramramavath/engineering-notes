# Hardware Transforms (2D & 3D)

| Metadata | Details |
| :--- | :--- |
| **Topic** | 2D/3D Transforms, Modern Independent Properties (`translate`, `rotate`, `scale`), Perspective, `preserve-3d`, 3D Card Flips |
| **Spec Level** | CSS Transforms Module Level 2 / CSS Transforms Module Level 1 |
| **Baseline Status** | Widely Available (`transform`, 3D transforms Baseline 2015; Independent properties `translate/rotate/scale` Baseline 2022) |
| **Browsers Tested** | Chrome 120+, Firefox 122+, Safari 17.2+, Edge 120+ |
| **Tested With** | Chromium Blink Compositor, GPU Layer Visualizer, Safari WebKit, Playwright |
| **Module** | [07-visual-styling](README.md) |

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

In modern web performance engineering, CSS transforms are the gold standard for high-performance visual motion. Unlike box model properties (`top`, `left`, `margin`, `width`, `height`), which trigger expensive CPU **Layout** and **Paint** recalculations on every frame, CSS transforms are executed strictly on the **Compositor thread** directly on the GPU.

Transforms allow developers to:
- **Execute 60fps/120fps Animations**: Reposition, rotate, and scale elements without causing layout thrashing or stutter.
- **Independent Transform Properties (Baseline 2022)**: Declare `translate: 0 -4px;`, `scale: 1.05;`, and `rotate: 45deg;` independently without clobbering each other.
- **Simulate True 3D Depth**: Project HTML elements into three-dimensional Euclidean space ($X, Y, Z$) with true vanishing point perspective (`perspective`), child 3D preservation (`transform-style: preserve-3d`), and backface culling (`backface-visibility: hidden`).

---

## 2. Core Concepts

### Modern Independent Transform Properties (Baseline 2022)
Historically, combining transforms required the `transform` shorthand:
```css
/* Legacy: Clobbers previous transforms if child overrides! */
.card { transform: translate(-50%, -50%) rotate(15deg) scale(1.2); }
```
CSS Transforms Level 2 introduced dedicated independent properties that cascade independently:
- `translate: <x> <y>? <z>?;` (e.g., `translate: 0 -8px;`)
- `rotate: [<axis>]? <angle>;` (e.g., `rotate: 180deg;` or `rotate: y 180deg;`)
- `scale: <x> <y>? <z>?;` (e.g., `scale: 1.05;`)

### 3D Coordinate Space
- **$+X$**: Extends to the right.
- **$+Y$**: Extends **downward** toward the bottom of the screen.
- **$+Z$**: Extends perpendicular to the screen **toward the viewer's eye**.

### Perspective & Vanishing Point
- `perspective: <length>`: Declared on the **parent container**. Establishes the distance between the viewer's eye and the $Z=0$ canvas plane (typically `800px` to `1200px`). Smaller values create extreme wide-angle distortion; larger values create subtle telephoto flattening.
- `perspective-origin`: Sets the vanishing point coordinate (defaults to `50% 50%`, center).

### Spatial Hierarchy: `preserve-3d` & `backface-visibility`
- `transform-style: preserve-3d`: Mandates that children exist in shared 3D spatial geometry. If left at `transform-style: flat` (the default), 3D child rotations are flattened into the 2D plane of the parent!
- `backface-visibility: hidden`: Hides the rear side of an element when rotated past 90 degrees in 3D space (crucial for card flipping).

---

## 3. How It Works Internally

```
Compositor Architecture: GPU Layer Offloading

1. Mutating Layout Property (top: 10px):
   JavaScript/CSS Trigger ===> [Recalculate Layout (CPU)]
                                         |
                                         v
                               [Repaint Pixels (CPU)]
                                         |
                                         v
                               [Upload to GPU (Compositor)] ===> 15fps JANK!

2. Mutating Transform (translate: 0 10px):
   JavaScript/CSS Trigger ===> [Direct GPU Compositor Layer Move] ===> 120fps SILK!
   (Zero Layout, Zero Paint!)
```

### The 3D Card Flip Anatomy
```
Container: perspective: 1000px;
Card Inner: transform-style: preserve-3d; transition: transform 0.6s;
            |
            +---> Front Face: backface-visibility: hidden;
            |
            +---> Back Face:  backface-visibility: hidden;
                              transform: rotateY(180deg);

When Card Inner rotates 180deg on Y-axis:
- Front face turns past 90deg -> Disappears!
- Back face turns into view -> Appears right-side up!
```

---

## 4. Syntax & Usage

### Baseline & Feature Support
- **Core 2D & 3D `transform`**: Baseline Widely Available.
- **Independent Properties (`translate`, `rotate`, `scale`)**: Baseline 2022 (Chrome 104+, Safari 15.4+, Firefox 72+, Edge 104+).
- **`preserve-3d` & `backface-visibility`**: Baseline Widely Available.

```css
/* Modern Independent Properties */
.floating-element {
  translate: 0 -8px;
  scale: 1.05;
  transition: translate 0.2s ease, scale 0.2s ease;
}

/* 3D Scene Root */
.scene-container {
  perspective: 1000px;
  perspective-origin: center center;
}

/* 3D Nested Object */
.spatial-cube {
  transform-style: preserve-3d;
}
```

---

## 5. Practical Examples

### Example 1: Production 3D Interactive Flip Card
```html
<div class="flip-card-scene">
  <div class="flip-card-inner">
    <div class="flip-card-face flip-card-front">
      <h3>Premium Platinum</h3>
      <p>•••• •••• •••• 4092</p>
    </div>
    <div class="flip-card-face flip-card-back">
      <div class="magnetic-strip"></div>
      <p class="cvv">CVV: 891</p>
    </div>
  </div>
</div>
```

```css
.flip-card-scene {
  width: 320px;
  height: 200px;
  perspective: 1000px; /* Establishes 3D viewing distance */
  cursor: pointer;
}

.flip-card-inner {
  position: relative;
  width: 100%;
  height: 100%;
  border-radius: 16px;
  transform-style: preserve-3d; /* Children live in true 3D space */
  transition: transform 0.6s cubic-bezier(0.16, 1, 0.3, 1);
}

.flip-card-scene:hover .flip-card-inner,
.flip-card-scene:focus-within .flip-card-inner {
  transform: rotateY(180deg);
}

.flip-card-face {
  position: absolute;
  inset: 0;
  border-radius: inherit;
  padding: 1.5rem;
  backface-visibility: hidden; /* Hides reverse side when flipped */
  display: flex;
  flex-direction: column;
  justify-content: space-between;
  box-shadow: 0 10px 15px -3px rgba(0, 0, 0, 0.1);
}

.flip-card-front {
  background: linear-gradient(135deg, #1e293b, #0f172a);
  color: #ffffff;
}

.flip-card-back {
  background: linear-gradient(135deg, #0f172a, #020617);
  color: #ffffff;
  transform: rotateY(180deg); /* Pre-rotated to face backward */
}

.magnetic-strip {
  width: 100%;
  height: 40px;
  background-color: #000000;
  margin-top: 1rem;
}
```

### Example 2: Isometric 3D Dashboard Mockup
```css
/* Tilts a flat UI dashboard into an isometric 3D presentation projection */
.isometric-stage {
  perspective: 1200px;
  display: flex;
  align-items: center;
  justify-content: center;
  padding: 4rem;
}

.isometric-dashboard {
  width: 600px;
  background-color: #ffffff;
  border-radius: 12px;
  box-shadow:
    -20px 20px 30px rgba(0, 0, 0, 0.15),
    -40px 40px 60px rgba(0, 0, 0, 0.1);
  transform: rotateX(55deg) rotateZ(-45deg);
  transform-style: preserve-3d;
  transition: transform 0.4s cubic-bezier(0.16, 1, 0.3, 1);
}

.isometric-dashboard:hover {
  transform: rotateX(45deg) rotateZ(-35deg) translateZ(20px);
}
```

### Example 3: Modern Modal Centering via `translate`
```css
/* Eliminates negative margin calculation; centers regardless of dynamic dimensions */
.modal-centered {
  position: fixed;
  top: 50%;
  left: 50%;
  translate: -50% -50%; /* Modern independent translate property */
  z-index: 1000;
  max-width: 500px;
  width: 90%;
  background: #ffffff;
  border-radius: 8px;
  padding: 2rem;
}
```

### Example 4: 3D Floating Layer Parallax Stack
```html
<div class="parallax-card">
  <div class="layer-bg"></div>
  <div class="layer-content">
    <h2>Spatial UI</h2>
    <p>Floating above background</p>
  </div>
</div>
```

```css
.parallax-card {
  perspective: 800px;
  transform-style: preserve-3d;
  width: 300px;
  height: 200px;
  position: relative;
}

.layer-bg {
  position: absolute;
  inset: 0;
  background: linear-gradient(135deg, #2563eb, #1d4ed8);
  border-radius: 12px;
  transform: translateZ(0px);
}

.layer-content {
  position: relative;
  color: #ffffff;
  padding: 1.5rem;
  /* Pops text 40px closer to the viewer along the Z-axis */
  transform: translateZ(40px);
  transform-style: preserve-3d;
}
```

### Example 5: Resolving Blur on Odd Pixel Translations
```css
/* BUG: Translating by 50% on an odd-width element (e.g., 301px * 50% = 150.5px)
   places glyphs on fractional subpixels, causing text to appear blurry! */

/* FIX: Use round() function in CSS or snap transforms with whole pixels */
.crisp-centered-box {
  position: absolute;
  top: 50%;
  left: 50%;
  translate: -50% -50%;
  /* Force hardware layer rasterization snapping */
  backface-visibility: hidden;
  transform: translateZ(0);
}
```

---

## 6. Best Practices

1. **Use Modern Independent Transform Properties**: Prefer `translate`, `rotate`, and `scale` over the monolithic `transform` property. They cascade modularly without overwriting each other.
2. **Never Animate `top`/`left` for Movement**: Always animate `translate`. `top`/`left` forces synchronous layout reflows; `translate` runs purely on the GPU compositor.
3. **Always Add `perspective` on the Parent, Not the Child**: Declare `perspective: 1000px;` on the parent container. This ensures all children share a unified, realistic vanishing point rather than each rendering with independent, colliding focal points.
4. **Always Declare `transform-style: preserve-3d` for Nested 3D Scenes**: Without `preserve-3d`, the browser flattens all child 3D transformations onto the parent's 2D plane.
5. **Add `backface-visibility: hidden` to Both Sides of a 3D Card**: Guarantees the back face does not bleed through or flicker during rotation.
6. **Honor `prefers-reduced-motion` on 3D Animations**: 3D spatial movement can trigger severe vestibular disorientation. Replace spatial card flips with subtle opacity fades for users who prefer reduced motion.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Correction |
| :--- | :--- | :--- |
| **Omitting `preserve-3d` on flip card inner wrapper** | Browser flattens the back face onto the 2D plane; card rotation appears broken or blank. | Set `transform-style: preserve-3d;` on the flipping element. |
| **Declaring `perspective` on the flipping element itself** | Each card element gets its own isolated vanishing point, distorting spatial angles. | Put `perspective: 1000px;` on the outer scene wrapper. |
| **Blurry text on 2D centered elements** | `translate: -50% -50%` on odd-pixel dimensions creates fractional subpixel rasterization. | Add `backface-visibility: hidden;` or ensure even dimensions. |
| **Overriding `transform` in hover states** | Writing `.btn:hover { transform: scale(1.1); }` destroys an existing `translate(-50%, -50%)`. | Use modern `scale: 1.1;` or repeat the translate values. |
| **Forgetting that transforms establish containing blocks** | Fixed-position modals inside a transformed ancestor become trapped inside that ancestor. | Keep modals outside transformed layout containers. |
| **Subpixel shaking during rotation** | Antialiasing shimmering occurs along edge borders during 3D rotation. | Add `outline: 1px solid transparent;` to smooth edge rasterization. |

---

## 8. Performance, Accessibility & Browser Support

### Reduced Motion Guard (Vestibular Disorder Safety)
```css
@media (prefers-reduced-motion: reduce) {
  .flip-card-inner {
    transition: none; /* Disable spatial 3D rotation */
  }
  
  .flip-card-scene:hover .flip-card-inner {
    transform: none;
  }
}
```

### Dedicated Compositor Layers via `will-change`
To instruct the browser to promote an element to its own dedicated GPU memory texture *before* an animation starts:
```css
.card {
  will-change: translate, scale;
}
```
> **Warning**: Do not apply `will-change` to hundreds of elements; GPU VRAM can easily be exhausted, degrading performance.

---

## 9. Comparison / When to Use What

| Technique | Layout Reflow | Paint Step | GPU Accelerated | Best Used For |
| :--- | :--- | :--- | :--- | :--- |
| **`translate: X Y`** | **None (0ms)** | **None (0ms)** | **Yes (Compositor)**| Moving elements, hover offsets, modal centering |
| **`top / left`** | **Yes (Full)** | **Yes (Full)** | No (CPU Layout) | Static document layout anchoring |
| **`scale: N`** | **None (0ms)** | **None (0ms)** | **Yes (Compositor)**| Button hover pulses, zoom reveals |
| **`width / height`** | **Yes (Full)** | **Yes (Full)** | No (CPU Layout) | Intrinsic box resizing |

---

## 10. Interview Questions

<details>
<summary>1. Why are CSS transforms significantly more performant than animating <code>top</code>, <code>left</code>, or <code>margin</code>?</summary>

Web browser rendering pipelines consist of four consecutive phases: **JavaScript -> Style -> Layout -> Paint -> Composite**.
- Modifying `top`, `left`, or `margin` mutates the geometric properties of the box model. The browser is forced to invalidate the layout tree and recompute the exact geometry and positions of not only that element, but all surrounding sibling and parent elements (**Layout reflow**), followed by re-rasterizing those pixels on the CPU (**Paint**).
- Modifying `translate`, `rotate`, or `scale` bypasses both the Layout and Paint phases entirely. The element is already cached on the GPU as a composited texture layer; the GPU simply updates the coordinate transform matrix on the compositor thread (**Composite phase only**), guaranteeing locked 60fps/120fps motion.
</details>

<details>
<summary>2. What is the difference between declaring <code>perspective</code> on a parent container versus declaring <code>transform: perspective(...)</code> on the element itself?</summary>

- **Parent Container (`perspective: 1000px;`)**: Establishes a **shared 3D stage** with a single unified vanishing point for all child elements. If three cards sit side-by-side, the left card tilts showing its right edge, the center card looks straight ahead, and the right card tilts showing its left edge, accurately matching human binocular vision.
- **Individual Element (`transform: perspective(1000px) rotateY(...)`)**: Each individual element generates its own private vanishing point directly at its own center. Multiple sibling cards will not visually correlate in spatial perspective.
</details>

<details>
<summary>3. What is the role of <code>transform-style: preserve-3d</code>?</summary>

By default, an element's rendering context is `transform-style: flat`. In flat mode, any 3D transformations applied to child elements are computed and immediately flattened (projected) onto the 2D plane of the parent element. 

Applying `transform-style: preserve-3d` instructs the browser engine to allow child elements to retain their independent $Z$-axis spatial coordinates, enabling them to intersect, rotate freely in 3D space, and participate in a true three-dimensional scene.
</details>

<details>
<summary>4. What are modern CSS Independent Transform Properties, and why were they added in Baseline 2022?</summary>

In CSS Transforms Level 1, all transformations had to be combined into a single `transform` shorthand string (`transform: translate(10px) rotate(45deg) scale(1.2)`). If a child class or hover state wanted to adjust only the scale, it had to repeat the entire string; otherwise, the translate and rotate were clobbered and lost.

CSS Transforms Level 2 introduced dedicated independent properties: `translate`, `rotate`, and `scale`. They can be declared, inherited, transitioned, and overridden completely independently in CSS without touching or resetting each other.
</details>

<details>
<summary>5. Why does <code>translate: -50% -50%</code> sometimes cause text to appear blurry, and how do you resolve it?</summary>

If an element has odd-numbered pixel dimensions (e.g., width of 315px), translating by 50% calculates to a fractional pixel offset ($315 \times 0.5 = 157.5\text{px}$). 

Because hardware pixels cannot be physically divided in half, the GPU rasterizer applies bilinear or bicubic filtering across the adjacent subpixels, resulting in slightly fuzzy, blurry typography. 

**Fixes**:
1. Ensure the element's width and height compute to even integer pixel values.
2. Force GPU rasterization snapping with `backface-visibility: hidden;` or `transform: translateZ(0);`.
</details>

<details>
<summary>6. How does <code>backface-visibility: hidden</code> work during a 3D card flip?</summary>

When an element is rotated in 3D space, the normal vector of its surface rotates. When the rotation angle exceeds 90 degrees (relative to the viewer's gaze), the element's reverse side is facing the camera. 

With `backface-visibility: hidden;`, the GPU compositor automatically culled (discards) the element from rendering once its surface normal points away from the camera, allowing the opposite card face underneath to be seen without flickering.
</details>

<details>
<summary>7. What happens to fixed-position elements inside a transformed container?</summary>

Applying any transform (2D or 3D) other than `none` causes that element to become the **containing block** for all descendant elements, including any descendants styled with `position: fixed`. The fixed child is trapped within the transformed container's coordinate box and will scroll with the parent rather than anchoring to the viewport.
</details>

<details>
<summary>8. How do the independent transform properties execute relative to the legacy <code>transform</code> shorthand?</summary>

Per the W3C specification, when independent properties and the `transform` shorthand are used together on the same element, the browser engine executes them in a strict mathematical order:
1. `translate` is applied first.
2. `rotate` is applied second.
3. `scale` is applied third.
4. The `transform` shorthand property matrix is applied last.
</details>

---

## 11. Practice Exercises

### Exercise 1: Build a Responsive Flip Card Component
**Goal**: Build a 3D flip card with front and back faces, smooth cubic-bezier easing, and keyboard focus support.

<details>
<summary>View Solution</summary>

```html
<div class="card-scene" tabindex="0">
  <div class="card-flipper">
    <div class="face front">Front Content</div>
    <div class="face back">Back Content</div>
  </div>
</div>
```

```css
.card-scene {
  width: 280px;
  height: 180px;
  perspective: 1000px;
  outline: none;
}

.card-scene:focus-visible {
  outline: 2px solid #2563eb;
  outline-offset: 4px;
}

.card-flipper {
  width: 100%;
  height: 100%;
  position: relative;
  transform-style: preserve-3d;
  transition: transform 0.5s cubic-bezier(0.16, 1, 0.3, 1);
}

.card-scene:hover .card-flipper,
.card-scene:focus-visible .card-flipper {
  transform: rotateY(180deg);
}

.face {
  position: absolute;
  inset: 0;
  border-radius: 12px;
  padding: 1.5rem;
  backface-visibility: hidden;
  display: flex;
  align-items: center;
  justify-content: center;
  font-weight: 700;
  color: #ffffff;
}

.front {
  background-color: #2563eb;
}

.back {
  background-color: #0f172a;
  transform: rotateY(180deg);
}
```
</details>

### Exercise 2: Master Independent Transform Transitions
**Goal**: Style a floating card that shifts up by 6px and scales by 1.03 using modern independent properties on hover.

<details>
<summary>View Solution</summary>

```css
.floating-card {
  background-color: #ffffff;
  border-radius: 8px;
  padding: 1.5rem;
  box-shadow: 0 4px 6px rgba(0, 0, 0, 0.05);
  translate: 0 0;
  scale: 1;
  transition: translate 0.2s ease, scale 0.2s ease, box-shadow 0.2s ease;
}

.floating-card:hover {
  translate: 0 -6px;
  scale: 1.03;
  box-shadow: 0 12px 20px rgba(0, 0, 0, 0.1);
}
```
</details>

### Exercise 3: Isometric Grid Plate
**Goal**: Tilt a 3x3 photo grid into an isometric projection.

<details>
<summary>View Solution</summary>

```css
.isometric-stage {
  perspective: 1000px;
  padding: 3rem;
}

.isometric-grid {
  display: grid;
  grid-template-columns: repeat(3, 120px);
  gap: 1rem;
  transform: rotateX(60deg) rotateZ(-45deg);
  transform-style: preserve-3d;
}
```
</details>

### Exercise 4: 3D Z-Axis Button Press
**Goal**: Depress a button into 3D space (`translateZ(-4px)`) on `:active`.

<details>
<summary>View Solution</summary>

```css
.spatial-btn-scene {
  perspective: 500px;
  display: inline-block;
}

.spatial-btn {
  padding: 0.75rem 1.5rem;
  background-color: #2563eb;
  color: #ffffff;
  border: none;
  border-radius: 6px;
  font-weight: 600;
  cursor: pointer;
  transform: translateZ(0);
  transition: transform 0.1s ease;
}

.spatial-btn:active {
  transform: translateZ(-6px);
}

.spatial-btn:focus-visible {
  outline: 2px solid #0f172a;
  outline-offset: 3px;
}
```
</details>

### Exercise 5: Vestibular Reduced Motion Fallback
**Goal**: Wrap a 3D flip card animation in a media query that gracefully switches to an opacity fade when reduced motion is preferred.

<details>
<summary>View Solution</summary>

```css
@media (prefers-reduced-motion: reduce) {
  .card-flipper {
    transition: none;
    transform: none !important;
  }

  .card-scene:hover .front {
    opacity: 0;
  }

  .card-scene:hover .back {
    opacity: 1;
    transform: none;
  }
}
```
</details>

### Exercise 6: Multi-Axis Independent Rotation
**Goal**: Animate a 3D badge continuously around both X and Y axes.

<details>
<summary>View Solution</summary>

```css
@keyframes spatialTumble {
  from {
    rotate: x 0deg;
  }
  50% {
    rotate: x 180deg;
  }
  to {
    rotate: x 360deg;
  }
}

.tumbling-token {
  width: 80px;
  height: 80px;
  border-radius: 50%;
  background: gold;
  animation: spatialTumble 4s linear infinite;
}

@media (prefers-reduced-motion: reduce) {
  .tumbling-token {
    animation: none;
  }
}
```
</details>

---

## 12. Summary / Cheat Sheet

| Property | Value Example | Primary Role |
| :--- | :--- | :--- |
| `translate` (Independent) | `translate: 0 -8px;` | Repositions along X/Y on GPU compositor |
| `rotate` (Independent) | `rotate: y 180deg;` | Rotates along specific axis independently |
| `scale` (Independent) | `scale: 1.05;` | Resizes element on GPU compositor |
| `perspective` | `perspective: 1000px;` | Declared on parent; sets 3D viewing distance |
| `transform-style` | `preserve-3d` | Preserves 3D spatial geometry for nested children |
| `backface-visibility` | `hidden` | Hides rear side of element when rotated past 90deg |

---

## 13. Further Reading
- [W3C CSS Transforms Module Level 2](https://www.w3.org/TR/css-transforms-2/)
- [MDN Web Docs: CSS Transforms Guide](https://developer.mozilla.org/en-US/docs/Web/CSS/CSS_transforms/Using_CSS_transforms)
- [MDN Web Docs: translate](https://developer.mozilla.org/en-US/docs/Web/CSS/translate)
- [web.dev: High performance animations](https://web.dev/articles/animations-guide)
- [David DeSandro: Intro to CSS 3D Transforms](https://3dtransforms.desandro.com/)

---

[Previous: 06-clip-path-and-masks.md](06-clip-path-and-masks.md) | [Module README](README.md) | [Next: 08-lists-and-counters.md](08-lists-and-counters.md)
