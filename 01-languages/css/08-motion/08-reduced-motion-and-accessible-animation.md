# Reduced Motion and Accessible Animation

| Metadata | Details |
| :--- | :--- |
| **Spec Level** | Media Queries Level 5 / WCAG 2.2 Level AA & AAA (W3C Recommendation) |
| **Baseline Status** | Widely Available (Supported across all modern browsers and operating systems) |
| **Browsers Tested** | Chrome 74+, Firefox 63+, Safari 10.1+, Edge 79+ |
| **Tested With** | axe-core, Lighthouse A11y, NVDA, VoiceOver, Chrome DevTools Rendering Emulation |

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

Animation on the web can enhance spatial orientation, communicate state transitions, and elevate visual delight. However, for a significant portion of users, animation causes severe physiological reactions:
- **Vestibular Disorders**: Inner ear imbalances where fast parallax scrolling, zoom transitions, or rotating objects cause dizziness, nausea, disorientation, and physical vertigo.
- **Photosensitive Epilepsy & Seizures**: Rapid strobe effects, flurrying flashes, or large contrast oscillations can trigger epileptic seizures.
- **Cognitive Distractions**: Users with Attention Deficit Hyperactivity Disorder (ADHD) or autism may experience extreme cognitive overload when constant looping animations, marquee banners, or autoplaying videos compete for attention.

To address this, the World Wide Web Consortium (W3C) established accessibility standards under **WCAG 2.2**:
- **Criterion 2.2.2: Pause, Stop, Hide (Level A)**: Moving, blinking, or scrolling content lasting longer than 5 seconds must have a mechanism for the user to pause, stop, or hide it.
- **Criterion 2.3.1: Three Flashes or Below Threshold (Level A)**: Web pages must not contain anything that flashes more than three times in any 1-second period.
- **Criterion 2.3.3: Animation from Interactions (Level AAA)**: Motion animation triggered by interaction can be disabled, unless the animation is essential to the functionality or the information being conveyed.

The `prefers-reduced-motion` media query allows operating systems (Windows, macOS, iOS, Android, Linux) to communicate the user's accessibility preferences directly to CSS, enabling websites to eliminate jarring motion while retaining functional state changes.

---

## 2. Core Concepts

### Reduced Motion vs. Zero Motion
A widespread misconception is that "reduced motion" means "kill all animations instantly". In practice:
- **Disallowed**: Large spatial translations ($>50\text{px}$), rapid zooms, viewport parallax, 3D rotations, bouncing physics, screen shaking, and continuous carousel autoplay.
- **Allowed & Recommended**: Subtle opacity cross-fades, instant color changes, outline focus rings, state changes, and progress bars. Opacity fades convey state changes without triggering the inner ear's vestibular ocular reflex.

### The `prefers-reduced-motion` Media Feature
The query evaluates two values:
1. `no-preference`: The user has not requested motion reduction in their operating system settings.
2. `reduce`: The user has toggled "Reduce Motion" or "Turn off unnecessary animations" in their OS accessibility panel.

```css
@media (prefers-reduced-motion: reduce) {
  * {
    /* Safe motion overrides */
  }
}
```

### The `0.01ms` Animation Duration Technique
If you bluntly set `transition: none !important` or `animation: none !important` across the DOM, JavaScript event listeners waiting for `transitionend` or `animationend` events will never fire. In modern frontend frameworks (such as React, Vue, or vanilla promise chains), this causes modals to stay stuck in the DOM or routing transitions to hang indefinitely.

The standardized safe alternative is:
```css
@media (prefers-reduced-motion: reduce) {
  *,
  *::before,
  *::after {
    animation-duration: 0.01ms !important;
    animation-iteration-count: 1 !important;
    transition-duration: 0.01ms !important;
    scroll-behavior: auto !important;
  }
}
```
Setting `0.01ms` collapses the visual animation down to an instantaneous frame, but the browser engine still queues and dispatches the native `transitionend` and `animationend` events, preserving full application functionality.

