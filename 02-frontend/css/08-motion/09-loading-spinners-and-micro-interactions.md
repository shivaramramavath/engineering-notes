# Loading Spinners and Micro-Interactions

| Metadata | Details |
| :--- | :--- |
| **Spec Level** | CSS Animations Level 1 / CSS Transforms Level 2 (W3C Recommendation) |
| **Baseline Status** | Widely Available (Supported across all modern browsers) |
| **Browsers Tested** | Chrome 43+, Firefox 16+, Safari 9+, Edge 12+ |
| **Tested With** | Chrome DevTools Rendering, Firefox Performance Profiler, Lighthouse A11y |

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

Micro-interactions are subtle, contained visual moments in user interface design that communicate status, provide immediate tactile feedback, guide user focus, and elevate overall polish. In modern web engineering, micro-interactions bridge the psychological gap between user action and server response:
- **Perceived Performance**: A well-crafted skeleton screen or responsive loading spinner reduces perceived wait time by providing continuous visual progress indicators.
- **State Affirmation**: A button that morphs into a spinner upon clicking and expands into a checkmark on completion assures the user that their request was received and processed without requiring disruptive full-page reloads.
- **Tactile Delight & Direct Manipulation**: Micro-interactions such as toggle switches, springy badge counters, and animated hearts make digital interfaces feel physical and responsive.

When implemented purely in CSS using GPU-composited properties (`transform` and `opacity`), loading animations and micro-interactions run smoothly at 60–120 FPS without tying up the main JavaScript execution thread.

---

## 2. Core Concepts

### Compositor-Only Micro-Interactions
To maintain silky frame rates even when the browser's JavaScript main thread is heavily occupied parsing JSON or hydrating framework components, micro-interactions must strictly animate GPU compositor properties:
1. `transform` (`translate()`, `rotate()`, `scale()`)
2. `opacity`

Animating layout properties (`width`, `height`, `margin`, `padding`, `top`, `left`) triggers constant layout reflows and repaints, resulting in stuttering, dropped frames, and poor Interaction to Next Paint (INP) scores.

