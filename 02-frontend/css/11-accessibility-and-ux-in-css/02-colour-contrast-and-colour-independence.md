# 02. Colour Contrast and Colour Independence

| Metadata | Details |
| :--- | :--- |
| **Spec Level & Status** | WCAG 2.1 / 2.2 Level AA & AAA Standards / WCAG 3.0 APCA Guidelines |
| **Browsers Tested** | Chrome 120+, Firefox 125+, Safari 17.4+ |
| **Tested With** | Modern DevTools Contrast Analyzers, Colour Contrast Analyser (CCA) |
| **Prerequisites** | `01-fundamentals/10-colours.md`, `11-accessibility-and-ux-in-css/01-focus-visible-and-focus-styles.md` |

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
Over 300 million people worldwide experience some form of color vision deficiency (protanopia, deuteranopia, tritanopia, or monochromacy), and billions experience situational visual impairments such as bright glare on mobile screens outdoors or low-brightness laptop batteries.

When web interfaces deploy low-contrast typography (such as light gray `#9ca3af` text on white `#ffffff` backgrounds) or communicate critical system states exclusively through color (e.g. coloring an invalid form input red without an accompanying icon or text message), content becomes completely illegible and unusable for huge segments of the global population.

### The Solution
Architecting accessible, resilient color palettes in CSS requires adhering to two foundational disciplines:
1. **Mathematical Contrast Verification**: Guaranteeing that all text and graphical UI controls satisfy WCAG 2.2 Level AA and AAA relative luminance thresholds.
2. **Color Independence (WCAG SC 1.4.1)**: Ensuring that color is **never used as the sole visual means** of conveying information, indicating an action, prompting a response, or distinguishing a visual element.

---

## 2. Core Concepts

### 1. WCAG 2.2 Contrast Ratio Thresholds

| Element Category | Minimum Definition | WCAG Level AA | WCAG Level AAA |
| :--- | :--- | :--- | :--- |
| **Normal Text** | Font size < 18pt (24px) or < 14pt (18.66px) bold | **4.5 : 1** | **7.0 : 1** |
| **Large Text** | Font size $\ge$ 18pt (24px) or $\ge$ 14pt (18.66px) bold | **3.0 : 1** | **4.5 : 1** |
| **UI Components** | Focus rings, form input borders, active toggles | **3.0 : 1** | N/A (AA requirement) |
| **Graphical Objects** | Standalone icons, charts, infographics | **3.0 : 1** | N/A (AA requirement) |
| **Incidental / Disabled** | Decorative graphics, inactive disabled buttons | No requirement | No requirement |

### 2. Relative Luminance & Contrast Formula
The WCAG 2 contrast ratio is calculated between the relative luminance of the lighter color ($L_1$) and the darker color ($L_2$):

$$\text{Contrast Ratio} = \frac{L_1 + 0.05}{L_2 + 0.05}$$

- Relative luminance ($L$) spans from $0.0$ (pitch black) to $1.0$ (pure white).
- The theoretical contrast range is from **1:1** (identical colors) to **21:1** (pure black `#000000` on pure white `#ffffff`).

### 3. APCA (Advanced Perceptual Contrast Algorithm)
Anticipated in **WCAG 3.0**, APCA replaces the mathematical ratio with a perceptual lightness contrast ($L^c$) algorithm that models how the human eye perceives contrast based on:
- **Spatial Frequency**: Smaller text requires significantly higher contrast than large headlines.
- **Font Weight**: Light thin fonts (e.g. 300 weight) require higher contrast than bold weights (700).
- **Polarity**: Dark text on a light background is perceived differently than light text on a dark background (Halation effect).

### 4. WCAG SC 1.4.1: Use of Color (Independence)
Color must be supplemented with at least one secondary visual differentiator:
- Underlines on links.
- Text labels, icons, or badges on alerts.
- Borders, hatch patterns, or varying shapes on graphs and charts.

---

## 3. How It Works Internally