---

## 3. How It Works Internally

```
OS Accessibility Setting
  │
  ├── Windows: Settings > Accessibility > Visual Effects > Animation Effects [Off]
  ├── macOS: System Settings > Accessibility > Display > Reduce Motion [Checked]
  ├── iOS: Settings > Accessibility > Motion > Reduce Motion [On]
  └── Android: Settings > Accessibility > Remove Animations [On]
        │
        ▼
Browser Engine (Blink / Gecko / WebKit)
  │ reads OS accessibility flags via platform APIs
  ▼
CSS OM Media Query Matcher:
  (prefers-reduced-motion: reduce) evaluates to TRUE
  │
  ├── 1. Disables @keyframes continuous loops
  ├── 2. Collapses duration of spatial transitions (transform)
  ├── 3. Converts spatial movement into instant opacity fades
  └── 4. Replaces smooth scroll behaviors with instant viewport jumps
```

```
Vestibular Reaction vs. Safe Motion Alternatives:
┌─────────────────────────┬─────────────────────────┬─────────────────────────┐
│ Triggering Pattern      │ Physical Reaction       │ Safe Accessible Pattern │
├─────────────────────────┼─────────────────────────┼─────────────────────────┤
│ Fullscreen Parallax     │ Vestibular disorientation,│ Static stacked layers or│
│                         │ motion sickness         │ subtle opacity fade     │
├─────────────────────────┼─────────────────────────┼─────────────────────────┤
│ Deep Zoom / Fly-through │ Nausea, vertigo         │ Cross-fade (opacity)    │
├─────────────────────────┼─────────────────────────┼─────────────────────────┤
│ Looping Bouncing Spinners│ Cognitive overload     │ Linear static bar or    │
│                         │                         │ minimal pulsed opacity  │
├─────────────────────────┼─────────────────────────┼─────────────────────────┤
│ Smooth Scroll Anchor    │ Disorientation during   │ Instant scroll jump     │
│ Jumps (5000px down)     │ rapid scroll sweeps     │ (scroll-behavior: auto) │
└─────────────────────────┴─────────────────────────┴─────────────────────────┘
```

---

## 4. Syntax & Usage

```
Baseline Availability:
├── Chrome / Edge: 74+ (Supported since March 2019)
├── Safari: 10.1+ (Supported since March 2017)
└── Firefox: 63+ (Supported since October 2018)
```

### Media Query Syntax
```css
/* Opt-in motion (progressive enhancement approach) */
.card-hover {
  opacity: 1;
}

@media (prefers-reduced-motion: no-preference) {
  .card-hover {
    transition: transform 0.3s cubic-bezier(0.16, 1, 0.3, 1);
  }
  .card-hover:hover {
    transform: translateY(-8px) scale(1.02);
  }
}

/* Fallback / override approach */
@media (prefers-reduced-motion: reduce) {
  .hero-parallax {
    transform: none !important;
  }
}
```

---

## 5. Practical Examples

### Example 1: Accessible Global Motion Reset
A safe, battle-tested universal reset that collapses animations without breaking JavaScript promise chains.

```css
@media (prefers-reduced-motion: reduce) {
  *,
  *::before,
  *::after {
    animation-duration: 0.01ms !important;
    animation-iteration-count: 1 !important;
    transition-duration: 0.01ms !important;
    scroll-behavior: auto !important;
  }
}
```

### Example 2: Transforming Parallax / Translation into Accessible Fade
Replace vestibular-triggering slide animations with calm, accessible opacity cross-fades.

```html
<section class="feature-card">
  <h3>Scalable Infrastructure</h3>
  <p>Our serverless edges deploy within milliseconds worldwide.</p>
</section>
```

```css
.feature-card {
  opacity: 1;
  transform: translateY(0);
  transition: opacity 0.3s ease, transform 0.3s ease;
}

/* Default dynamic presentation */
@media (prefers-reduced-motion: no-preference) {
  .feature-card.is-hidden {
    opacity: 0;
    transform: translateY(40px);
  }
}

/* Reduced motion preference: remove translation completely */
@media (prefers-reduced-motion: reduce) {
  .feature-card {
    transform: none !important;
  }
  .feature-card.is-hidden {
    opacity: 0;
    transform: none;
  }
}
```

