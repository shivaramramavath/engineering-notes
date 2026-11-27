# Tabs, Accordions, and Toggles in Modern CSS

| Topic | Module | Level | Baseline Status | Browsers Tested | Date | Prerequisites |
|---|---|---|---|---|---|---|
| Tabs, Accordions & Toggles | 13: Layout & Component Recipes | Intermediate / Advanced | Widely Available | Chrome 120+, Firefox 120+, Safari 17.4+ | 2026-03-31 | Details/Summary, Grid Animation, Transitions |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Usage](#4-syntax--usage)
5. [Practical Examples](#5-practical-examples)
6. [Best Practices](#6-best-practices)
7. [Common Mistakes & Pitfalls](#7-Common-mistakes--pitfalls)
8. [Performance, Accessibility & Browser Support](#8-performance-accessibility--browser-support)
9. [Comparison: Interactive Disclosure Architectures](#9-comparison-interactive-disclosure-architectures)
10. [Interview Questions](#10-interview-questions)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary / Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

Interactive disclosure widgets—tabs, FAQ accordions, expanders, and switch toggles—are indispensable UI patterns used to manage information density, preserve cognitive focus, and group related workflows.

Historically, implementing these widgets was plagued by classic web problems:
- **The `height: auto` Animation Trap**: Animating an accordion from collapsed (0px) to expanded (`auto`) was impossible in pure CSS. Developers resorted to `max-height: 1000px` hacks, which broke timing curves and caused sluggish, delayed collapse animations.
- **Accordion Mutex JavaScript Overhead**: Grouping FAQ accordions so that opening one item automatically closes siblings required custom JavaScript state managers.
- **Broken Keyboard Accessibility in Tabs**: Custom `<div>` tabs frequently lacked keyboard arrow-key navigation (<kbd>→</kbd>, <kbd>←</kbd>) and ARIA `tablist` bindings, creating accessibility barriers for screen reader users.

Modern CSS and standard HTML resolve these issues natively:
- **Native `<details name="group">`**: Exclusive accordions (where opening one item collapses others) are now built into standard HTML/CSS without a single line of JavaScript.
- **The CSS Grid `0fr` to `1fr` Transition Technique**: Provides silky-smooth, mathematically accurate height animations to dynamic content heights without `max-height` hacks.
- **`interpolate-size: allow-keywords`**: Enables direct transitions to `height: auto` natively in modern browser engines.
- **Accessible Radio and Switch Patterns**: Provides declarative tab panels and toggle controls that honor full screen reader keyboard navigation.

---

## 2. Core Concepts

### 1. The Native `<details>` and `<summary>` Element
The HTML `<details>` element provides native browser disclosure logic:
- Built-in keyboard accessibility: Pressing <kbd>Enter</kbd> or <kbd>Space</kbd> on `<summary>` toggles the `open` attribute.
- Built-in find-in-page integration: Browsers automatically expand collapsed `<details>` if matching text is found via <kbd>Ctrl+F</kbd>.
- Exclusive grouping via `name`: Adding `name="faq"` across multiple `<details>` elements creates a native accordion group where only one item can be expanded at a time.

```html
<details name="faq-group">
  <summary>How does the distributed runtime operate?</summary>
  <p>Nodes communicate over an encrypted peer-to-peer wireguard mesh.</p>
</details>

<details name="faq-group">
  <summary>What is the failover latency?</summary>
  <p>Failover executes sub-10ms across regional clusters.</p>
</details>
```

### 2. The CSS Grid Dynamic Height Transition
Because `height: auto` traditionally could not be transitioned, CSS developers use a CSS Grid wrapper with `grid-template-rows`:
```css
.accordion-content {
  display: grid;
  grid-template-rows: 0fr;
  transition: grid-template-rows 250ms ease;
}

.accordion[open] .accordion-content {
  grid-template-rows: 1fr;
}

.accordion-inner {
  overflow: hidden;
  min-height: 0; /* Crucial for 0fr collapse */
}
```
When `grid-template-rows` transitions from `0fr` to `1fr`, the browser animates smoothly to the exact computed height of the child content, preserving linear easing curves without delays.

---

## 3. How It Works Internally

```
[User Clicks Summary]
        │
        ▼
[Browser toggles `open` attribute on <details>]
        │
        ├── Native Name Mutex: Browser checks if sibling has name="faq"
        │     └── If matching sibling is open -> Closes sibling automatically
        │
        └── Match Selector: details[open] .accordion-grid-wrapper
              │
              ▼
        [Grid Track Interpolation: 0fr -> 1fr]
              │
              ├── min-height: 0 allows row track to compress to 0px
              ├── Inner content measured during layout
              └── GPU Compositor smoothly interpolates row height
```

By animating the grid track fraction (`fr`), the browser computes the target height dynamically, eliminating the awkward pauses and velocity mismatches inherent to legacy `max-height: 1000px` hacks.

---

## 4. Syntax & Usage

### Baseline Browser Support
| Feature | Baseline Status | Chrome / Edge | Firefox | Safari |
|---|---|---|---|---|
| `<details>` and `<summary>` | Widely Available | 12+ | 49+ | 6+ |
| Exclusive Accordion (`name` attribute) | Widely Available | 120+ | 125+ | 17.2+ |
| Grid `0fr` to `1fr` Transitions | Widely Available | 107+ | 115+ | 16.0+ |
| `::marker` Pseudo-Element Styling | Widely Available | 86+ | 80+ | 14.0+ |

### Core Accordion Boilerplate
```css
/* Remove default disclosure triangle */
summary {
  list-style: none;
  cursor: pointer;
  display: flex;
  align-items: center;
  justify-content: space-between;
  padding: 1rem;
  font-weight: 600;
}

summary::-webkit-details-marker {
  display: none;
}

/* Custom animated chevron */
summary .chevron {
  transition: transform 200ms ease;
}

details[open] summary .chevron {
  transform: rotate(180deg);
}

/* Smooth Grid Expansion */
.accordion-body {
  display: grid;
  grid-template-rows: 0fr;
  transition: grid-template-rows 250ms cubic-bezier(0.16, 1, 0.3, 1);
}

details[open] .accordion-body {
  grid-template-rows: 1fr;
}

.accordion-body > div {
  overflow: hidden;
  min-height: 0;
}
```

---

## 5. Practical Examples

### Example 1: Pure HTML/CSS Animated Exclusive Accordion Group
```html
<div class="faq-accordion-group">
  <details name="billing-faq" class="accordion-item" open>
    <summary>
      <span>What payment methods are supported?</span>
      <span class="chevron" aria-hidden="true">▾</span>
    </summary>
    <div class="accordion-body">
      <div class="accordion-inner">
        <p>We accept all major credit cards, automated ACH transfers, wire transfers, and cryptocurrency settlement for enterprise commitments.</p>
      </div>
    </div>
  </details>

  <details name="billing-faq" class="accordion-item">
    <summary>
      <span>Can I pause our cluster subscription?</span>
      <span class="chevron" aria-hidden="true">▾</span>
    </summary>
    <div class="accordion-body">
      <div class="accordion-inner">
        <p>Yes. Subscriptions can be paused for up to 90 consecutive calendar days. Storage volumes remain encrypted and persistent during pauses.</p>
      </div>
    </div>
  </details>
</div>
```

```css
.faq-accordion-group {
  max-width: 640px;
  border: 1px solid #e2e8f0;
  border-radius: 10px;
  overflow: hidden;
  background: #ffffff;
}

.accordion-item {
  border-bottom: 1px solid #e2e8f0;
}

.accordion-item:last-child {
  border-bottom: none;
}

summary {
  list-style: none;
  display: flex;
  justify-content: space-between;
  align-items: center;
  padding: 1.25rem 1.5rem;
  font-weight: 600;
  color: #0f172a;
  cursor: pointer;
  background: #ffffff;
  transition: background-color 150ms ease;
}

summary::-webkit-details-marker {
  display: none;
}

summary:hover {
  background-color: #f8fafc;
}

summary:focus-visible {
  outline: 2px solid #2563eb;
  outline-offset: -2px;
}

.chevron {
  font-size: 1.25rem;
  color: #64748b;
  transition: transform 200ms cubic-bezier(0.16, 1, 0.3, 1);
}

details[open] .chevron {
  transform: rotate(180deg);
  color: #2563eb;
}

/* Grid Animation */
.accordion-body {
  display: grid;
  grid-template-rows: 0fr;
  transition: grid-template-rows 250ms cubic-bezier(0.16, 1, 0.3, 1);
}

details[open] .accordion-body {
  grid-template-rows: 1fr;
}

.accordion-inner {
  overflow: hidden;
  min-height: 0;
}

.accordion-inner p {
  margin: 0;
  padding: 0 1.5rem 1.25rem 1.5rem;
  color: #475569;
  line-height: 1.6;
}
```

---

### Example 2: Accessible CSS-Only Radio Tab Bar
```html
<div class="css-tab-container">
  <input type="radio" name="code-tabs" id="tab-ts" class="tab-radio" checked>
  <label for="tab-ts" class="tab-label">TypeScript</label>

  <input type="radio" name="code-tabs" id="tab-py" class="tab-radio">
  <label for="tab-py" class="tab-label">Python</label>

  <input type="radio" name="code-tabs" id="tab-go" class="tab-radio">
  <label for="tab-go" class="tab-label">Go</label>

  <div class="tab-panels">
    <div class="panel panel-ts">
      <pre><code>import { Client } from "@api/sdk";
const client = new Client({ apiKey: process.env.API_KEY });</code></pre>
    </div>
    <div class="panel panel-py">
      <pre><code>from api_sdk import Client
client = Client(api_key=os.environ["API_KEY"])</code></pre>
    </div>
    <div class="panel panel-go">
      <pre><code>client := sdk.NewClient(os.Getenv("API_KEY"))</code></pre>
    </div>
  </div>
</div>
```

```css
.css-tab-container {
  display: flex;
  flex-wrap: wrap;
  max-width: 600px;
  background: #0f172a;
  border-radius: 8px;
  padding: 0.5rem;
  color: #ffffff;
}

.tab-radio {
  position: absolute;
  opacity: 0;
  pointer-events: none;
}

.tab-label {
  padding: 0.5rem 1rem;
  font-weight: 500;
  font-size: 0.875rem;
  color: #94a3b8;
  cursor: pointer;
  border-radius: 6px;
  transition: all 150ms ease;
}

.tab-label:hover {
  color: #ffffff;
}

/* Checked styling */
#tab-ts:checked ~ label[for="tab-ts"],
#tab-py:checked ~ label[for="tab-py"],
#tab-go:checked ~ label[for="tab-go"] {
  background: #1e293b;
  color: #38bdf8;
  font-weight: 600;
}

/* Keyboard focus indicator on active label */
.tab-radio:focus-visible + .tab-label {
  outline: 2px solid #38bdf8;
  outline-offset: 2px;
}

.tab-panels {
  width: 100%;
  margin-top: 0.5rem;
  border-top: 1px solid #1e293b;
  padding-top: 0.75rem;
}

.panel {
  display: none;
  font-family: monospace;
  font-size: 0.875rem;
}

#tab-ts:checked ~ .tab-panels .panel-ts,
#tab-py:checked ~ .tab-panels .panel-py,
#tab-go:checked ~ .tab-panels .panel-go {
  display: block;
}

.panel pre {
  margin: 0;
  padding: 0.5rem;
  color: #e2e8f0;
}
```

---

### Example 3: Sliding Active Pill Tab Navigation
```html
<nav class="pill-tabs" aria-label="Project Views">
  <div class="pill-track">
    <button type="button" class="pill-btn active">Overview</button>
    <button type="button" class="pill-btn">Deployments</button>
    <button type="button" class="pill-btn">Environment</button>
    <button type="button" class="pill-btn">Metrics</button>
  </div>
</nav>
```

```css
.pill-tabs {
  display: inline-flex;
  background: #f1f5f9;
  padding: 0.35rem;
  border-radius: 9999px;
  border: 1px solid #e2e8f0;
}

.pill-track {
  display: flex;
  gap: 0.25rem;
}

.pill-btn {
  background: transparent;
  border: none;
  padding: 0.5rem 1.125rem;
  border-radius: 9999px;
  font-size: 0.875rem;
  font-weight: 500;
  color: #64748b;
  cursor: pointer;
  transition: color 150ms ease, background-color 150ms ease, box-shadow 150ms ease;
}

.pill-btn:hover {
  color: #0f172a;
}

.pill-btn:focus-visible {
  outline: 2px solid #2563eb;
  outline-offset: 2px;
}

.pill-btn.active {
  background: #ffffff;
  color: #0f172a;
  font-weight: 600;
  box-shadow: 0 1px 3px rgba(0, 0, 0, 0.1);
}
```

---

### Example 4: Nested Multi-Tier Settings Accordion
```html
<details class="settings-tier-1" open>
  <summary>Security & Authentication</summary>
  <div class="tier-content">
    <details class="settings-tier-2">
      <summary>Two-Factor Authentication (2FA)</summary>
      <div class="nested-body">
        <p>Configured using WebAuthn security keys.</p>
      </div>
    </details>

    <details class="settings-tier-2">
      <summary>Active Session Revocation</summary>
      <div class="nested-body">
        <p>Terminate all active JWT bearer tokens across mobile and web.</p>
      </div>
    </details>
  </div>
</details>
```

```css
.settings-tier-1 {
  border: 1px solid #cbd5e1;
  border-radius: 8px;
  background: #ffffff;
  padding: 0.5rem;
  max-width: 500px;
}

.settings-tier-1 > summary {
  font-size: 1.125rem;
  font-weight: 700;
  padding: 0.75rem;
  cursor: pointer;
}

.tier-content {
  padding-left: 1rem;
  border-left: 2px solid #e2e8f0;
  margin: 0.5rem 0 0.5rem 0.5rem;
  display: flex;
  flex-direction: column;
  gap: 0.5rem;
}

.settings-tier-2 {
  border: 1px solid #e2e8f0;
  border-radius: 6px;
  background: #f8fafc;
}

.settings-tier-2 summary {
  font-size: 0.95rem;
  font-weight: 600;
  padding: 0.5rem 0.75rem;
  cursor: pointer;
}

.nested-body {
  padding: 0.75rem;
  color: #64748b;
  font-size: 0.875rem;
}
```

---

### Example 5: High-Contrast Accessible Toggle Switch
```html
<label class="a11y-toggle">
  <input type="checkbox" role="switch" aria-checked="false">
  <span class="toggle-slider"></span>
  <span class="toggle-text">Dark Mode</span>
</label>
```

```css
.a11y-toggle {
  display: inline-flex;
  align-items: center;
  gap: 0.75rem;
  cursor: pointer;
  user-select: none;
}

.a11y-toggle input {
  position: absolute;
  width: 1px;
  height: 1px;
  clip: rect(0 0 0 0);
  clip-path: inset(50%);
  overflow: hidden;
  white-space: nowrap;
}

.toggle-slider {
  width: 48px;
  height: 26px;
  background: #94a3b8;
  border-radius: 9999px;
  position: relative;
  transition: background-color 200ms ease;
  border: 2px solid transparent;
}

.toggle-slider::after {
  content: "";
  position: absolute;
  top: 2px;
  left: 2px;
  width: 18px;
  height: 18px;
  background: #ffffff;
  border-radius: 50%;
  transition: transform 200ms cubic-bezier(0.16, 1, 0.3, 1);
}

.a11y-toggle input:checked + .toggle-slider {
  background: #2563eb;
}

.a11y-toggle input:checked + .toggle-slider::after {
  transform: translateX(22px);
}

.a11y-toggle input:focus-visible + .toggle-slider {
  outline: 2px solid #2563eb;
  outline-offset: 3px;
}

/* High Contrast OS Mode */
@media (forced-colors: active) {
  .toggle-slider {
    border: 2px solid ButtonText;
  }
  .toggle-slider::after {
    background: ButtonText;
  }
  .a11y-toggle input:checked + .toggle-slider::after {
    background: Highlight;
  }
}
```

---

## 6. Best Practices

1. **Leverage Native HTML `<details name="...">` for Accordion Groups**: Avoid writing custom JavaScript event handlers to maintain accordion exclusivity. Native browser grouping handles states natively with zero script overhead.
2. **Always Use the CSS Grid `0fr` to `1fr` Height Technique**: Discard legacy `max-height: 1000px` hacks. Grid track transitions animate to the exact pixel height smoothly without timing curve delays.
3. **Preserve Keyboard Navigation in Tab Bars**: If building tab panels via custom radio inputs, ensure arrow keys cycle through labels and pressing <kbd>Space</kbd> selects the active tab.
4. **Remove Default Summary Triangles Cleanly**: Standardize summary marker removal using both `summary { list-style: none; }` and `summary::-webkit-details-marker { display: none; }`.
5. **Set `min-height: 0` on Animated Grid Children**: Grid track items cannot collapse to `0fr` unless their direct child declares `min-height: 0` and `overflow: hidden`.
6. **Provide Visible Focus States**: Toggle switches and accordion summaries must render crisp, high-contrast `:focus-visible` rings.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Root Cause | Symptom | Correct Architecture |
|---|---|---|---|
| **Laggy Max-Height Accordion** | Using `max-height: 1000px` for transitions | Collapse animation delays for hundreds of milliseconds | Transition `grid-template-rows: 0fr` to `1fr` |
| **Grid Row Won't Collapse** | Omitting `min-height: 0` on inner accordion wrapper | Content remains stuck at full height or jumps abruptly | Declare `min-height: 0; overflow: hidden;` on the inner container |
| **Broken Find-in-Page** | Hiding accordion content via `display: none` in custom divs | Browser <kbd>Ctrl+F</kbd> cannot search or expand collapsed text | Use native `<details>` which browsers auto-expand for search |
| **Inaccessible Custom Switch** | Using `display: none` on `<input type="checkbox">` | Switch cannot receive keyboard focus; screen readers miss state | Use visually-hidden clip-path technique |
| **Double Arrow Markers** | Forgetting `::-webkit-details-marker { display: none; }` | Safari renders native disclosure arrow next to custom chevron | Declare both `list-style: none` and WebKit marker reset |
| **Broken Tab Focus Order** | Setting `order` in Flexbox on tab panels | Tabbing order jumps erratically across screen | Maintain clean matching DOM source order |

---

## 8. Performance, Accessibility & Browser Support

### Layout Performance
- **Compositor Efficiency**: When animating `<details>` height using CSS Grid, the browser calculates row heights dynamically without triggering style invalidation outside the accordion container.
- **Paint Containment**: Apply `contain: paint` to long FAQ accordion lists to isolate internal expand/collapse repaints from the rest of the document.

### Accessibility Protocols
- **Semantic Disclosure**: `<details>` and `<summary>` provide native ARIA `disclosure` semantics that are supported across 100% of modern screen readers.
- **Switch Role**: Checkbox switches must include `role="switch"` and `aria-checked="true|false"` so assistive technology announces "Switch, checked" rather than "Checkbox".

---

## 9. Comparison: Interactive Disclosure Architectures

| Feature | Native `<details name>` | Grid Animated Accordion | CSS Radio Tabs | JS ARIA Tablist |
|---|---|---|---|---|
| **Zero JavaScript** | Yes (100% Native) | Yes (100% Native) | Yes (100% Native) | No (Requires script) |
| **Find-in-Page Support** | Yes (Auto-expands) | Yes (Auto-expands) | No | No |
| **Exclusive Grouping** | Built-in via `name` | Built-in via `name` | Built-in via `name` | Manual script |
| **Smooth Height Transition** | Instant (or discrete) | Perfect (via `0fr -> 1fr`)| Tab fade | Tab fade |
| **Baseline Status** | Widely Available | Widely Available | Widely Available | Universally Supported |

---

## 10. Interview Questions

<details>
<summary><strong>1. How does the CSS Grid `0fr` to `1fr` technique solve the long-standing `height: auto` animation problem?</strong></summary>

**Answer:**
CSS transitions cannot interpolate between fixed values (e.g. `0px`) and the keyword `auto` because the layout engine cannot determine intermediate numerical values during layout calculation.

Historically, authors worked around this with `max-height: 1000px`. However, if the actual content is only 100px tall, 90% of the transition duration is spent animating invisible empty space, causing severe delays on collapse.

The CSS Grid technique defines a wrapper:
```css
.accordion-wrapper {
  display: grid;
  grid-template-rows: 0fr;
  transition: grid-template-rows 250ms ease;
}
.open .accordion-wrapper {
  grid-template-rows: 1fr;
}
.accordion-inner {
  overflow: hidden;
  min-height: 0;
}
```
Grid row tracks *can* be interpolated between `0fr` and `1fr`. Because `1fr` automatically resolves to the exact intrinsic content height, the animation begins and ends with perfect mathematical precision.
</details>

<details>
<summary><strong>2. How does the HTML `name` attribute create exclusive accordions in native `<details>` elements?</strong></summary>

**Answer:**
As part of HTML5 / Baseline 2024 specifications, multiple `<details>` elements sharing the same `name` attribute value (e.g. `<details name="faq">`) form an exclusive accordion group.

When a user clicks a closed `<details>` element in the group, the browser engine automatically finds any currently open sibling `<details>` sharing that `name` and sets its `open` attribute to `false`, closing it automatically without requiring any JavaScript event listeners.
</details>

<details>
<summary><strong>3. Why is find-in-page (<kbd>Ctrl+F</kbd>) an essential accessibility advantage of native `<details>`?</strong></summary>

**Answer:**
When accordions are implemented using custom JavaScript `<div>` toggles with `display: none`, the content is completely inaccessible to browser find-in-page searches. 

Native `<details>` elements implement standard "hidden until found" semantics. When a user presses <kbd>Ctrl+F</kbd> and types a search phrase matching text inside a collapsed `<details>`, the browser automatically expands the element and smoothly scrolls the user to the matching keyword.
</details>

<details>
<summary><strong>4. Why is `min-height: 0` mandatory for the child of a `grid-template-rows: 0fr` container?</strong></summary>

**Answer:**
In CSS Grid, grid items have a default minimum sizing constraint of `min-height: auto` (which computes to the element's minimum content height). 

Even if the parent grid track is set to `0fr`, the child's `min-height: auto` prevents the track from shrinking below the intrinsic height of the content. Explicitly setting `min-height: 0` overrides this default, allowing the row track to collapse completely to `0px`.
</details>

<details>
<summary><strong>5. How do you remove the default disclosure triangle in `<details>` across all modern browsers?</strong></summary>

**Answer:**
Removing the marker requires targeting both the standard CSS `list-style` property and WebKit's legacy pseudo-element:
```css
summary {
  list-style: none; /* Modern standard (Firefox, Chrome 120+, Safari 17+) */
}
summary::-webkit-details-marker {
  display: none;    /* Legacy WebKit/Blink engine reset */
}
```
</details>

<details>
<summary><strong>6. What ARIA attributes are required when building an accessible toggle switch?</strong></summary>

**Answer:**
1. `role="switch"`: Signals to assistive technology that this is a binary toggle switch rather than a standard checkbox.
2. `aria-checked="true|false"`: Indicates whether the switch is currently on or off.
3. Accessible name: Provided via a connected `<label>` or `aria-label`.
</details>

<details>
<summary><strong>7. How does `interpolate-size: allow-keywords` change CSS height transitions in modern browsers?</strong></summary>

**Answer:**
`interpolate-size: allow-keywords` is a new CSS primitive that instructs the browser engine to allow animations directly between numerical values and intrinsic sizing keywords (`auto`, `min-content`, `max-content`). 

Once declared on `:root` (`interpolate-size: allow-keywords;`), developers can directly transition `height: 0` to `height: auto` without requiring the CSS Grid `0fr` to `1fr` workaround.
</details>

<details>
<summary><strong>8. How should high contrast mode (`forced-colors: active`) be styled on an active tab indicator?</strong></summary>

**Answer:**
Under `forced-colors: active`, background colors are removed. If an active tab is indicated solely by a subtle background color change (`background: #ffffff`), the active state becomes invisible.

To ensure clarity:
```css
@media (forced-colors: active) {
  .pill-btn.active {
    border: 2px solid Highlight;
  }
}
```
</details>

---

## 11. Practice Exercises

### Exercise 1: Build a Pure CSS Grid-Animated Accordion
**Task**: Build a `<details>` component where the content height expands and collapses smoothly over 200ms using CSS Grid.
<details>
<summary>View Verified Solution</summary>

```css
details {
  border: 1px solid #cbd5e1;
  border-radius: 8px;
  max-width: 500px;
}

summary {
  list-style: none;
  padding: 1rem;
  font-weight: 600;
  cursor: pointer;
}

summary::-webkit-details-marker {
  display: none;
}

.grid-expand {
  display: grid;
  grid-template-rows: 0fr;
  transition: grid-template-rows 200ms ease;
}

details[open] .grid-expand {
  grid-template-rows: 1fr;
}

.grid-expand > div {
  overflow: hidden;
  min-height: 0;
  padding: 0 1rem 1rem 1rem;
}
```
</details>

---

### Exercise 2: Rotating Chevron on `<details>` Toggle
**Task**: Create an arrow indicator inside `<summary>` that smoothly rotates 180 degrees when the `<details>` opens.
<details>
<summary>View Verified Solution</summary>

```css
summary {
  display: flex;
  justify-content: space-between;
  align-items: center;
  cursor: pointer;
}

.arrow {
  display: inline-block;
  transition: transform 200ms ease;
}

details[open] .arrow {
  transform: rotate(180deg);
}
```
</details>

---

### Exercise 3: Accessible Pill Toggle Switch
**Task**: Build a rounded toggle switch with a sliding thumb, ensuring keyboard tab focus renders a clear outline ring.
<details>
<summary>View Verified Solution</summary>

```css
.switch {
  position: relative;
  display: inline-block;
  width: 44px;
  height: 24px;
}

.switch input {
  opacity: 0;
  width: 0;
  height: 0;
}

.slider {
  position: absolute;
  inset: 0;
  background-color: #cbd5e1;
  border-radius: 9999px;
  transition: background-color 200ms ease;
  cursor: pointer;
}

.slider::before {
  content: "";
  position: absolute;
  top: 2px;
  left: 2px;
  width: 20px;
  height: 20px;
  background-color: #ffffff;
  border-radius: 50%;
  transition: transform 200ms ease;
}

.switch input:checked + .slider {
  background-color: #2563eb;
}

.switch input:checked + .slider::before {
  transform: translateX(20px);
}

.switch input:focus-visible + .slider {
  outline: 2px solid #2563eb;
  outline-offset: 2px;
}
```
</details>

---

### Exercise 4: Exclusive Native Accordion Setup
**Task**: Write the semantic HTML for three FAQ items where expanding any single question automatically collapses the other two.
<details>
<summary>View Verified Solution</summary>

```html
<details name="my-exclusive-faq" open>
  <summary>Question 1</summary>
  <p>Answer 1...</p>
</details>

<details name="my-exclusive-faq">
  <summary>Question 2</summary>
  <p>Answer 2...</p>
</details>

<details name="my-exclusive-faq">
  <summary>Question 3</summary>
  <p>Answer 3...</p>
</details>
```
</details>

---

### Exercise 5: Pure CSS Segmented View Controller
**Task**: Create a 3-option segmented control (Day / Week / Month) that changes active highlight using radio inputs.
<details>
<summary>View Verified Solution</summary>

```html
<div class="segmented-control">
  <input type="radio" name="timeframe" id="tf-day" checked>
  <label for="tf-day">Day</label>

  <input type="radio" name="timeframe" id="tf-week">
  <label for="tf-week">Week</label>

  <input type="radio" name="timeframe" id="tf-month">
  <label for="tf-month">Month</label>
</div>
```

```css
.segmented-control {
  display: inline-flex;
  background: #e2e8f0;
  padding: 0.25rem;
  border-radius: 8px;
}

.segmented-control input {
  position: absolute;
  opacity: 0;
}

.segmented-control label {
  padding: 0.4rem 0.875rem;
  font-size: 0.875rem;
  font-weight: 500;
  color: #475569;
  cursor: pointer;
  border-radius: 6px;
  transition: all 120ms ease;
}

.segmented-control input:checked + label {
  background: #ffffff;
  color: #0f172a;
  box-shadow: 0 1px 2px rgba(0, 0, 0, 0.1);
}
```
</details>

---

### Exercise 6: Subtle Hover Fade on Collapsed Summaries
**Task**: Add a smooth background highlight transition on `<summary>` hover and active press.
<details>
<summary>View Verified Solution</summary>

```css
summary {
  background-color: transparent;
  transition: background-color 150ms ease, color 150ms ease;
}

summary:hover {
  background-color: #f8fafc;
  color: #2563eb;
}

summary:active {
  background-color: #f1f5f9;
}
```
</details>

---

## 12. Summary / Cheat Sheet

```
+------------------------------------------------------------------------------------+
|                      TABS, ACCORDIONS & TOGGLES CHEAT SHEET                        |
+------------------------------------------------------------------------------------+
| 1. Native Exclusive Accordion:                                                     |
|    <details name="faq">...</details>                                               |
+------------------------------------------------------------------------------------+
| 2. Grid Height Transition:                                                         |
|    .body { display: grid; grid-template-rows: 0fr; transition: 250ms ease; }       |
|    details[open] .body { grid-template-rows: 1fr; }                                |
|    .inner { overflow: hidden; min-height: 0; }                                     |
+------------------------------------------------------------------------------------+
| 3. Summary Marker Reset:                                                           |
|    summary { list-style: none; } summary::-webkit-details-marker { display: none; } |
+------------------------------------------------------------------------------------+
| 4. Accessible Switch Semantics:                                                    |
|    <input type="checkbox" role="switch" aria-checked="false">                      |
+------------------------------------------------------------------------------------+
| 5. Chevron Rotation:                                                               |
|    details[open] .chevron { transform: rotate(180deg); }                           |
+------------------------------------------------------------------------------------+
| 6. Radio Tab Activation:                                                           |
|    #tab-1:checked ~ .panels .panel-1 { display: block; }                           |
+------------------------------------------------------------------------------------+
```

---

## 13. Further Reading
- [HTML Standard: The `<details>` and `<summary>` Elements](https://html.spec.whatwg.org/multipage/interactive-elements.html#the-details-element)
- [MDN Web Docs: Exclusive Accordions with `name`](https://developer.mozilla.org/en-US/docs/Web/HTML/Element/details#name)
- [web.dev: How to Animate to `height: auto` with CSS Grid](https://web.dev/articles/css-wrapped-2023#grid-template-rows)
- [W3C WAI-ARIA Authoring Practices: Accordion Pattern](https://www.w3.org/WAI/ARIA/apg/patterns/accordion/)
- [W3C WAI-ARIA Authoring Practices: Tabs Pattern](https://www.w3.org/WAI/ARIA/apg/patterns/tabs/)

---

## Navigation
- **Previous**: [Responsive Tables Recipes](file:///e:/Learning/engineering-notes/01-languages/css/13-layout-and-component-recipes/10-responsive-tables-recipes.md)
- **Next**: [Buttons, Badges, and Alerts](file:///e:/Learning/engineering-notes/01-languages/css/13-layout-and-component-recipes/12-buttons-badges-and-alerts.md)