### Relative Luminance Human Sensitivity Model
The human eye does not perceive all light wavelengths equally. The human retina is heavily packed with green-sensitive M-cones and red-sensitive L-cones, but has few blue-sensitive S-cones. The sRGB relative luminance formula reflects this biological reality:

$$L = 0.2126 \times R + 0.7152 \times G + 0.0722 \times B$$

```
HUMAN EYE SENSITIVITY WEIGHTS:
┌─────────────────────────────────────────────────────────────┐
│ Green: 71.52% (Dominates perceived brightness)              │
│ Red:   21.26% (Moderate perceived brightness)               │
│ Blue:   7.22% (Weak perceived brightness)                   │
└─────────────────────────────────────────────────────────────┘
```
A pure saturated blue (`#0000ff`) has a relative luminance of only $0.0722$, whereas a pure green (`#00ff00`) has a relative luminance of $0.7152$. Consequently, blue on black has poor contrast, but green on black has immense contrast.

---

## 4. Syntax & Usage

### 1. Accessible Link Styling with Color Independence
Links within running text must have a distinct visual signifier other than color alone (WCAG SC 1.4.1). An underline is the universal standard:

```css
/* Accessible inline link */
.prose a {
  color: #1d4ed8; /* AA compliant on white */
  text-decoration: underline;
  text-decoration-thickness: 1.5px;
  text-underline-offset: 3px;
  transition: color 0.15s ease, text-decoration-color 0.15s ease;
}

.prose a:hover {
  color: #1e40af;
  text-decoration-thickness: 2.5px;
}

.prose a:focus-visible {
  outline: 2px solid #1d4ed8;
  outline-offset: 3px;
  border-radius: 2px;
}
```

### 2. Form Input Border Contrast (WCAG 2.2 SC 1.4.11)
Default browser input borders often fail the 3:1 non-text contrast requirement. Author CSS must guarantee input boundaries are visible:

```css
.form-input {
  /* #64748b on white #ffffff has a 4.6:1 contrast ratio (exceeds 3:1 AA requirement) */
  border: 1.5px solid #64748b;
  border-radius: 0.375rem;
  padding: 0.625rem 0.875rem;
  background-color: #ffffff;
  color: #0f172a;
}

.form-input:hover {
  border-color: #334155;
}

.form-input:focus-visible {
  border-color: #0284c7;
  outline: 2px solid #0284c7;
  outline-offset: 2px;
}
```

---

## 5. Practical Examples

### Example 1: Multi-State Status Badges with Icon + Text (Color Independence)
Never rely on green, yellow, or red colors alone to communicate status. Pair each state with an icon and clear text:

```html
<div class="status-stack">
  <!-- Success -->
  <span class="badge badge--success">
    <svg class="badge__icon" viewBox="0 0 20 20" fill="currentColor" aria-hidden="true">
      <path fill-rule="evenodd" d="M10 18a8 8 0 100-16 8 8 0 000 16zm3.707-9.293a1 1 0 00-1.414-1.414L9 10.586 7.707 9.293a1 1 0 00-1.414 1.414l2 2a1 1 0 001.414 0l4-4z" clip-rule="evenodd" />
    </svg>
    <span>Operational</span>
  </span>

  <!-- Error -->
  <span class="badge badge--error">
    <svg class="badge__icon" viewBox="0 0 20 20" fill="currentColor" aria-hidden="true">
      <path fill-rule="evenodd" d="M10 18a8 8 0 100-16 8 8 0 000 16zM8.707 7.293a1 1 0 00-1.414 1.414L8.586 10l-1.293 1.293a1 1 0 101.414 1.414L10 11.414l1.293 1.293a1 1 0 001.414-1.414L11.414 10l1.293-1.293a1 1 0 00-1.414-1.414L10 8.586 8.707 7.293z" clip-rule="evenodd" />
    </svg>
    <span>Failed</span>
  </span>
</div>
```

