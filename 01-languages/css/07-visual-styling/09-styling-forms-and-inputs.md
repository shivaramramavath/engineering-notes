# Form Controls & Input Styling

| Metadata | Details |
| :--- | :--- |
| **Topic** | Accessible Form Controls, `accent-color`, `appearance: none`, Floating Labels (`:placeholder-shown`), Modern Validation (`:user-invalid`), `::file-selector-button` |
| **Spec Level** | CSS Basic User Interface Module Level 4 / HTML Living Standard (Forms) |
| **Baseline Status** | Widely Available (`accent-color` Baseline 2022; `::file-selector-button` Baseline 2022; `:user-valid` / `:user-invalid` Baseline 2023) |
| **Browsers Tested** | Chrome 120+, Firefox 122+, Safari 17.2+, Edge 120+ |
| **Tested With** | NVDA, VoiceOver, Playwright, Chrome DevTools A11y Tree |
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

HTML form controls (`<input>`, `<select>`, `<textarea>`, `<button>`) have historically been the most frustrating elements to style in CSS. Operating systems (Windows, macOS, iOS, Android) inject proprietary, platform-specific native controls into the web page—often rendering mismatched fonts, rigid heights, and unchangeable blue/gray outlines.

To bypass these limitations, legacy frontend code frequently replaced native form elements with non-semantic `<div>` and `<span>` tags, creating catastrophic accessibility bugs (loss of keyboard focus, screen reader announcements, and mobile autofill).

Modern CSS fundamentally solves form styling:
- **`accent-color` (Baseline 2022)**: Colors native checkboxes, radios, range sliders, and progress bars with a single CSS declaration while preserving native OS accessibility.
- **`appearance: none`**: Strips default OS bevels and chrome, giving developers a blank canvas.
- **`:user-valid` & `:user-invalid` (Baseline 2023)**: Eliminates the dreaded "form screaming red on initial page load" bug by firing validation styles *only after* the user has actively interacted with and blurred the input.
- **Pure CSS Floating Labels**: Powered cleanly by `:placeholder-shown` and sibling selectors without JavaScript listeners.
- **Standardized File Uploads**: `::file-selector-button` allows native file picker buttons to be styled directly in CSS.

---

## 2. Core Concepts

### `accent-color` (The 1-Line Solution)
Rather than writing 60 lines of complex SVG checkbox overrides, `accent-color` recolors native controls instantly:
```css
:root {
  accent-color: #2563eb;
}
```
Applies automatically to:
1. `<input type="checkbox">`
2. `<input type="radio">`
3. `<input type="range">`
4. `<progress>`

The browser engine automatically calculates a contrasting checkmark or thumb indicator (white or black) according to WCAG contrast algorithms.

### `appearance: none`
Strips the native platform UI widget rendering:
```css
input[type="search"], select {
  appearance: none;
  -webkit-appearance: none;
}
```
Allows complete customization of borders, padding, background colors, and custom SVG dropdown arrows.

### Modern Form Validation: `:invalid` vs `:user-invalid`
- **`:invalid` (Problematic)**: Evaluates immediately on page load. If an `<input type="email" required>` is empty on first render, it is immediately `:invalid`, highlighting forms in harsh red before the user even types a character.
- **`:user-invalid` (Baseline 2023 - Modern Standard)**: Only evaluates after the user has interacted with the control (e.g., typed text and moved focus away with an error). Provides empathetic, professional UX.

### The `:placeholder-shown` Floating Label
By giving an `<input>` an empty space placeholder (`placeholder=" "`), CSS can detect whether the input currently contains text using `:placeholder-shown`:
- When empty: `:placeholder-shown` matches; the `<label>` sits inside the input field like a placeholder.
- When focused or filled: `:not(:placeholder-shown)` or `:focus`; the label shrinks and floats to the top border.

---

## 3. How It Works Internally

```
Validation UX State Machine: :invalid vs :user-invalid

User loads page with empty required email field:
1. :invalid        ===> FIRES IMMEDIATELY! (Shows RED border to new user!)
2. :user-invalid   ===> IDLE (Clean neutral border; waits for user input)

User types "hello" (invalid email syntax):
   User is still typing ===> IDLE (Does not interrupt with errors)

User tabs away (blur event):
   :user-invalid   ===> FIRES NOW! (Helpfully indicates invalid email syntax)
```

