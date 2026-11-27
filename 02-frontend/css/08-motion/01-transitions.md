# CSS Transitions

| Metadata | Details |
| :--- | :--- |
| **Topic** | State Transitions, `transition` Shorthand & Longhands, Asymmetric Timing, Multiple Transition Stacks, DOM Lifecycle Events |
| **Spec Level** | CSS Transitions Level 1 / CSS Transitions Level 2 |
| **Baseline Status** | Widely Available (`transition` Baseline 2015 across all major desktop and mobile browsers) |
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

In default CSS, property changes occur instantly: when a button is hovered, its background color abruptly snaps from blue to navy in a single paint frame. While functional, abrupt state changes feel jarring, mechanical, and unrefined.

**CSS Transitions** provide a declarative mechanism to interpolate smoothly between two distinct states over a defined duration. When a trigger (such as `:hover`, `:focus-visible`, `:active`, or a JavaScript class change) modifies a style rule, the browser's animation engine automatically calculates intermediate frames at 60fps or 120fps.

Mastering CSS transitions is fundamental to professional UI development:
- **Spatial Cognitive Continuity**: Visual feedback bridges state changes, confirming user intent.
- **Asymmetrical Interaction Design**: Real-world physics requires fast responses to user actions (e.g., a button lighting up in 100ms on hover) and graceful, deliberate decay when the action ceases (e.g., fading back out in 300ms).
- **The Hazard of `transition: all`**: Blindly writing `transition: all` introduces severe performance overhead, accidentally interpolating layout properties like `margin` or `width` and causing layout thrashing.
- **JavaScript Synchronization**: Leveraging the DOM `transitionend` event to coordinate data fetching, dialog unmounting, and accessibility focus management.

---

## 2. Core Concepts

### The Transition Sub-Properties
1. **`transition-property`**: The specific CSS property (or comma-separated list of properties) to animate. Never use `all` in production; specify explicit animatable properties (e.g., `background-color, transform, opacity`).
2. **`transition-duration`**: The length of time the animation takes to complete, specified in seconds (`s`) or milliseconds (`ms`) (e.g., `0.25s`, `250ms`).
3. **`transition-timing-function`**: The acceleration curve governing progress over time: `linear`, `ease` (default), `ease-in`, `ease-out`, `ease-in-out`, or custom `cubic-bezier()`.
4. **`transition-delay`**: Time to wait before beginning interpolation (e.g., `0.1s`). Negative delays cause the animation to begin immediately mid-stream.

### The Shorthand Parsing Engine
```css
/* Syntax: [property] [duration] [timing-function]? [delay]? */
.button {
  transition: background-color 0.2s ease, transform 0.2s cubic-bezier(0.16, 1, 0.3, 1);
}
```
**Parsing Rule**: When two time values appear in the shorthand, the **first time value is always the duration**, and the **second time value is always the delay**.

### Asymmetrical Timing (Enter vs Exit)
In natural physical systems, applying force causes immediate acceleration, while release causes gradual friction decay. 
- **Hover/Enter**: Set on the `:hover` pseudo-class (short duration, snappy curve).
- **Exit/Rest**: Set on the base selector (slightly longer duration, smooth easing).

```css
/* Base selector controls EXIT transition */
.dialog-card {
  opacity: 0;
  translate: 0 10px;
  transition: opacity 0.3s ease-out, translate 0.3s ease-out;
}

/* Active state controls ENTER transition */
.dialog-card.active {
  opacity: 1;
  translate: 0 0;
  transition: opacity 0.15s ease-in, translate 0.15s cubic-bezier(0.16, 1, 0.3, 1);
}
```

---

## 3. How It Works Internally

```
Transition Lifecycle State Machine

1. IDLE STATE: Element sits at initial property value (e.g., opacity: 0.5)
       |
       v
2. TRIGGER EVENT: :hover pseudo-class matches (requests opacity: 1.0)
       |
       v
3. DELAY PHASE: transition-delay countdown begins (if > 0s)
       |
       v
4. START EVENT: DOM fires `transitionrun` and `transitionstart`
       |
       v
5. INTERPOLATION: Browser compositor updates property frame-by-frame
       |
       v
6. END EVENT: Interpolation reaches target; DOM fires `transitionend`
```