### Feedback Loop Stages of a Micro-Interaction
Every complete micro-interaction follows a 4-phase cycle (Dan Saffer's Micro-interaction Model):
1. **Trigger**: User initiates action (e.g. `:hover`, `:active`, `:focus-visible`, or class added via JS).
2. **Rules**: What happens upon trigger (e.g., button dims, spinner reveals, scale compresses).
3. **Feedback**: Visual movement signaling progress (e.g. continuous rotation or skeleton shimmer).
4. **Loops & Modes**: What happens when complete (e.g., transitions to success checkmark or resets).

```
[Trigger: Click] ──> [Feedback: Scale 0.96] ──> [Feedback: Spinner Active] ──> [Complete: Checkmark]
```

### Accessibility of Status Indicators
Purely visual animations are invisible to assistive technologies. Loading spinners and micro-interactions must incorporate proper ARIA attributes:
- `role="status"`: Announces updates to screen readers politely without interrupting ongoing announcements.
- `aria-live="polite"`: Instructs assistive tech to announce changes once current speech tasks finish.
- `aria-busy="true"`: Informs screen readers that an element or container is currently updating.
- Visually hidden helper text (`<span class="sr-only">Loading...</span>`).

---

## 3. How It Works Internally

```
Main Thread vs. Compositor Execution during Loading:

Main Thread (JavaScript & DOM):
  [Fetch / Parse JSON] ─────── [Compute React VDOM] ─────── [Commit to DOM]
  (Main thread is 100% BLOCKED during heavy JS evaluation)

Compositor Thread (GPU Acceleration):
  [Spinner 0deg] ──> [Spinner 90deg] ──> [Spinner 180deg] ──> [Spinner 360deg]
  (Runs completely independently on GPU! Zero dropped frames, buttery 60/120 FPS)
```

```
Skeleton Shimmer Wave Mechanics:
┌────────────────────────────────────────────────────────┐
│ [===================== Skeleton Card =================]│
│            ┌───────────────┐                           │
│  Base Gray │ Shimmer Peak  │ Base Gray                 │
│  (#e2e8f0) │ (#f8fafc)     │ (#e2e8f0)                 │
│            └───────────────┘                           │
│  <────────── Transform: translateX(-100% -> +100%) ──> │
└────────────────────────────────────────────────────────┘
Shimmer uses a CSS pseudo-element (::after) with a linear-gradient background
and translates horizontally on the Compositor thread using transform: translateX().
```

---

## 4. Syntax & Usage

```
Baseline Availability:
├── Supported natively across all modern browsers (Chrome, Safari, Firefox, Edge).
├── GPU hardware acceleration active by default on transform and opacity.
```

### Essential Spinner Pattern
```css
.spinner {
  width: 2rem;
  height: 2rem;
  border: 3px solid rgba(0, 0, 0, 0.1);
  border-top-color: #3b82f6;
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

## 5. Practical Examples

### Example 1: Classic High-Performance Dual-Ring Spinner
Smooth, GPU-accelerated dual ring spinner with semi-transparent track.

```html
<div class="spinner-container" role="status" aria-live="polite">
  <div class="ring-spinner"></div>
  <span class="sr-only">Loading user profile...</span>
</div>
```

```css
.ring-spinner {
  width: 2.5rem;
  height: 2.5rem;
  border-radius: 50%;
  border: 3px solid #e2e8f0;
  border-top-color: #2563eb;
  border-right-color: #2563eb;
  animation: dualSpin 0.75s cubic-bezier(0.68, -0.55, 0.27, 1.55) infinite;
}

@keyframes dualSpin {
  0% {
    transform: rotate(0deg);
  }
  100% {
    transform: rotate(360deg);
  }
}

.sr-only {
  position: absolute;
  width: 1px;
  height: 1px;
  padding: 0;
  margin: -1px;
  overflow: hidden;
  clip: rect(0, 0, 0, 0);
  white-space: nowrap;
  border: 0;
}
```

### Example 2: Pulsing Sonar Ring (Radar Indicator)
Continuous ripple waves often used for live presence, voice recording, or incoming notifications.

```html
<div class="status-indicator">
  <span class="sonar-beacon"></span>
  <span class="status-label">System Online</span>
</div>
```

```css
.status-indicator {
  display: inline-flex;
  align-items: center;
  gap: 0.75rem;
  font-family: system-ui, sans-serif;
  font-size: 0.875rem;
  color: #1e293b;
}

.sonar-beacon {
  position: relative;
  width: 0.75rem;
  height: 0.75rem;
  border-radius: 50%;
  background-color: #10b981;
}

.sonar-beacon::after {
  content: '';
  position: absolute;
  inset: -4px;
  border-radius: 50%;
  border: 2px solid #10b981;
  animation: sonarWave 1.8s ease-out infinite;
}

@keyframes sonarWave {
  0% {
    transform: scale(0.8);
    opacity: 0.9;
  }
  100% {
    transform: scale(2.4);
    opacity: 0;
  }
}
```

### Example 3: Zero-Layout-Shift Skeleton Card Shimmer
A content placeholder that sweeps a shimmering light band across cards without triggering layout recalculations.

```html
<div class="skeleton-card" aria-hidden="true">
  <div class="skeleton-avatar"></div>
  <div class="skeleton-text-group">
    <div class="skeleton-line skeleton-title"></div>
    <div class="skeleton-line skeleton-subtitle"></div>
  </div>
</div>
```

```css
.skeleton-card {
  display: flex;
  gap: 1rem;
  padding: 1.5rem;
  background-color: #ffffff;
  border-radius: 0.75rem;
  border: 1px solid #e2e8f0;
  max-width: 24rem;
}

.skeleton-avatar {
  width: 3.5rem;
  height: 3.5rem;
  border-radius: 50%;
  background-color: #e2e8f0;
  position: relative;
  overflow: hidden;
}

.skeleton-text-group {
  flex: 1;
  display: flex;
  flex-direction: column;
  gap: 0.5rem;
  justify-content: center;
}

.skeleton-line {
  height: 0.875rem;
  border-radius: 0.25rem;
  background-color: #e2e8f0;
  position: relative;
  overflow: hidden;
}

.skeleton-title {
  width: 70%;
}

.skeleton-subtitle {
  width: 90%;
}

/* GPU-Composited Shimmer Pseudo-Element */
.skeleton-avatar::after,
.skeleton-line::after {
  content: '';
  position: absolute;
  inset: 0;
  transform: translateX(-100%);
  background: linear-gradient(
    90deg,
    rgba(255, 255, 255, 0) 0%,
    rgba(255, 255, 255, 0.6) 50%,
    rgba(255, 255, 255, 0) 100%
  );
  animation: shimmerMove 1.6s infinite;
}

@keyframes shimmerMove {
  100% {
    transform: translateX(100%);
  }
}
```

### Example 4: State-Morphing Button (Idle -> Loading -> Success)
A complete interactive submit button that compresses, displays an embedded spinner, and resolves into a success checkmark.

```html
<button class="morph-button is-loading" aria-live="polite">
  <span class="btn-text">Submit Payment</span>
  <span class="btn-spinner" aria-hidden="true"></span>
  <svg class="btn-check" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="3" aria-hidden="true">
    <polyline points="20 6 9 17 4 12"></polyline>
  </svg>
</button>
```

```css
.morph-button {
  position: relative;
  display: inline-flex;
  align-items: center;
  justify-content: center;
  min-width: 10rem;
  height: 3rem;
  padding: 0 1.5rem;
  font-family: inherit;
  font-size: 0.95rem;
  font-weight: 600;
  color: #ffffff;
  background-color: #2563eb;
  border: none;
  border-radius: 0.5rem;
  cursor: pointer;
  overflow: hidden;
  transition:
    background-color 0.25s ease,
    transform 0.15s ease,
    min-width 0.3s cubic-bezier(0.16, 1, 0.3, 1);
}

.morph-button:hover {
  background-color: #1d4ed8;
}

.morph-button:active {
  transform: scale(0.97);
}

.btn-text,
.btn-spinner,
.btn-check {
  transition: opacity 0.2s ease, transform 0.2s ease;
}

/* Spinner Icon */
.btn-spinner {
  position: absolute;
  width: 1.25rem;
  height: 1.25rem;
  border: 2px solid rgba(255, 255, 255, 0.3);
  border-top-color: #ffffff;
  border-radius: 50%;
  opacity: 0;
  transform: scale(0.5);
  animation: spin 0.6s linear infinite;
}

/* Checkmark Icon */
.btn-check {
  position: absolute;
  width: 1.5rem;
  height: 1.5rem;
  opacity: 0;
  transform: scale(0.5);
}

/* Loading State */
.morph-button.is-loading {
  pointer-events: none;
}

.morph-button.is-loading .btn-text {
  opacity: 0;
  transform: translateY(-8px);
}

.morph-button.is-loading .btn-spinner {
  opacity: 1;
  transform: scale(1);
}

/* Success State */
.morph-button.is-success {
  background-color: #10b981;
}

.morph-button.is-success .btn-text,
.morph-button.is-success .btn-spinner {
  opacity: 0;
}

.morph-button.is-success .btn-check {
  opacity: 1;
  transform: scale(1);
}
```

### Example 5: Interactive Micro-Interactions: Toggle Switch & Spring Heart
Pure CSS tactile toggle switch and springy favorite button.

```html
<label class="toggle-switch">
  <input type="checkbox" class="toggle-input">
  <span class="toggle-slider"></span>
</label>

<button class="heart-btn" aria-label="Add to favorites">
  <svg class="heart-icon" viewBox="0 0 24 24" fill="currentColor">
    <path d="M12 21.35l-1.45-1.32C5.4 15.36 2 12.28 2 8.5 2 5.42 4.42 3 7.5 3c1.74 0 3.41.81 4.5 2.09C13.09 3.81 14.76 3 16.5 3 19.58 3 22 5.42 22 8.5c0 3.78-3.4 6.86-8.55 11.54L12 21.35z"/>
  </svg>
</button>
```

```css
/* Tactile Toggle Switch */
.toggle-switch {
  position: relative;
  display: inline-block;
  width: 3.25rem;
  height: 1.75rem;
}

.toggle-input {
  opacity: 0;
  width: 0;
  height: 0;
}

.toggle-slider {
  position: absolute;
  inset: 0;
  background-color: #cbd5e1;
  border-radius: 2rem;
  cursor: pointer;
  transition: background-color 0.25s ease;
}

.toggle-slider::before {
  content: '';
  position: absolute;
  height: 1.25rem;
  width: 1.25rem;
  left: 0.25rem;
  bottom: 0.25rem;
  background-color: #ffffff;
  border-radius: 50%;
  box-shadow: 0 2px 4px rgba(0, 0, 0, 0.2);
  transition: transform 0.25s cubic-bezier(0.34, 1.56, 0.64, 1);
}

.toggle-input:checked + .toggle-slider {
  background-color: #2563eb;
}

.toggle-input:checked + .toggle-slider::before {
  transform: translateX(1.5rem);
}

.toggle-input:focus-visible + .toggle-slider {
  outline: 2px solid #2563eb;
  outline-offset: 2px;
}

/* Springy Heart Micro-Interaction */
.heart-btn {
  background: none;
  border: none;
  padding: 0.5rem;
  cursor: pointer;
  color: #94a3b8;
  display: inline-flex;
  align-items: center;
  justify-content: center;
  border-radius: 50%;
  transition: color 0.2s ease;
}

.heart-btn:focus-visible {
  outline: 2px solid #ef4444;
  outline-offset: 2px;
}

.heart-icon {
  width: 1.75rem;
  height: 1.75rem;
  transition: transform 0.35s cubic-bezier(0.175, 0.885, 0.32, 1.275);
}

.heart-btn.is-active {
  color: #ef4444;
}

.heart-btn.is-active .heart-icon {
  animation: heartPop 0.4s cubic-bezier(0.175, 0.885, 0.32, 1.275);
}

@keyframes heartPop {
  0% {
    transform: scale(1);
  }
  40% {
    transform: scale(1.4);
  }
  70% {
    transform: scale(0.85);
  }
  100% {
    transform: scale(1);
  }
}
```

---

## 6. Best Practices

1. **Strictly animate `transform` and `opacity` only**: Never animate `width`, `height`, `left`, `margin`, or `padding` during micro-interactions or spinners. Keeping animations off the main thread preserves 60 FPS performance and ensures good Core Web Vitals (INP and CLS).
2. **Combine with `role="status"` and `aria-live="polite"`**: Always announce asynchronous loading states to assistive technologies.
3. **Use spring or overshoot timing for tactile feedback**: Use `cubic-bezier(0.34, 1.56, 0.64, 1)` to give toggle thumbs and heart icons physical bounce and weight.
4. **Avoid looping spinners for indeterminate operations lasting $< 400\text{ms}$**: Showing a flash of a spinner for 100ms creates visual jitter. Only display spinners when operations take longer than 300–400ms.
5. **Always provide keyboard focus rings**: When creating custom checkboxes, toggles, or icon buttons, pair `:focus-visible` with high-contrast outlines.
6. **Gracefully support `prefers-reduced-motion`**: Convert 360-degree spinners into calm pulsing bars or gentle opacity oscillations for vestibular-sensitive users.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Consequence | Correction |
| :--- | :--- | :--- |
| Animating `border-width` or `margin` on hover | Triggers layout reflows across the entire page, degrading scrolling performance. | Use `transform: scale()` or box-shadow simulation instead. |
| Missing `aria-live` or text alternative on spinners | Screen reader users receive zero indication that data is loading. | Add `<span class="sr-only">Loading...</span>` with `role="status"`. |
| Shimmer animation running on non-GPU properties | Heavy CPU utilization and dropped frames during page initialization. | Animate `transform: translateX()` on an overlaid `::after` pseudo-element. |
| Infinite spinner with no timeout fallback | If an API request fails, spinner loops endlessly, leaving the user confused. | Implement JS timeout error handling that morphs the spinner into an error badge. |
| Forgetting `pointer-events: none` on loading buttons | User double-clicks the button while loading, causing duplicate transaction requests. | Add `pointer-events: none` to the loading state. |
| Overly bouncy timing functions on small buttons | UI feels unstable, sloppy, or cartoonish. | Restrict overshoot easing (`cubic-bezier`) to amplitude multipliers under $1.4$. |

---

## 8. Performance, Accessibility & Browser Support

### Performance
- **Compositor Acceleration**: Spinners running on `transform: rotate()` operate entirely within GPU textures.
- **Zero CLS (Cumulative Layout Shift)**: Skeleton loaders should exactly match the dimensions of the incoming content to ensure zero layout shift once real data resolves.

### Accessibility (A11y)
- Screen reader announcements:

```html
<div role="status" aria-live="polite">
  <div class="spinner" aria-hidden="true"></div>
  <span class="sr-only">Fetching latest transactions...</span>
</div>
```

- Accessible motion reduction:

```css
@media (prefers-reduced-motion: reduce) {
  .spinner,
  .btn-spinner {
    animation: gentlePulse 1.5s ease-in-out infinite;
  }
  @keyframes gentlePulse {
    0%, 100% { opacity: 0.3; }
    50% { opacity: 1; }
  }
}
```

---

## 9. Comparison / When to Use What

| Indicator Pattern | Perceived Wait Time | Best Used For | Drawbacks |
| :--- | :--- | :--- | :--- |
| **Skeleton Screens** | Lowest (perceived fastest) | Initial page loads, dashboard cards, feeds | Requires markup tailored to content shape |
| **Inline Button Spinner** | Low | Form submissions, payment transactions | Limited screen real estate |
| **Global Circular Spinner** | Medium | Full-page navigation, heavy file downloads | Does not preview layout; feels longer |
| **Linear Progress Bar** | Low (if determinate) | Known file uploads, multi-step wizards | Ineffective if total duration is unknown |

---

## 10. Interview Questions

### Q1: Why should skeleton screens be preferred over centered loading spinners for dashboard feeds?
<details>
<summary>View Answer</summary>
Cognitive research shows that skeleton screens reduce perceived waiting time because they mimic the structure and layout of the incoming data, allowing the user's brain to anticipate where content will appear. Additionally, setting explicit dimensions on skeleton blocks reserves layout space, eliminating Cumulative Layout Shift (CLS) when final data renders.
</details>

### Q2: How do you prevent a loading spinner from causing dropped frames on an animation thread during heavy JavaScript parsing?
<details>
<summary>View Answer</summary>
By ensuring the spinner animates strictly using `transform: rotate(360deg)`. The browser's Compositor thread manages `transform` and `opacity` independently of the main JavaScript execution thread. Even if JavaScript blocks the main thread for 500ms calculating data, the GPU compositor continues spinning the loader at 60 FPS.
</details>

### Q3: What ARIA attributes are required for an accessible loading indicator?
<details>
<summary>View Answer</summary>
1. `role="status"`: Identifies the region as an advisory container for status messages.
2. `aria-live="polite"`: Informs assistive technology to announce updates at the next graceful pause in speech.
3. `aria-busy="true"`: Applied to the container or form being loaded.
4. `aria-hidden="true"`: Placed on the visual spinner element itself to prevent screen readers from reading decorative icons or SVGs.
5. Visually hidden text (`.sr-only`) providing meaningful text like `"Loading content..."`.
</details>

### Q4: What is the benefit of using `cubic-bezier(0.34, 1.56, 0.64, 1)` over standard `ease-in-out` for micro-interactions?
<details>
<summary>View Answer</summary>
The third parameter coordinate exceeding `1.0` ($y2 = 1.56$) creates an overshoot or "spring" effect. The animated element travels slightly past its final destination before snapping back into place. This simulates real-world physical inertia, making buttons, toggles, and modal pops feel tangible, responsive, and tactile.
</details>

### Q5: Why is setting `overflow: hidden` on a skeleton card container critical for performance?
<details>
<summary>View Answer</summary>
Skeleton shimmer effects typically translate a wide gradient pseudo-element (`::after`) from `-100%` to `+100%`. Without `overflow: hidden` on the parent card, this moving pseudo-element would expand the bounding box of the parent or create temporary horizontal scrollbars on the document, causing layout thrashing and clipping artifacts.
</details>

### Q6: How should an interactive micro-interaction handle keyboard navigation?
<details>
<summary>View Answer</summary>
The element must remain focusable via native elements (`<button>`, `<input>`) or `tabindex="0"`, trigger on `Enter` / `Space` key presses, and display an explicit `:focus-visible` outline. Micro-interaction animations should trigger cleanly on both keyboard focus and mouse hover.
</details>

### Q7: What is the "flash of loading state" problem and how is it mitigated?
<details>
<summary>View Answer</summary>
If an API responds in under 150ms, rendering a loading spinner immediately results in a distracting 100ms flicker. This is mitigated by adding an animation delay (e.g., `animation-delay: 250ms`) or managing a JavaScript debounce timer so the spinner is only displayed if the request takes longer than 250–300ms.
</details>

### Q8: How does `will-change: transform` impact low-power mobile devices during infinite animations?
<details>
<summary>View Answer</summary>
`will-change: transform` prompts the browser to promote the element to its own dedicated GPU composite layer ahead of time. While this eliminates initial promotion lag, overusing `will-change` across dozens of simultaneous elements consumes excessive GPU memory (VRAM), which can crash low-power mobile browsers. It should be applied judiciously to active spinners or animated overlays.
</details>

---

## 11. Practice Exercises

### Exercise 1: Build a Pure CSS Three-Dot Bouncing Loader
**Objective**: Build a horizontal three-dot loading indicator with staggered bounce animations using `transform: translateY()`.
<details>
<summary>View Solution</summary>

```css
.dot-loader {
  display: flex;
  align-items: center;
  gap: 0.5rem;
}

.dot-loader span {
  width: 0.75rem;
  height: 0.75rem;
  background-color: #2563eb;
  border-radius: 50%;
  animation: dotBounce 1.2s infinite ease-in-out;
}

.dot-loader span:nth-child(2) {
  animation-delay: 0.2s;
}

.dot-loader span:nth-child(3) {
  animation-delay: 0.4s;
}

@keyframes dotBounce {
  0%, 80%, 100% {
    transform: translateY(0);
    opacity: 0.4;
  }
  40% {
    transform: translateY(-10px);
    opacity: 1;
  }
}
```
</details>

### Exercise 2: Circular Progress Wheel with Conic Gradient
**Objective**: Create a pure CSS circular loader with a continuous sweeping conic-gradient tail.
<details>
<summary>View Solution</summary>

```css
.conic-spinner {
  width: 3rem;
  height: 3rem;
  border-radius: 50%;
  background: conic-gradient(from 0deg, rgba(37, 99, 235, 0) 0%, #2563eb 100%);
  mask: radial-gradient(farthest-side, transparent calc(100% - 4px), #000 0);
  -webkit-mask: radial-gradient(farthest-side, transparent calc(100% - 4px), #000 0);
  animation: spin 0.8s linear infinite;
}

@keyframes spin {
  to {
    transform: rotate(360deg);
  }
}
```
</details>

### Exercise 3: Expanding Ripple Touch Feedback (Material Ripple)
**Objective**: Create a pure CSS ripple feedback effect on button click using an expanding pseudo-element.
<details>
<summary>View Solution</summary>

```css
.ripple-button {
  position: relative;
  overflow: hidden;
  padding: 0.75rem 1.75rem;
  font-family: inherit;
  font-size: 1rem;
  color: #ffffff;
  background-color: #4f46e5;
  border: none;
  border-radius: 0.375rem;
  cursor: pointer;
}

.ripple-button::after {
  content: '';
  position: absolute;
  top: 50%;
  left: 50%;
  width: 5px;
  height: 5px;
  background: rgba(255, 255, 255, 0.5);
  opacity: 0;
  border-radius: 100%;
  transform: scale(1) translate(-50%, -50%);
  transform-origin: top left;
}

.ripple-button:active::after {
  animation: rippleEffect 0.4s ease-out;
}

@keyframes rippleEffect {
  0% {
    transform: scale(0) translate(-50%, -50%);
    opacity: 1;
  }
  100% {
    transform: scale(40) translate(-50%, -50%);
    opacity: 0;
  }
}
```
</details>

### Exercise 4: Notification Badge Pop Animation
**Objective**: Animate a badge counter to pop outward with an elastic bounce whenever its numerical content changes.
<details>
<summary>View Solution</summary>

```css
.badge-counter {
  display: inline-flex;
  align-items: center;
  justify-content: center;
  min-width: 1.25rem;
  height: 1.25rem;
  padding: 0 0.375rem;
  font-size: 0.75rem;
  font-weight: 700;
  color: #ffffff;
  background-color: #ef4444;
  border-radius: 1rem;
  animation: badgePop 0.35s cubic-bezier(0.175, 0.885, 0.32, 1.4);
}

@keyframes badgePop {
  0% {
    transform: scale(0.4);
    opacity: 0;
  }
  70% {
    transform: scale(1.25);
  }
  100% {
    transform: scale(1);
    opacity: 1;
  }
}
```
</details>

### Exercise 5: Shimmer Wave Skeleton Profile Card
**Objective**: Build a complete multi-line user card skeleton with synchronized shimmer progression.
<details>
<summary>View Solution</summary>

```css
.card-skeleton {
  position: relative;
  overflow: hidden;
  background: #f1f5f9;
  border-radius: 0.5rem;
  padding: 1rem;
  width: 18rem;
}

.card-skeleton::after {
  content: '';
  position: absolute;
  inset: 0;
  transform: translateX(-100%);
  background: linear-gradient(
    90deg,
    rgba(255, 255, 255, 0) 0%,
    rgba(255, 255, 255, 0.7) 50%,
    rgba(255, 255, 255, 0) 100%
  );
  animation: sweep 1.5s infinite;
}

@keyframes sweep {
  100% {
    transform: translateX(100%);
  }
}
```
</details>

### Exercise 6: Micro-Interaction Focus Ring with Offset Animation
**Objective**: Design an accessible, eye-catching focus ring for custom interactive inputs that smoothly scales into place.
<details>
<summary>View Solution</summary>

```css
.custom-input {
  border: 1px solid #cbd5e1;
  padding: 0.625rem 0.875rem;
  border-radius: 0.375rem;
  font-size: 1rem;
  transition: border-color 0.2s ease, box-shadow 0.2s cubic-bezier(0.16, 1, 0.3, 1);
}

.custom-input:focus-visible {
  outline: none;
  border-color: #2563eb;
  box-shadow: 0 0 0 4px rgba(37, 99, 235, 0.2);
}
```
</details>

---

## 12. Summary / Cheat Sheet

```css
/* Compositor-Safe Infinite Spinner */
.spinner {
  width: 2rem;
  height: 2rem;
  border: 3px solid #e2e8f0;
  border-top-color: #2563eb;
  border-radius: 50%;
  animation: spin 0.8s linear infinite;
}
@keyframes spin {
  to { transform: rotate(360deg); }
}

/* GPU Shimmer Overlay */
.shimmer::after {
  content: '';
  position: absolute;
  inset: 0;
  transform: translateX(-100%);
  background: linear-gradient(90deg, transparent, rgba(255,255,255,0.6), transparent);
  animation: shimmer 1.5s infinite;
}
@keyframes shimmer {
  to { transform: translateX(100%); }
}
```

- **Compositor Rule**: Restrict animation strictly to `transform` and `opacity`.
- **Accessibility**: Include `role="status"`, `aria-live="polite"`, and `.sr-only` text alternatives.
- **Physical Dynamics**: Use cubic-bezier curves with overshoot ($y2 > 1.0$) for tactile button and badge interactions.
- **Reduced Motion**: Fall back to subtle opacity pulses rather than rotational motion.

---

## 13. Further Reading

- [MDN Web Docs: CSS Animations](https://developer.mozilla.org/en-US/docs/Web/CSS/CSS_animations)
- [web.dev: Animations guide and best practices](https://web.dev/animations-guide/)
- [Dan Saffer: Microinteractions: Designing with Details](https://www.oreilly.com/library/view/microinteractions/9781449342937/)
- [W3C WAI: Status Messages (WCAG 2.2 Success Criterion 4.1.3)](https://www.w3.org/WAI/WCAG22/Understanding/status-messages.html)
- [Smashing Magazine: Designing Better Loading Experiences](https://www.smashingmagazine.com/2020/04/better-loading-experiences/)

---

### Navigation
- **Previous**: [08-reduced-motion-and-accessible-animation.md](file:///e:/Learning/engineering-notes/01-languages/css/08-motion/08-reduced-motion-and-accessible-animation.md)
- **Next**: [../09-modern-css/README.md](file:///e:/Learning/engineering-notes/01-languages/css/09-modern-css/README.md)