### The Label Floating Transform
```
[Empty State (placeholder-shown)]
+------------------------------------+
| Email address                      |  <-- label sits inside input
+------------------------------------+

[Focused / Filled State]
  Email address                         <-- label scales down (scale: 0.85)
+------------------------------------+      and translates upward
| alex@example.com                   |
+------------------------------------+
```

---

## 4. Syntax & Usage

### Baseline & Feature Support
- **`accent-color`**: Baseline 2022 (Chrome 93+, Safari 15.4+, Firefox 92+, Edge 93+).
- **`::file-selector-button`**: Baseline 2022 (Chrome 89+, Safari 15.4+, Firefox 82+).
- **`:user-valid` / `:user-invalid`**: Baseline 2023 (Chrome 119+, Safari 16.5+, Firefox 88+).

```css
/* Global Brand Theme */
:root {
  accent-color: #2563eb;
}

/* User-Driven Validation Styling */
input:user-invalid {
  border-color: #ef4444;
  outline-color: #ef4444;
}

input:user-valid {
  border-color: #10b981;
}

/* Custom File Upload Button */
input[type="file"]::file-selector-button {
  padding: 0.5rem 1rem;
  border-radius: 6px;
  background-color: #0f172a;
  color: #ffffff;
  border: none;
  cursor: pointer;
  margin-right: 1rem;
  transition: background-color 0.2s ease;
}

input[type="file"]::file-selector-button:hover {
  background-color: #1e293b;
}
```

---

## 5. Practical Examples

### Example 1: Accessible Custom Toggle Switch (Pure Semantic Checkbox)
```html
<label class="toggle-switch">
  <input type="checkbox" role="switch" class="toggle-input">
  <span class="toggle-track">
    <span class="toggle-thumb"></span>
  </span>
  <span class="toggle-label">Enable Cloud Telemetry</span>
</label>
```

```css
.toggle-switch {
  display: inline-flex;
  align-items: center;
  gap: 0.75rem;
  cursor: pointer;
  user-select: none;
  font-family: system-ui, sans-serif;
  font-size: 0.9375rem;
}

/* Hide native checkbox visually while keeping it fully reachable by keyboard/SR */
.toggle-input {
  position: absolute;
  width: 1px;
  height: 1px;
  padding: 0;
  margin: -1px;
  overflow: hidden;
  clip: rect(0, 0, 0, 0);
  border: 0;
}

.toggle-track {
  width: 48px;
  height: 26px;
  background-color: #cbd5e1;
  border-radius: 9999px;
  position: relative;
  transition: background-color 0.25s ease;
  border: 1px solid transparent; /* WHCM safety */
}

.toggle-thumb {
  position: absolute;
  top: 2px;
  left: 2px;
  width: 20px;
  height: 20px;
  background-color: #ffffff;
  border-radius: 50%;
  box-shadow: 0 1px 3px rgba(0, 0, 0, 0.2);
  transition: translate 0.25s cubic-bezier(0.16, 1, 0.3, 1);
}

/* Active Checked State */
.toggle-input:checked + .toggle-track {
  background-color: #2563eb;
}

.toggle-input:checked + .toggle-track .toggle-thumb {
  translate: 22px 0;
}

/* Keyboard Focus Ring */
.toggle-input:focus-visible + .toggle-track {
  outline: 2px solid #2563eb;
  outline-offset: 3px;
}
```

### Example 2: Pure CSS Floating Label Input
```html
<div class="floating-input-group">
  <input type="email" id="user-email" class="floating-input" placeholder=" " required>
  <label for="user-email" class="floating-label">Email Address</label>
</div>
```

```css
.floating-input-group {
  position: relative;
  max-width: 360px;
  margin-top: 1rem;
}

.floating-input {
  width: 100%;
  padding: 1.125rem 1rem 0.625rem;
  font-size: 1rem;
  border: 1px solid #cbd5e1;
  border-radius: 8px;
  background-color: #ffffff;
  color: #0f172a;
  outline: none;
  transition: border-color 0.2s ease, box-shadow 0.2s ease;
}

.floating-label {
  position: absolute;
  top: 1rem;
  left: 1rem;
  color: #64748b;
  font-size: 1rem;
  pointer-events: none;
  transform-origin: left top;
  transition: transform 0.2s ease, color 0.2s ease;
}

/* When focused OR when text has been typed (placeholder is NOT shown) */
.floating-input:focus ~ .floating-label,
.floating-input:not(:placeholder-shown) ~ .floating-label {
  transform: translateY(-0.6rem) scale(0.75);
  color: #2563eb;
}

.floating-input:focus-visible {
  border-color: #2563eb;
  box-shadow: 0 0 0 3px rgba(37, 99, 235, 0.15);
}

.floating-input:user-invalid {
  border-color: #ef4444;
}

.floating-input:user-invalid ~ .floating-label {
  color: #ef4444;
}
```

