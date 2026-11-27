# Timing Functions & Mathematical Easing

| Metadata | Details |
| :--- | :--- |
| **Topic** | Acceleration Curves, `cubic-bezier()` Math, Overshoot Springs, `steps()` Frame Sprites, Modern `linear()` Complex Physics Easing |
| **Spec Level** | CSS Easing Functions Level 1 / CSS Easing Functions Level 2 |
| **Baseline Status** | Widely Available (`cubic-bezier`, `steps` Baseline 2015; `linear()` complex easing Baseline 2023 across all modern engines) |
| **Browsers Tested** | Chrome 120+, Firefox 122+, Safari 17.2+, Edge 120+ |
| **Tested With** | Chromium Blink Compositor, Safari WebKit, Playwright, Chrome DevTools Easing Editor |
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

In physical reality, objects never move at constant, linear velocities. Mass, friction, inertia, and momentum dictate that real objects accelerate smoothly from rest, decelerate before stopping, and exhibit elastic recoil when encountering resistance.

A user interface animated with pure `linear` easing feels robotic, cheap, and unnatural. In contrast, thoughtful easing curves provide organic weight and polish:
- **`cubic-bezier()`**: Standard parametric curve defined by two control points ($P_1$ and $P_2$). By extending the $Y$-coordinates above $1.0$ or below $0.0$, developers create tactile spring overshoots and anticipation pullbacks.
- **`steps()`**: Quantizes time into discrete steps, essential for retro pixel sprite animations, clock second hands, and terminal typing effects.
- **The `linear()` Revolution (Baseline 2023)**: Solves the fundamental limitation of cubic Bézier curves (which can only have one peak and cannot oscillate). The modern `linear()` function enables piecewise mathematical approximations of **true decaying spring physics** and multi-bounce collisions directly in pure CSS without loading 30 KB JavaScript physics libraries.

---

## 2. Core Concepts

### The Standard Easing Keywords
- `linear`: Constant velocity ($0.0, 0.0, 1.0, 1.0$). Best for color cycles, infinite spinners, and camera pans.
- `ease` (Default): Slow start, fast middle, slow end (`cubic-bezier(0.25, 0.1, 0.25, 1.0)`).
- `ease-in`: Accelerates from zero velocity (`cubic-bezier(0.42, 0, 1.0, 1.0)`). Best for elements exiting the screen.
- `ease-out`: Decelerates to zero velocity (`cubic-bezier(0, 0, 0.58, 1.0)`). Best for elements entering the screen.
- `ease-in-out`: Symmetric acceleration and deceleration.

### Anatomy of `cubic-bezier(x1, y1, x2, y2)`
A cubic Bézier curve is bounded between $(0, 0)$ and $(1, 1)$, where the $X$-axis represents normalized time and the $Y$-axis represents animation progress:
- **$X$-Coordinates ($x_1, x_2$)**: Strictly clamped between `0.0` and `1.0` (time cannot travel backward).
- **$Y$-Coordinates ($y_1, y_2$)**: Unconstrained! Can exceed `1.0` (causing an **overshoot bounce**) or dip below `0.0` (causing an **anticipation recoil**).

### The Stepped Timing Function: `steps(n, <jump-term>)`
Divides the animation duration into $N$ equal intervals:
- `steps(4, jump-end)`: Traditional step function.
- Jump terms: `jump-start`, `jump-end`, `jump-none`, `jump-both`.
- Keywords: `step-start` ($\equiv \text{steps}(1, \text{jump-start})$), `step-end`.

### The Modern `linear()` Easing Function (Baseline 2023)
Allows defining arbitrary piecewise linear stops that the browser interpolates linearly between:
```css
/* Spring bounce simulation in pure CSS */
animation-timing-function: linear(
  0, 0.063, 0.25 18.2%, 0.625 36.4%, 0.875 54.5%,
  1.063 72.7%, 0.984 86.4%, 1
);
```
Enables multi-bounce collisions, elastic overshoots, and organic fluid motion.

---

## 3. How It Works Internally

```
Cubic Bézier vs Modern linear() Spring Physics

1. Standard cubic-bezier(0.34, 1.56, 0.64, 1) (Single Overshoot):
   Progress (Y)
     1.5 |          _--_   <=== Single overshoot peak
     1.0 |        /      \________ (Settles smoothly at 1.0)
     0.5 |       /
     0.0 +------+-----------------
                0.5              1.0 Time (X)

2. Modern linear() Spring Function (Multi-Decay Oscillations):
   Progress (Y)
     1.2 |       _--_
     1.0 |     /      \   _/\   _   <=== Multiple decaying bounces!
     0.8 |    /         \/   \/  \_ (True physical spring simulation!)
     0.0 +---+-----------------------
             0.2   0.4   0.6 0.8 1.0 Time (X)
```