### Example 3: Autoplaying Carousel with Pause/Stop Control (WCAG 2.2 Success Criterion 2.2.2)
Provide interactive pausing mechanisms combined with automatic reduced-motion suspension.

```html
<div class="carousel-container" aria-roledescription="carousel" aria-label="Featured News">
  <div class="carousel-track">
    <div class="carousel-slide" aria-label="1 of 3">News Story 1</div>
    <div class="carousel-slide" aria-label="2 of 3">News Story 2</div>
  </div>
  <button id="carousel-toggle" class="btn-pause" aria-label="Pause automatic carousel sliding">
    Pause Animation
  </button>
</div>
```

```css
.carousel-track {
  display: flex;
  width: 300%;
  animation: slideShow 15s infinite ease-in-out;
}

@keyframes slideShow {
  0%, 28% { transform: translateX(0%); }
  33%, 61% { transform: translateX(-33.333%); }
  66%, 95% { transform: translateX(-66.666%); }
  100% { transform: translateX(0%); }
}

/* Pause when button is active or hover */
.carousel-container.is-paused .carousel-track,
.carousel-container:hover .carousel-track {
  animation-play-state: paused;
}

/* Automatic cancellation for reduced motion users */
@media (prefers-reduced-motion: reduce) {
  .carousel-track {
    animation: none;
    transform: none;
    width: 100%;
    overflow-x: auto;
  }
  .btn-pause {
    display: none;
  }
}
```

### Example 4: Accessible Infinite Loading Spinner
Continuous spinning wheels can cause disorientation or nausea when looked at closely. Convert an infinite rotation into a calm pulsing indicator.

```html
<div class="loader" role="status" aria-label="Loading content...">
  <span class="sr-only">Loading...</span>
</div>
```

```css
.loader {
  width: 2.5rem;
  height: 2.5rem;
  border: 4px solid #e2e8f0;
  border-top-color: #2563eb;
  border-radius: 50%;
  animation: spin 0.8s linear infinite;
}

@keyframes spin {
  to {
    transform: rotate(360deg);
  }
}

/* Accessible reduced-motion alternative */
@media (prefers-reduced-motion: reduce) {
  .loader {
    animation: pulseLoader 1.5s ease-in-out infinite;
    border: 4px solid #2563eb;
    background-color: transparent;
  }
}

@keyframes pulseLoader {
  0%, 100% {
    opacity: 0.3;
    transform: scale(0.95);
  }
  50% {
    opacity: 1;
    transform: scale(1);
  }
}
```

### Example 5: In-App User Toggle Coupled with OS Preferences
Allowing users to override system settings with an explicit in-app website toggle button.

```html
<button id="motion-toggle-btn" class="settings-toggle" aria-pressed="false">
  Reduce Motion: Off
</button>
```

```css
:root {
  --anim-duration: 0.35s;
  --anim-transform: translateY(12px);
}

/* System reduced motion preference */
@media (prefers-reduced-motion: reduce) {
  :root {
    --anim-duration: 0.01ms;
    --anim-transform: translateY(0);
  }
}

/* User manual override via data attribute */
html[data-reduced-motion="true"] {
  --anim-duration: 0.01ms;
  --anim-transform: translateY(0);
}

html[data-reduced-motion="false"] {
  --anim-duration: 0.35s;
  --anim-transform: translateY(12px);
}

.modal-dialog {
  opacity: 1;
  transition: opacity var(--anim-duration) ease, transform var(--anim-duration) ease;
}

.modal-dialog.hidden {
  opacity: 0;
  transform: var(--anim-transform);
}
```

---

## 6. Best Practices