### Example 3: Styled Custom `<select>` Dropdown Arrow
```html
<div class="custom-select-wrapper">
  <select class="styled-select" aria-label="Select Cloud Region">
    <option value="us-east-1">US East (N. Virginia)</option>
    <option value="eu-central-1">Europe (Frankfurt)</option>
    <option value="ap-southeast-1">Asia Pacific (Singapore)</option>
  </select>
</div>
```

```css
.custom-select-wrapper {
  position: relative;
  max-width: 320px;
}

.styled-select {
  width: 100%;
  padding: 0.625rem 2.5rem 0.625rem 1rem;
  font-size: 0.9375rem;
  font-family: system-ui, sans-serif;
  color: #1e293b;
  background-color: #ffffff;
  border: 1px solid #cbd5e1;
  border-radius: 6px;
  cursor: pointer;
  
  /* Strips native browser arrow */
  appearance: none;
  -webkit-appearance: none;
}

/* Custom SVG Arrow via Pseudo-Element on Wrapper */
.custom-select-wrapper::after {
  content: "";
  position: absolute;
  right: 1rem;
  top: 50%;
  translate: 0 -50%;
  width: 0.5rem;
  height: 0.5rem;
  border-right: 2px solid #64748b;
  border-bottom: 2px solid #64748b;
  transform: rotate(45deg);
  pointer-events: none; /* Allows click to pass into the select */
}

.styled-select:focus-visible {
  outline: 2px solid #2563eb;
  outline-offset: 2px;
}
```

### Example 4: Styled Custom Range Slider
```css
/* Cross-browser range slider styling */
input[type="range"] {
  width: 100%;
  height: 6px;
  background: #e2e8f0;
  border-radius: 9999px;
  outline: none;
  appearance: none;
  cursor: pointer;
}

/* Chromium Slider Thumb */
input[type="range"]::-webkit-slider-thumb {
  appearance: none;
  width: 18px;
  height: 18px;
  border-radius: 50%;
  background: #2563eb;
  border: 2px solid #ffffff;
  box-shadow: 0 1px 3px rgba(0, 0, 0, 0.2);
  cursor: pointer;
  transition: transform 0.15s ease;
}

input[type="range"]::-webkit-slider-thumb:hover {
  transform: scale(1.15);
}

/* Firefox Slider Thumb */
input[type="range"]::-moz-range-thumb {
  width: 18px;
  height: 18px;
  border-radius: 50%;
  background: #2563eb;
  border: 2px solid #ffffff;
  box-shadow: 0 1px 3px rgba(0, 0, 0, 0.2);
  cursor: pointer;
}

input[type="range"]:focus-visible {
  outline: 2px solid #2563eb;
  outline-offset: 4px;
}
```

### Example 5: Disabled and Read-Only Form States
```css
/* Explicit visual semantics for disabled inputs */
input:disabled,
select:disabled,
button:disabled {
  background-color: #f1f5f9;
  color: #94a3b8;
  border-color: #e2e8f0;
  cursor: not-allowed;
  opacity: 0.8;
}

input:read-only {
  background-color: #f8fafc;
  border-style: dashed;
}
```

---

## 6. Best Practices

1. **Always Associate `<label for="id">` with Inputs**: Never leave form controls unlabelled. Screen readers depend on explicit label relationships.
2. **Use `accent-color` First**: Before writing complex custom checkbox/radio markup, evaluate whether setting `accent-color: #brand-hex;` satisfies design requirements with zero custom code.
3. **Use `:user-invalid` Instead of `:invalid`**: Prevent validation styling from alerting the user before they have even touched the input field.
4. **Never Use `display: none` on Checkboxes/Radios**: If creating custom toggle switches, hide the native input visually using a screen-reader-only utility (`width: 1px; height: 1px; clip: rect(0 0 0 0);`). Setting `display: none` removes the control from keyboard focus!
5. **Always Set `pointer-events: none` on Custom Dropdown Arrows**: When overlaying a custom SVG chevron over a `<select>`, set `pointer-events: none;` on the chevron so clicks pass into the `<select>` to open the menu.
6. **Ensure Touch Targets Are at Least 44x44px**: Checkbox and radio hit zones should include their `<label>` text to allow easy tapping on mobile phones.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Correction |
| :--- | :--- | :--- |
| **Hiding `<input type="checkbox">` with `display: none`** | Removes the element from the accessibility tree; keyboard users cannot tab or toggle it. | Use `.sr-only` CSS clipping so the element remains focusable. |
| **Using `:invalid` on initial page load** | Displays red error borders on untouched empty required fields, frustrating users. | Use `:user-invalid` (Baseline 2023). |
| **Custom `<select>` arrow blocking clicks** | Clicking the custom arrow image fails to open the dropdown menu. | Add `pointer-events: none;` to the arrow pseudo-element. |
| **Stripping outlines with `outline: none`** | Keyboard users have no indication which field currently has focus. | Use `:focus-visible { outline: 2px solid ... }`. |
| **Missing `<label>` tags** | Violates WCAG 1.3.1; screen reader announces "Edit text, blank" without context. | Wrap in `<label>` or use `for="..."` ID mapping. |
| **Checkbox without `role="switch"` when used as toggle** | Screen reader announces "Checked" instead of "On / Off" state. | Add `role="switch"` to toggle inputs. |