---

## 4. Syntax & Usage

### Baseline & Feature Support
- **Standard Easing & `cubic-bezier()`**: Baseline Widely Available.
- **`steps()` with jump terms**: Baseline Widely Available.
- **`linear()` piecewise easing**: Baseline 2023 (Chrome 113+, Safari 17.2+, Firefox 112+, Edge 113+).

```css
/* Snappy Contemporary Elastic Pop Curve */
.pop-card {
  transition: transform 0.3s cubic-bezier(0.34, 1.56, 0.64, 1);
}

/* 8-Frame Retro Sprite Sheet Animation */
.game-character {
  animation: runCycle 0.8s steps(8, jump-end) infinite;
}

/* Modern Baseline 2023 Spring Easing */
.springy-modal {
  transition: translate 0.6s linear(
    0, 0.009, 0.035 2.1%, 0.141 4.4%, 0.723 12.9%, 0.938 16.7%,
    1.017 20.2%, 1.049 22.8%, 1.054 25.5%, 1.042 28.5%,
    1.016 32.5%, 0.998 36.8%, 0.991 41.5%, 0.992 46.5%,
    1 58.5%
  );
}
```

---

## 5. Practical Examples

### Example 1: Physical Elastic Drawer Spring via `linear()`
```html
<aside class="elastic-drawer">
  <div class="drawer-inner">
    <h3>System Status</h3>
    <p>All distributed nodes operational.</p>
  </div>
</aside>
```

```css
.elastic-drawer {
  position: fixed;
  bottom: 0;
  left: 0;
  right: 0;
  height: 280px;
  background-color: #0f172a;
  color: #ffffff;
  padding: 2rem;
  border-top: 1px solid #334155;
  translate: 0 100%;
  
  /* Modern CSS Spring Easing (Zero JavaScript!) */
  transition: translate 0.75s linear(
    0, 0.006, 0.025 2.8%, 0.101 6.1%, 0.539 18.9%, 0.721 25.3%,
    0.849 31.6%, 0.931 38.0%, 0.977 44.3%, 0.999 50.6%,
    1.007 57.0%, 1.008 63.3%, 1.004 72.8%, 1
  );
}

.elastic-drawer.is-open {
  translate: 0 0;
}
```

### Example 2: Retro Sprite Sheet Walk Cycle via `steps()`
```html
<div class="sprite-stage">
  <div class="sprite-runner"></div>
</div>
```

```css
.sprite-stage {
  width: 64px;
  height: 64px;
  overflow: hidden;
}

.sprite-runner {
  width: 512px; /* 8 frames * 64px = 512px */
  height: 64px;
  background-image: url("/assets/runner-8frames.png");
  
  /* steps(8) jumps strictly between the 8 frames without blurry interpolation */
  animation: playSprite 0.6s steps(8, jump-end) infinite;
}

@keyframes playSprite {
  from { translate: 0 0; }
  to { translate: -512px 0; }
}
```

### Example 3: Anticipation Recoil Button Hover
```css
/* Dips downward slightly (anticipation) before rocketing upward */
.rocket-button {
  padding: 0.875rem 1.75rem;
  background-color: #2563eb;
  color: #ffffff;
  border: none;
  border-radius: 8px;
  cursor: pointer;
  translate: 0 0;

  /* Y1 is negative (-0.35), causing an initial downward dip before rising */
  transition: translate 0.3s cubic-bezier(0.6, -0.35, 0.2, 1.4);
}

.rocket-button:hover {
  translate: 0 -8px;
}

.rocket-button:focus-visible {
  outline: 2px solid #2563eb;
  outline-offset: 3px;
}
```

### Example 4: Terminal Typing Text Effect via `steps()`
```html
<div class="terminal-window">
  <p class="typewriter-text">Initialising quantum hypervisor...</p>
</div>
```

```css
@keyframes typing {
  from { width: 0; }
  to { width: 34ch; } /* 34 characters */
}

@keyframes blinkCursor {
  50% { border-color: transparent; }
}

.typewriter-text {
  font-family: ui-monospace, SFMono-Regular, monospace;
  font-size: 1.125rem;
  color: #10b981;
  white-space: nowrap;
  overflow: hidden;
  border-right: 3px solid #10b981;
  width: 34ch;
  
  /* steps(34) reveals one character per discrete step */
  animation:
    typing 2.5s steps(34) forwards,
    blinkCursor 0.75s step-end infinite;
}
```