1. **Adopt a "Subtle Replacement" philosophy**: Don't simply strip all visual feedback. When removing translations, retain opacity cross-fades or background color transitions so users still receive clear interaction feedback.
2. **Never use `transition: none !important` globally**: Blanket `none` breaks `transitionend` events and causes JavaScript logic (like state machines, modals, or routing frameworks) to stall. Always use `transition-duration: 0.01ms !important`.
3. **Disable smooth scrolling**: Smooth scrolling across large page spans can induce severe motion sickness. Ensure `html { scroll-behavior: auto !important; }` is included in your reduced-motion stylesheet.
4. **Provide user-facing in-app toggles**: Many users browse on managed work computers or public kiosks where they cannot change OS-level accessibility settings. Provide an in-app motion toggle in settings or footer navigation.
5. **Honor WCAG 2.2 Criterion 2.2.2 for carousels**: Any animated component that autoplays for more than 5 seconds must have an accessible, keyboard-operable pause button.
6. **Limit flashing rates**: Ensure visual effects never flash between 3 Hz and 50 Hz to prevent triggering photosensitive seizures (WCAG 2.3.1).

---

## 7. Common Mistakes & Pitfalls

| Mistake | Consequence | Correction |
| :--- | :--- | :--- |
| `* { transition: none !important; }` | Breaks JavaScript components waiting for `transitionend` event; modals lock up. | Use `transition-duration: 0.01ms !important;`. |
| Leaving smooth scrolling enabled (`scroll-behavior: smooth`) | Clicking anchor links sweeps users rapidly through hundreds of pixels, triggering vertigo. | Override with `scroll-behavior: auto !important;` inside reduced-motion query. |
| Removing focus indicators during motion reduction | Keyboard users lose their visual focus position, failing WCAG 2.4.7. | Ensure focus outlines remain sharp and visible without animated growth. |
| Forgetting infinite video/Lottie backgrounds | Video continues looping in the background, distracting neurodivergent users. | Pause or swap continuous looping videos with static poster frames. |
| Relying solely on OS preferences | Users on locked-down office machines cannot toggle OS settings. | Implement an in-app toggle that syncs with `data-reduced-motion` attribute. |
| Removing functional feedback entirely | User clicks a button and cannot tell if their form was submitted. | Use instant color changes or text indicators instead of sliding animations. |

---

## 8. Performance, Accessibility & Browser Support

### Performance
- Replacing complex transforms and continuous canvas/CSS animations with static states reduces CPU and GPU thread utilization, dramatically lowering device battery consumption.

### Accessibility Standards Compliance
- **WCAG 2.2 Criterion 2.2.2 (Level A)**: Autoplaying animations $> 5\text{s}$ must have pause/stop controls.
- **WCAG 2.2 Criterion 2.3.1 (Level A)**: Flashing must remain $\le 3\text{ flashes/sec}$.
- **WCAG 2.2 Criterion 2.3.3 (Level AAA)**: Motion animation triggered by interaction can be turned off.

### Testing Accessibility in DevTools
In Google Chrome / Microsoft Edge DevTools:
1. Open DevTools (`F12` or `Ctrl+Shift+I`).
2. Press `Ctrl+Shift+P` (Command Palette).
3. Type `Show Rendering` and press Enter.
4. Locate **Emulate CSS media feature prefers-reduced-motion** and select `prefers-reduced-motion: reduce`.

---

## 9. Comparison / When to Use What

| Motion Type | Default (`no-preference`) | Reduced Motion (`reduce`) |
| :--- | :--- | :--- |
| **Page Navigation** | Smooth slide or directional swipe | Instant jump or 100ms opacity cross-fade |
| **Dropdown Menus** | Scale up ($0.8 \to 1.0$) + drop down | Instant display or gentle opacity fade |
| **Hero Parallax** | Layer translation at varying scroll speeds | Static background image |
| **Buttons & CTAs** | Elastic bounce or ripple wave | Instant color/border change |
| **Progress Spinners** | 360-degree continuous spin | Pulsing opacity or numerical percentage |
| **Scroll Anchors** | `scroll-behavior: smooth` | `scroll-behavior: auto` |