```css
.badge {
  display: inline-flex;
  align-items: center;
  gap: 0.375rem;
  padding: 0.25rem 0.625rem;
  border-radius: 9999px;
  font-size: 0.8125rem;
  font-weight: 600;
  border: 1px solid transparent;
}

.badge__icon {
  inline-size: 1rem;
  block-size: 1rem;
}

/* Success: Dark green #14532d on light green #dcfce7 = 7.8:1 contrast (AAA compliant) */
.badge--success {
  background-color: #dcfce7;
  color: #14532d;
  border-color: #86efac;
}

/* Error: Dark red #7f1d1d on light red #fee2e2 = 8.6:1 contrast (AAA compliant) */
.badge--error {
  background-color: #fee2e2;
  color: #7f1d1d;
  border-color: #fca5a5;
}
```

### Example 2: Form Error State with Multi-Modal Feedback
When a form field fails validation, indicate the error through border contrast, an error icon, assistive ARIA attributes, and explicit text:

```html
<div class="form-field form-field--error">
  <label for="user-email" class="form-label">Email Address</label>
  <div class="input-wrapper">
    <input
      type="email"
      id="user-email"
      class="form-input"
      value="invalid-email"
      aria-invalid="true"
      aria-describedby="email-error-msg"
    >
    <span class="error-icon" aria-hidden="true">⚠</span>
  </div>
  <p id="email-error-msg" class="error-message">
    Please enter a valid email address containing an '@' symbol.
  </p>
</div>
```

```css
.form-field {
  display: flex;
  flex-direction: column;
  gap: 0.375rem;
  max-inline-size: 360px;
}

.form-label {
  font-size: 0.875rem;
  font-weight: 600;
  color: #1e293b;
}

.input-wrapper {
  position: relative;
  display: flex;
  align-items: center;
}

.input-wrapper .form-input {
  inline-size: 100%;
  padding-inline-end: 2.5rem;
}

/* Error state: high-contrast red border 3.6:1 against background */
.form-field--error .form-input {
  border-color: #b91c1c;
  outline-color: #b91c1c;
}

.error-icon {
  position: absolute;
  right: 0.75rem;
  color: #b91c1c;
  font-size: 1.125rem;
  pointer-events: none;
}

/* Error text: #991b1b on white #ffffff has a 5.8:1 contrast (AA compliant) */
.error-message {
  font-size: 0.8125rem;
  color: #991b1b;
  margin: 0;
  font-weight: 500;
}
```

### Example 3: Chart Data Independence (Textures & Patterns)
Data visualizations must never rely on slice color alone. In CSS and SVG, use distinct line dashes or SVG fill patterns:

```html
<svg class="chart-legend" viewBox="0 0 300 60" aria-label="Quarterly Revenue Trends">
  <!-- Pattern 1: Solid with square symbol -->
  <rect x="10" y="10" width="16" height="16" fill="#1e40af" />
  <text x="36" y="23" font-size="12" fill="#0f172a">Series A (Solid)</text>

  <!-- Pattern 2: Diagonal stripes with triangle symbol -->
  <rect x="150" y="10" width="16" height="16" fill="url(#stripes)" stroke="#0f172a" />
  <text x="176" y="23" font-size="12" fill="#0f172a">Series B (Striped)</text>

  <defs>
    <pattern id="stripes" width="4" height="4" patternUnits="userSpaceOnUse" patternTransform="rotate(45)">
      <line x1="0" y1="0" x2="0" y2="4" stroke="#047857" stroke-width="2" />
    </pattern>
  </defs>
</svg>
```

