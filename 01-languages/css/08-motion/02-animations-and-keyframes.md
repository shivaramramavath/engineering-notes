# Keyframes & Complex Animations

| Metadata | Details |
| :--- | :--- |
| **Topic** | `@keyframes` Syntax, The 8 Animation Sub-properties, `animation-fill-mode` Mechanics, Alternating Iterations, `animation-play-state` |
| **Spec Level** | CSS Animations Level 1 / CSS Animations Level 2 |
| **Baseline Status** | Widely Available (`@keyframes`, `animation` Baseline 2015 across all modern browsers) |
| **Browsers Tested** | Chrome 120+, Firefox 122+, Safari 17.2+, Edge 120+ |
| **Tested With** | Chromium Blink Compositor, Safari WebKit, Playwright, Chrome DevTools Animation Inspector |
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

While CSS Transitions interpolate between two states (State A to State B) in response to a user trigger, **CSS Keyframe Animations** allow developers to author autonomous, multi-stage, choreographed visual sequences without requiring user interaction or JavaScript execution.

Using `@keyframes`, an animation can traverse dozens of intermediate waypoints ($0\%, 25\%, 60\%, 100\%$), alternate direction indefinitely, pulse continuously in the background, or execute intricate page-load entrance reveals.

Mastering keyframe animations requires understanding:
- **The 8 Sub-Properties of the Animation Engine**: Precise configuration of duration, timing curves, iteration counts, direction vectors, and delay intervals.
- **The Mystery of `animation-fill-mode`**: How to control what styles apply *before* the animation begins during its delay window (`backwards`) and what styles persist *after* the animation ends (`forwards`).
- **Interactive Control via `animation-play-state`**: Pausing and resuming animations on user hover or interaction.
- **Compositor Efficiency**: Structuring keyframe declarations strictly around GPU-promoted properties to prevent main-thread layout thrashing.

---

## 2. Core Concepts

### The `@keyframes` At-Rule
Defines the visual waypoints of an animation:
```css
@keyframes pulseGlow {
  0% {
    transform: scale(1);
    opacity: 0.8;
  }
  50% {
    transform: scale(1.08);
    opacity: 1;
  }
  100% {
    transform: scale(1);
    opacity: 0.8;
  }
}
```
- Keywords `from` and `to` are aliases for `0%` and `100%`.
- If `0%` or `100%` is omitted, the browser automatically samples the element's existing computed styles as the start or end frame.

### The 8 Animation Sub-Properties
1. **`animation-name`**: Matches the identifier declared in `@keyframes`.
2. **`animation-duration`**: Time for one full cycle (e.g., `2s`, `350ms`).
3. **`animation-timing-function`**: Easing between keyframe waypoints (`linear`, `ease-in-out`, `cubic-bezier()`).
4. **`animation-delay`**: Time before the animation starts (e.g., `0.5s`).
5. **`animation-iteration-count`**: Number of times to repeat (`1`, `3`, or `infinite`).
6. **`animation-direction`**:
   - `normal` (Default): $0\% \to 100\%$ on every cycle.
   - `reverse`: $100\% \to 0\%$ on every cycle.
   - `alternate`: $0\% \to 100\%$, then $100\% \to 0\%$, alternating.
   - `alternate-reverse`: Starts backward, then alternates forward.
7. **`animation-fill-mode`**: Controls styling outside the active animation duration (detailed below).
8. **`animation-play-state`**: `running` (default) or `paused`.

### The Crucial Mechanics of `animation-fill-mode`
By default (`animation-fill-mode: none`), an animation has zero effect on an element before it starts (during the delay period) and immediately releases its styles the instant it ends, causing properties to snap back to the base CSS rule.

| `animation-fill-mode` | During Delay Period (Before Start) | After Animation Ends |
| :--- | :--- | :--- |
| **`none`** (Default) | Base CSS styles apply | Base CSS styles apply (snaps back) |
| **`forwards`** | Base CSS styles apply | **Retains final keyframe values** (100% or 0%) |
| **`backwards`** | **Applies initial keyframe values** (0% or 100%) | Base CSS styles apply (snaps back) |
| **`both`** | **Applies initial keyframe values** | **Retains final keyframe values** |

---

## 3. How It Works Internally

```
The animation-fill-mode: both Timeline

Time:   0s -------- (Delay Phase: 1s) -------- 1s -------- (Duration: 2s) -------- 3s -------->
State:  [backwards: Pre-applies 0% keyframe]   [Active Animation Interpolation]   [forwards: Locks 100% frame]
        (Prevents FOUC during delay!)          (0% -> 100%)                       (Prevents snapping back!)
```