---

## 10. Interview Questions

### Q1: What is the physiological purpose behind the `prefers-reduced-motion` media query?
<details>
<summary>View Answer</summary>
The query exists to protect users with vestibular spectrum disorders, photosensitive epilepsy, and cognitive disabilities. Fast spatial motion, parallax, and zooming cause the inner ear's vestibular ocular reflex to conflict with visual perception, inducing vertigo, migraines, and physical nausea. Cognitive distractions from continuous loops can also overwhelm neurodivergent users.
</details>

### Q2: Why is `* { transition: none !important; }` considered an anti-pattern for accessible resets?
<details>
<summary>View Answer</summary>
Setting `transition: none` completely eliminates the transition lifecycle. JavaScript libraries, modern component frameworks, and native web components often rely on awaiting the `transitionend` event to clean up DOM nodes, remove backdrop overlays, or resolve promises. If `transition: none` is set, `transitionend` never fires, causing UI components to freeze. Setting `transition-duration: 0.01ms` collapses visual motion while still firing the required events.
</details>

### Q3: How do WCAG 2.2 Criterion 2.2.2 and Criterion 2.3.3 differ?
<details>
<summary>View Answer</summary>
Criterion 2.2.2 ("Pause, Stop, Hide", Level A) governs content that moves, blinks, or scrolls automatically (such as stock tickers, carousels, or animated GIFs) for more than 5 seconds without user interaction. Criterion 2.3.3 ("Animation from Interactions", Level AAA) governs motion animations triggered *by user actions* (like page transitions, button hover effects, or parallax scrolling) and requires that these interaction-driven animations can be disabled.
</details>

### Q4: When is animation considered "essential" under WCAG 2.3.3?
<details>
<summary>View Answer</summary>
An animation is considered essential if removing it would fundamentally alter the meaning or functionality of the application. Examples include games where projectile physics or collision detection is part of gameplay, interactive simulations demonstrating planetary orbits or physics principles, and video editing scrubbers. In contrast, decorative UI transitions, button bounces, and page wipes are never essential.
</details>

### Q5: How can a developer test `prefers-reduced-motion` without changing their operating system settings?
<details>
<summary>View Answer</summary>
Developers can emulate the preference directly in browser developer tools:
1. In Chrome / Edge: Open the Rendering tab from the Command Menu and toggle "Emulate CSS media feature prefers-reduced-motion: reduce".
2. In Firefox: Open DevTools > Inspector, click the small icon for media query simulation, or visit `about:config` and set `ui.prefersReducedMotion` to `1`.
3. In Playwright / Cypress: Call `page.emulateMedia({ reducedMotion: 'reduce' })`.
</details>

### Q6: Can you achieve accessible motion without removing animation entirely?
<details>
<summary>View Answer</summary>
Yes. Reduced motion does not mean zero motion. It specifically targets large spatial movement, rotation, scaling, and high-frequency flashing. Replacing large physical translations with subtle opacity cross-fades or instant border/color highlights preserves visual hierarchy and state change feedback while remaining safe for vestibular-sensitive users.
</details>

### Q7: What are the risks of using `scroll-behavior: smooth` without a reduced motion check?
<details>
<summary>View Answer</summary>
When a user clicks an in-page anchor link to a section far down the page (e.g., `#footer`), `scroll-behavior: smooth` forces the viewport to rapidly pan across thousands of pixels. This extended rapid scroll simulates motion blur across the user's entire field of vision, which is a frequent trigger for severe vestibular nausea and dizziness.
</details>

### Q8: How should Lottie animations and SVG motion graphics be handled under `prefers-reduced-motion`?
<details>
<summary>View Answer</summary>
Lottie animations should listen to `window.matchMedia('(prefers-reduced-motion: reduce)')` in JavaScript. When matched, developers should invoke `animation.goToAndStop(finalFrame, true)` or pause the animation immediately. For SVG animations written with CSS, media queries should freeze the keyframe progression or hide animated embellishments.
</details>