### Example 4: Ensuring Non-Text UI Contrast (Switch Toggle)
```css
.switch {
  --switch-width: 48px;
  --switch-height: 26px;
  position: relative;
  display: inline-block;
  width: var(--switch-width);
  height: var(--switch-height);
}

.switch input {
  opacity: 0;
  width: 0;
  height: 0;
}

.switch-slider {
  position: absolute;
  inset: 0;
  background-color: #cbd5e1; /* Track background */
  /* Border guarantees 3:1 contrast against adjacent white card backgrounds */
  border: 2px solid #475569;
  border-radius: 9999px;
  cursor: pointer;
  transition: 0.2s;
}

.switch-slider::before {
  position: absolute;
  content: "";
  height: 16px;
  width: 16px;
  left: 3px;
  bottom: 3px;
  background-color: #0f172a;
  border-radius: 50%;
  transition: 0.2s;
}

/* Checked state */
.switch input:checked + .switch-slider {
  background-color: #0284c7;
  border-color: #0369a1;
}

.switch input:checked + .switch-slider::before {
  transform: translateX(22px);
  background-color: #ffffff;
}

.switch input:focus-visible + .switch-slider {
  outline: 2px solid #0284c7;
  outline-offset: 3px;
}
```

### Example 5: High-Contrast Mode Border Guard
```css
/* Decorative borders vanish in Windows High Contrast Mode unless declared with 1px border */
.glassmorphism-card {
  background: rgba(255, 255, 255, 0.8);
  box-shadow: 0 4px 6px rgba(0, 0, 0, 0.1);
  /* The transparent border is invisible in standard mode but rendered as a high-contrast boundary in HCM */
  border: 1px solid transparent;
  border-radius: 12px;
}
```

---

## 6. Best Practices

1. **Verify Body Text Exceeds 4.5:1**: Ensure standard paragraphs, list items, and article copy achieve at least 4.5:1 relative luminance contrast against their parent background.
2. **Never Omit Underlines on Inline Links**: Links within text blocks should always feature a text underline; if underlines are removed aesthetically, links must achieve a 3:1 contrast ratio against the surrounding body text AND provide a non-color hover/focus signifier.
3. **Audit Input Borders for 3:1 Contrast**: Input fields, select dropdowns, and checkboxes must have borders with at least 3:1 contrast against the page background to satisfy WCAG 2.2 SC 1.4.11.
4. **Always Pair Status Colors with Icons or Text**: Every notification, toast, badge, or error indicator must provide an icon, badge text, or geometric symbol in addition to color styling.
5. **Enforce Contrast Verification in CI**: Integrate automated tools such as `@axe-core/cli` or Lighthouse in your deployment pipeline to catch contrast regressions before merging.
6. **Test with Color-Blindness Emulation**: Use Chrome DevTools $\rightarrow$ Rendering tab $\rightarrow$ "Emulate vision deficiencies" (Protanopia, Deuteranopia, Tritanopia, Achromatopsia) to verify information legibility.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Consequence | Correct Pattern |
| :--- | :--- | :--- |
| **Using placeholder text as a form label** | Placeholders are low-contrast and vanish on typing; severe accessibility failure. | Use explicit `<label>` tags with high contrast; keep placeholders purely as format hints. |
| **Using `#888` or `#999` for secondary text** | Fails the 4.5:1 AA contrast ratio on white backgrounds. | Use `#595959` or `#4b5563` (which achieve $\ge 4.5:1$ on `#ffffff`). |
| **Highlighting active navigation tabs with color alone** | Color-blind users cannot identify which tab is selected. | Add a thick border-bottom, bold font-weight, and `aria-selected="true"`. |
| **Relying on low-opacity text (`opacity: 0.6`)** | Opacity blends with background, unpredictably degrading contrast below 4.5:1. | Define explicit, pre-calculated hex or OKLCH color tokens with verified contrast. |
| **Colored links without underlines or 3:1 contrast to body** | Users cannot distinguish links from normal text without hovering with a mouse. | Keep `text-decoration: underline` on inline links. |
| **Ignoring non-text contrast on graphical chart legends** | Pie charts and bar graphs become unreadable for color-blind users. | Add pattern fills, distinct border strokes, or direct data labels. |

---

## 8. Performance, Accessibility & Browser Support