### Stacking Order of Multiple Animations
Multiple animations can be chained in a single declaration:
```css
.card {
  animation:
    fadeIn 0.4s ease forwards,
    subtleFloat 3s ease-in-out 0.4s infinite alternate;
}
```
The browser compositor evaluates both keyframe sequences simultaneously, combining their matrix transforms.

---

## 4. Syntax & Usage

### Baseline & Feature Support
- **`@keyframes` & `animation`**: Baseline Widely Available.
- **`animation-fill-mode`**: Baseline Widely Available.
- **`animation-play-state`**: Baseline Widely Available.

```css
/* Production Entrance Animation */
@keyframes slideUpFade {
  from {
    opacity: 0;
    translate: 0 20px;
  }
  to {
    opacity: 1;
    translate: 0 0;
  }
}

.modal-entrance {
  animation: slideUpFade 0.3s cubic-bezier(0.16, 1, 0.3, 1) 0.1s both;
}
```

---

## 5. Practical Examples

### Example 1: Infinite Pulsing Radar Beacon
```html
<div class="beacon-wrapper">
  <div class="beacon-core"></div>
  <div class="beacon-ring"></div>
</div>
```

```css
.beacon-wrapper {
  position: relative;
  width: 24px;
  height: 24px;
  display: flex;
  align-items: center;
  justify-content: center;
}

.beacon-core {
  width: 10px;
  height: 10px;
  border-radius: 50%;
  background-color: #10b981;
}

@keyframes sonarPing {
  0% {
    transform: scale(0.8);
    opacity: 0.8;
  }
  70% {
    transform: scale(2.4);
    opacity: 0;
  }
  100% {
    transform: scale(2.4);
    opacity: 0;
  }
}

.beacon-ring {
  position: absolute;
  inset: 0;
  border-radius: 50%;
  border: 2px solid #10b981;
  animation: sonarPing 2s cubic-bezier(0, 0, 0.2, 1) infinite;
}
```

### Example 2: Pausing Infinite Animations on Hover via `animation-play-state`
```html
<div class="scrolling-ticker">
  <div class="ticker-track">
    <span>BTC: $94,200</span>
    <span>ETH: $3,450</span>
    <span>SOL: $185</span>
  </div>
</div>
```

```css
.scrolling-ticker {
  overflow: hidden;
  white-space: nowrap;
  background-color: #0f172a;
  color: #38bdf8;
  padding: 0.75rem 0;
}

@keyframes marqueeScroll {
  from { translate: 0 0; }
  to { translate: -50% 0; }
}

.ticker-track {
  display: inline-flex;
  gap: 3rem;
  animation: marqueeScroll 20s linear infinite;
  cursor: pointer;
}

/* User can pause the ticker to read numbers comfortably */
.ticker-track:hover,
.ticker-track:focus-within {
  animation-play-state: paused;
}
```

### Example 3: Staggered Multi-Card Page Load Orchestration
```html
<div class="stats-grid">
  <div class="stat-card" style="--stagger-delay: 0.05s;">
    <h3>Active Nodes</h3>
    <p>1,420</p>
  </div>
  <div class="stat-card" style="--stagger-delay: 0.15s;">
    <h3>Throughput</h3>
    <p>42.8 GB/s</p>
  </div>
  <div class="stat-card" style="--stagger-delay: 0.25s;">
    <h3>Uptime</h3>
    <p>99.99%</p>
  </div>
</div>
```

```css
@keyframes cardReveal {
  from {
    opacity: 0;
    translate: 0 16px;
    scale: 0.98;
  }
  to {
    opacity: 1;
    translate: 0 0;
    scale: 1;
  }
}

.stat-card {
  padding: 1.5rem;
  background-color: #ffffff;
  border-radius: 8px;
  border: 1px solid #e2e8f0;
  box-shadow: 0 1px 3px rgba(0, 0, 0, 0.05);

  /* both applies 'from' styles during the delay and locks 'to' styles at the end */
  animation: cardReveal 0.4s cubic-bezier(0.16, 1, 0.3, 1) var(--stagger-delay) both;
}
```

### Example 4: Attention-Seeking Shake Error State
```html
<input type="password" class="auth-input input-error" placeholder="Enter Master Key">
```

```css
@keyframes shakeHorizontal {
  0%, 100% { translate: 0 0; }
  20%, 60% { translate: -8px 0; }
  40%, 80% { translate: 8px 0; }
}

.input-error {
  border-color: #ef4444;
  animation: shakeHorizontal 0.4s ease-in-out;
}
```