### Animatable vs Non-Animatable Properties
Only properties with continuous mathematical or interpolatable values can transition:
- **Animatable**: Colors (`rgb`, `hsl`, `oklch`), lengths (`px`, `rem`, `%`), transforms, opacities, filters, shadows.
- **Non-Animatable (Historically)**: Discrete keywords (`display`, `visibility`, `overflow`). *(Note: Baseline 2024 introduces discrete transitions via `transition-behavior: allow-discrete`, detailed in Note 07).*

---

## 4. Syntax & Usage

### Baseline & Feature Support
- **CSS Transitions**: Baseline Widely Available (supported across all modern desktop and mobile browsers since 2015).
- **`transitionrun`, `transitionstart`, `transitionend`, `transitioncancel` DOM Events**: Baseline Widely Available.

```css
/* Multi-Property Transition */
.interactive-action-btn {
  background-color: #2563eb;
  color: #ffffff;
  translate: 0 0;
  box-shadow: 0 2px 4px rgba(0, 0, 0, 0.1);

  /* Explicit multi-property transition stack */
  transition:
    background-color 0.2s ease,
    translate 0.2s cubic-bezier(0.16, 1, 0.3, 1),
    box-shadow 0.2s ease;
}

.interactive-action-btn:hover {
  background-color: #1d4ed8;
  translate: 0 -2px;
  box-shadow: 0 8px 16px rgba(0, 0, 0, 0.15);
}

.interactive-action-btn:focus-visible {
  outline: 2px solid #2563eb;
  outline-offset: 2px;
}
```

---

## 5. Practical Examples

### Example 1: Asymmetrical Drawer Navigation Toggle
```html
<nav class="side-drawer" id="drawer">
  <div class="drawer-header"><h3>Navigation</h3></div>
  <ul class="drawer-links">
    <li><a href="#">Dashboard</a></li>
    <li><a href="#">Security</a></li>
  </ul>
</nav>
```

```css
/* Base Drawer: Slow, graceful exit */
.side-drawer {
  position: fixed;
  top: 0;
  left: 0;
  width: 280px;
  height: 100vh;
  background-color: #0f172a;
  color: #ffffff;
  padding: 2rem 1.5rem;
  z-index: 1000;
  
  /* Off-screen closed state */
  translate: -100% 0;
  
  /* Exit transition: 350ms with natural decelerating ease */
  transition: translate 0.35s cubic-bezier(0.4, 0, 0.2, 1);
}

/* Open State: Fast, responsive entrance */
.side-drawer.is-open {
  translate: 0 0;
  /* Enter transition: 200ms snappy response */
  transition: translate 0.2s cubic-bezier(0, 0, 0.2, 1);
}
```

### Example 2: Interactive Card Hover with Independent Layers
```html
<article class="feature-card">
  <div class="card-icon">⚡</div>
  <h3>Edge Caching</h3>
  <p>Deliver assets from sixty global points of presence.</p>
</article>
```

```css
.feature-card {
  padding: 2rem;
  border-radius: 12px;
  background-color: #ffffff;
  border: 1px solid #e2e8f0;
  translate: 0 0;
  box-shadow: 0 1px 3px rgba(0, 0, 0, 0.05);

  /* Target specific properties individually */
  transition:
    translate 0.25s cubic-bezier(0.16, 1, 0.3, 1),
    box-shadow 0.25s ease,
    border-color 0.2s ease;
}

.feature-card .card-icon {
  font-size: 2rem;
  display: inline-block;
  scale: 1;
  transition: scale 0.25s cubic-bezier(0.34, 1.56, 0.64, 1); /* Bouncy spring curve */
}

.feature-card:hover {
  translate: 0 -4px;
  border-color: #93c5fd;
  box-shadow: 0 12px 24px -4px rgba(37, 99, 235, 0.12);
}

.feature-card:hover .card-icon {
  scale: 1.2;
}
```

