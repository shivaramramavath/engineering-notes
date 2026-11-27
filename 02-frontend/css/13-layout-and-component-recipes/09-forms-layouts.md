# Form Layouts and Input Styling in Modern CSS

| Topic | Module | Level | Baseline Status | Browsers Tested | Date | Prerequisites |
|---|---|---|---|---|---|---|
| Form Layouts & Input Styling | 13: Layout & Component Recipes | Intermediate / Advanced | Widely Available | Chrome 120+, Firefox 120+, Safari 17.4+ | 2026-03-31 | Grid, Flexbox, Pseudo-classes, Transitions |

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
9. [Comparison: Form Styling Strategies](#9-comparison-form-styling-strategies)
10. [Interview Questions](#10-interview-questions)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary / Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

Forms are the transactional engine of the web. Every signup, checkout, checkout funnel, configuration dashboard, and enterprise data entry workflow relies entirely on form controls. 

Historically, styling web forms was notoriously difficult:
- **Premature Validation Shaming**: Using `:invalid` caused newly opened forms to immediately flash red error states on blank required fields before the user even typed their first character.
- **Inaccessible Custom Controls**: Replacing browser native checkboxes and radios often involved `display: none` on the `<input>`, which stripped keyboard navigation, tab indices, and screen reader announcements.
- **Broken Label Floating**: JavaScript listeners tracked focus and blur events to calculate coordinate offsets for floating labels, causing jank during autofill.
- **Form Row Alignment Wars**: Aligning inputs, labels, prefix icons, and submit buttons across disparate device widths involved brittle floats and inline-block margin hacks.

Modern CSS transforms form development through standards-based features:
- **`:user-valid` and `:user-invalid`**: Applies validation styling only *after* the user interacts with the control and commits focus out (blur), completely eliminating premature validation errors.
- **`accent-color` and `appearance: none`**: Styles native checkboxes, radios, ranges, and progress elements cleanly while preserving 100% of underlying browser accessibility semantics.
- **CSS Grid Form Matrices**: Organizes multi-column form layouts with automatic responsive wrapping and explicit label-input association.
- **`:placeholder-shown` Floating Labels**: Enables pure CSS animated floating labels that correctly detect prefilled text and browser autofill.

---

## 2. Core Concepts

### 1. The Interaction-Aware Validation Pseudo-Classes
CSS now distinguishes between an element being invalid in the abstract vs invalid in user context:
- `:invalid`: Evaluates true immediately on page load if a required field is empty.
- `:user-invalid`: Evaluates true **only after** the user has interacted with the input (typed and blurred, or attempted form submission).

```
[Page Loads] ────────────────► Field is empty:
                              - :invalid is TRUE (Hidden by default)
                              - :user-invalid is FALSE (No red borders shown!)

[User Types invalid email] ──► User tabs away (blur event):
                              - :user-invalid becomes TRUE -> Displays red error border
```

### 2. The Semantic Label-Input Pairing
Form inputs must always be paired with explicit `<label>` elements via the `for`/`id` attribute or direct DOM nesting. Never rely solely on `placeholder` attributes as labels, which disappear when typing begins and fail WCAG 3.3.2 requirements.

### 3. Accessible Custom Controls without `display: none`
To custom-style checkboxes and toggle switches, the `<input>` element must remain in the DOM to receive focus and announce states. Instead of `display: none`, use `appearance: none` or clip-path visually hidden techniques:
```css
/* Clean modern custom checkbox */
input[type="checkbox"] {
  appearance: none;
  width: 1.25rem;
  height: 1.25rem;
  border: 2px solid #94a3b8;
  border-radius: 4px;
  cursor: pointer;
}

input[type="checkbox"]:checked {
  background-color: #2563eb;
  border-color: #2563eb;
  background-image: url("data:image/svg+xml,...check-icon...");
}
```

---

## 3. How It Works Internally

Modern browser engines process form layout and validation state transitions through coordinated style cascades and accessibility tree updates:

```
[User Input Event]
        │
        ▼
[Check Native Validity Constraints (type="email", required, minlength="8")]
        │
        ├── If user has interacted & blurred -> Match :user-invalid
        │         │
        │         ▼
        │   [Trigger CSS Transition]
        │   - border-color: #ef4444
        │   - box-shadow: 0 0 0 3px rgba(239, 68, 68, 0.2)
        │
        └── Sibling Selector Matches (+ .error-message)
                  │
                  ▼
            [Reveal Accessible Error Node]
            - display: block
            - aria-live / aria-describedby informs Assistive Tech
```

When styling inputs with `accent-color`, the browser engine automatically computes matching checkmarks, radio pips, and focus outlines that maintain WCAG 2.1 contrast ratios against the specified brand accent hue.

---

## 4. Syntax & Usage

### Baseline Browser Support
| Feature | Baseline Status | Chrome / Edge | Firefox | Safari |
|---|---|---|---|---|
| `:user-valid` / `:user-invalid` | Widely Available | 119+ | 88+ | 16.4+ |
| `accent-color` | Widely Available | 93+ | 92+ | 15.4+ |
| `:has()` Form Validation | Widely Available | 105+ | 121+ | 15.4+ |
| `:placeholder-shown` | Widely Available | 47+ | 51+ | 9.0+ |

### Core Boilerplate: Interaction-Aware Input
```css
.form-input {
  width: 100%;
  padding: 0.625rem 0.875rem;
  border: 1px solid #cbd5e1;
  border-radius: 6px;
  font-size: 1rem;
  color: #0f172a;
  background: #ffffff;
  transition: border-color 150ms ease, box-shadow 150ms ease;
}

.form-input:focus-visible {
  outline: none;
  border-color: #2563eb;
  box-shadow: 0 0 0 3px rgba(37, 99, 235, 0.2);
}

/* User-interacted error state */
.form-input:user-invalid {
  border-color: #ef4444;
}

.form-input:user-invalid:focus-visible {
  box-shadow: 0 0 0 3px rgba(239, 68, 68, 0.2);
}

/* User-interacted success state */
.form-input:user-valid {
  border-color: #10b981;
}
```

---

## 5. Practical Examples

### Example 1: Responsive Multi-Column Checkout Form Layout
```html
<form class="checkout-grid">
  <div class="field-group span-2">
    <label for="bill-name">Full Name</label>
    <input type="text" id="bill-name" class="form-input" required autocomplete="name">
  </div>

  <div class="field-group span-2">
    <label for="bill-email">Email Address</label>
    <input type="email" id="bill-email" class="form-input" required autocomplete="email">
  </div>

  <div class="field-group span-full">
    <label for="bill-address">Street Address</label>
    <input type="text" id="bill-address" class="form-input" required autocomplete="address-line1">
  </div>

  <div class="field-group span-2">
    <label for="bill-city">City</label>
    <input type="text" id="bill-city" class="form-input" required autocomplete="address-level2">
  </div>

  <div class="field-group">
    <label for="bill-state">State / Province</label>
    <input type="text" id="bill-state" class="form-input" required autocomplete="address-level1">
  </div>

  <div class="field-group">
    <label for="bill-zip">Postal Code</label>
    <input type="text" id="bill-zip" class="form-input" required autocomplete="postal-code">
  </div>
</form>
```

```css
.checkout-grid {
  display: grid;
  grid-template-columns: repeat(4, 1fr);
  gap: 1.25rem 1rem;
  max-width: 800px;
  background: #ffffff;
  padding: 2rem;
  border-radius: 12px;
  border: 1px solid #e2e8f0;
}

.field-group {
  display: flex;
  flex-direction: column;
  gap: 0.375rem;
}

.field-group label {
  font-size: 0.875rem;
  font-weight: 600;
  color: #334155;
}

.field-group.span-full {
  grid-column: span 4;
}

.field-group.span-2 {
  grid-column: span 2;
}

.form-input {
  padding: 0.625rem 0.75rem;
  border: 1px solid #cbd5e1;
  border-radius: 6px;
  font-size: 1rem;
  font-family: inherit;
}

.form-input:focus-visible {
  outline: none;
  border-color: #2563eb;
  box-shadow: 0 0 0 3px rgba(37, 99, 235, 0.2);
}

@media (max-width: 680px) {
  .checkout-grid {
    grid-template-columns: 1fr;
  }
  .field-group.span-2,
  .field-group.span-full {
    grid-column: span 1;
  }
}
```

---

### Example 2: Pure CSS Animated Floating Labels
```html
<div class="floating-field">
  <input type="text" id="username" class="floating-input" placeholder=" " required>
  <label for="username" class="floating-label">Account Username</label>
</div>
```

```css
.floating-field {
  position: relative;
  margin-top: 1rem;
}

.floating-input {
  width: 100%;
  padding: 1.125rem 0.875rem 0.5rem 0.875rem;
  font-size: 1rem;
  border: 1px solid #cbd5e1;
  border-radius: 6px;
  background: #ffffff;
  outline: none;
  transition: border-color 150ms ease;
}

.floating-input:focus-visible {
  border-color: #2563eb;
  box-shadow: 0 0 0 3px rgba(37, 99, 235, 0.15);
}

.floating-label {
  position: absolute;
  top: 50%;
  left: 0.875rem;
  transform: translateY(-50%);
  color: #64748b;
  font-size: 1rem;
  pointer-events: none;
  transition: transform 150ms ease, font-size 150ms ease, color 150ms ease;
}

/* Float label to top when input is focused OR has value (placeholder is NOT shown) */
.floating-input:focus ~ .floating-label,
.floating-input:not(:placeholder-shown) ~ .floating-label {
  top: 0.5rem;
  transform: translateY(0);
  font-size: 0.75rem;
  font-weight: 600;
  color: #2563eb;
}
```

---

### Example 3: Leading & Trailing Icon Input Adornments
```html
<div class="input-with-adornment">
  <span class="adornment-icon prefix" aria-hidden="true">🔍</span>
  <input type="search" placeholder="Search audit logs..." aria-label="Search logs">
  <button type="button" class="adornment-action suffix" aria-label="Clear query">✕</button>
</div>
```

```css
.input-with-adornment {
  position: relative;
  display: flex;
  align-items: center;
  width: 100%;
  max-width: 400px;
}

.input-with-adornment input {
  width: 100%;
  padding: 0.625rem 2.5rem 0.625rem 2.5rem;
  border: 1px solid #cbd5e1;
  border-radius: 8px;
  font-size: 0.95rem;
  outline: none;
  transition: border-color 150ms ease;
}

.input-with-adornment input:focus-visible {
  border-color: #2563eb;
  box-shadow: 0 0 0 3px rgba(37, 99, 235, 0.15);
}

.adornment-icon.prefix {
  position: absolute;
  left: 0.875rem;
  color: #94a3b8;
  pointer-events: none;
}

.adornment-action.suffix {
  position: absolute;
  right: 0.75rem;
  background: transparent;
  border: none;
  color: #94a3b8;
  cursor: pointer;
  padding: 0.25rem;
  border-radius: 4px;
}

.adornment-action.suffix:hover {
  color: #0f172a;
}
```

---

### Example 4: Accessible Custom Toggle Switch via Pure CSS
```html
<label class="toggle-switch">
  <input type="checkbox" role="switch" aria-checked="false">
  <span class="switch-track">
    <span class="switch-thumb"></span>
  </span>
  <span class="switch-label">Enable Multi-Factor Authentication</span>
</label>
```

```css
.toggle-switch {
  display: inline-flex;
  align-items: center;
  gap: 0.75rem;
  cursor: pointer;
  user-select: none;
}

/* Retain native input in DOM for keyboard tab focus, but hide visually */
.toggle-switch input {
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

.switch-track {
  position: relative;
  width: 44px;
  height: 24px;
  background: #cbd5e1;
  border-radius: 9999px;
  transition: background-color 200ms ease;
  display: inline-block;
}

.switch-thumb {
  position: absolute;
  top: 2px;
  left: 2px;
  width: 20px;
  height: 20px;
  background: #ffffff;
  border-radius: 50%;
  box-shadow: 0 1px 3px rgba(0, 0, 0, 0.2);
  transition: transform 200ms cubic-bezier(0.16, 1, 0.3, 1);
}

/* Checked state */
.toggle-switch input:checked + .switch-track {
  background: #2563eb;
}

.toggle-switch input:checked + .switch-track .switch-thumb {
  transform: translateX(20px);
}

/* High-contrast focus-visible ring */
.toggle-switch input:focus-visible + .switch-track {
  outline: 2px solid #2563eb;
  outline-offset: 2px;
}

.switch-label {
  font-size: 0.95rem;
  font-weight: 500;
  color: #1e293b;
}
```

---

### Example 5: Drag & Drop File Upload Dropzone
```html
<div class="dropzone-container">
  <input type="file" id="file-upload" class="dropzone-input" multiple aria-label="Upload files">
  <div class="dropzone-ui">
    <div class="upload-icon">📁</div>
    <p><strong>Click to upload</strong> or drag and drop files here</p>
    <span>ZIP, CSV, or JSON up to 50MB</span>
  </div>
</div>
```

```css
.dropzone-container {
  position: relative;
  border: 2px dashed #cbd5e1;
  border-radius: 12px;
  background: #f8fafc;
  padding: 2.5rem 1.5rem;
  text-align: center;
  transition: border-color 200ms ease, background-color 200ms ease;
}

.dropzone-container:hover,
.dropzone-container:has(.dropzone-input:focus-visible) {
  border-color: #2563eb;
  background: #eff6ff;
}

.dropzone-container:has(.dropzone-input:focus-visible) {
  outline: 2px solid #2563eb;
  outline-offset: 4px;
}

.dropzone-input {
  position: absolute;
  inset: 0;
  width: 100%;
  height: 100%;
  opacity: 0;
  cursor: pointer;
}

.dropzone-ui {
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 0.5rem;
  pointer-events: none;
}

.upload-icon {
  font-size: 2.5rem;
}

.dropzone-ui p {
  margin: 0;
  color: #334155;
  font-size: 1rem;
}

.dropzone-ui span {
  font-size: 0.8125rem;
  color: #94a3b8;
}
```

---

## 6. Best Practices

1. **Always Favor `:user-invalid` over `:invalid`**: Prevent validation errors from flashing prematurely when users first land on blank required form fields.
2. **Never Hide Form Inputs with `display: none`**: To style custom radio buttons, switches, or file upload dropzones, keep the input in the DOM using `opacity: 0` or standard visually-hidden clip rectangles to preserve keyboard and screen reader accessibility.
3. **Explicitly Group Related Controls with `<fieldset>` and `<legend>`**: Always wrap groups of radio buttons or related checkbox options in semantic `<fieldset>` tags with clear descriptive legends.
4. **Reserve Vertical Space for Error Messages**: Reserve height for validation messages (`min-height: 1.25rem`) below inputs to prevent jarring layout shifts when error text renders dynamically.
5. **Enforce Touch Target Sizes**: All checkboxes, radio buttons, and submit buttons must provide at least a 44x44px clickable/tappable area.
6. **Support Browser Autofill Elegantly**: Style `:-webkit-autofill` using internal box-shadow hacks to prevent the browser from overriding your dark-mode backgrounds with bright yellow tinting:
   ```css
   input:-webkit-autofill {
     -webkit-box-shadow: 0 0 0 1000px #1e293b inset;
     -webkit-text-fill-color: #ffffff;
   }
   ```

---

## 7. Common Mistakes & Pitfalls

| Mistake | Root Cause | Symptom | Correct Architecture |
|---|---|---|---|
| **Premature Validation Flash** | Styling `:invalid` directly | Red borders flash on empty required fields immediately on page load | Use `:user-invalid` to style only after user commit |
| **Inaccessible Custom Checkbox** | Setting `input { display: none; }` | Keyboard users cannot focus checkbox; screen reader ignores state | Use `appearance: none` or absolute visually-hidden positioning |
| **Placeholder as Label** | Omitting `<label>` and using `placeholder` | User begins typing; label vanishes; cognitive context lost | Always provide visible semantic `<label>` above input |
| **Yellow Autofill Clash** | Browser default autofill overrides input style | Dark-themed input turns jarring bright yellow upon autofill | Apply `-webkit-box-shadow: 0 0 0 1000px var(--bg) inset;` |
| **Clipped Form Grid on Mobile** | Hard-coding 4 columns without responsive collapse | Form fields crush to 30px width or overflow mobile viewport | Add `@media (max-width: 640px) { grid-template-columns: 1fr; }` |
| **Unlinked Error Text** | Error text div lacks `id` association | Sighted users see error; screen readers fail to announce it | Wire `aria-describedby="error-id"` from input to error message |

---

## 8. Performance, Accessibility & Browser Support

### Performance Safeguards
- **Zero JS Floating Labels**: Implementing floating labels via `:placeholder-shown` and sibling selectors eliminates the need for expensive `focus` and `input` event listeners.
- **Hardware-Accelerated Transitions**: Animate toggle switch thumbs and floating labels using `transform` rather than `left` or `top` to keep transitions running on the compositor thread.

### Accessibility Protocols
- **Error Identification**: Ensure all invalid fields are programmatically flagged via `aria-invalid="true"` or native HTML constraint validation attributes (`required`, `pattern`).
- **Focus Order**: Maintain logical source order matching visual layout. Never use `order` or negative positioning to swap input sequences in ways that confuse keyboard tabbing.

---

## 9. Comparison: Form Styling Strategies

| Feature | Modern `:user-invalid` | Native `accent-color` | Visually-Hidden Custom Input | Heavy JS Form Lib |
|---|---|---|---|---|
| **Premature Error Defense** | Built-in (Native) | N/A | Dependent on CSS selectors | Managed in script |
| **Screen Reader Semantics** | 100% Native | 100% Native | 100% Native | Variable / Prone to bugs |
| **Design System Customization**| Full CSS styling | Accent hue only | Complete visual control | Complete visual control |
| **Performance Overhead** | Zero bytes JS | Zero bytes JS | Zero bytes JS | 30KB - 80KB JS runtime |
| **Baseline Support** | Widely Available | Widely Available | Widely Available | N/A |

---

## 10. Interview Questions

<details>
<summary><strong>1. Why is `:user-invalid` vastly superior to `:invalid` for form UX?</strong></summary>

**Answer:**
The `:invalid` pseudo-class evaluates the validity of an input at all times. If an input has the `required` attribute, it is technically invalid the millisecond the page renders. If a developer styles `input:invalid { border-color: red; }`, every required field on the blank form will flash red immediately, confusing users before they even type.

`:user-invalid` represents an input that is invalid *only after the user has actively interacted with it* (e.g. focused, modified, and blurred out, or attempted to submit the form). This aligns perfectly with human expectations: validation errors appear only after an error has actually been made.
</details>

<details>
<summary><strong>2. How does the `:placeholder-shown` trick enable pure CSS floating labels?</strong></summary>

**Answer:**
1. The input is initialized with an empty space placeholder: `<input placeholder=" ">`.
2. When the input is completely empty, the browser matches `:placeholder-shown`. The label is positioned over the input field like a normal placeholder.
3. When the user either focuses the input OR types text into it, `:placeholder-shown` evaluates to `false` (it matches `:not(:placeholder-shown)`).
4. The CSS selector `input:not(:placeholder-shown) ~ label, input:focus ~ label` applies `transform: translateY(-100%) scale(0.75)`, smoothly animating the label to the top collar of the input box without requiring any JavaScript event listeners.
</details>

<details>
<summary><strong>3. How does `accent-color` simplify branding form controls?</strong></summary>

**Answer:**
`accent-color` allows developers to customize the accent hue of native form elements (checkboxes, radio buttons, range sliders, progress bars) with a single CSS line:
```css
:root {
  accent-color: #2563eb;
}
```
The browser takes this accent color and automatically renders native controls tinted with the brand hue, including computing accessible contrast ratios for checkmarks and radio pips, without needing custom SVG overlays or fragile DOM hacks.
</details>

<details>
<summary><strong>4. How do you override the default yellow autofill background in WebKit/Blink browsers?</strong></summary>

**Answer:**
WebKit/Blink injects default user-agent styles using `:-webkit-autofill`. Because `background-color` is often protected, developers override it using an internal inset `box-shadow` and text fill color:
```css
input:-webkit-autofill,
input:-webkit-autofill:hover,
input:-webkit-autofill:focus {
  -webkit-box-shadow: 0 0 0 1000px #ffffff inset;
  -webkit-text-fill-color: #0f172a;
  transition: background-color 5000s ease-in-out 0s;
}
```
</details>

<details>
<summary><strong>5. Why is `display: none` on `<input type="checkbox">` an accessibility violation, and what is the proper workaround?</strong></summary>

**Answer:**
Setting `display: none` completely removes the input element from both the layout tree and the accessibility tree. As a result:
- Keyboard users cannot press <kbd>Tab</kbd> to reach the checkbox.
- Screen readers cannot announce the checkbox or its checked/unchecked status.

The proper workaround is the **visually hidden pattern**:
```css
.visually-hidden {
  position: absolute;
  width: 1px;
  height: 1px;
  padding: 0;
  margin: -1px;
  overflow: hidden;
  clip: rect(0, 0, 0, 0);
  border: 0;
}
```
The input remains focusable in the DOM, allowing `:focus-visible` styling and assistive technologies to function normally while rendering a custom visual replacement adjacent to it.
</details>

<details>
<summary><strong>6. How does CSS `:has()` allow styling a parent form group based on input validity?</strong></summary>

**Answer:**
Traditionally, styling a parent `.field-group` when an input was invalid required JavaScript to toggle an `.has-error` class on the parent.

With `:has()`, CSS does this declaratively:
```css
.field-group:has(:user-invalid) {
  background-color: #fef2f2;
  border-left: 4px solid #ef4444;
}
```
If any input inside `.field-group` matches `:user-invalid`, the parent container immediately updates its background and border styles.
</details>

<details>
<summary><strong>7. How do you style a range input (`<input type="range">`) across browsers?</strong></summary>

**Answer:**
Range styling requires targeting pseudo-elements for both the track and thumb:
- Chromium / Safari: Target `input[type="range"]::-webkit-slider-runnable-track` and `::-webkit-slider-thumb`.
- Firefox: Target `input[type="range"]::-moz-range-track` and `::-moz-range-thumb`.
- Reset base styling first with `appearance: none; background: transparent;`.
</details>

<details>
<summary><strong>8. How should high contrast mode (`forced-colors: active`) be styled on custom form checkboxes?</strong></summary>

**Answer:**
Under `forced-colors: active`, background colors are removed. If custom checkboxes use background images or colors for checkmarks, the checkmarks disappear.

To ensure accessibility:
- Define explicit borders using system keywords: `border: 2px solid ButtonText;`.
- Use CSS `mark` pseudo-elements or SVGs with `fill: HighlightText;`.
- For checked states, assign `background-color: Highlight;`.
</details>

---

## 11. Practice Exercises

### Exercise 1: Interaction-Aware Input Styling
**Task**: Style an email input that shows a subtle grey border initially, a blue focus ring when active, a red border when `:user-invalid`, and a green border when `:user-valid`.
<details>
<summary>View Verified Solution</summary>

```css
.email-input {
  width: 100%;
  padding: 0.625rem 0.875rem;
  border: 1px solid #cbd5e1;
  border-radius: 6px;
  font-size: 1rem;
  outline: none;
  transition: border-color 150ms ease, box-shadow 150ms ease;
}

.email-input:focus-visible {
  border-color: #2563eb;
  box-shadow: 0 0 0 3px rgba(37, 99, 235, 0.2);
}

.email-input:user-invalid {
  border-color: #ef4444;
}

.email-input:user-invalid:focus-visible {
  box-shadow: 0 0 0 3px rgba(239, 68, 68, 0.2);
}

.email-input:user-valid {
  border-color: #10b981;
}

.email-input:user-valid:focus-visible {
  box-shadow: 0 0 0 3px rgba(16, 185, 129, 0.2);
}
```
</details>

---

### Exercise 2: Responsive Form Action Button Bar
**Task**: Create a bottom action bar with "Cancel" and "Save Changes" buttons. The buttons should be aligned right on desktop, but stretch to 100% full width and stack vertically on screens below 480px.
<details>
<summary>View Verified Solution</summary>

```html
<div class="form-actions">
  <button type="button" class="btn btn-cancel">Cancel</button>
  <button type="submit" class="btn btn-save">Save Changes</button>
</div>
```

```css
.form-actions {
  display: flex;
  justify-content: flex-end;
  gap: 1rem;
  margin-top: 2rem;
}

.btn {
  padding: 0.625rem 1.25rem;
  border-radius: 6px;
  font-weight: 600;
  cursor: pointer;
}

.btn-cancel {
  background: #f1f5f9;
  border: 1px solid #cbd5e1;
  color: #475569;
}

.btn-save {
  background: #2563eb;
  border: 1px solid #1d4ed8;
  color: #ffffff;
}

@media (max-width: 480px) {
  .form-actions {
    flex-direction: column-reverse; /* Primary action on top */
  }
  .btn {
    width: 100%;
    text-align: center;
  }
}
```
</details>

---

### Exercise 3: Modern Custom Radio Button Group
**Task**: Style an accessible radio button group where the selected option highlights with a solid brand fill and distinct white center dot.
<details>
<summary>View Verified Solution</summary>

```css
.radio-group input[type="radio"] {
  appearance: none;
  width: 1.25rem;
  height: 1.25rem;
  border: 2px solid #94a3b8;
  border-radius: 50%;
  outline: none;
  display: inline-grid;
  place-content: center;
  cursor: pointer;
  margin: 0;
}

.radio-group input[type="radio"]::before {
  content: "";
  width: 0.5rem;
  height: 0.5rem;
  border-radius: 50%;
  transform: scale(0);
  transition: transform 120ms ease;
  background-color: #2563eb;
}

.radio-group input[type="radio"]:checked {
  border-color: #2563eb;
}

.radio-group input[type="radio"]:checked::before {
  transform: scale(1);
}

.radio-group input[type="radio"]:focus-visible {
  outline: 2px solid #2563eb;
  outline-offset: 2px;
}
```
</details>

---

### Exercise 4: Dynamic Password Strength Indicator
**Task**: Using CSS `:has()` and custom data attributes on an input, color-code a strength bar underneath a password field.
<details>
<summary>View Verified Solution</summary>

```html
<div class="password-wrapper" data-strength="strong">
  <input type="password" placeholder="Enter password">
  <div class="strength-bar"></div>
</div>
```

```css
.strength-bar {
  height: 4px;
  width: 100%;
  background: #e2e8f0;
  border-radius: 2px;
  margin-top: 0.5rem;
  position: relative;
  overflow: hidden;
}

.strength-bar::after {
  content: "";
  position: absolute;
  top: 0;
  left: 0;
  height: 100%;
  transition: width 200ms ease, background-color 200ms ease;
}

.password-wrapper[data-strength="weak"] .strength-bar::after {
  width: 33%;
  background-color: #ef4444;
}

.password-wrapper[data-strength="medium"] .strength-bar::after {
  width: 66%;
  background-color: #eab308;
}

.password-wrapper[data-strength="strong"] .strength-bar::after {
  width: 100%;
  background-color: #10b981;
}
```
</details>

---

### Exercise 5: Textarea with Dynamic Vertical Resize Only
**Task**: Prevent textareas from being resized horizontally (which breaks grid layouts) while allowing users to stretch them vertically.
<details>
<summary>View Verified Solution</summary>

```css
.safe-textarea {
  width: 100%;
  min-height: 120px;
  max-height: 400px;
  resize: vertical; /* Blocks horizontal resize blowout */
  padding: 0.75rem;
  border: 1px solid #cbd5e1;
  border-radius: 6px;
  font-family: inherit;
  font-size: 1rem;
}
```
</details>

---

### Exercise 6: Inline Input Currency Prefix
**Task**: Create an input field with an un-editable `$` dollar sign prefix locked to the left side without using absolute positioning.
<details>
<summary>View Verified Solution</summary>

```html
<div class="currency-input-group">
  <span class="currency-symbol">$</span>
  <input type="number" step="0.01" placeholder="0.00">
</div>
```

```css
.currency-input-group {
  display: flex;
  align-items: center;
  border: 1px solid #cbd5e1;
  border-radius: 6px;
  padding: 0 0.75rem;
  background: #ffffff;
}

.currency-input-group:focus-within {
  border-color: #2563eb;
  box-shadow: 0 0 0 3px rgba(37, 99, 235, 0.15);
}

.currency-symbol {
  color: #64748b;
  font-weight: 600;
  user-select: none;
}

.currency-input-group input {
  border: none;
  outline: none;
  padding: 0.625rem 0.5rem;
  width: 100%;
  font-size: 1rem;
}

.currency-input-group input:focus-visible {
  outline: 2px solid #2563eb;
  border-radius: 4px;
}
```
</details>

---

## 12. Summary / Cheat Sheet

```
+------------------------------------------------------------------------------------+
|                         FORM LAYOUTS & STYLING CHEAT SHEET                         |
+------------------------------------------------------------------------------------+
| 1. Interaction-Aware Validity:                                                     |
|    input:user-invalid { border-color: #ef4444; }                                   |
|    input:user-valid   { border-color: #10b981; }                                   |
+------------------------------------------------------------------------------------+
| 2. Native Branding Tint:                                                           |
|    :root { accent-color: #2563eb; }                                                |
+------------------------------------------------------------------------------------+
| 3. Pure CSS Floating Label:                                                        |
|    input:not(:placeholder-shown) ~ label, input:focus ~ label { top: 0; scale: 0.75; }|
+------------------------------------------------------------------------------------+
| 4. Parent Form Validation via :has():                                              |
|    .field-group:has(:user-invalid) { border-left: 4px solid red; }                |
+------------------------------------------------------------------------------------+
| 5. Autofill Background Override:                                                   |
|    input:-webkit-autofill { -webkit-box-shadow: 0 0 0 1000px #fff inset; }          |
+------------------------------------------------------------------------------------+
| 6. Grid Column Spanning:                                                           |
|    .full-width { grid-column: 1 / -1; }                                            |
+------------------------------------------------------------------------------------+
```

---

## 13. Further Reading
- [W3C HTML5 Form Controls Specification](https://html.spec.whatwg.org/multipage/forms.html)
- [MDN Web Docs: `:user-invalid`](https://developer.mozilla.org/en-US/docs/Web/CSS/:user-invalid)
- [MDN Web Docs: `accent-color`](https://developer.mozilla.org/en-US/docs/Web/CSS/accent-color)
- [web.dev: Sign-in and Form Best Practices](https://web.dev/learn/forms/)
- [W3C WAI-ARIA Form Testing Practices](https://www.w3.org/WAI/ARIA/apg/patterns/)

---

## Navigation
- **Previous**: [Hero Sections and Landing Layouts](file:///e:/Learning/engineering-notes/01-languages/css/13-layout-and-component-recipes/08-hero-sections-and-landing-layouts.md)
- **Next**: [Responsive Tables Recipes](file:///e:/Learning/engineering-notes/01-languages/css/13-layout-and-component-recipes/10-responsive-tables-recipes.md)
