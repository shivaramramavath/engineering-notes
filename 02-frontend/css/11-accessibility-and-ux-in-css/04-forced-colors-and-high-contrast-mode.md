# 04. Forced Colors and High Contrast Mode

| Metadata | Details |
| :--- | :--- |
| **Spec Level & Status** | CSS Color Module Level 4 / CSS Forced Colors Module Level 1 / W3C Candidate Recommendation |
| **Browsers Tested** | Chrome 89+, Edge 89+, Firefox 89+, Safari 16.4+ (Universal Baseline) |
| **Tested With** | Windows 11 Contrast Themes (Aquatic, Desert, Dusk, Night Sky), DevTools Forced Colors Emulation |
| **Prerequisites** | `01-fundamentals/10-colours.md`, `11-accessibility-and-ux-in-css/03-prefers-color-scheme-contrast-and-reduced-data.md` |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Usage](#4-syntax--usage)
5. Practical Examples
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

### The Problem
Millions of users with severe visual impairments (including macular degeneration, cataract recovery, glaucoma, diabetic retinopathy, and extreme photophobia) rely on operating system high-contrast display modes—most notably **Windows Contrast Themes** (formerly Windows High Contrast Mode / WHCM).

In these modes, the operating system forcibly strips away the author's custom styling: background colors, background images, shadows, text colors, and subtle border colors are completely eliminated and replaced by a restricted, high-contrast system color palette. If web applications are constructed using fragile visual assumptions (such as using `box-shadow` instead of borders, relying on background icons, or failing to preserve semantic outlines), interfaces become fragmented, invisible, or utterly unusable for high-contrast users.

### The Solution
The modern Web Platform standardizes high-contrast support via the **CSS Forced Colors Module**:
- `@media (forced-colors: active)` allows authors to detect when forced colors mode is active and adjust layout structures accordingly.
- **CSS System Colors** (`Canvas`, `CanvasText`, `ButtonFace`, `ButtonText`, `Highlight`, `LinkText`) provide semantic, platform-agnostic color keywords that automatically map to the user's active OS contrast theme.
- The `forced-color-adjust` property enables selective opt-outs for elements where preserving true colors is functionally essential (e.g., color picker swatches or photographic filters).
- The **Transparent Border Pattern** guarantees that buttons, cards, and input fields retain visible boundaries in Forced Colors Mode without impacting standard visual designs.

---

## 2. Core Concepts

### 1. What the Browser Engine Strips in Forced Colors Mode
When `forced-colors: active` is engaged, the user agent automatically overrides the following CSS properties with system defaults:
- `color` $\rightarrow$ mapped to `CanvasText` (or context equivalent)
- `background-color` $\rightarrow$ mapped to `Canvas` (or `ButtonFace` on buttons)
- `background-image` $\rightarrow$ stripped to `none` (except for `linear-gradient` on borders)
- `box-shadow` $\rightarrow$ stripped to `none`
- `text-shadow` $\rightarrow$ stripped to `none`
- `border-color` $\rightarrow$ mapped to `CanvasText` or `ButtonBorder`
- `outline-color` $\rightarrow$ mapped to `Highlight` on focus

```
STANDARD BROWSER RENDERING                FORCED COLORS (WHCM) RENDERING
┌─────────────────────────────────┐      ┌─────────────────────────────────┐
│ [Custom Brand Blue Button]      │      │ ┌─────────────────────────────┐ │
│ Background: #2563eb             │ ───► │ │ BUTTON TEXT (ButtonText)    │ │
│ Box-Shadow: 0 4px 6px (Shadow)  │      │ └─────────────────────────────┘ │
│ Text: White                     │      │ Border: System ButtonBorder     │
└─────────────────────────────────┘      │ Background: System ButtonFace   │
                                         └─────────────────────────────────┘
```

### 2. Standard CSS System Color Keywords (CSS Color 4)

| System Color Keyword | Semantic Purpose & Mapping |
| :--- | :--- |
| **`Canvas`** | Background surface of application content or windows. |
| **`CanvasText`** | Foreground text displayed on `Canvas` surfaces. |
| **`LinkText`** | Hyperlinks that have not yet been visited. |
| **`VisitedText`** | Hyperlinks that have been visited. |
| **`ButtonFace`** | Background surface of clickable buttons and control elements. |
| **`ButtonText`** | Foreground text on clickable buttons and controls. |
| **`ButtonBorder`** | Visible border enclosing buttons and interactive controls. |
| **`Field`** | Background surface of input fields and text entry controls. |
| **`FieldText`** | Text typed inside input fields and controls. |
| **`Highlight`** | Background color of selected text, items, or active focus rings. |
| **`HighlightText`** | Foreground text color displayed on top of `Highlight` backgrounds. |
| **`GrayText`** | Disabled, inactive text and controls. |
| **`Mark`** | Background of highlighted/marked text (e.g. `<mark>` tag). |
| **`MarkText`** | Text color on `<mark>` surfaces. |

---

## 3. How It Works Internally

### Forced Colors Cascade Algorithm
When the operating system engages a Contrast Theme, the browser's style engine applies an internal **User-Agent High-Priority Rule Layer**:

```
AUTHOR STYLESHEET
  .btn {
    background-color: #2563eb;
    box-shadow: 0 4px 10px rgba(0,0,0,0.2);
    border: none;
  }
       │
       ▼
BROWSER FORCED COLORS RESOLUTION PASS:
  1. Inspect element: Is forced-color-adjust: none set?
     - IF YES: Preserve author colors.
     - IF NO (Default): Apply Forced Colors Rules:
       - background-color: ButtonFace;
       - color: ButtonText;
       - box-shadow: none; (SHADOW DESTROYED!)
       - border: none;     (NO BORDER PRESERVED! ELEMENT BLENDS INTO CANVAS!)
```

**The Fatal Result of `border: none`**: Because the author used `box-shadow` instead of a border, and the OS stripped `box-shadow`, the button loses all visual boundaries and disappears into the background!

---

## 4. Syntax & Usage

### 1. The Transparent Border Pattern
The industry standard technique to ensure UI controls never disappear in Forced Colors Mode:

```css
/* Modern Button with invisible border in standard mode */
.c-button {
  background-color: #2563eb;
  color: #ffffff;
  /* Invisible in standard browsing, but converts to visible ButtonBorder in WHCM */
  border: 2px solid transparent;
  border-radius: 6px;
  padding: 0.5rem 1rem;
}
```

### 2. Targeting Forced Colors via Media Query
```css
@media (forced-colors: active) {
  /* Provide system-color indicators for custom interactive elements */
  .custom-tag--active {
    outline: 2px solid Highlight;
    outline-offset: 2px;
  }

  /* Ensure SVG icons use system text color */
  .icon {
    fill: CanvasText;
  }
}
```

### 3. Opting Out via `forced-color-adjust`
Use `forced-color-adjust: none` only when true color representation is essential to the functionality of the control:

```css
/* Color Picker Palette Swatch */
.color-swatch {
  forced-color-adjust: none; /* OS will not override the background color */
  width: 2rem;
  height: 2rem;
  border-radius: 50%;
  border: 2px solid CanvasText; /* Add visible system border so swatch doesn't vanish */
}
```

---

## 5. Practical Examples

### Example 1: Accessible Modal Dialog in Forced Colors
Modal dialogs often rely on subtle drop-shadows and dimmed overlays. In forced colors mode, shadows vanish, and backdrops can turn pitch black:

```html
<dialog class="accessible-dialog" open>
  <h2 class="dialog-title">Confirm Database Flush</h2>
  <p class="dialog-body">This action cannot be undone. All cache records will be purged.</p>
  <div class="dialog-actions">
    <button type="button" class="btn btn-secondary">Cancel</button>
    <button type="button" class="btn btn-danger">Confirm Flush</button>
  </div>
</dialog>
```

```css
.accessible-dialog {
  background-color: #ffffff;
  color: #0f172a;
  border-radius: 0.75rem;
  padding: 1.5rem;
  max-width: 450px;
  box-shadow: 0 20px 25px -5px rgba(0, 0, 0, 0.2);
  /* Transparent border protects modal boundary in Forced Colors */
  border: 2px solid transparent;
}

.accessible-dialog::backdrop {
  background-color: rgba(0, 0, 0, 0.5);
  backdrop-filter: blur(4px);
}

@media (forced-colors: active) {
  .accessible-dialog {
    /* Explicitly bind to Canvas and CanvasText */
    background-color: Canvas;
    color: CanvasText;
    border-color: CanvasText;
  }

  .accessible-dialog::backdrop {
    /* Ensure backdrop remains visible in high contrast */
    background-color: rgba(0, 0, 0, 0.7);
  }

  .btn-danger {
    /* Distinguish destructive button using double border */
    border: 3px double ButtonText;
  }
}
```

### Example 2: SVG Icons and `currentColor`
When SVG icons use hardcoded hex fills (`fill="#2563eb"`), the browser cannot map them to the system text color in Forced Colors Mode, causing icons to become invisible on inverted system backgrounds.

```html
<!-- BAD: Hardcoded fill -->
<button class="nav-btn">
  <svg viewBox="0 0 20 20" fill="#2563eb"><circle cx="10" cy="10" r="8"/></svg>
  <span>Settings</span>
</button>

<!-- GOOD: currentColor inheritance -->
<button class="nav-btn">
  <svg class="nav-btn__icon" viewBox="0 0 20 20" fill="currentColor">
    <circle cx="10" cy="10" r="8"/>
  </svg>
  <span>Settings</span>
</button>
```

```css
.nav-btn {
  display: inline-flex;
  align-items: center;
  gap: 0.5rem;
  padding: 0.5rem 1rem;
  color: #1e40af;
  background-color: #eff6ff;
  border: 1px solid transparent;
  border-radius: 4px;
}

.nav-btn__icon {
  width: 1.25rem;
  height: 1.25rem;
  /* Automatically inherits system ButtonText or CanvasText in Forced Colors! */
  fill: currentColor;
}
```

### Example 3: Accessible Custom Radio Buttons in Forced Colors
Custom radio buttons that replace native `<input type="radio">` with custom `<div>`s often vanish completely in WHCM unless explicitly styled with system colors:

```html
<label class="custom-radio">
  <input type="radio" name="plan" class="custom-radio__input">
  <span class="custom-radio__circle" aria-hidden="true"></span>
  <span class="custom-radio__label">Pro Tier ($29/mo)</span>
</label>
```

```css
.custom-radio {
  display: inline-flex;
  align-items: center;
  gap: 0.5rem;
  cursor: pointer;
}

.custom-radio__input {
  position: absolute;
  opacity: 0;
  width: 0;
  height: 0;
}

.custom-radio__circle {
  width: 1.25rem;
  height: 1.25rem;
  border: 2px solid #64748b;
  border-radius: 50%;
  display: inline-block;
  position: relative;
}

.custom-radio__input:checked + .custom-radio__circle {
  border-color: #2563eb;
  background-color: #ffffff;
}

.custom-radio__input:checked + .custom-radio__circle::after {
  content: "";
  position: absolute;
  top: 3px;
  left: 3px;
  width: 8px;
  height: 8px;
  border-radius: 50%;
  background-color: #2563eb;
}

/* HIGH CONTRAST / FORCED COLORS ADAPTATION */
@media (forced-colors: active) {
  .custom-radio__circle {
    border-color: CanvasText;
  }

  .custom-radio__input:checked + .custom-radio__circle::after {
    /* Use Highlight system color so the selected dot is clearly visible */
    background-color: Highlight;
  }

  .custom-radio__input:focus-visible + .custom-radio__circle {
    outline: 2px solid Highlight;
    outline-offset: 3px;
  }
}
```

### Example 4: Selected State in Data Tables and Navigation
When CSS background highlights (`background-color: #e0f2fe`) are stripped, selected items become indistinguishable from unselected items:

```css
.nav-item {
  display: block;
  padding: 0.75rem 1rem;
  color: #334155;
  border-left: 3px solid transparent;
}

.nav-item.is-selected {
  background-color: #eff6ff;
  color: #1d4ed8;
  font-weight: 600;
  border-left-color: #1d4ed8;
}

/* Forced Colors Enhancement */
@media (forced-colors: active) {
  .nav-item.is-selected {
    /* System Highlight color applied to border */
    border-left: 4px solid Highlight;
    /* Optional: forced background highlight */
    background-color: Highlight;
    color: HighlightText;
  }
}
```

### Example 5: Color Swatch Palette with `forced-color-adjust: none`
```html
<div class="swatch-picker" role="radiogroup" aria-label="Product Color Options">
  <button type="button" class="swatch" style="--swatch-color: #ef4444;" aria-label="Crimson Red"></button>
  <button type="button" class="swatch" style="--swatch-color: #3b82f6;" aria-label="Royal Blue"></button>
</div>
```

```css
.swatch {
  width: 2.5rem;
  height: 2.5rem;
  border-radius: 50%;
  background-color: var(--swatch-color);
  border: 2px solid transparent;
  cursor: pointer;
}

@media (forced-colors: active) {
  .swatch {
    /* Prevent the OS from replacing the red and blue colors with CanvasText! */
    forced-color-adjust: none;
    /* Provide a crisp system border so swatches remain distinct against the canvas */
    border: 2px solid ButtonBorder;
  }

  .swatch:focus-visible {
    outline: 3px solid Highlight;
    outline-offset: 3px;
  }
}
```

---

## 6. Best Practices

1. **Always Use `border: 1px solid transparent` on Floating Elements**: Any card, modal, or button that relies on `box-shadow` must include an invisible transparent border to guarantee boundaries in WHCM.
2. **Never Use Hardcoded Hex Colors on SVG Icons**: Set `fill="currentColor"` or `stroke="currentColor"` on inline SVGs so they inherit the OS `CanvasText` or `ButtonText`.
3. **Use CSS System Colors Inside `@media (forced-colors: active)`**: Reference standard keywords (`CanvasText`, `Highlight`, `ButtonBorder`) when enhancing high-contrast states.
4. **Reserve `forced-color-adjust: none` Strictly for Critical Real-World Colors**: Only use this property for photographic filters, color palette pickers, and video elements. Never apply it to entire pages or standard buttons.
5. **Differentiate Selected States with Borders or Outlines**: Because background color highlights are stripped by the OS, use `border`, `outline`, or text decoration to signify selection.
6. **Test with Windows Contrast Themes Regularly**: Emulate forced colors in DevTools and test on an actual Windows machine using the 4 default themes: Aquatic, Desert, Dusk, and Night Sky.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Consequence | Correct Pattern |
| :--- | :--- | :--- |
| **Using `border: none` and relying solely on `box-shadow`** | Elements lose all visual boundaries and vanish into the background canvas. | Apply `border: 1px solid transparent`. |
| **Setting `forced-color-adjust: none` on `body` or `.app`** | Completely defeats the user's accessibility settings, blinding low-vision users. | Never opt out globally; honor user contrast choices. |
| **Using CSS background images for critical iconography** | The OS strips `background-image` to `none`, causing all icon graphics to disappear. | Use inline `<svg>` with `currentColor` or `<img>` elements. |
| **Using hardcoded colors for `:focus-visible` outlines** | Hardcoded outlines may have 1:1 zero contrast against the user's chosen contrast theme. | Use `outline: 2px solid Highlight` in `@media (forced-colors: active)`. |
| **Assuming WHCM is always black text on white or white on black** | Users create customized themes (e.g. green on black, yellow on navy). | Never assume specific colors; rely strictly on CSS System Color keywords. |
| **Omitting visual boundaries on form inputs** | Inputs blend into the page canvas, making it impossible to see where to click. | Ensure `border: 1.5px solid transparent` or standard input borders are set. |

---

## 8. Performance, Accessibility & Browser Support

### Universal Modern Support
- **`@media (forced-colors: active)`**: Supported across Chrome 89+, Edge 89+, Firefox 89+, and Safari 16.4+.
- **CSS System Colors (CSS Color 4)**: Universally supported in all evergreen browsers.
- **`forced-color-adjust`**: Supported in Chrome 89+, Edge 89+, Firefox 89+, and Safari 16.4+.

### DevTools Testing Workflow
1. Open Chrome DevTools $\rightarrow$ Press `Cmd+Shift+P` / `Ctrl+Shift+P`.
2. Type "Rendering" and select **Show Rendering**.
3. Locate **Emulate CSS media feature forced-colors** and select **forced-colors: active**.
4. The page will instantly transform to reflect Windows High Contrast Mode rendering.

---

## 9. Comparison / When to Use What

| Technique | Operating Context | Browser Override Behavior | Best Suited For |
| :--- | :--- | :--- | :--- |
| **`prefers-contrast: more`** | OS "Increase Contrast" toggle | Author retains full control over colors | Author-designed high contrast styling |
| **`forced-colors: active`** | OS Contrast Themes (WHCM) | Browser forcibly overrides author colors | Accommodating system-enforced palettes |
| **`forced-color-adjust: none`** | Specific elements | Prevents OS override on targeted node | Color swatches, image filters, maps |
| **CSS System Colors** | Both normal and forced colors | Dynamically binds to OS active palette | Borders, focus rings, selected states |

---

## 10. Interview Questions

### Q1: What happens to a website when Windows Contrast Themes (Forced Colors Mode) is turned on?
<details>
<summary>Answer</summary>
The browser engine engages a high-priority User-Agent style pass that overrides author colors with a restricted, user-selected system color palette:
1. `color` is forced to `CanvasText`.
2. `background-color` is forced to `Canvas` (or `ButtonFace` on buttons).
3. All `box-shadow` and `text-shadow` declarations are stripped to `none`.
4. All `background-image` declarations (except border gradients) are stripped to `none`.
5. Border colors are remapped to system borders (`ButtonBorder` or `CanvasText`).
Elements that relied on background colors or drop-shadows for visual differentiation disappear unless reinforced with borders or system color keywords.
</details>

### Q2: What is the "Transparent Border Pattern," and why is it considered essential for high-contrast accessibility?
<details>
<summary>Answer</summary>
In modern web design, buttons and cards often feature zero borders (`border: none`) and rely on subtle drop shadows (`box-shadow: 0 4px 6px rgba(0,0,0,0.1)`) or background color differences to define their shape.

In Forced Colors Mode, the OS strips all `box-shadow` and background colors. Without a border, the button or card completely dissolves into the background canvas.
The **Transparent Border Pattern** applies `border: 1px solid transparent` (or 2px) to the element. In standard browsing, the transparent border is completely invisible. In Forced Colors Mode, however, the browser engine recognizes the border and automatically remaps `transparent` to the system's `ButtonBorder` or `CanvasText`, preserving the element's structural boundary without altering the normal visual design.
</details>

### Q3: When is it legally and architecturally appropriate to use `forced-color-adjust: none`?
<details>
<summary>Answer</summary>
`forced-color-adjust: none` should only be used when preserving real-world color is functionally indispensable to understanding or operating the UI. Legitimate use cases include:
1. **Color Pickers & Swatches**: Where a user must select an actual red, blue, or yellow product variant.
2. **Medical / Scientific Imaging**: Heatmaps, MRI scans, or satellite imagery where color gradients convey critical diagnostic data.
3. **Syntax Highlighting**: Where code token legibility depends on multi-color tokens (provided contrast remains acceptable).
It is an anti-pattern and serious accessibility violation to apply `forced-color-adjust: none` to entire web pages or standard UI buttons simply because a designer wants to protect brand colors.
</details>

### Q4: Why should inline SVGs always use `fill="currentColor"` rather than hardcoded hex values in accessible design systems?
<details>
<summary>Answer</summary>
When an SVG uses a hardcoded fill like `fill="#2563eb"`, the browser does not recognize it as general document text, and may fail to override it in Forced Colors Mode. If the user's high-contrast theme has a navy blue canvas, an icon with `#2563eb` becomes completely invisible.

By declaring `fill="currentColor"`, the SVG icon dynamically inherits the computed CSS `color` property of its parent container. Because the parent's `color` is automatically remapped by the OS to `CanvasText`, `ButtonText`, or `HighlightText`, the icon will always match the active high-contrast text color with guaranteed readability.
</details>

### Q5: List four CSS System Color keywords introduced in CSS Color Module Level 4 and explain their semantic roles.
<details>
<summary>Answer</summary>
1. **`Canvas` / `CanvasText`**: Represents the background surface of the page/window and the primary body text displayed upon it.
2. **`ButtonFace` / `ButtonText`**: Represents the background of clickable buttons and the label text rendered on them.
3. **`Highlight` / `HighlightText`**: Represents the background color of user-selected text, active options, or focused items, and the text color placed over that highlight.
4. **`GrayText`**: Represents disabled, inactive, or non-operable text and control elements.
</details>

### Q6: How does `:focus-visible` interact with Windows High Contrast Mode?
<details>
<summary>Answer</summary>
In Forced Colors Mode, standard author focus colors (e.g. `outline: 2px solid #3b82f6`) may conflict with or vanish against the user's custom theme background.
The browser automatically maps focus outlines to the system **`Highlight`** color keyword. To ensure your custom focus indicators behave flawlessly in both standard and forced colors modes, combine `outline` with `Highlight` inside `@media (forced-colors: active)`:
```css
@media (forced-colors: active) {
  :focus-visible {
    outline: 3px solid Highlight;
    outline-offset: 3px;
  }
}
```
</details>

### Q7: Why do CSS background images disappear in Forced Colors Mode, and what HTML element should be used instead for informative imagery?
<details>
<summary>Answer</summary>
In Forced Colors Mode, `background-image` is set to `none` because designers historically placed low-contrast patterns or textures behind text, rendering high-contrast text unreadable.

If an author placed an informative icon or graphic using `background-image: url("icon.png")`, that icon disappears entirely in WHCM. For informative visual graphics, authors must always use semantic HTML elements like `<img>` (with proper `alt` text) or inline `<svg>` elements, which are treated as DOM content and preserved by the browser.
</details>

### Q8: How can a data visualization line chart remain accessible in Windows High Contrast Mode?
<details>
<summary>Answer</summary>
In Forced Colors Mode, all colored lines in a multi-series chart will be converted to the same `CanvasText` color, rendering the series indistinguishable.
To remain accessible:
1. Use different stroke patterns (`stroke-dasharray: 4 4`, `stroke-dasharray: 8 2`).
2. Add distinct marker shapes (circles, squares, triangles) at each data point.
3. Use `forced-color-adjust: none` on the chart container if and only if high-contrast color palettes are also supplied.
</details>

---

## 11. Practice Exercises

### Exercise 1: Build a High-Contrast Resilient Pill Badge
**Objective**: Build a pill badge component that looks sleek in standard mode (soft background and subtle border) and converts into a clearly delineated badge in Forced Colors Mode.

<details>
<summary>Solution</summary>

```html
<span class="pill-badge pill-badge--warning">
  <svg class="pill-badge__icon" viewBox="0 0 16 16" fill="currentColor" aria-hidden="true">
    <path d="M8 1a7 7 0 100 14A7 7 0 008 1zm0 3a.75.75 0 01.75.75v4a.75.75 0 01-1.5 0v-4A.75.75 0 018 4zm0 8a1 1 0 100-2 1 1 0 000 2z"/>
  </svg>
  <span>High Latency</span>
</span>
```

```css
.pill-badge {
  display: inline-flex;
  align-items: center;
  gap: 0.375rem;
  padding: 0.25rem 0.75rem;
  font-size: 0.8125rem;
  font-weight: 600;
  border-radius: 9999px;
  background-color: #fef3c7;
  color: #92400e;
  /* Transparent border guarantees boundary visibility in Forced Colors */
  border: 1px solid transparent;
}

.pill-badge__icon {
  width: 1rem;
  height: 1rem;
  fill: currentColor;
}

@media (forced-colors: active) {
  .pill-badge {
    /* Bind border to CanvasText so badge perimeter is sharp */
    border-color: CanvasText;
  }
}
```
</details>

### Exercise 2: Styling a Responsive Accordion for High-Contrast Mode
**Objective**: Style an HTML `<details>` accordion so that the expanded state is clearly distinguished using system colors and borders in WHCM.

<details>
<summary>Solution</summary>

```html
<details class="hcm-accordion">
  <summary class="hcm-accordion__summary">
    <span>View Deployment Logs</span>
    <span class="hcm-accordion__arrow" aria-hidden="true">&#9662;</span>
  </summary>
  <div class="hcm-accordion__content">
    <pre><code>Build successful in 4.2s. Container pushed to registry.</code></pre>
  </div>
</details>
```

```css
.hcm-accordion {
  border: 1px solid #e5e7eb;
  border-radius: 0.5rem;
  overflow: clip;
}

.hcm-accordion__summary {
  display: flex;
  justify-content: space-between;
  align-items: center;
  padding: 1rem 1.25rem;
  cursor: pointer;
  font-weight: 600;
  background-color: #f8fafc;
}

.hcm-accordion__summary:focus-visible {
  outline: 2px solid #2563eb;
  outline-offset: -2px;
}

.hcm-accordion__content {
  padding: 1.25rem;
  background-color: #ffffff;
  border-top: 1px solid #e5e7eb;
}

@media (forced-colors: active) {
  .hcm-accordion {
    border: 2px solid CanvasText;
  }

  .hcm-accordion[open] .hcm-accordion__summary {
    /* Highlight the active open header */
    border-bottom: 2px solid CanvasText;
  }

  .hcm-accordion__summary:focus-visible {
    outline: 3px solid Highlight;
    outline-offset: -3px;
  }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **Windows Contrast Themes (WHCM)**: OS replaces all author colors, shadows, and background images with system defaults.
- **The Transparent Border Pattern**: Always declare `border: 1px solid transparent` on buttons, cards, and inputs so borders appear in Forced Colors Mode.
- **System Color Keywords**:
  - `Canvas` / `CanvasText` (Background / Text)
  - `ButtonFace` / `ButtonText` / `ButtonBorder` (Interactive Controls)
  - `Highlight` / `HighlightText` (Selected Items & Focus)
  - `GrayText` (Disabled Controls)
- **Inline SVGs**: Always use `fill="currentColor"` so icons automatically inherit high-contrast text colors.
- **`forced-color-adjust: none`**: Use ONLY on swatches, photos, and real-world color pickers; never apply to broad layouts.
- **Testing**: Enable "Emulate CSS media feature forced-colors: active" in Chrome DevTools Rendering panel.

---

## 13. Further Reading
- [W3C CSS Forced Colors Module Level 1](https://www.w3.org/TR/css-forced-colors-1/)
- [MDN Web Docs: forced-colors](https://developer.mozilla.org/en-US/docs/Web/CSS/@media/forced-colors)
- [Microsoft Learn: Styling for Windows High Contrast Mode](https://learn.microsoft.com/en-us/microsoft-edge/web-platform/high-contrast-mode)
- [Melanie Richards: WHCM and System Colors Quick Reference](https://www.tempertemper.net/blog/styling-for-windows-high-contrast-mode)
- [Smashing Magazine: Operating System and Browser High Contrast Modes](https://www.smashingmagazine.com/2022/03/windows-high-contrast-mode-css/)

---

| Previous Note | Next Note |
| :--- | :--- |
| [03. Prefers Color Scheme, Contrast and Reduced Data](03-prefers-color-scheme-contrast-and-reduced-data.md) | [05. Zoom, Reflow and Text Spacing](05-zoom-reflow-and-text-spacing.md) |