### Example 5: High-Performance Natural Deceleration
```css
/* Emulates native mobile momentum scrolling deceleration */
.momentum-dismiss {
  /* Fast start, ultra-smooth gradual tail stop */
  transition: translate 0.4s cubic-bezier(0.16, 1, 0.3, 1);
}
```

---

## 6. Best Practices

1. **Use `cubic-bezier(0.16, 1, 0.3, 1)` as Your Default Ease-Out**: The standard browser `ease-out` is sluggish; `(0.16, 1, 0.3, 1)` provides the crisp, modern feel used by iOS and macOS.
2. **Deploy `linear()` for Complex Physical Springs**: Use the modern `linear()` easing function for modal popups, notification toasts, and sliding drawers to emulate real mass and tension without JavaScript physics libraries.
3. **Use `steps(n)` for Discrete Frame Animations**: Whenever animating sprite sheets or typewriters, always use `steps()` to eliminate unwanted intermediate frame blur.
4. **Keep Easing Durations Aligned with Physics**: A spring curve with multiple bounces needs a longer duration (`0.6s`–`0.8s`) so the oscillations are perceptible; a standard snap transition needs a short duration (`0.2s`–`0.25s`).
5. **Always Test Recoil and Overshoot Boundaries**: Ensure that negative or overshoot $Y$-coordinates (`y > 1.0`) do not cause elements to clip outside `overflow: hidden` parent containers.
6. **Disable Bouncing Curves in `prefers-reduced-motion`**: Spring oscillations and bouncy overshoots are particularly disorienting for users with vestibular disorders.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Correction |
| :--- | :--- | :--- |
| **Invalid $X$ coordinates in `cubic-bezier()`** | Declaring $X < 0$ or $X > 1$ is syntactically invalid (time cannot reverse). | Keep $x_1$ and $x_2$ strictly within `[0.0, 1.0]`. |
| **Using `linear` on UI buttons** | Feels rigid, mechanical, and unpolished. | Use `ease` or custom `cubic-bezier()`. |
| **Overshoot clipped by `overflow: hidden`** | An element with $y > 1.0$ bounces past container boundary and gets sliced off. | Add padding or avoid hard container clipping. |
| **Blurry sprite sheet animation** | Using `linear` or `ease` causes the sprite sheet to slide smoothly, blurring frames together. | Use `steps(frame-count, jump-end)`. |
| **Overly aggressive spring oscillations** | Excessive multi-bounce animations feel gimmicky and delay user interaction. | Keep spring oscillations subtle (1 to 2 micro-rebounds maximum). |
| **Missing fallback for `linear()` on pre-2023 browsers** | Older browsers discard the `linear()` easing declaration. | Provide a standard `cubic-bezier()` fallback above `linear()`. |

---

## 8. Performance, Accessibility & Browser Support

### GPU Acceleration of Complex Easing
Whether using simple `ease` or a 30-stop `linear()` spring curve, the browser engine samples the mathematical curve into a discrete lookup table during the styling phase. **Interpolation runs entirely on the GPU compositor thread**, incurring zero performance penalty regardless of how complex the mathematical equation is.

---

## 9. Comparison / When to Use What

| Easing Type | Max Peaks / Bounces | Step-Based | Can Overshoot ($> 1.0$) | Best Used For |
| :--- | :--- | :--- | :--- | :--- |
| **Standard Keywords** | 0 | No | No | Basic color transitions, simple fades |
| **`cubic-bezier()`** | 1 peak | No | **Yes (Single spring overshoot)**| Snappy button pops, cards, standard UI |
| **`steps()`** | N/A | **Yes** | No | Sprite sheets, terminal typing, pixel art |
| **Modern `linear()`** | **Infinite (Multi-bounce)**| No | **Yes (True physics springs)**| Floating modals, notifications, sliding sheets |

---

## 10. Interview Questions

<details>
<summary>1. Why can the $Y$-coordinates in <code>cubic-bezier(x1, y1, x2, y2)</code> exceed $1.0$ or fall below $0.0$, but the $X$-coordinates cannot?</summary>

The $X$-axis of a CSS timing curve represents **time progress**. Physical time in animation can never run backward or exceed the scheduled timeline boundary; therefore, $x_1$ and $x_2$ must strictly satisfy $0.0 \le x \le 1.0$.

The $Y$-axis represents **property progress** (e.g., position, scale, opacity). Allowing $Y$ to exceed $1.0$ means the element temporarily travels past its destination before pulling back (an **overshoot bounce**). Allowing $Y$ to fall below $0.0$ means the element moves slightly in the opposite direction before accelerating toward the target (an **anticipation recoil**).
</details>