### Tools for Automated Verification
- **Chrome DevTools Inspect Element**: Color picker displays immediate WCAG AA / AAA checkmarks and contrast ratio values.
- **Lighthouse Accessibility Audit**: Flags all elements failing the 4.5:1 (text) and 3:1 (non-text) criteria.
- **The Colour Contrast Analyser (CCA)**: Standalone desktop eye-dropper tool for measuring native OS contrast.

---

## 9. Comparison / When to Use What

| Standard / Algorithm | Scope | Threshold | Best Suited For |
| :--- | :--- | :--- | :--- |
| **WCAG 2.2 AA (Normal Text)** | Text < 18pt / 14pt bold | **4.5 : 1** | Standard web production requirement |
| **WCAG 2.2 AA (Large Text)** | Text $\ge$ 18pt / 14pt bold | **3.0 : 1** | Hero headings, large banner typography |
| **WCAG 2.2 AA (Non-Text)** | UI controls, icons, borders | **3.0 : 1** | Form inputs, buttons, sliders, icons |
| **WCAG 2.2 AAA (Enhanced)** | Enhanced readability | **7.0 : 1** | Medical, government, and legal portals |
| **WCAG 3.0 APCA** | Perceptual spatial lightness | Variable ($L^c 60-90$) | Modern design token algorithms |

---

## 10. Interview Questions

### Q1: What are the exact mathematical contrast ratio requirements for text and non-text elements under WCAG 2.2 Level AA?
<details>
<summary>Answer</summary>
Under WCAG 2.2 Level AA:
1. **Normal Text** (under 18pt regular or under 14pt bold): Must achieve a relative luminance contrast ratio of at least **4.5 : 1** against its background (SC 1.4.3).
2. **Large Text** (at least 18pt regular or 14pt bold): Must achieve a contrast ratio of at least **3.0 : 1** (SC 1.4.3).
3. **Non-Text Elements** (visual information required to identify UI components, active states, and graphical objects like icons and form borders): Must achieve a contrast ratio of at least **3.0 : 1** against adjacent colors (SC 1.4.11).
</details>

### Q2: What is WCAG Success Criterion 1.4.1 (Use of Color), and how do you achieve compliance in form error handling?
<details>
<summary>Answer</summary>
SC 1.4.1 mandates that color is not used as the only visual means of conveying information, indicating an action, prompting a response, or distinguishing a visual element.

In form validation, simply turning the input border red violates SC 1.4.1 because users with color blindness (e.g. protanopia) may perceive the red border as identical to a standard gray or brown border. To comply:
1. Provide a visible textual error message beneath the input (e.g. "Email is required").
2. Include an error icon (e.g. an exclamation mark symbol).
3. Connect the message programmatically using `aria-invalid="true"` and `aria-describedby="error-id"`.
4. Style the focus ring and border with high contrast.
</details>

### Q3: Why does pure blue (`#0000ff`) on a black background have poor contrast, whereas pure green (`#00ff00`) on black has exceptional contrast?
<details>
<summary>Answer</summary>
This occurs because the WCAG Relative Luminance formula ($L = 0.2126 R + 0.7152 G + 0.0722 B$) models the human eye's spectral sensitivity. The human retina contains vastly more cones sensitive to green light than to blue light.

- Pure blue (`#0000ff`) has a relative luminance of only $0.0722$. Against black ($0.0$), the contrast ratio is $\frac{0.0722 + 0.05}{0.0 + 0.05} = 2.44 : 1$, failing the WCAG 4.5:1 minimum threshold.
- Pure green (`#00ff00`) has a relative luminance of $0.7152$. Against black, the contrast ratio is $\frac{0.7152 + 0.05}{0.0 + 0.05} = 15.3 : 1$, easily passing WCAG Level AAA.
</details>