---

## 8. Performance, Accessibility & Browser Support

### Accessibility Compliance (WCAG 2.2 AA)
- **Criterion 3.3.2 (Labels or Instructions)**: Labels or instructions must be provided when content requires user input.
- **Criterion 2.4.7 (Focus Visible)**: Form controls must have high-visibility focus states.
- **Criterion 2.5.5 (Target Size)**: Interactive touch target size must be at least **44 by 44 CSS pixels**.

---

## 9. Comparison / When to Use What

| Input Pattern | Accessibility Effort | Visual Polish | Browser Support | Best Used For |
| :--- | :--- | :--- | :--- | :--- |
| **Native with `accent-color`** | **Zero (Built-in)** | High (Native feel) | Baseline 2022 | Enterprise tools, settings pages |
| **SR-Only Input + Custom Track** | Moderate (Requires ARIA) | **Maximum (Custom toggles)** | 100% | Marketing toggles, branded switches |
| **Floating Labels** | Low | High (Compact) | 100% | Login screens, mobile signups |
| **`::file-selector-button`** | **Zero (Built-in)** | High | Baseline 2022 | Document/avatar file uploads |

---

## 10. Interview Questions

<details>
<summary>1. What is the difference between the <code>:invalid</code> and <code>:user-invalid</code> pseudo-classes?</summary>

- `:invalid` represents any form control whose value does not satisfy its constraints (such as `required` or `type="email"`). It matches **immediately upon initial page load**. If a required field is empty, `:invalid` is immediately active, causing forms to display premature red error borders before the user has touched the field.
- `:user-invalid` (Baseline 2023) only matches if the control is invalid **and the user has actively interacted with it** (such as typing text and blurring focus). It prevents premature error validation and provides intuitive, user-friendly feedback.
</details>

<details>
<summary>2. What is the purpose of the CSS <code>accent-color</code> property?</summary>

`accent-color` provides a standardized, one-line CSS property to apply custom brand colors to user-interface form controls: `<input type="checkbox">`, `<input type="radio">`, `<input type="range">`, and `<progress>`. 

Rather than rewriting complex custom DOM markup to replace native checkboxes, `accent-color` colors the native control while allowing the browser layout engine to automatically compute contrasting checkmark symbols and retain all native accessibility semantics and OS keyboard behaviors.
</details>

<details>
<summary>3. Why is hiding a native checkbox with <code>display: none</code> an accessibility violation?</summary>

When an element has `display: none;`, the browser completely removes it from the accessibility tree. As a consequence:
1. Keyboard users can never tab into or reach the input.
2. Screen readers will not announce the presence, name, or checked state of the input.
3. Mobile browsers cannot autofill the input.

To create custom toggle switches, developers must hide the input visually using off-screen or zero-dimension clipping (`width: 1px; height: 1px; clip: rect(0 0 0 0);`) so it remains accessible to keyboard and screen reader users.
</details>

<details>
<summary>4. How does the pure CSS floating label pattern work using <code>:placeholder-shown</code>?</summary>

The pattern requires placing the `<label>` *after* the `<input>` in the DOM and setting an empty space as the placeholder (`placeholder=" "`).
1. When the input is empty and unfocused, `:placeholder-shown` matches. CSS positions the label directly inside the input like standard placeholder text.
2. When the user focuses the input (`:focus`) or types content (`:not(:placeholder-shown)`), the sibling selector (`input:focus ~ label`) triggers a CSS transform (`translateY(-12px) scale(0.75)`), smoothly floating the label above the field.
</details>