### Example 5: Coordinated JavaScript Animation Listeners
```javascript
const toast = document.querySelector('.toast-notification');

toast.classList.add('animate-slide-in');

// Hooking into native CSS animation lifecycle events
toast.addEventListener('animationstart', (e) => {
  console.log(`Animation ${e.animationName} started!`);
});

toast.addEventListener('animationend', (e) => {
  if (e.animationName === 'slideOut') {
    toast.remove(); // Safely remove from DOM after CSS finishes
  }
});
```

---

## 6. Best Practices

1. **Always Use `animation-fill-mode: both` on Delayed Entrances**: When animating an element entering the screen with a delay (`0.2s`), setting `both` prevents the un-styled element from flashing briefly on screen before disappearing into the 0% keyframe.
2. **Animate Only GPU-Accelerated Properties**: Restrict `@keyframes` rules to `transform`, `translate`, `scale`, `rotate`, and `opacity`. Avoid animating `width`, `height`, or `top`.
3. **Always Allow Infinite Animations to Be Paused**: Any continuous animation (spinners, marquees, ambient loops) must provide a mechanism to pause (e.g., `:hover`, `:focus`, or an explicit pause button) per WCAG 2.2.2.
4. **Use `alternate` Direction for Seamless Breathing Loops**: Instead of writing redundant keyframes ($0\% \to 50\% \to 100\%$), write a two-frame animation ($0\% \to 100\%$) and declare `animation-direction: alternate; animation-iteration-count: infinite;`.
5. **Check `e.animationName` in Event Listeners**: Always verify which animation finished if multiple animations are chained together.
6. **Honor `prefers-reduced-motion`**: Completely disable or clamp infinite animations for users requesting reduced motion.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Correction |
| :--- | :--- | :--- |
| **Omitting `animation-fill-mode: forwards` on exit animations** | When the fade-out completes, the element instantly pops back to 100% opacity. | Add `forwards` or `both` to lock the final state. |
| **Flash of Unstyled Content (FOUC) during animation delay** | With `fill-mode: none`, element displays normally during delay before jumping to 0% frame. | Use `animation-fill-mode: backwards` or `both`. |
| **Animating `margin-top` for a drop-in reveal** | Triggers expensive CPU layout reflows on every single frame, causing stutter. | Animate `translate: 0 -20px;` to `translate: 0 0;`. |
| **Infinite animations causing high battery drain** | Continuously running animations keep the GPU active indefinitely. | Pause animations when off-screen using IntersectionObserver. |
| **Using `!important` inside `@keyframes`** | By CSS specification, `!important` inside keyframe declarations is completely ignored! | Remove `!important` from `@keyframes`. |
| **Forgetting to provide reduced motion fallback** | Infinite spinning or pulsing can cause physical nausea for users with vestibular disorders. | Wrap in `@media (prefers-reduced-motion: reduce)`. |

---

## 8. Performance, Accessibility & Browser Support

### Accessibility Compliance (WCAG 2.2 AA)
- **Criterion 2.2.2 (Pause, Stop, Hide)**: For any moving, blinking or scrolling information that starts automatically, lasts more than 5 seconds, and is presented in parallel with other content, there must be a mechanism for the user to pause, stop, or hide it.
- **Criterion 2.3.1 (Three Flashes or Below Threshold)**: Web pages do not contain anything that flashes more than three times in any one second period.

### Reduced Motion Override
```css
@media (prefers-reduced-motion: reduce) {
  *,
  *::before,
  *::after {
    animation-duration: 0.01ms !important;
    animation-iteration-count: 1 !important;
  }
}
```

---

## 9. Comparison / When to Use What

| Dimension | CSS Transitions | CSS `@keyframes` |
| :--- | :--- | :--- |
| **Stages** | Strictly 2 states ($A \to B$) | **Infinite arbitrary waypoints ($0\% \to 100\%$)** |
| **Trigger** | Requires user action or class change | **Can execute automatically on page load** |
| **Repetition** | Single run per trigger | **Can loop infinitely (`infinite`)** |
| **Reversibility** | **Smoothly interruptible and reversible** | Non-trivial to reverse mid-stream |
| **Fill Modes** | Built-in (State remains active) | Configurable (`forwards`, `backwards`, `both`) |

---

## 10. Interview Questions

<details>
<summary>1. Explain the exact difference between <code>forwards</code>, <code>backwards</code>, and <code>both</code> in <code>animation-fill-mode</code>.</summary>