### Q4: How does the proposed WCAG 3.0 APCA (Advanced Perceptual Contrast Algorithm) differ from the WCAG 2 contrast formula?
<details>
<summary>Answer</summary>
1. **Perceptual Non-Linearity**: WCAG 2 uses a simple mathematical ratio of relative luminance. APCA models how human photoreceptors and the visual cortex process light, taking into account spatial frequency (font size and stroke width).
2. **Font Weight Integration**: In APCA, thin fonts require significantly higher contrast values than heavy/bold fonts to achieve readability.
3. **Polarity Sensitivity**: WCAG 2 treats black text on white and white text on black as mathematically identical ($21:1$). APCA accounts for the "Halation Effect"—white text on dark backgrounds causes optical flare in the human eye, meaning dark mode requires different perceptual contrast tuning than light mode.
</details>

### Q5: If a designer insists on removing underlines from links inside a paragraph of body text, what secondary criteria must be satisfied to remain WCAG compliant?
<details>
<summary>Answer</summary>
According to W3C WCAG technique G183, if links are not underlined by default:
1. The link text must have a **3:1 contrast ratio** against the surrounding non-link body text.
2. The link text must still achieve a **4.5:1 contrast ratio** against the page background.
3. The link must provide an additional visual indicator (such as an underline, border, or background color change) **on hover and on keyboard focus**.
Because finding a color that simultaneously achieves 3:1 against surrounding dark text and 4.5:1 against the white background is mathematically difficult, keeping the underline is universally recommended.
</details>

### Q6: How should disabled buttons be styled from an accessibility and contrast perspective?
<details>
<summary>Answer</summary>
Under WCAG SC 1.4.3, inactive/disabled components are technically exempt from contrast requirements. However, from a UX perspective:
1. If disabled buttons are made too low contrast, users with low vision cannot read what the button does or perceive that an action exists.
2. If disabled buttons are given high contrast, users confuse them with active, clickable buttons.
The best architectural practice is to avoid disabled buttons where possible. Keep buttons active and operable; if clicked when invalid, trigger accessible error messaging that informs the user *why* the form cannot be submitted and *which* fields require correction.
</details>

### Q7: What is the purpose of adding `border: 1px solid transparent` on buttons styled with subtle box shadows?
<details>
<summary>Answer</summary>
In Windows High Contrast Mode (`forced-colors: active`), the operating system strips all CSS `box-shadow` declarations and background colors to enforce high contrast.
If a button relies entirely on `box-shadow` or subtle background gradient for its visual boundary, the button becomes invisible against the canvas in High Contrast Mode. Adding `border: 1px solid transparent` is invisible in normal browsing, but in High Contrast Mode, the browser automatically converts `transparent` into a visible high-contrast system border (`ButtonBorder`), preserving the button's boundary.
</details>

### Q8: How can CSS Custom Properties be used to build a design system that guarantees contrast compliance?
<details>
<summary>Answer</summary>
By pairing background tokens with dedicated, pre-calculated text tokens:
```css
:root {
  --surface-primary: #ffffff;
  --text-on-primary: #0f172a; /* 16:1 contrast */

  --surface-brand: #1d4ed8;
  --text-on-brand: #ffffff;   /* 5.2:1 contrast */
}
```
Components must never mix and match arbitrary text and surface tokens; they must strictly consume paired tokens (e.g. `.card { background: var(--surface-brand); color: var(--text-on-brand); }`), guaranteeing that any theme permutation maintains certified WCAG AA/AAA compliance by construction.
</details>

---

## 11. Practice Exercises

### Exercise 1: Accessible Link Styling in Content Typography
**Objective**: Style inline links within a blog post to guarantee 4.5:1 contrast on white, clear underline aesthetics, and high-contrast hover/focus states.

<details>
<summary>Solution</summary>