<details>
<summary>5. What does the <code>::file-selector-button</code> pseudo-element target?</summary>

`::file-selector-button` targets the native button inside an `<input type="file">`. 
Historically, this button was notoriously difficult to style and required browser-specific prefixes (`::-webkit-file-upload-button`). The standardized `::file-selector-button` allows styling background, padding, borders, typography, and hover states directly.
</details>

<details>
<summary>6. Why is <code>pointer-events: none</code> mandatory on custom dropdown arrow overlays?</summary>

When styling `<select>` with `appearance: none`, developers typically place a custom SVG chevron icon over the right side of the select element. 
If `pointer-events: none` is omitted, clicking directly on the chevron graphic will target the chevron rather than the `<select>`, failing to trigger the native OS dropdown selection menu.
</details>

<details>
<summary>7. What is the role of <code>role="switch"</code> on an HTML checkbox?</summary>

Applying `role="switch"` to `<input type="checkbox">` informs assistive technologies that the control represents a dynamic on/off toggle switch rather than a traditional form checkbox. 

Screen readers will announce the element as "Switch, On" or "Switch, Off" instead of "Checkbox, Checked".
</details>

<details>
<summary>8. How do you style disabled and read-only inputs distinctly?</summary>

- `input:disabled`: For controls that cannot be focused or submitted with form data. Usually styled with low opacity (`opacity: 0.6; cursor: not-allowed;`).
- `input:read-only`: For controls that can be selected, focused, and submitted, but cannot be modified by the user. Usually styled with dashed borders or a neutral background.
</details>

---

## 11. Practice Exercises

### Exercise 1: Single-Line Global Form Theming with `accent-color`
**Goal**: Theme all form checkboxes, radios, and range inputs in a design system to emerald green.

<details>
<summary>View Solution</summary>

```css
:root {
  accent-color: #059669; /* Emerald 600 */
}
```
</details>

### Exercise 2: Build an Accessible Pure CSS Toggle Switch
**Goal**: Construct an on/off toggle switch with smooth thumb animation, keyboard focus indicator, and screen reader safety.

<details>
<summary>View Solution</summary>

```html
<label class="accessible-switch">
  <input type="checkbox" role="switch" class="sr-input">
  <span class="track">
    <span class="thumb"></span>
  </span>
  <span class="label-text">Airplane Mode</span>
</label>
```

```css
.accessible-switch {
  display: inline-flex;
  align-items: center;
  gap: 0.75rem;
  cursor: pointer;
}

.sr-input {
  position: absolute;
  width: 1px;
  height: 1px;
  padding: 0;
  margin: -1px;
  overflow: hidden;
  clip: rect(0, 0, 0, 0);
  border: 0;
}

.track {
  width: 44px;
  height: 24px;
  border-radius: 9999px;
  background-color: #cbd5e1;
  position: relative;
  transition: background-color 0.2s ease;
}

.thumb {
  position: absolute;
  top: 2px;
  left: 2px;
  width: 20px;
  height: 20px;
  border-radius: 50%;
  background-color: #ffffff;
  box-shadow: 0 1px 3px rgba(0, 0, 0, 0.2);
  transition: translate 0.2s ease;
}

.sr-input:checked + .track {
  background-color: #10b981;
}

.sr-input:checked + .track .thumb {
  translate: 20px 0;
}

.sr-input:focus-visible + .track {
  outline: 2px solid #10b981;
  outline-offset: 3px;
}
```
</details>

### Exercise 3: Implement Empathetic Validation with `:user-invalid`
**Goal**: Style a required text input so it displays a red border only after the user leaves an empty field.

<details>
<summary>View Solution</summary>

```css
.form-input {
  width: 100%;
  padding: 0.625rem 0.875rem;
  border: 1px solid #cbd5e1;
  border-radius: 6px;
  outline: none;
}

.form-input:user-invalid {
  border-color: #ef4444;
  box-shadow: 0 0 0 3px rgba(239, 68, 68, 0.15);
}

.form-input:user-valid {
  border-color: #10b981;
}

.form-input:focus-visible {
  border-color: #2563eb;
  box-shadow: 0 0 0 3px rgba(37, 99, 235, 0.15);
}
```
</details>

### Exercise 4: Custom Styled File Upload Button
**Goal**: Style an `<input type="file">` button with an inverted dark theme and hover animation.

<details>
<summary>View Solution</summary>

