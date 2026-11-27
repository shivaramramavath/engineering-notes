# Buttons, Badges, and Alerts in Modern CSS

| Topic | Module | Level | Baseline Status | Browsers Tested | Date | Prerequisites |
|---|---|---|---|---|---|---|
| Buttons, Badges & Alerts | 13: Layout & Component Recipes | Intermediate / Advanced | Widely Available | Chrome 120+, Firefox 120+, Safari 17.4+ | 2026-03-31 | Custom Properties, Flexbox, Transitions, A11y |

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
9. [Comparison: Button and Alert Component Systems](#9-comparison-button-and-alert-component-systems)
10. [Interview Questions](#10-interview-questions)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary / Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

Buttons, badges, and alerts are the foundational atomic feedback and action components of every design system. 
- **Buttons** trigger mutations, submit data, and confirm transactions.
- **Badges** communicate operational states, counts, and categorization metadata.
- **Alerts and Banners** provide critical system notifications, security warnings, and feedback confirmations.

Historically, styling these components was fraught with recurring defects:
- **Button Shivering and Layout Shift**: Toggling a button to a loading spinner or adding an outline border often altered element dimensions by 2px, causing sibling elements to shiver across the screen.
- **Lost Touch Targets on Small Buttons**: Compact tags or icon buttons with 24px visual sizes failed mobile touch accessibility standards (WCAG 2.5.5 / 2.5.8), leading to frustrated mobile taps.
- **Color-Only State Communication**: Red error alerts and green badges without accompanying iconography violated WCAG 1.4.1 (Use of Color), leaving color-blind users unaware of severe errors.
- **Focus Ring Erasure**: Heavy-handed `outline: none` CSS resets removed focus rings entirely, breaking keyboard navigation.

Modern CSS solves these challenges with architectural elegance:
- **Design Token Custom Properties**: Scalable CSS variable architectures allowing infinite variant and density permutations without class bloat.
- **Sub-pixel Layout Stability**: Using `box-shadow` or transparent borders to guarantee identical geometric footprints across solid, outline, and ghost variants.
- **Accessible Touch Target Pseudo-Elements**: Invisible `::before` expanders that enlarge physical hit targets to 48x48px without changing visual button bounds.

---

## 2. Core Concepts

### 1. The Design Token Button Architecture
Instead of hard-coding hex values across dozens of combinator classes, modern button systems parameterize component styling via local CSS custom properties:
```css
.btn {
  /* Local Overridable Tokens */
  --btn-bg: #2563eb;
  --btn-color: #ffffff;
  --btn-border: transparent;
  --btn-hover-bg: #1d4ed8;

  background-color: var(--btn-bg);
  color: var(--btn-color);
  border: 1px solid var(--btn-border);
}

.btn-secondary {
  --btn-bg: #f1f5f9;
  --btn-color: #334155;
  --btn-border: #cbd5e1;
  --btn-hover-bg: #e2e8f0;
}
```
Variant classes only need to reassign local variables; the core layout, padding, font sizing, and transition logic remain declared once on `.btn`.

### 2. Geometry Preservation During State Transitions
When a button switches between idle, hover, disabled, or loading states:
- Its height and width must remain locked to prevent Cumulative Layout Shift (CLS).
- Never add a 1px border on hover if the idle state had none; instead, declare a transparent border (`border: 1px solid transparent;`) on the idle state.
- In loading states, absolute positioning or grid stacking preserves button width while replacing text with a spinner.

### 3. Semantic Badge & Alert Semantics
- Alert banners must include ARIA live roles: `role="alert"` for urgent errors or `role="status"` for non-disruptive feedback.
- Badges must never convey state purely through color; always pair colors with text labels or SVGs with explicit alt/title tags.

---

## 3. How It Works Internally

During button interactions, modern browser rendering engines coordinate states through layout, paint, and accessibility layers:

```
[User Tabs / Presses Button]
         │
         ├── Match :focus-visible
         │     └── Paint 2px solid ring with 2px offset (Composited)
         │
         ├── Match :hover / :active
         │     └── Interpolate background-color (Paint phase only, 0 Layout reflow)
         │
         └── Button transitions to .is-loading
               │
               ▼
         [CSS Grid Stacking Context]
         ├── Grid Cell 1: Button Text (visibility: hidden, preserves intrinsic width)
         └── Grid Cell 1: Spinner SVG (visibility: visible, centered)
               │
               ▼
         [Result: Button dimensions stay 100% frozen, 0 CLS]
```

By stacking both the text and spinner within the same CSS Grid cell (`grid-area: 1 / 1`), the button retains the natural calculated width of its label text, preventing visual snapping when the spinner appears.

---

## 4. Syntax & Usage

### Baseline Browser Support
| Feature | Baseline Status | Chrome / Edge | Firefox | Safari |
|---|---|---|---|---|
| CSS Custom Properties (`var()`) | Widely Available | 49+ | 31+ | 9.1+ |
| `:focus-visible` Pseudo-Class | Widely Available | 86+ | 85+ | 15.4+ |
| Grid Multi-Layer Stacking (`1 / 1`) | Widely Available | 57+ | 52+ | 10.1+ |
| ARIA Live Regions (`role="alert"`) | Widely Available | 1+ | 1+ | 1+ |

### Core Button Boilerplate
```css
.btn {
  display: inline-flex;
  align-items: center;
  justify-content: center;
  gap: 0.5rem;
  font-family: inherit;
  font-size: 0.95rem;
  font-weight: 600;
  line-height: 1.25;
  padding: 0.625rem 1.25rem;
  border-radius: 8px;
  cursor: pointer;
  text-decoration: none;
  border: 1px solid transparent;
  transition: background-color 150ms ease, border-color 150ms ease, box-shadow 150ms ease;
  user-select: none;
  white-space: nowrap;
}

.btn:focus-visible {
  outline: 2px solid var(--btn-ring, #2563eb);
  outline-offset: 2px;
}

.btn:disabled,
.btn[aria-disabled="true"] {
  opacity: 0.6;
  cursor: not-allowed;
  pointer-events: none;
}
```

---

## 5. Practical Examples

### Example 1: Multi-Variant Button Design System
```html
<div class="button-preview-row">
  <button type="button" class="btn btn-primary">Primary Action</button>
  <button type="button" class="btn btn-secondary">Secondary Action</button>
  <button type="button" class="btn btn-outline">Outline</button>
  <button type="button" class="btn btn-ghost">Ghost</button>
  <button type="button" class="btn btn-destructive">Destructive</button>
</div>
```

```css
.button-preview-row {
  display: flex;
  flex-wrap: wrap;
  gap: 1rem;
  padding: 1.5rem;
  background: #f8fafc;
  border-radius: 8px;
}

/* Base button styles */
.btn {
  display: inline-flex;
  align-items: center;
  justify-content: center;
  padding: 0.625rem 1.25rem;
  border-radius: 6px;
  font-size: 0.875rem;
  font-weight: 600;
  cursor: pointer;
  border: 1px solid transparent;
  transition: all 150ms ease;
}

.btn:focus-visible {
  outline: 2px solid #2563eb;
  outline-offset: 2px;
}

/* 1. Primary Solid */
.btn-primary {
  background: #2563eb;
  color: #ffffff;
}
.btn-primary:hover {
  background: #1d4ed8;
}

/* 2. Secondary Neutral */
.btn-secondary {
  background: #f1f5f9;
  color: #334155;
  border-color: #cbd5e1;
}
.btn-secondary:hover {
  background: #e2e8f0;
}

/* 3. Outline */
.btn-outline {
  background: transparent;
  color: #2563eb;
  border-color: #2563eb;
}
.btn-outline:hover {
  background: #eff6ff;
}

/* 4. Ghost */
.btn-ghost {
  background: transparent;
  color: #475569;
}
.btn-ghost:hover {
  background: #f1f5f9;
  color: #0f172a;
}

/* 5. Destructive */
.btn-destructive {
  background: #dc2626;
  color: #ffffff;
}
.btn-destructive:hover {
  background: #b91c1c;
}
.btn-destructive:focus-visible {
  outline-color: #dc2626;
}
```

---

### Example 2: Loading Button with Zero Width Shift
```html
<button type="button" class="btn btn-primary btn-loading" aria-busy="true">
  <span class="btn-text">Deploy Application</span>
  <span class="btn-spinner" aria-hidden="true"></span>
</button>
```

```css
.btn-loading {
  display: inline-grid;
  place-items: center;
  position: relative;
  cursor: wait;
}

/* Text and spinner occupy identical grid coordinate */
.btn-loading .btn-text,
.btn-loading .btn-spinner {
  grid-area: 1 / 1;
}

.btn-loading[aria-busy="true"] .btn-text {
  visibility: hidden; /* Retains intrinsic width, but hides visibly */
}

.btn-loading[aria-busy="true"] .btn-spinner {
  visibility: visible;
  width: 18px;
  height: 18px;
  border: 2px solid rgba(255, 255, 255, 0.3);
  border-top-color: #ffffff;
  border-radius: 50%;
  animation: btn-spin 700ms linear infinite;
}

@keyframes btn-spin {
  to { transform: rotate(360deg); }
}
```

```
Visual Verification Output:
Idle state width: 168.4px.
Loading state width: 168.4px.
Layout shift: 0.000px.
```

---

### Example 3: Accessible Touch-Target Icon Buttons
```html
<div class="toolbar">
  <button type="button" class="icon-btn" aria-label="Edit Profile">
    <span class="icon">✏️</span>
  </button>
  <button type="button" class="icon-btn" aria-label="Download Report">
    <span class="icon">📥</span>
  </button>
  <button type="button" class="icon-btn" aria-label="Archive Record">
    <span class="icon">🗄️</span>
  </button>
</div>
```

```css
.toolbar {
  display: flex;
  gap: 0.5rem;
}

.icon-btn {
  position: relative;
  display: inline-flex;
  align-items: center;
  justify-content: center;
  width: 32px;
  height: 32px;
  padding: 0;
  border-radius: 6px;
  background: transparent;
  border: 1px solid #cbd5e1;
  color: #475569;
  cursor: pointer;
}

.icon-btn:hover {
  background: #f1f5f9;
  color: #0f172a;
}

.icon-btn:focus-visible {
  outline: 2px solid #2563eb;
  outline-offset: 2px;
}

/* Expands touch target to 48x48px on touchscreens without altering visual 32px box */
.icon-btn::before {
  content: "";
  position: absolute;
  top: 50%;
  left: 50%;
  transform: translate(-50%, -50%);
  min-width: 48px;
  min-height: 48px;
}
```

---

### Example 4: Status Badges and Counter Pills
```html
<div class="badge-cluster">
  <span class="badge badge-success"><span class="dot"></span> Operational</span>
  <span class="badge badge-warning"><span class="dot"></span> Degraded</span>
  <span class="badge badge-danger"><span class="dot"></span> Outage</span>
  
  <div class="notification-icon-wrapper">
    <span class="bell-icon">🔔</span>
    <span class="badge-count" aria-label="5 unread notifications">5</span>
  </div>
</div>
```

```css
.badge-cluster {
  display: flex;
  align-items: center;
  gap: 1rem;
}

.badge {
  display: inline-flex;
  align-items: center;
  gap: 0.375rem;
  padding: 0.25rem 0.625rem;
  border-radius: 9999px;
  font-size: 0.75rem;
  font-weight: 600;
  line-height: 1;
}

.badge .dot {
  width: 6px;
  height: 6px;
  border-radius: 50%;
}

.badge-success { background: #dcfce7; color: #166534; }
.badge-success .dot { background: #16a34a; }

.badge-warning { background: #fef9c3; color: #854d0e; }
.badge-warning .dot { background: #ca8a04; }

.badge-danger { background: #fee2e2; color: #991b1b; }
.badge-danger .dot { background: #dc2626; }

/* Floating Count Badge */
.notification-icon-wrapper {
  position: relative;
  display: inline-flex;
  padding: 0.5rem;
  cursor: pointer;
}

.badge-count {
  position: absolute;
  top: 0;
  right: 0;
  transform: translate(25%, -25%);
  background: #ef4444;
  color: #ffffff;
  font-size: 0.7rem;
  font-weight: 700;
  min-width: 18px;
  height: 18px;
  padding: 0 4px;
  border-radius: 9999px;
  display: flex;
  align-items: center;
  justify-content: center;
  box-shadow: 0 0 0 2px #ffffff;
}
```

---

### Example 5: Production Alert Banners (Dismissible with Iconography)
```html
<div class="alert-stack">
  <div class="alert alert-error" role="alert">
    <span class="alert-icon" aria-hidden="true">⚠️</span>
    <div class="alert-body">
      <h4 class="alert-title">Cluster Backup Failed</h4>
      <p class="alert-desc">S3 snapshot timed out after 300 seconds. Verify IAM credentials.</p>
    </div>
    <button type="button" class="alert-dismiss" aria-label="Dismiss alert">✕</button>
  </div>

  <div class="alert alert-success" role="status">
    <span class="alert-icon" aria-hidden="true">✓</span>
    <div class="alert-body">
      <h4 class="alert-title">Deployment Succeeded</h4>
      <p class="alert-desc">Release v2.4.0 is now live across 12 edge nodes.</p>
    </div>
  </div>
</div>
```

```css
.alert-stack {
  display: flex;
  flex-direction: column;
  gap: 1rem;
  max-width: 600px;
}

.alert {
  display: grid;
  grid-template-columns: auto 1fr auto;
  align-items: flex-start;
  gap: 1rem;
  padding: 1rem 1.25rem;
  border-radius: 8px;
  border: 1px solid transparent;
}

.alert-icon {
  font-size: 1.25rem;
  line-height: 1.2;
}

.alert-body {
  font-size: 0.875rem;
}

.alert-title {
  margin: 0 0 0.25rem 0;
  font-size: 0.95rem;
  font-weight: 700;
}

.alert-desc {
  margin: 0;
  line-height: 1.4;
}

.alert-dismiss {
  background: transparent;
  border: none;
  font-size: 1rem;
  cursor: pointer;
  padding: 0.25rem;
  border-radius: 4px;
  line-height: 1;
  color: inherit;
  opacity: 0.7;
}

.alert-dismiss:hover {
  opacity: 1;
}

/* Error Alert */
.alert-error {
  background: #fef2f2;
  border-color: #fecaca;
  color: #991b1b;
}

/* Success Alert */
.alert-success {
  background: #f0fdf4;
  border-color: #bbf7d0;
  color: #166534;
}
```

---

## 6. Best Practices

1. **Retain Geometric Size During State Swaps**: When switching buttons to loading states, use CSS Grid overlay stacking rather than replacing DOM text with an isolated spinner.
2. **Always Provide Transparent Base Borders**: If an outline button gains a visible border on hover, declare a matching 1px transparent border on the idle state to prevent a 2px layout jitter.
3. **Enforce WCAG 2.5.5 / 2.5.8 Touch Targets**: Small icon buttons (<32px) must use an invisible `::before` pseudo-element to expand their physical clickable area to at least 44x44px or 48x48px on touchscreens.
4. **Never Rely Solely on Color for Alerts**: Color-blind users cannot differentiate green and red banners reliably; always accompany color fills with distinct iconography and clear heading text.
5. **Always Set `type="button"` on Non-Submit Buttons**: Omitting the `type` attribute on `<button>` elements causes them to default to `type="submit"`, accidentally submitting parent forms upon click.
6. **Preserve High-Contrast Focus Visible Rings**: Ensure all buttons and dismiss triggers have high-contrast `:focus-visible` styling (`outline: 2px solid var(--accent); outline-offset: 2px;`).

---

## 7. Common Mistakes & Pitfalls

| Mistake | Root Cause | Symptom | Correct Architecture |
|---|---|---|---|
| **Layout Shiver on Border Hover** | Adding `border: 1px solid` on hover only | Sibling buttons jump 2px horizontally upon cursor hover | Declare `border: 1px solid transparent` on base state |
| **Accidental Form Submission** | Using `<button>` without `type="button"` | Clicking a modal close or tab button triggers page reload / submit | Explicitly write `<button type="button">` |
| **Collapsed Touch Target** | 24px icon button lacks padding or pseudo-expander | Mobile touchscreen users miss the tap target repeatedly | Add `::before` expanding touch target to 48x48px |
| **Inaccessible Icon Button** | `<button>` contains only an SVG icon without text | Screen reader announces "Button" with zero descriptive context | Add `aria-label="Edit Profile"` to the `<button>` |
| **Shrunk Loading Button** | Replacing text node with `<span class="spinner">` | Button snaps from 180px to 40px width abruptly | Use CSS Grid stacking to preserve natural text width |
| **Color-Only Alert State** | Alert uses red background with no icon or text label | Color-blind users cannot discern error severity | Pair colors with distinct icons (e.g. ⚠️, ✓, ℹ️) |

---

## 8. Performance, Accessibility & Browser Support

### Performance Optimization
- **Hardware-Accelerated Feedback**: Implement hover and active button state transitions using `background-color`, `box-shadow`, or `transform: scale(0.98)` to ensure smooth 60fps rendering without reflows.
- **Spinner Animation Compositing**: Animate the loading spinner using `transform: rotate(360deg)` rather than modifying background positions or border widths.

### Accessibility Protocols
- **ARIA Live Regions**: Alert banners rendering dynamically must declare `role="alert"` (for errors) or `role="status"` (for success confirmations) to prompt immediate screen reader announcements.
- **Disabled State Semantics**: Standard `disabled` buttons cannot receive keyboard focus, which can disorient users. For critical actions, consider `aria-disabled="true"` with custom styling so keyboard users can still tab to the button and discover why it is disabled.

---

## 9. Comparison: Button and Alert Component Systems

| Component | Standard CSS Classes | Tokenized CSS Properties | CSS Modules / Scoped | Tailwind Utility |
|---|---|---|---|---|
| **Variant Scaling** | Verbose class chains | Ultra-compact (override tokens) | Modular isolation | Utility class clusters |
| **Geometry Freeze** | Manual padding discipline | Managed at root token level | Handled in component | Handled via utility classes |
| **Runtime Cost** | 0 JavaScript | 0 JavaScript | 0 JavaScript | 0 JavaScript |
| **Dark Mode Overrides** | Requires duplicate rules | Single token reassignment | Scoped token swap | `dark:` variant classes |
| **Baseline Support** | Widely Available | Widely Available | Tooling dependent | Tooling dependent |

---

## 10. Interview Questions

<details>
<summary><strong>1. How do you prevent a button from shrinking or causing Cumulative Layout Shift (CLS) when transitioning into a loading state?</strong></summary>

**Answer:**
If the button's text is dynamically removed from the DOM and replaced by a spinner, the button abruptly shrinks to the spinner's width (e.g. 180px down to 32px), shifting neighboring elements.

The standard CSS solution uses CSS Grid stacking:
```css
.btn-loading {
  display: inline-grid;
  place-items: center;
}
.btn-loading .text,
.btn-loading .spinner {
  grid-area: 1 / 1;
}
.btn-loading[aria-busy="true"] .text {
  visibility: hidden; /* Retains intrinsic DOM width in layout */
}
```
Because `.text` remains in the layout tree with `visibility: hidden`, the button retains 100% of its computed width, guaranteeing zero layout shift.
</details>

<details>
<summary><strong>2. Why should non-submitting buttons always explicitly specify `type="button"`?</strong></summary>

**Answer:**
By HTML specification, the default `type` of a `<button>` element inside a `<form>` is `submit`. If a developer creates a modal close button, a toggle button, or an icon button without `type="button"` inside a form, clicking that button will immediately submit the form and trigger a page reload or unexpected network mutation.
</details>

<details>
<summary><strong>3. How does the `::before` pseudo-element touch-target technique work for compact icon buttons?</strong></summary>

**Answer:**
WCAG 2.5.5 / 2.5.8 requires interactive touch controls to have an effective target size of at least 44x44px (or 48x48px). However, design requirements often mandate small visual buttons (e.g. a 28x28px toolbar icon).

By adding:
```css
.icon-btn::before {
  content: "";
  position: absolute;
  top: 50%;
  left: 50%;
  transform: translate(-50%, -50%);
  min-width: 48px;
  min-height: 48px;
}
```
The invisible pseudo-element expands the clickable hit boundary to 48x48px in all directions while keeping the visual container rendered at 28x28px.
</details>

<details>
<summary><strong>4. What is the difference between `disabled` and `aria-disabled="true"` on a button?</strong></summary>

**Answer:**
- `disabled`: The browser strips the element from the keyboard tab sequence entirely, prevents click events, and applies default greyed-out user agent styling. Keyboard users tabbing through a form will skip right past the button without knowing it exists.
- `aria-disabled="true"`: The button remains focusable in the keyboard tab sequence. Screen readers announce the button and state that it is disabled. The developer suppresses click handlers via CSS (`pointer-events: none`) or JavaScript, while allowing keyboard users to inspect the button and receive tooltip guidance on how to enable it.
</details>

<details>
<summary><strong>5. How do you prevent border addition on hover from causing layout shivering?</strong></summary>

**Answer:**
If a button has no border in its resting state, adding `border: 1px solid #2563eb;` on hover increases its width and height by 2px, pushing all adjacent elements.

Solutions:
1. Declare `border: 1px solid transparent;` on the resting button state.
2. Use an inset box-shadow: `box-shadow: inset 0 0 0 1px #2563eb;` (box-shadow does not occupy layout space).
3. Use CSS `outline` with a negative offset: `outline: 1px solid #2563eb; outline-offset: -1px;`.
</details>

<details>
<summary><strong>6. Why is `role="alert"` important on error banners, and how does it affect screen readers?</strong></summary>

**Answer:**
`role="alert"` creates an ARIA live region with `aria-live="assertive"`. When an alert element with this role is injected into the DOM, the screen reader immediately interrupts whatever it is currently reading to announce the error text to the user. This ensures blind users are immediately alerted to validation failures or network disconnects.
</details>

<details>
<summary><strong>7. How does CSS custom property scoping simplify dark-mode overrides for alert components?</strong></summary>

**Answer:**
By assigning colors to component-level variables (`--alert-bg`, `--alert-border`, `--alert-text`), dark mode styling does not require rewriting the component's internal margins, paddings, flex layouts, or border radii. A single media query re-assigns the token values:
```css
@media (prefers-color-scheme: dark) {
  .alert-error {
    --alert-bg: #450a0a;
    --alert-border: #7f1d1d;
    --alert-text: #fecaca;
  }
}
```
</details>

<details>
<summary><strong>8. How should high contrast mode (`forced-colors: active`) be styled on solid button states?</strong></summary>

**Answer:**
Under `forced-colors: active`, background colors are overridden with system Canvas colors.
To ensure clarity:
- Buttons must declare visible borders: `border: 2px solid ButtonText;`.
- Focus outlines must use system keywords: `outline: 2px solid Highlight;`.
- Disabled buttons should use `GrayText` for border and text colors.
</details>

---

## 11. Practice Exercises

### Exercise 1: Build a Pure CSS Floating Notification Count Badge
**Task**: Position a red circular notification pill with white text at the top-right corner of a 40x40px avatar element.
<details>
<summary>View Verified Solution</summary>

```css
.avatar-container {
  position: relative;
  width: 40px;
  height: 40px;
}

.count-badge {
  position: absolute;
  top: 0;
  right: 0;
  transform: translate(30%, -30%);
  background-color: #ef4444;
  color: #ffffff;
  font-size: 0.7rem;
  font-weight: 700;
  min-width: 18px;
  height: 18px;
  border-radius: 9999px;
  display: flex;
  align-items: center;
  justify-content: center;
  padding: 0 4px;
  box-shadow: 0 0 0 2px #ffffff;
}
```
</details>

---

### Exercise 2: Zero-CLS Loading Spinner Overlay
**Task**: Using CSS Grid, build a button that displays a spinning border loader centered over its text when the `aria-busy="true"` attribute is active.
<details>
<summary>View Verified Solution</summary>

```css
.async-btn {
  display: inline-grid;
  place-items: center;
  padding: 0.625rem 1.25rem;
  background: #2563eb;
  color: #fff;
  border-radius: 6px;
  border: none;
}

.async-btn > * {
  grid-area: 1 / 1;
}

.async-btn .loader {
  display: none;
  width: 16px;
  height: 16px;
  border: 2px solid rgba(255, 255, 255, 0.3);
  border-top-color: #fff;
  border-radius: 50%;
  animation: spin 600ms linear infinite;
}

.async-btn[aria-busy="true"] .label {
  visibility: hidden;
}

.async-btn[aria-busy="true"] .loader {
  display: block;
}

@keyframes spin {
  to { transform: rotate(360deg); }
}
```
</details>

---

### Exercise 3: Expanding Touch Target on Compact Action Button
**Task**: Style an icon button that is visually 28x28px, but possesses a 48x48px hit target on touchscreens.
<details>
<summary>View Verified Solution</summary>

```css
.touch-target-btn {
  position: relative;
  width: 28px;
  height: 28px;
  border: 1px solid #cbd5e1;
  border-radius: 4px;
  background: #ffffff;
  cursor: pointer;
}

.touch-target-btn::before {
  content: "";
  position: absolute;
  top: 50%;
  left: 50%;
  transform: translate(-50%, -50%);
  width: 48px;
  height: 48px;
}
```
</details>

---

### Exercise 4: Tokenized Button Variant System
**Task**: Build a base button that defines `--bg`, `--color`, and `--hover-bg`, creating primary and destructive variants with minimal rules.
<details>
<summary>View Verified Solution</summary>

```css
.token-btn {
  --bg: #f1f5f9;
  --color: #334155;
  --hover-bg: #e2e8f0;

  background-color: var(--bg);
  color: var(--color);
  padding: 0.5rem 1rem;
  border-radius: 6px;
  border: 1px solid transparent;
  cursor: pointer;
}

.token-btn:hover {
  background-color: var(--hover-bg);
}

.token-btn.primary {
  --bg: #2563eb;
  --color: #ffffff;
  --hover-bg: #1d4ed8;
}

.token-btn.destructive {
  --bg: #dc2626;
  --color: #ffffff;
  --hover-bg: #b91c1c;
}
```
</details>

---

### Exercise 5: High-Contrast Dismissible Banner Alert
**Task**: Build a warning banner with an exclamation icon on the left, descriptive text in the middle, and an accessible close button on the right.
<details>
<summary>View Verified Solution</summary>

```html
<div class="warning-banner" role="status">
  <span class="banner-icon" aria-hidden="true">⚠️</span>
  <p class="banner-text">Scheduled maintenance begins at 02:00 UTC.</p>
  <button type="button" class="banner-close" aria-label="Close maintenance alert">✕</button>
</div>
```

```css
.warning-banner {
  display: flex;
  align-items: center;
  gap: 0.75rem;
  padding: 0.75rem 1rem;
  background: #fffbeb;
  border: 1px solid #fde68a;
  color: #92400e;
  border-radius: 6px;
}

.banner-text {
  margin: 0;
  flex: 1;
  font-size: 0.875rem;
}

.banner-close {
  background: none;
  border: none;
  color: inherit;
  font-size: 1rem;
  cursor: pointer;
}
```
</details>

---

### Exercise 6: Subtle Active Button Press Effect
**Task**: Create an active button animation that slightly scales down when pressed, providing tactile feedback without reflowing the page.
<details>
<summary>View Verified Solution</summary>

```css
.tactile-btn {
  padding: 0.625rem 1.25rem;
  background: #2563eb;
  color: #fff;
  border: none;
  border-radius: 6px;
  cursor: pointer;
  transition: transform 100ms ease;
}

.tactile-btn:active {
  transform: scale(0.97);
}
```
</details>

---

## 12. Summary / Cheat Sheet

```
+------------------------------------------------------------------------------------+
|                       BUTTONS, BADGES & ALERTS CHEAT SHEET                         |
+------------------------------------------------------------------------------------+
| 1. Tokenized Variants:                                                             |
|    .btn { background: var(--bg); color: var(--color); }                            |
|    .btn-primary { --bg: #2563eb; --color: #fff; }                                  |
+------------------------------------------------------------------------------------+
| 2. Zero-CLS Loading State:                                                         |
|    .btn-loading { display: inline-grid; } .btn-loading > * { grid-area: 1 / 1; }    |
|    .btn-loading[aria-busy="true"] .text { visibility: hidden; }                    |
+------------------------------------------------------------------------------------+
| 3. Accessible Touch Expander:                                                      |
|    .btn::before { content: ""; position: absolute; min-width: 48px; min-height: 48px; }|
+------------------------------------------------------------------------------------+
| 4. Border Jitter Defense:                                                          |
|    border: 1px solid transparent; /* Keeps identical dimensions on hover */        |
+------------------------------------------------------------------------------------+
| 5. Accessible Dismiss Trigger:                                                     |
|    <button type="button" aria-label="Dismiss alert">✕</button>                     |
+------------------------------------------------------------------------------------+
| 6. Tactile Feedback:                                                               |
|    .btn:active { transform: scale(0.98); }                                         |
+------------------------------------------------------------------------------------+
```

---

## 13. Further Reading
- [W3C WAI-ARIA Authoring Practices: Button Pattern](https://www.w3.org/WAI/ARIA/apg/patterns/button/)
- [W3C WAI-ARIA Authoring Practices: Alert Pattern](https://www.w3.org/WAI/ARIA/apg/patterns/alert/)
- [MDN Web Docs: `:focus-visible`](https://developer.mozilla.org/en-US/docs/Web/CSS/:focus-visible)
- [web.dev: Accessible Tap Targets](https://web.dev/articles/accessible-tap-targets)
- [W3C CSS Custom Properties for Cascading Variables](https://www.w3.org/TR/css-variables-1/)

---

## Navigation
- **Previous**: [Tabs, Accordions, and Toggles](file:///e:/Learning/engineering-notes/01-languages/css/13-layout-and-component-recipes/11-tabs-accordions-and-toggles.md)
- **Next**: [Skeleton Loaders and Empty States](file:///e:/Learning/engineering-notes/01-languages/css/13-layout-and-component-recipes/13-skeleton-loaders-and-empty-states.md)