---

## 11. Practice Exercises

### Exercise 1: Build a Vestibular-Safe Button Hover Effect
**Objective**: Build a button that scales and lifts on hover for standard users, but smoothly transitions background color only for reduced-motion users.
<details>
<summary>View Solution</summary>

```css
.cta-button {
  padding: 0.75rem 1.5rem;
  background-color: #2563eb;
  color: #ffffff;
  border: none;
  border-radius: 0.375rem;
  cursor: pointer;
  transition: background-color 0.2s ease, transform 0.2s ease;
}

.cta-button:hover {
  background-color: #1d4ed8;
}

@media (prefers-reduced-motion: no-preference) {
  .cta-button:hover {
    transform: translateY(-2px) scale(1.02);
  }
}

@media (prefers-reduced-motion: reduce) {
  .cta-button {
    transform: none !important;
  }
}
```
</details>

### Exercise 2: Accessible Skeleton Loading Shimmer
**Objective**: Build a skeleton loading placeholder that displays an animated gradient shimmer by default, but replaces it with a gentle static opacity pulse when reduced motion is preferred.
<details>
<summary>View Solution</summary>

```css
.skeleton-card {
  width: 100%;
  height: 12rem;
  border-radius: 0.5rem;
  background-color: #e2e8f0;
}

@media (prefers-reduced-motion: no-preference) {
  .skeleton-card {
    background: linear-gradient(
      90deg,
      #e2e8f0 25%,
      #f1f5f9 50%,
      #e2e8f0 75%
    );
    background-size: 200% 100%;
    animation: shimmer 1.5s infinite;
  }

  @keyframes shimmer {
    to {
      background-position: -200% 0;
    }
  }
}

@media (prefers-reduced-motion: reduce) {
  .skeleton-card {
    animation: gentlePulse 2s infinite ease-in-out;
  }

  @keyframes gentlePulse {
    0%, 100% { opacity: 0.6; }
    50% { opacity: 1; }
  }
}
```
</details>

### Exercise 3: Safe Dialog Modal Entry Transition
**Objective**: Create a modal dialog that scales in from `scale(0.8)` for standard users, but uses a pure opacity fade for reduced-motion users.
<details>
<summary>View Solution</summary>

```css
.modal-overlay {
  position: fixed;
  inset: 0;
  background-color: rgba(0, 0, 0, 0.5);
  display: flex;
  align-items: center;
  justify-content: center;
  opacity: 0;
  transition: opacity 0.25s ease;
}

.modal-content {
  background: white;
  padding: 2rem;
  border-radius: 0.5rem;
  opacity: 0;
  transition: opacity 0.25s ease, transform 0.25s ease;
}

@media (prefers-reduced-motion: no-preference) {
  .modal-content {
    transform: scale(0.85);
  }
  .modal-overlay.active .modal-content {
    transform: scale(1);
    opacity: 1;
  }
}

@media (prefers-reduced-motion: reduce) {
  .modal-content {
    transform: none !important;
  }
  .modal-overlay.active .modal-content {
    opacity: 1;
  }
}

.modal-overlay.active {
  opacity: 1;
}
```
</details>

### Exercise 4: Accessible Tooltip Appearance
**Objective**: Ensure tooltips appear immediately without sliding animations under reduced motion.
<details>
<summary>View Solution</summary>

```css
.tooltip {
  position: absolute;
  background-color: #1e293b;
  color: white;
  padding: 0.5rem 0.75rem;
  border-radius: 0.25rem;
  opacity: 0;
  pointer-events: none;
  transition: opacity 0.2s ease, transform 0.2s ease;
}

@media (prefers-reduced-motion: no-preference) {
  .tooltip {
    transform: translateY(6px);
  }
  .has-tooltip:hover .tooltip,
  .has-tooltip:focus-visible .tooltip {
    opacity: 1;
    transform: translateY(0);
  }
}

@media (prefers-reduced-motion: reduce) {
  .tooltip {
    transform: none !important;
  }
  .has-tooltip:hover .tooltip,
  .has-tooltip:focus-visible .tooltip {
    opacity: 1;
  }
}
```
</details>