<details>
<summary>2. What problem does the modern CSS <code>linear()</code> easing function solve that cubic Bézier curves could never solve?</summary>

A cubic Bézier curve is governed by a 3rd-degree polynomial equation with strictly two control points. Mathematically, it can produce at most **one single inflection peak**. It is physically impossible for a cubic Bézier curve to oscillate, bounce twice, or simulate a decaying spring.

The CSS `linear()` easing function (Baseline 2023) allows developers to define a piecewise sequence of coordinates and percentage stops. The browser performs fast linear interpolation between these stops, enabling multi-bounce physics, elastic dampening, and complex easing trajectories in pure CSS.
</details>

<details>
<summary>3. How does <code>steps(n, jump-end)</code> differ from <code>steps(n, jump-start)</code>?</summary>

- `jump-end` (default): The first step jump occurs at the **end** of the first interval ($t = \frac{1}{n}$). The element stays at its initial $0\%$ state for the first time chunk.
- `jump-start`: The first step jump occurs at the **start** of the animation ($t = 0$). The element immediately jumps to the first step value the instant the animation begins.
</details>

<details>
<summary>4. What is the cubic Bézier equivalent of the classic iOS spring curve?</summary>

A widely adopted cubic Bézier approximation for the snappy, high-momentum iOS/macOS interface feel is:
```css
cubic-bezier(0.16, 1, 0.3, 1)
```
It features an extremely rapid initial acceleration followed by a long, graceful, asymptotic deceleration curve.
</details>

<details>
<summary>5. How do you create an overshoot button pop effect using <code>cubic-bezier</code>?</summary>

Set $y_2$ to a value greater than $1.0$ on the `scale` or `translate` property:
```css
.pop-btn:hover {
  scale: 1.1;
  transition: scale 0.25s cubic-bezier(0.34, 1.56, 0.64, 1);
}
```
The button scales up to approximately $1.15$ before snapping back to rest at $1.10$.
</details>

<details>
<summary>6. How can you provide a safe fallback for the modern <code>linear()</code> function on older browsers?</summary>

Declare a standard `cubic-bezier` timing function immediately before the `linear()` declaration:
```css
.modal {
  /* Fallback for pre-2023 browsers */
  transition: transform 0.5s cubic-bezier(0.34, 1.56, 0.64, 1);
  /* Modern Baseline 2023 piecewise spring */
  transition: transform 0.6s linear(0, 0.05 10%, 0.7 40%, 1.05 70%, 1);
}
```
Browsers that do not recognize `linear()` will discard it and utilize the preceding cubic Bézier curve.
</details>

<details>
<summary>7. What is the performance impact of a <code>linear()</code> function with 40 coordinate stops?</summary>

Near zero. When CSS parses a `linear()` timing function, the browser pre-calculates the stops into a compact lookup table on the main thread during stylesheet compilation. When the animation runs, the GPU compositor samples the pre-computed table. It does not perform dynamic math on every frame.
</details>

<details>
<summary>8. Why should you avoid spring/bounce curves on elements entering from off-screen?</summary>

If an element enters from off-screen (e.g., `translate: 0 -100vh`) with an anticipation curve ($y < 0$), the element will first move further off-screen before entering, delaying the visual response. 
Entrance animations should use **deceleration curves** (`ease-out`, `(0.16, 1, 0.3, 1)`), entering the screen at maximum velocity and settling gracefully.
</details>

---

## 11. Practice Exercises

### Exercise 1: Build an Elastic Pop-In Badge
**Goal**: Style a notification pill that pops into view with a playful 1.5x overshoot bounce.

<details>
<summary>View Solution</summary>

```css
@keyframes popIn {
  from {
    scale: 0;
    opacity: 0;
  }
  to {
    scale: 1;
    opacity: 1;
  }
}

.elastic-badge {
  display: inline-flex;
  padding: 0.25rem 0.75rem;
  background-color: #ef4444;
  color: #ffffff;
  border-radius: 9999px;
  font-weight: 700;
  font-size: 0.75rem;
  
  /* Cubic bezier overshoot: Y2 = 1.6 */
  animation: popIn 0.35s cubic-bezier(0.34, 1.6, 0.64, 1) forwards;
}
```
</details>

### Exercise 2: Pure CSS Decaying Spring with `linear()`
**Goal**: Build a modal entrance that bounces and settles naturally using modern piecewise `linear()`.

<details>
<summary>View Solution</summary>