`animation-fill-mode` dictates how an element is styled before an animation starts and after it ends:
- **`none`** (Default): The animation has no effect outside its active run time. During an `animation-delay`, default CSS styles apply. When the animation finishes, styles immediately snap back to default CSS.
- **`forwards`**: After the animation finishes, the element permanently retains the computed property values of the **final keyframe** (100% for `normal`, 0% for `reverse`).
- **`backwards`**: During the `animation-delay` window *before* the animation begins, the element immediately adopts the property values of the **first keyframe** (0% for `normal`, 100% for `reverse`), preventing a flash of unstyled content.
- **`both`**: The element adopts the first keyframe values during the delay window **and** retains the final keyframe values after completion.
</details>

<details>
<summary>2. What happens if you declare <code>!important</code> inside a <code>@keyframes</code> block?</summary>

According to the W3C CSS Animations Level 1 specification, any declaration inside a `@keyframes` rule containing `!important` is **syntactically invalid and completely ignored by the CSS parser**. Keyframes do not participate in standard cascading specificity battles; all keyframe rules are evaluated uniformly by the animation engine.
</details>

<details>
<summary>3. What is the role of <code>animation-play-state</code> and how can it be used for accessibility?</summary>

`animation-play-state` controls whether an animation is currently `running` or `paused`. 
When switched to `paused`, the animation freezes immediately at its current intermediate frame without resetting to the beginning. 

It is critical for WCAG 2.2.2 compliance: pairing `animation-play-state: paused` with `:hover`, `:focus-within`, or a user-facing toggle button allows users with reading or attention impairments to halt auto-scrolling tickers or distracting animations.
</details>

<details>
<summary>4. What are the four native DOM events dispatched by CSS animations?</summary>

1. `animationstart`: Dispatched when the animation begins (after `animation-delay` expires).
2. `animationiteration`: Dispatched at the end of each iteration cycle (only fires if `animation-iteration-count > 1`).
3. `animationend`: Dispatched when the animation completes all scheduled iterations.
4. `animationcancel`: Dispatched if the animation is aborted (e.g., if `animation-name` changes or the element is removed from the DOM before completion).
</details>

<details>
<summary>5. How does <code>animation-direction: alternate</code> simplify keyframe authoring?</summary>

Without `alternate`, creating a continuous breathing or pulsating loop requires authoring three keyframe waypoints: $0\%$, $50\%$, and $100\%$, where $0\%$ and $100\%$ are identical duplicates. 

With `animation-direction: alternate; animation-iteration-count: infinite;`, you only define two frames: $0\%$ (contracted) and $100\%$ (expanded). The browser automatically animates forward on odd cycles ($0\% \to 100\%$) and in reverse on even cycles ($100\% \to 0\%$), saving code and guaranteeing mathematically symmetrical easing.
</details>

<details>
<summary>6. What happens if you omit the <code>0%</code> or <code>100%</code> keyframe in <code>@keyframes</code>?</summary>

If the `0%` (or `from`) keyframe is omitted, the browser automatically captures the element's existing computed styles at the start of the animation as the initial waypoint. 
If the `100%` (or `to`) keyframe is omitted, the browser interpolates toward the element's base computed styles at the end of the animation.
</details>

<details>
<summary>7. Why does chaining multiple animations with <code>forwards</code> sometimes produce unexpected property overrides?</summary>

If two chained animations target the same property (e.g., both mutate `transform`), the animation listed last in the comma-separated list takes precedence according to standard cascade rules. 
If Animation A has `forwards`, its locked 100% keyframe values might conflict with or override subsequent keyframes in Animation B unless properly isolated.
</details>

<details>
<summary>8. How can you dynamically adjust an animation's speed or delay using CSS Custom Properties?</summary>

By binding CSS custom properties directly inside the shorthand declaration:
```css
.card {
  animation: reveal var(--anim-duration, 0.4s) ease var(--anim-delay, 0s) both;
}
```
JavaScript or HTML inline styles (`style="--anim-delay: 0.15s;"`) can dynamically stagger animations across hundreds of elements without writing hundreds of individual CSS classes.
</details>

---

## 11. Practice Exercises

### Exercise 1: Build an Infinite Glowing Radar Beacon
**Goal**: Create a pulsing emerald beacon that radiates outward and fades to 0 opacity infinitely.

<details>
<summary>View Solution</summary>

```css
@keyframes beaconPulse {
  0% {
    transform: scale(0.9);
    opacity: 0.8;
  }
  70% {
    transform: scale(2.2);
    opacity: 0;
  }
  100% {
    transform: scale(2.2);
    opacity: 0;
  }
}

.radar-ping {
  width: 20px;
  height: 20px;
  border-radius: 50%;
  background-color: #10b981;
  position: relative;
}

.radar-ping::after {
  content: "";
  position: absolute;
  inset: 0;
  border-radius: 50%;
  border: 2px solid #10b981;
  animation: beaconPulse 2s cubic-bezier(0, 0, 0.2, 1) infinite;
}
```
</details>