### Exercise 5: Parallax Section Neutralization
**Objective**: Build a hero section with multi-layered parallax effects that flatten cleanly into a static, legible hero layout under reduced motion.
<details>
<summary>View Solution</summary>

```css
.parallax-bg {
  position: absolute;
  inset: 0;
  background-image: url('/assets/mountains.jpg');
  background-size: cover;
  background-position: center;
}

@media (prefers-reduced-motion: no-preference) {
  .parallax-bg {
    transform: translateY(var(--scroll-offset, 0px));
    will-change: transform;
  }
}

@media (prefers-reduced-motion: reduce) {
  .parallax-bg {
    transform: none !important;
    position: absolute;
  }
}
```
</details>

### Exercise 6: Complete Accessible Motion System Reset Utility
**Objective**: Write an accessible, production-ready CSS snippet to include at the root of a design system.
<details>
<summary>View Solution</summary>

```css
@media (prefers-reduced-motion: reduce) {
  html:focus-within {
    scroll-behavior: auto !important;
  }

  *,
  *::before,
  *::after {
    animation-duration: 0.01ms !important;
    animation-iteration-count: 1 !important;
    transition-duration: 0.01ms !important;
    scroll-behavior: auto !important;
  }
}
```
</details>

---

## 12. Summary / Cheat Sheet

```css
/* Universal Accessible Reset */
@media (prefers-reduced-motion: reduce) {
  *,
  *::before,
  *::after {
    animation-duration: 0.01ms !important;
    animation-iteration-count: 1 !important;
    transition-duration: 0.01ms !important;
    scroll-behavior: auto !important;
  }
}

/* Progressive Enhancement Pattern */
.animated-box {
  opacity: 1; /* Safe fallback */
}

@media (prefers-reduced-motion: no-preference) {
  .animated-box {
    transition: transform 0.3s cubic-bezier(0.16, 1, 0.3, 1);
  }
  .animated-box:hover {
    transform: translateY(-8px);
  }
}
```

- **Vestibular Safety**: Avoid large spatial sweeps, scaling, zooming, and rapid movement.
- **Never `transition: none`**: Always use `0.01ms` so `transitionend` events still fire.
- **WCAG 2.2**: Adhere to Criteria 2.2.2 (Pause, Stop, Hide) and 2.3.3 (Animation from Interactions).
- **Graceful Alternative**: Replace translation with gentle opacity cross-fades rather than stripping all visual interaction feedback.

---

## 13. Further Reading

- [MDN Web Docs: prefers-reduced-motion](https://developer.mozilla.org/en-US/docs/Web/CSS/@media/prefers-reduced-motion)
- [W3C WCAG 2.2: Understanding Success Criterion 2.3.3 (Animation from Interactions)](https://www.w3.org/WAI/WCAG22/Understanding/animation-from-interactions.html)
- [W3C WCAG 2.2: Understanding Success Criterion 2.2.2 (Pause, Stop, Hide)](https://www.w3.org/WAI/WCAG22/Understanding/pause-stop-hide.html)
- [A List Apart: Designing Safer Web Animation For Motion Sensitivity](https://alistapart.com/article/designing-safer-web-animation-for-motion-sensitivity/)
- [Smashing Magazine: Respecting Users’ Motion Preferences](https://www.smashingmagazine.com/2021/10/respecting-users-motion-preferences/)

---

### Navigation
- **Previous**: [07-starting-style-and-discrete-animation.md](file:///e:/Learning/engineering-notes/01-languages/css/08-motion/07-starting-style-and-discrete-animation.md)
- **Next**: [09-loading-spinners-and-micro-interactions.md](file:///e:/Learning/engineering-notes/01-languages/css/08-motion/09-loading-spinners-and-micro-interactions.md)