```css
.spring-modal {
  max-width: 450px;
  padding: 2rem;
  background-color: #ffffff;
  border-radius: 12px;
  box-shadow: 0 20px 25px -5px rgba(0, 0, 0, 0.1);
  translate: 0 100px;
  opacity: 0;
  
  /* Fallback */
  transition: translate 0.5s cubic-bezier(0.34, 1.5, 0.64, 1), opacity 0.3s ease;
}

.spring-modal.active {
  translate: 0 0;
  opacity: 1;
  
  /* Modern Baseline 2023 Spring */
  transition-timing-function: linear(
    0, 0.009, 0.035 2.1%, 0.141 4.4%, 0.723 12.9%, 0.938 16.7%,
    1.017 20.2%, 1.049 22.8%, 1.054 25.5%, 1.042 28.5%,
    1.016 32.5%, 0.998 36.8%, 0.991 41.5%, 0.992 46.5%, 1 58.5%
  );
  transition-duration: 0.7s;
}
```
</details>

### Exercise 3: 4-Frame Retro Loading Spinner via `steps()`
**Goal**: Create a rotating cross spinner that snaps between 4 quadrants at 90-degree increments.

<details>
<summary>View Solution</summary>

```css
@keyframes quadrantSnap {
  from { rotate: 0deg; }
  to { rotate: 360deg; }
}

.pixel-spinner {
  width: 24px;
  height: 24px;
  border: 3px solid #cbd5e1;
  border-top-color: #2563eb;
  border-radius: 50%;
  animation: quadrantSnap 0.8s steps(4, jump-end) infinite;
}
```
</details>

### Exercise 4: Anticipation Pullback Button
**Goal**: Build a button that pulls back by 3px before springing forward by 6px on hover.

<details>
<summary>View Solution</summary>

```css
.anticipation-btn {
  padding: 0.75rem 1.5rem;
  background-color: #0f172a;
  color: #ffffff;
  border-radius: 6px;
  border: none;
  cursor: pointer;
  translate: 0 0;
  transition: translate 0.25s cubic-bezier(0.6, -0.4, 0.2, 1.4);
}

.anticipation-btn:hover {
  translate: 0 -6px;
}

.anticipation-btn:focus-visible {
  outline: 2px solid #2563eb;
  outline-offset: 3px;
}
```
</details>

### Exercise 5: Linear Continuous Radar Rotation
**Goal**: Ensure a radar scanner sweeps with constant angular velocity using `linear`.

<details>
<summary>View Solution</summary>

```css
@keyframes sweep {
  from { rotate: 0deg; }
  to { rotate: 360deg; }
}

.radar-sweep {
  animation: sweep 3s linear infinite;
}
```
</details>

### Exercise 6: Accessible Spring Override
**Goal**: Neutralize high-overshoot curves to standard linear fades when reduced motion is preferred.

<details>
<summary>View Solution</summary>

```css
@media (prefers-reduced-motion: reduce) {
  .elastic-badge,
  .spring-modal,
  .anticipation-btn {
    transition-timing-function: ease-out !important;
    animation-timing-function: ease-out !important;
  }
}
```
</details>

---

## 12. Summary / Cheat Sheet

| Timing Function | Syntax Example | Key Role |
| :--- | :--- | :--- |
| `linear` | `transition-timing-function: linear;` | Constant velocity (spinners, color cycles) |
| `cubic-bezier()` | `cubic-bezier(0.16, 1, 0.3, 1)` | Modern high-momentum deceleration (Standard UI) |
| `cubic-bezier()` (Spring)| `cubic-bezier(0.34, 1.56, 0.64, 1)`| Single elastic overshoot bounce |
| `steps(n)` | `steps(8, jump-end)` | Quantized discrete steps (sprite sheets, typing) |
| `linear()` (Physics) | `linear(0, 0.2 20%, 1.05 60%, 1)` | Multi-bounce physical spring simulation (Baseline 2023) |

---

## 13. Further Reading
- [W3C CSS Easing Functions Level 1](https://www.w3.org/TR/css-easing-1/)
- [W3C CSS Easing Functions Level 2 - linear()](https://www.w3.org/TR/css-easing-2/#linear-easing-function)
- [MDN Web Docs: cubic-bezier()](https://developer.mozilla.org/en-US/docs/Web/CSS/easing-function#cubic-bezier())
- [web.dev: Linear easing generator](https://linear-easing-generator.netlify.app/)
- [Cubic-Bezier.com by Lea Verou](https://cubic-bezier.com/)

---

[Previous: 02-animations-and-keyframes.md](02-animations-and-keyframes.md) | [Module README](README.md) | [Next: 04-animation-performance.md](04-animation-performance.md)