### Example 3: Staggered Micro-Transition Lists via `transition-delay`
```html
<ul class="stagger-menu">
  <li class="item item-1"><a href="#">Overview</a></li>
  <li class="item item-2"><a href="#">Deployments</a></li>
  <li class="item item-3"><a href="#">Monitoring</a></li>
  <li class="item item-4"><a href="#">Settings</a></li>
</ul>
```

```css
.stagger-menu .item {
  opacity: 0;
  translate: -12px 0;
  transition: opacity 0.3s ease, translate 0.3s ease;
}

/* When the menu opens, stagger child entrance using transition-delay */
.stagger-menu.is-visible .item-1 { transition-delay: 0.05s; }
.stagger-menu.is-visible .item-2 { transition-delay: 0.10s; }
.stagger-menu.is-visible .item-3 { transition-delay: 0.15s; }
.stagger-menu.is-visible .item-4 { transition-delay: 0.20s; }

.stagger-menu.is-visible .item {
  opacity: 1;
  translate: 0 0;
}
```

### Example 4: Coordinating JavaScript with `transitionend`
```javascript
const drawer = document.querySelector('.side-drawer');
const closeBtn = document.querySelector('.close-btn');

closeBtn.addEventListener('click', () => {
  drawer.classList.remove('is-open');

  // Wait for the CSS transition to completely finish before cleaning up DOM
  drawer.addEventListener('transitionend', function handler(e) {
    // Ensure we only listen to the primary transform property
    if (e.propertyName === 'translate' || e.propertyName === 'transform') {
      drawer.removeEventListener('transitionend', handler);
      drawer.setAttribute('aria-hidden', 'true');
      console.log('Drawer transition finished; DOM clean!');
    }
  });
});
```

### Example 5: Non-Disruptive Negative Delay
```css
/* Starting a transition midway through its progress curve */
.pulsing-indicator {
  width: 12px;
  height: 12px;
  border-radius: 50%;
  background-color: #10b981;
  opacity: 1;
  /* Negative delay starts the transition 0.5s into a 1s cycle */
  transition: opacity 1s linear -0.5s;
}
```

---

## 6. Best Practices

1. **Never Use `transition: all` in Production**: Explicitly name target properties (`transition: background-color 0.2s, translate 0.2s;`). `all` inadvertently animates unintended properties (such as during window resize or dynamic layout reflows), destroying performance.
2. **Prioritize GPU-Promoted Compositor Properties**: Whenever possible, restrict transitions to `translate`, `scale`, `rotate`, and `opacity`. These animate purely on the compositor thread without layout or paint recalculations.
3. **Use Asymmetric Timings for Tactile Feel**: User-initiated interactions (`:hover`, `:active`) should respond fast (100ms–180ms); release transitions should be slightly longer and gentler (250ms–350ms).
4. **Always Honor `prefers-reduced-motion`**: Users with vestibular disorders must not be subjected to disorienting spatial movement. Reduce durations or switch to opacity fades.
5. **Always Filter `e.propertyName` in `transitionend` Listeners**: If an element transitions three properties (`opacity`, `transform`, `background-color`), the `transitionend` event fires **three separate times**. Always check `if (e.propertyName === 'translate')` to prevent duplicate callbacks.
6. **Keep Durations Under 400ms for UI Micro-Interactions**: Interface controls should feel snappy. Any transition lasting longer than 400ms feels sluggish and impedes power users.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Correction |
| :--- | :--- | :--- |
| **Using `transition: all 0.3s`** | Forces browser to inspect and animate every single computed style, triggering layout jank. | List explicit properties: `transition: opacity 0.2s, translate 0.2s;`. |
| **Attempting to transition to/from `height: auto`** | Browser cannot mathematically interpolate between a fixed length and `auto`. | Use modern grid row interpolation (`grid-template-rows: 0fr` to `1fr`). |
| **Listening to `transitionend` without property filtering** | Fires once for every transitioning property, executing callback multiple times. | Check `if (e.propertyName === 'target-prop')`. |
| **Setting transition on `:hover` only** | When the mouse leaves the element, the hover rule deactivates and styles snap instantly back! | Set the base transition on the default class; override only values in `:hover`. |
| **Excessive transition durations ($> 600\text{ms}$)** | Makes user interface feel unresponsive and slow. | Keep UI interaction transitions between 150ms and 300ms. |
| **Transitioning `top` or `margin` for movement** | Triggers CPU Layout reflows on every single animation frame. | Use `translate` or `transform`. |