### Exercise 2: Master `animation-fill-mode: both` on Staggered Cards
**Goal**: Author a staggered card grid where cards stay hidden during a 0.2s delay and lock in place when finished.

<details>
<summary>View Solution</summary>

```css
@keyframes slideUpCard {
  from {
    opacity: 0;
    translate: 0 24px;
  }
  to {
    opacity: 1;
    translate: 0 0;
  }
}

.stagger-card {
  animation: slideUpCard 0.4s cubic-bezier(0.16, 1, 0.3, 1) var(--delay, 0s) both;
}
```
</details>

### Exercise 3: Pauseable Marquee Ticker
**Goal**: Build a continuous horizontal ticker that pauses cleanly whenever the user hovers over it.

<details>
<summary>View Solution</summary>

```css
@keyframes tickerMove {
  from { translate: 0 0; }
  to { translate: -50% 0; }
}

.ticker-content {
  display: inline-flex;
  gap: 2rem;
  animation: tickerMove 15s linear infinite;
}

.ticker-content:hover,
.ticker-content:focus-within {
  animation-play-state: paused;
}
```
</details>

### Exercise 4: Alternating Breathing Button Glow
**Goal**: Use `animation-direction: alternate` to create a continuous glowing pulse on an action button.

<details>
<summary>View Solution</summary>

```css
@keyframes glowPulse {
  from {
    box-shadow: 0 0 4px rgba(37, 99, 235, 0.2);
  }
  to {
    box-shadow: 0 0 20px rgba(37, 99, 235, 0.8);
  }
}

.glowing-btn {
  padding: 0.75rem 1.5rem;
  background-color: #2563eb;
  color: #ffffff;
  border: none;
  border-radius: 6px;
  animation: glowPulse 1.5s ease-in-out infinite alternate;
}

.glowing-btn:focus-visible {
  outline: 2px solid #ffffff;
  outline-offset: 3px;
}
```
</details>

### Exercise 5: DOM Cleanup via `animationend`
**Goal**: Write a JavaScript snippet that safely removes a notification toast when its exit animation completes.

<details>
<summary>View Solution</summary>

```javascript
function dismissToast(toastElement) {
  toastElement.classList.add('toast-exit');
  
  toastElement.addEventListener('animationend', function cleanup(e) {
    if (e.animationName === 'fadeSlideOut') {
      toastElement.removeEventListener('animationend', cleanup);
      toastElement.remove();
    }
  });
}
```
</details>

### Exercise 6: Accessible Infinite Animation Override
**Goal**: Enforce accessibility compliance by clamping all infinite animations to 1 cycle for users with reduced motion.

<details>
<summary>View Solution</summary>

```css
@media (prefers-reduced-motion: reduce) {
  *,
  *::before,
  *::after {
    animation-iteration-count: 1 !important;
    animation-duration: 0.01ms !important;
  }
}
```
</details>

---

## 12. Summary / Cheat Sheet

| Sub-Property | Example | Role |
| :--- | :--- | :--- |
| `animation-name` | `pulse` | Connects element to `@keyframes pulse` |
| `animation-duration` | `0.4s` | Duration of one single cycle |
| `animation-timing-function` | `cubic-bezier(0.16, 1, 0.3, 1)` | Acceleration curve |
| `animation-iteration-count` | `infinite \| 3` | Number of repetitions |
| `animation-direction` | `alternate` | Alternates between forward and reverse |
| `animation-fill-mode` | `both` | Pre-applies 0% during delay; locks 100% on end |
| `animation-play-state` | `paused` | Freezes animation in place on hover or focus |

---

## 13. Further Reading
- [W3C CSS Animations Level 1](https://www.w3.org/TR/css-animations-1/)
- [MDN Web Docs: @keyframes](https://developer.mozilla.org/en-US/docs/Web/CSS/@keyframes)
- [MDN Web Docs: animation-fill-mode](https://developer.mozilla.org/en-US/docs/Web/CSS/animation-fill-mode)
- [web.dev: Complex animation with keyframes](https://web.dev/learn/css/animations/)
- [CSS-Tricks: animation-fill-mode](https://css-tricks.com/almanac/properties/a/animation-fill-mode/)

---

[Previous: 01-transitions.md](01-transitions.md) | [Module README](README.md) | [Next: 03-timing-functions-and-linear-easing.md](03-timing-functions-and-linear-easing.md)