```css
.file-picker-input {
  font-family: system-ui, sans-serif;
  font-size: 0.875rem;
  color: #64748b;
}

.file-picker-input::file-selector-button {
  padding: 0.625rem 1.25rem;
  font-size: 0.875rem;
  font-weight: 600;
  border-radius: 6px;
  background-color: #0f172a;
  color: #ffffff;
  border: none;
  cursor: pointer;
  margin-right: 1rem;
  transition: background-color 0.2s ease;
}

.file-picker-input::file-selector-button:hover {
  background-color: #2563eb;
}
```
</details>

### Exercise 5: Custom Chevron `<select>` Dropdown
**Goal**: Build a cross-browser `<select>` input with a custom CSS chevron arrow that passes click events.

<details>
<summary>View Solution</summary>

```html
<div class="select-container">
  <select class="clean-select">
    <option>Production Cluster</option>
    <option>Staging Cluster</option>
  </select>
</div>
```

```css
.select-container {
  position: relative;
  display: inline-block;
  width: 260px;
}

.clean-select {
  width: 100%;
  padding: 0.625rem 2.5rem 0.625rem 1rem;
  appearance: none;
  -webkit-appearance: none;
  border: 1px solid #cbd5e1;
  border-radius: 6px;
  background-color: #ffffff;
  font-size: 0.875rem;
  cursor: pointer;
}

.select-container::after {
  content: "";
  position: absolute;
  right: 1rem;
  top: 50%;
  translate: 0 -60%;
  width: 6px;
  height: 6px;
  border-right: 2px solid #64748b;
  border-bottom: 2px solid #64748b;
  transform: rotate(45deg);
  pointer-events: none; /* Required for click through */
}

.clean-select:focus-visible {
  outline: 2px solid #2563eb;
  outline-offset: 2px;
}
```
</details>

### Exercise 6: Floating Label Signup Input
**Goal**: Create a floating label email field that shifts to small uppercase blue text on focus.

<details>
<summary>View Solution</summary>

```html
<div class="floating-group">
  <input type="text" id="username" class="float-input" placeholder=" " required>
  <label for="username" class="float-label">Account Name</label>
</div>
```

```css
.floating-group {
  position: relative;
  margin-top: 1rem;
}

.float-input {
  width: 100%;
  padding: 1rem 0.75rem 0.5rem;
  border: 1px solid #cbd5e1;
  border-radius: 6px;
  outline: none;
}

.float-input:focus-visible {
  outline: 2px solid #2563eb;
  outline-offset: 2px;
}

.float-label {
  position: absolute;
  top: 0.875rem;
  left: 0.75rem;
  color: #64748b;
  pointer-events: none;
  transform-origin: left top;
  transition: transform 0.2s ease, color 0.2s ease;
}

.float-input:focus ~ .float-label,
.float-input:not(:placeholder-shown) ~ .float-label {
  transform: translateY(-0.5rem) scale(0.75);
  color: #2563eb;
}
```
</details>

---

## 12. Summary / Cheat Sheet

| Feature | Syntax / Selector | Primary Purpose |
| :--- | :--- | :--- |
| `accent-color` | `accent-color: #2563eb;` | 1-line brand coloring for native inputs |
| `appearance` | `appearance: none;` | Strips native OS bevels and chrome |
| `:user-invalid` | `input:user-invalid` | Error styles only *after* user interaction |
| `:placeholder-shown` | `input:not(:placeholder-shown)` | Detects filled state for floating labels |
| `::file-selector-button`| `input[type="file"]::file-selector-button` | Styles native file picker buttons |
| `role="switch"` | `<input type="checkbox" role="switch">` | Accessible ARIA toggle switch semantics |

---

## 13. Further Reading
- [W3C CSS Basic User Interface Module Level 4](https://www.w3.org/TR/css-ui-4/)
- [MDN Web Docs: accent-color](https://developer.mozilla.org/en-US/docs/Web/CSS/accent-color)
- [MDN Web Docs: :user-invalid](https://developer.mozilla.org/en-US/docs/Web/CSS/:user-invalid)
- [web.dev: Modern CSS form styling](https://web.dev/learn/forms/)
- [Sara Soueidan: Inclusively Hiding Elements](https://www.sarasoueidan.com/blog/inclusively-hiding-and-styling-checkboxes-and-radio-buttons/)

---

[Previous: 08-lists-and-counters.md](08-lists-and-counters.md) | [Module README](README.md) | [Next: 10-scrollbars-and-scroll-behaviour.md](10-scrollbars-and-scroll-behaviour.md)