---

## 8. Performance, Accessibility & Browser Support

### Accessibility Compliance (WCAG 2.2 AA)
- **Criterion 2.3.3 (Animation from Interactions)**: Motion animation triggered by user action can be turned off unless the animation is essential to functionality.
- **The 0.01ms Strategy**: When disabling animations under `prefers-reduced-motion`, setting `transition: none` breaks JavaScript code that awaits `transitionend`. Instead, set `transition-duration: 0.01ms;`. The transition completes imperceptibly to human eyes while reliably firing the `transitionend` event in the DOM.

---

## 9. Comparison / When to Use What

| Motion Technique | Trigger Requirement | Loop / Multi-Keyframe | Interruptible | Best Used For |
| :--- | :--- | :--- | :--- | :--- |
| **CSS Transitions** | State change (`:hover`, class toggle)| No (Strictly 2 states: A -> B) | **Yes (Reverses smoothly)** | Hover effects, toggles, button clicks, drawers |
| **CSS `@keyframes`** | Automatic or class mount | **Yes (Infinite, 0% -> 100%)** | Complex (Restarts from 0) | Spinners, breathing glows, complex choreographies |
| **Web Animations API**| JavaScript programmatic | Yes | Full control | Physics springs, gesture-bound drag gestures |

---

## 10. Interview Questions

<details>
<summary>1. Why is <code>transition: all</code> considered a major architectural anti-pattern in production CSS?</summary>

1. **Performance Degradation**: When `all` is declared, the browser engine cannot optimize GPU compositor promotion in advance. It must monitor all computed styles on every frame to check for changes, often triggering unnecessary CPU paint cycles.
2. **Unintended Transitions**: If dynamic data causes a container's `height`, `padding`, or `margin` to shift, or if media queries trigger on window resize, elements styled with `transition: all` will animate their layout changes slowly, producing a bizarre, sluggish "stretching" effect across the page.
3. **Best Practice**: Always explicitly enumerate the exact properties being animated: `transition: opacity 0.2s ease, translate 0.2s ease;`.
</details>

<details>
<summary>2. Why does declaring a transition strictly inside a <code>:hover</code> rule cause a visual defect when the mouse leaves?</summary>

When a transition is declared strictly on the pseudo-class:
```css
.btn:hover {
  background-color: #2563eb;
  transition: background-color 0.3s;
}
```
When the user hovers over the button, the `:hover` rule activates and smoothly interpolates to blue over 0.3s. However, the instant the cursor leaves the button, the `:hover` rule ceases to match. The browser falls back to the base `.btn` rule—which has no transition declared—causing the background color to **instantly snap** back to its original state without animating. 

To achieve smooth forward and reverse transitions, the `transition` property must be placed on the base selector (`.btn`).
</details>

<details>
<summary>3. In the transition shorthand <code>transition: transform 0.3s ease 0.1s;</code>, how does the CSS parser distinguish duration from delay?</summary>

Per the W3C CSS Transitions specification, whenever two `<time>` values are provided in the shorthand syntax:
- The **first time value encountered** is always assigned to `transition-duration`.
- The **second time value encountered** is always assigned to `transition-delay`.
In this example, `0.3s` is the duration, and `0.1s` is the delay.
</details>

<details>
<summary>4. What happens when an active transition is interrupted before reaching its final state?</summary>

CSS transitions are naturally interruptible. If a button transitioning to blue over 0.5s is un-hovered at the 0.2s mark, the browser does not jump or restart from the beginning; it smoothly reverses the interpolation curve from its current intermediate computed value back to the resting state, preventing visual stutter.
</details>

<details>
<summary>5. How does a negative <code>transition-delay</code> value behave?</summary>