```css
.article-body a {
  /* Blue-700 (#1d4ed8) on white (#ffffff) has 5.2:1 contrast (AA compliant) */
  color: #1d4ed8;
  text-decoration: underline;
  text-decoration-color: #93c5fd;
  text-decoration-thickness: 2px;
  text-underline-offset: 3px;
  transition: color 0.15s ease, text-decoration-color 0.15s ease;
}

.article-body a:hover {
  color: #1e40af;
  text-decoration-color: #1e40af;
}

.article-body a:focus-visible {
  outline: 2px solid #1d4ed8;
  outline-offset: 3px;
  border-radius: 2px;
  background-color: #eff6ff;
}
```
</details>

### Exercise 2: Accessible Toast Notification System
**Objective**: Build a multi-type toast notification component (Info, Warning, Error) ensuring WCAG AA contrast and color independence with icons and accessible labels.

<details>
<summary>Solution</summary>

```html
<div class="toast-container" role="region" aria-label="Notifications">
  <div class="toast toast--warning" role="alert">
    <span class="toast__icon" aria-hidden="true">⚠</span>
    <div class="toast__body">
      <strong class="toast__title">Storage Warning:</strong>
      <span>Your project has reached 85% of allocated disk quota.</span>
    </div>
  </div>
</div>
```

```css
.toast-container {
  position: fixed;
  bottom: 1.5rem;
  right: 1.5rem;
  display: flex;
  flex-direction: column;
  gap: 0.75rem;
  z-index: 1000;
}

.toast {
  display: flex;
  align-items: flex-start;
  gap: 0.75rem;
  padding: 1rem 1.25rem;
  border-radius: 0.5rem;
  max-width: 400px;
  box-shadow: 0 4px 6px -1px rgba(0, 0, 0, 0.1);
  border: 1px solid transparent;
}

.toast__icon {
  font-size: 1.25rem;
  line-height: 1;
}

.toast__title {
  display: block;
  margin-bottom: 0.25rem;
}

/* Warning: Dark brown/amber #78350f on amber-100 #fef3c7 = 8.1:1 contrast (AAA compliant) */
.toast--warning {
  background-color: #fef3c7;
  color: #78350f;
  border-color: #fde68a;
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **WCAG AA Minimum Contrast**:
  - Normal text: **4.5 : 1**
  - Large text ($\ge 18pt$ or $\ge 14pt$ bold): **3.0 : 1**
  - UI components and graphical icons: **3.0 : 1**
- **WCAG AAA Enhanced Contrast**:
  - Normal text: **7.0 : 1**
  - Large text: **4.5 : 1**
- **Color Independence (SC 1.4.1)**: Never rely on color alone to communicate errors, states, or selections. Always combine with text labels, icons, or underlines.
- **Link Underlines**: Keep underlines on inline links. If removing them, links must achieve 3:1 contrast against surrounding body text AND provide non-color hover/focus indicators.
- **Form Controls**: Input borders must achieve at least 3:1 contrast against adjacent backgrounds.
- **High-Contrast Fallbacks**: Use `border: 1px solid transparent` so subtle box shadows become visible borders in Windows High Contrast Mode.

---

## 13. Further Reading
- [W3C WCAG 2.2: Understanding Success Criterion 1.4.3 Contrast (Minimum)](https://www.w3.org/WAI/WCAG22/Understanding/contrast-minimum.html)
- [W3C WCAG 2.2: Understanding Success Criterion 1.4.11 Non-text Contrast](https://www.w3.org/WAI/WCAG22/Understanding/non-text-contrast.html)
- [W3C WCAG 2.2: Understanding Success Criterion 1.4.1 Use of Color](https://www.w3.org/WAI/WCAG22/Understanding/use-of-color.html)
- [APCA Contrast Calculator & Guidelines](https://www.myndex.com/APCA/)
- [WebAIM: Color Contrast Checker](https://webaim.org/resources/contrastchecker/)

---

| Previous Note | Next Note |
| :--- | :--- |
| [01. Focus Visible and Focus Styles](01-focus-visible-and-focus-styles.md) | [03. Prefers Color Scheme, Contrast and Reduced Data](03-prefers-color-scheme-contrast-and-reduced-data.md) |