A positive delay (e.g., `0.2s`) causes the transition to pause for 0.2s before beginning. 
A **negative delay** (e.g., `-0.2s`) causes the transition to start immediately upon trigger, but as if it had **already been running for 0.2 seconds**. The animation begins partway through its timeline curve.
</details>

<details>
<summary>6. Why does setting <code>transition: none</code> in <code>prefers-reduced-motion</code> sometimes break JavaScript single-page applications?</summary>

Many modern UI frameworks and component libraries (e.g., modal managers, drawer routers) attach a `transitionend` event listener to orchestrate unmounting elements from the DOM after an exit animation completes. 

If CSS sets `transition: none !important;`, no transition ever runs, and the browser **never fires the `transitionend` event**. As a result, the JavaScript promise or callback never resolves, and the modal dialog hangs in the DOM permanently. Setting `transition-duration: 0.01ms;` satisfies reduced-motion requirements while ensuring `transitionend` fires reliably.
</details>

<details>
<summary>7. What four DOM events are dispatched during a CSS transition lifecycle?</summary>

1. `transitionrun`: Dispatched the moment a transition is first created (before any delay countdown begins).
2. `transitionstart`: Dispatched when the transition actually begins visual interpolation (after `transition-delay` expires).
3. `transitionend`: Dispatched when the transition successfully reaches its target value.
4. `transitioncancel`: Dispatched if the transition is interrupted before completion (such as if the property changes or the element is removed from the DOM).
</details>

<details>
<summary>8. Can you transition between <code>display: none</code> and <code>display: block</code> with standard CSS Transitions Level 1?</summary>

In CSS Transitions Level 1, no. `display` is a discrete keyword property without interpolatable intermediate values; changing `display` from `none` to `block` causes an instantaneous switch that bypasses any transition on other properties. 
*(Note: CSS Transitions Level 2 and Baseline 2024 solve this with `@starting-style` and `transition-behavior: allow-discrete`, covered in detail in Note 07).*
</details>

---

## 11. Practice Exercises

### Exercise 1: Build Snappy Asymmetrical Action Button
**Goal**: Create an action button that scales and darkens in 120ms on hover, but releases smoothly in 300ms on unhover.

<details>
<summary>View Solution</summary>

```css
.asymmetric-btn {
  padding: 0.75rem 1.5rem;
  background-color: #2563eb;
  color: #ffffff;
  border: none;
  border-radius: 6px;
  cursor: pointer;
  translate: 0 0;
  
  /* Rest / Exit transition: smooth decelerating return */
  transition:
    background-color 0.3s ease,
    translate 0.3s cubic-bezier(0.16, 1, 0.3, 1);
}

.asymmetric-btn:hover {
  background-color: #1d4ed8;
  translate: 0 -2px;
  
  /* Active / Enter transition: immediate tactile response */
  transition:
    background-color 0.12s ease,
    translate 0.12s cubic-bezier(0, 0, 0.2, 1);
}

.asymmetric-btn:focus-visible {
  outline: 2px solid #2563eb;
  outline-offset: 2px;
}
```
</details>

### Exercise 2: Multi-Layer Card Lift with Independent Shadow
**Goal**: Author a card transition where `translate` uses a spring-like cubic-bezier and `box-shadow` uses gentle linear easing.

<details>
<summary>View Solution</summary>

```css
.lift-card {
  background-color: #ffffff;
  border-radius: 12px;
  padding: 1.5rem;
  box-shadow: 0 2px 4px rgba(0, 0, 0, 0.05);
  translate: 0 0;
  
  transition:
    translate 0.25s cubic-bezier(0.34, 1.56, 0.64, 1),
    box-shadow 0.25s ease;
}

.lift-card:hover {
  translate: 0 -6px;
  box-shadow: 0 16px 24px rgba(0, 0, 0, 0.1);
}
```
</details>

### Exercise 3: Cascade Staggered Dropdown Menu
**Goal**: Write CSS that cascades three dropdown items with 50ms incremental delays when the menu opens.

<details>
<summary>View Solution</summary>

```html
<ul class="dropdown-list">
  <li class="drop-item">Profile</li>
  <li class="drop-item">Billing</li>
  <li class="drop-item">Log Out</li>
</ul>
```

```css
.drop-item {
  opacity: 0;
  translate: 0 -8px;
  transition: opacity 0.2s ease, translate 0.2s ease;
}

.dropdown-list.is-open .drop-item:nth-child(1) {
  transition-delay: 0.05s;
}

.dropdown-list.is-open .drop-item:nth-child(2) {
  transition-delay: 0.10s;
}

.dropdown-list.is-open .drop-item:nth-child(3) {
  transition-delay: 0.15s;
}

.dropdown-list.is-open .drop-item {
  opacity: 1;
  translate: 0 0;
}
```
</details>

### Exercise 4: Clean JavaScript `transitionend` Event Hook
**Goal**: Write an event listener that executes a callback exactly once after a card's `translate` transition finishes.

<details>
<summary>View Solution</summary>

```javascript
function animateAndRemove(element) {
  element.classList.add('fade-out');

  function handleTransitionEnd(event) {
    if (event.propertyName === 'opacity') {
      element.removeEventListener('transitionend', handleTransitionEnd);
      element.remove();
    }
  }

  element.addEventListener('transitionend', handleTransitionEnd);
}
```
</details>

### Exercise 5: Vestibular Safe Transition Override
**Goal**: Write a global reduced-motion utility using the 0.01ms pattern to keep JS lifecycle listeners functional.

<details>
<summary>View Solution</summary>

```css
@media (prefers-reduced-motion: reduce) {
  *,
  *::before,
  *::after {
    /* 0.01ms allows JS transitionend to fire while eliminating visible motion */
    transition-duration: 0.01ms !important;
    transition-delay: 0ms !important;
  }
}
```
</details>

### Exercise 6: Micro-Interaction Icon Rotate on Accordion Toggle
**Goal**: Smoothly rotate an accordion chevron icon 180 degrees when its parent is expanded.

<details>
<summary>View Solution</summary>

```css
.accordion-header .chevron {
  width: 16px;
  height: 16px;
  rotate: 0deg;
  transition: rotate 0.3s cubic-bezier(0.16, 1, 0.3, 1);
}

.accordion-item[aria-expanded="true"] .chevron {
  rotate: 180deg;
}
```
</details>

---

## 12. Summary / Cheat Sheet

| Property / Concept | Example Syntax | Role |
| :--- | :--- | :--- |
| `transition` (Shorthand) | `transition: translate 0.2s ease;` | Declares property, duration, timing, and delay |
| `transition-property` | `transition-property: opacity, translate;` | Explicitly lists animatable properties (Avoid `all`!) |
| `transition-delay` | `transition-delay: 0.1s;` | Postpones start of transition |
| Asymmetrical Timing | Base: `0.3s ease-out`; Hover: `0.1s ease-in` | Snappy interactive entry, graceful deceleration exit |
| `transitionend` Event | `el.addEventListener('transitionend', ...)` | Coordinates JavaScript cleanup after CSS animation completes |
| Reduced Motion Pattern | `transition-duration: 0.01ms !important;` | Disables visual motion without breaking JS event listeners |

---

## 13. Further Reading
- [W3C CSS Transitions Level 1](https://www.w3.org/TR/css-transitions-1/)
- [MDN Web Docs: CSS Transitions Guide](https://developer.mozilla.org/en-US/docs/Web/CSS/CSS_transitions/Using_CSS_transitions)
- [MDN Web Docs: transition-property](https://developer.mozilla.org/en-US/docs/Web/CSS/transition-property)
- [web.dev: The CSS transition property](https://web.dev/learn/css/transitions/)
- [Josh W. Comeau: The Rules of CSS Transitions](https://www.joshwcomeau.com/animation/css-transitions/)

---

[Previous: Module 07 README](file:///e:/Learning/engineering-notes/01-languages/css/07-visual-styling/README.md) | [Module README](README.md) | [Next: 02-animations-and-keyframes.md](02-animations-and-keyframes.md)
