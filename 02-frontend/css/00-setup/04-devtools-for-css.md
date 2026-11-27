# DevTools for CSS

| Metadata | Details |
|:---|:---|
| **Level** | Foundational |
| **Prerequisites** | `00-setup/02-how-the-browser-renders-css.md`, `00-setup/03-ways-to-add-css.md` |
| **Estimated Reading Time** | 30 minutes |
| **Status** | Current |
| **Baseline Status** | Widely Available (Universal across Chrome, Firefox, Safari, Edge) |
| **Browsers Tested** | Chrome DevTools 120+, Firefox DevTools 121+, Safari Web Inspector 17.2+ |
| **Tested With** | Chrome DevTools Protocol (CDP), stylelint, Playwright |

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

### What are CSS DevTools?
**CSS DevTools** are diagnostic and profiling instruments built directly into modern browser rendering engines (Chromium DevTools, Firefox Developer Tools, and Safari Web Inspector). They allow engineers to inspect the live **CSSOM**, observe the **Cascade** resolution in real time, analyze box-model geometry, debug grid and flexbox layout algorithms, simulate responsive viewports, and audit accessibility contrast ratios.

### Why are DevTools Essential for CSS Engineers?
Writing CSS without DevTools is like debugging code without a stack trace. In static source code, you see declarations; in DevTools, you see:
1. **The Winning Declarations**: Exactly which rule beat others in the cascade and why (strikethrough indicates overridden declarations).
2. **Computed vs Specified Values**: How relative values (like `1.5rem`, `clamp()`, or `100vw`) resolve into absolute screen pixels (`px`).
3. **Invisible Layout Geometry**: Interactive visual overlays showing grid lines, tracks, gaps, flex containers, and margins.
4. **Hardware Compositing & Paints**: Real-time layer boundaries and paint flashing, identifying CPU bottlenecks.
5. **Accessibility Simulation**: Vision impairment filters, forced-color themes, and color-contrast compliance scores.

---

## 2. Core Concepts

### 1. Elements Panel: Styles vs. Computed Pane
When an HTML element is selected in the DOM tree, DevTools provides two primary styling panes:
- **Styles Pane**: Displays all CSS rules that match the selected element, ordered by cascade priority (highest at the top: inline styles, followed by matching rules sorted by specificity and layer order). Overridden or invalid properties are crossed out with a strikethrough line (`line-through`).
- **Computed Pane**: Displays the final, absolute computed values after the browser has evaluated all cascade rules, inheritance, and unit conversions (e.g., converting `rem` to `px`, or `var(--primary)` to `#0284c7`). Checking "Show all" displays default User Agent styles.

### 2. Interactive Box Model Diagram
Positioned at the top or bottom of the Styles/Computed pane, this interactive diagram represents the four nested boxes of CSS layout:
```
+---------------------------------------------------+
| MARGIN  (Orange / Peach)                          |
|   +-------------------------------------------+   |
|   | BORDER  (Yellow)                          |   |
|   |   +-----------------------------------+   |   |
|   |   | PADDING  (Purple / Green)         |   |   |
|   |   |   +---------------------------+   |   |   |
|   |   |   | CONTENT (Blue)            |   |   |   |
|   |   |   | width x height            |   |   |   |
|   |   |   +---------------------------+   |   |   |
|   |   +-----------------------------------+   |   |
|   +-------------------------------------------+   |
+---------------------------------------------------+
```
Hovering over each box highlights the corresponding spatial region in the browser viewport. Double-clicking any number allows instantaneous live editing of margins, borders, or padding.

### 3. Layout Overlays (CSS Grid & Flexbox Badges)
Modern DevTools display interactive badges (`grid`, `flex`, `subgrid`) next to elements in the DOM tree:
- Clicking the `grid` badge activates a colored persistent overlay in the viewport displaying grid lines, track numbers, area names, and gap zones.
- Clicking the `flex` badge highlights flex items, cross-axis alignment, and distribution lines.

### 4. Forcing Pseudo-Classes (`:hov`, `:act`, `:foc`, `:vis`)
Hovering a mouse over an element while trying to inspect its styles in DevTools is clumsy. The **Toggle Element State** panel allows forcing interactive pseudo-classes:
- `:hover`
- `:active`
- `:focus`
- `:focus-visible`
- `:focus-within`
- `:target`

### 5. The Rendering Drawer (Emulation & Diagnostics)
Accessible via `Esc` &rarr; More Tools &rarr; **Rendering**:
- **Emulate CSS Media**: Test `@media (prefers-color-scheme: dark)`, `@media (prefers-reduced-motion: reduce)`, and `@media (forced-colors: active)`.
- **Emulate Vision Deficiencies**: Simulate Protanopia (red-blind), Deuteranopia (green-blind), Tritanopia, and Blurred Vision.
- **Paint Flashing**: Highlights screen areas in green whenever they are repainted by the CPU.
- **Layout Shift Regions**: Highlights elements in blue when they trigger Cumulative Layout Shift (CLS).

### 6. CSS Coverage Panel
Identifies unused CSS rules across the entire application:
- Displays total CSS bytes downloaded vs. percentage actually executed/rendered on the current page.
- Highlights unexecuted selectors in red directly in the Sources pane.

---

## 3. How It Works Internally

### The Chrome DevTools Protocol (CDP) Styling Agent
DevTools communicates with the browser engine's core via structured IPC (Inter-Process Communication) commands.

```
+--------------------------------------------------------------------+
| DEVTOOLS FRONTEND (UI)                                             |
| Styles Pane | Computed Pane | Box Model | Layout Inspector         |
+--------------------------------------------------------------------+
                                |  ^
    CDP: CSS.getMatchedStylesForNode()  |  | CDP: CSS.styleSheetChanged
                                v  |
+--------------------------------------------------------------------+
| BROWSER RENDERING ENGINE (Blink / Gecko / WebKit Core)             |
|                                                                    |
|  DOM Tree  ---------> Matcher Engine ---------> Matched Rules     |
|                              |                         |           |
|                              v                         v           |
|                         Style Resolver ---------> ComputedStyle    |
+--------------------------------------------------------------------+
```

1. **`CSS.getMatchedStylesForNode(nodeId)`**: Returns the full cascade list:
   - Inline styles (`style` attribute).
   - Matched CSS rules (with stylesheet ID, selector list, specificity, and line numbers).
   - Cascade Layer information.
   - Inherited styles from parent nodes.
   - Pseudo-elements (`::before`, `::after`).
2. **`CSS.getComputedStyleForNode(nodeId)`**: Returns an array of canonical key-value pairs representing the final computed pixel dimensions and RGBA colors directly from the engine's internal `ComputedStyle` memory buffer.
3. **Live Mutations (`CSS.setPropertyText`)**: When you edit a value in DevTools, the engine updates the in-memory stylesheet, marks the node's styling as "dirty", and schedules a microtask to recalculate styles and re-layout the DOM tree without reloading the page.

---

## 4. Syntax & Usage

```
+-------------------------------------------------------------------------+
| Browser DevTools Feature Parity Matrix                                   |
| - Chrome / Edge: Industry standard CDP, CSS Coverage, Layers, Rendering |
| - Firefox: Superior CSS Grid & Shape Inspector, font variation editor   |
| - Safari: Excellent CSS Container Queries debugging, visual timelines   |
+-------------------------------------------------------------------------+
```

### Keyboard Shortcuts for Instant CSS Debugging

| Shortcut (Windows / Linux) | Shortcut (macOS) | Action |
|:---|:---|:---|
| `F12` or `Ctrl + Shift + C` | `Cmd + Option + C` | Inspect element directly under cursor |
| `Ctrl + Shift + P` | `Cmd + Shift + P` | Open DevTools Command Palette |
| `Esc` | `Esc` | Toggle secondary bottom drawer (Console, Rendering, Coverage) |
| `Up` / `Down` Arrow | `Up` / `Down` Arrow | Increment / decrement numeric CSS values by `1` |
| `Shift + Up` / `Down` | `Shift + Up` / `Down` | Increment / decrement numeric CSS values by `10` |
| `Alt + Up` / `Down` | `Option + Up` / `Down` | Increment / decrement numeric CSS values by `0.1` |

---

## 5. Practical Examples

### Example 1: Inspecting Overridden Declarations & Specificity
Consider this HTML and CSS:
```html
<button class="btn btn-primary" id="save-btn">Save</button>
```
```css
/* Rule A: Specificity [0, 0, 1] */
button {
  background-color: #64748b;
  padding: 8px 16px;
}

/* Rule B: Specificity [0, 1, 0] */
.btn-primary {
  background-color: #0284c7;
}

/* Rule C: Specificity [1, 0, 0] */
#save-btn {
  background-color: #10b981;
}
```

*DevTools Inspection View in Styles Pane*:
```
#save-btn {                     /* Wins! Specificity [1, 0, 0] */
  background-color: #10b981;
}

.btn-primary {
  <s>background-color: #0284c7;</s> /* Strikethrough: Overridden by #save-btn */
}

button {
  <s>background-color: #64748b;</s> /* Strikethrough: Overridden */
  padding: 8px 16px;            /* Active: Inherited layout */
}
```

### Example 2: Interactive Grid Line Debugging
```html
<div class="grid-layout">
  <div class="sidebar">Sidebar</div>
  <div class="content">Main Content</div>
</div>
```
```css
.grid-layout {
  display: grid;
  grid-template-columns: 240px 1fr;
  grid-template-rows: auto;
  gap: 24px;
}
```
*DevTools Action*:
1. Select `.grid-layout` in the Elements panel.
2. Click the `grid` badge next to the tag or check the checkbox in the **Layout** pane.
3. DevTools paints striped overlays:
   - Line `[1]`, `[2]`, `[3]` markers appear above columns.
   - The 24px gutter is highlighted with a violet diagonal hatched pattern.
   - Track size `240px` and flexible track `1fr` calculations are displayed live.

### Example 3: Edge Case — Debugging Margin Collapsing
A classic CSS puzzle where two vertical margins do not sum together:
```html
<div class="box-top">Top Box</div>
<div class="box-bottom">Bottom Box</div>
```
```css
.box-top {
  margin-bottom: 30px;
  background: #fef08a;
}
.box-bottom {
  margin-top: 20px;
  background: #bae6fd;
}
```
*DevTools Verification*:
1. In the Computed pane or Box Model viewer, hover over `.box-top`. You see an orange margin extending 30px below.
2. Hover over `.box-bottom`. Its 20px orange margin overlaps inside the 30px space.
3. The total measured distance between boxes in the viewport is **30px** (the maximum of the two), proving margin collapse directly in the Box Model diagram.

### Example 4: Diagnosing Expensive Layout Reflows (Paint Flashing)
```css
/* Bad practice: Animating margin triggers layout every frame */
.bouncing-ball {
  animation: bounce 1s infinite alternate;
}

@keyframes bounce {
  from { margin-top: 0px; }
  to { margin-top: 100px; }
}
```
*DevTools Diagnostic*:
1. Open DevTools &rarr; `Esc` &rarr; **Rendering** panel.
2. Check **Paint flashing**.
3. *Symptom*: A bright green rectangle flashes violently over the entire parent container 60 times a second, proving the CPU is rasterizing pixels repeatedly.
4. *Fix*: Change `margin-top` to `transform: translateY(100px)`.
5. *Result*: Paint flashing vanishes completely! The animation now runs smoothly on the GPU compositor thread.

### Example 5: Simulating Accessibility Modes (Dark Scheme & Contrast)
```css
:root {
  --bg-color: #ffffff;
  --text-color: #0f172a;
}

@media (prefers-color-scheme: dark) {
  :root {
    --bg-color: #0f172a;
    --text-color: #f8fafc;
  }
}

@media (forced-colors: active) {
  .badge {
    border: 1px solid CanvasText;
  }
}
```
*DevTools Verification Workflow*:
1. Open the **Rendering** drawer.
2. Under "Emulate CSS media feature prefers-color-scheme", select **prefers-color-scheme: dark**.
3. The Elements Styles pane instantly activates the `@media (prefers-color-scheme: dark)` block and strikes through the default light variables.
4. Verify computed color with Playwright CDP session:
```javascript
// Verified via DevTools Protocol test:
const computed = window.getComputedStyle(document.documentElement);
console.log('Background:', computed.getPropertyValue('--bg-color')); // #0f172a
```

---

## 6. Best Practices

1. **Always Check the Computed Pane for Real Layout Values**: The Styles pane shows author intentions (`100%`, `auto`, `2rem`), but the Computed pane shows reality (`842.5px`).
2. **Use the Layout Tab for Complex Grid and Flex Debugging**: Do not guess track widths or alignment issues; enable Grid overlays and Track Size labels.
3. **Audit Unused CSS via the Coverage Tab**: Before shipping a production release, run the Coverage tool to identify dead legacy rules bloating bundle size.
4. **Test Dark Mode & Reduced Motion via Rendering Emulation**: Avoid manually altering OS-level settings during development; toggle them instantly inside DevTools.
5. **Use Color Contrast Droppers in DevTools**: Clicking any color swatch in the Styles pane opens an interactive picker showing WCAG AA and AAA contrast ratio thresholds against the background.
6. **Leverage Local Overrides for Rapid Prototyping**: In Chrome DevTools Sources &rarr; Overrides, persist your live CSS tweaks to local disk files without switching back and forth to your code editor.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Root Cause | Symptom / Failure | Correct Pattern |
|:---|:---|:---|:---|
| Trusting crossed-out rules without reading warning icons | Ignoring browser error markers | Assuming CSS is working when an invalid property value caused a parse rejection. | Look for yellow triangle warning icons next to invalid property names. |
| Inspecting hover states by moving the mouse | Hover state disconnects when moving cursor to DevTools | Cannot inspect `:hover` or tooltip styles because mouse left the element. | Force state using `:hov` &rarr; `:hover` in the Styles panel. |
| Confusing `auto` margins with padding | Misreading the Box Model diagram | Layout spacing looks wrong; element fails to center. | Orange represents margin; green represents padding in DevTools overlay. |
| Forgetting CSS Layer Precedence | Inspecting specificity without checking `@layer` | A selector with `0-1-0` beats `1-0-0` because it resides in a higher unlayered block. | Inspect the `@layer` hierarchy at the top of each rule block in Styles pane. |
| Editing styles in production without Local Overrides | Refreshing page loses all DevTools edits | 30 minutes of careful CSS debugging wiped out on accidental page reload. | Enable "Local Overrides" or copy CSS changes from the "Changes" panel. |
| Overlooking stacking context boundaries | Attempting to fix `z-index` by raising numbers to `999999` | Element remains buried beneath sibling containers. | Check the **Layers** panel to inspect stacking contexts and composite layers. |

---

## 8. Performance, Accessibility & Browser Support

### Rendering & Layer Inspection
Modern DevTools include a dedicated **Layers** tool (DevTools &rarr; More Tools &rarr; Layers):
- Renders a 3D interactive view of GPU composited layers.
- Clicking a layer displays the exact reason Blink promoted it (e.g., `will-change: transform`, `transform: translateZ(0)`, or overlapping another composited layer).
- Helps engineers prevent "layer explosion" where excessive layers exhaust GPU video memory (VRAM).

### Accessibility Auditing Built into DevTools
- **Contrast Checker**: Inspecting any text element displays contrast scores (e.g., `12.4` with two green checkmarks for WCAG AA & AAA).
- **Accessibility Tree**: Toggling the accessibility icon in the DOM tree switches from the raw DOM to the browser's internal Accessibility Object Model (AOM), revealing accessible names and ARIA roles.

---

## 9. Comparison / When to Use What

| Feature | Primary Use Case | When NOT to Use |
|:---|:---|:---|
| **Styles Pane** | Modifying declarations, testing selectors, checking cascade order | Measuring precise layout pixel boundaries |
| **Computed Pane** | Verifying resolved `px` dimensions, checking inherited font sizes | Writing new selectors or experimenting with styling |
| **Layout Tab** | Visualizing Grid tracks, gaps, Flex items, and line names | Simple inline text styling or color inspection |
| **Rendering Drawer** | Emulating dark mode, forced colors, reduced motion, vision loss | Inspecting single DOM element properties |
| **Layers Panel** | Diagnosing GPU compositing bottlenecks, 3D layer explosions | General styling and typography adjustments |
| **Coverage Panel** | Eliminating dead CSS across multi-page production apps | Real-time debugging of interactive hover states |

---

## 10. Interview Questions

<details>
<summary><strong>1. In Chrome DevTools Styles pane, what does a strikethrough property with an info icon mean versus a strikethrough property without one?</strong></summary>

A strikethrough property **without** an icon means the property was syntactically valid but was **overridden by another rule** higher in the cascade (e.g., higher specificity, later source order, or an inline style). A strikethrough property **with a yellow warning triangle** means the property or value is **invalid or unsupported** by the browser's CSS parser (e.g., a typo like `dispaly: flex;` or an experimental property missing required syntax).
</details>

<details>
<summary><strong>2. How does DevTools display Cascade Layers (`@layer`), and how do they alter visual rule ordering?</strong></summary>

DevTools displays the name of the containing `@layer` directly in the rule header (e.g., `@layer components`). In the Styles pane, rules are grouped and ordered according to layer precedence: unlayered styles appear at the top (highest priority), followed by layers in descending order of precedence. Even if a selector inside `@layer reset` has higher specificity (e.g., `#nav`), DevTools will strike it through if a lower-specificity selector in `@layer components` overrides it.
</details>

<details>
<summary><strong>3. How do you inspect a tooltip or dropdown menu that disappears as soon as you move your mouse away from the element?</strong></summary>

There are three primary techniques:
1. Select the trigger element and check the `:hover` checkbox in DevTools `:hov` panel.
2. In the Chrome DevTools Console, execute `setTimeout(() => { debugger; }, 3000);`, trigger the menu with your mouse, and wait 3 seconds. The JavaScript debugger will pause the entire page execution, freezing the DOM in place so you can inspect without triggering blur or mouseout.
3. In the Elements panel, right-click the parent node and select **Break on &rarr; Subtree modifications**.
</details>

<details>
<summary><strong>4. What is the difference between inspecting a CSS custom property in the Styles pane versus the Computed pane?</strong></summary>

In the Styles pane, you see the author declaration (e.g., `--brand-hue: 210;` or `color: var(--brand-hue);`). Hovering over the `var()` token displays a tooltip previewing the resolved value. In the Computed pane, custom properties are listed alphabetically (or filtered via search), showing the exact calculated value inherited by that specific node after evaluating the DOM tree hierarchy.
</details>

<details>
<summary><strong>5. How can you identify if an element creates a new Stacking Context using DevTools?</strong></summary>

In Chrome DevTools, open the **Layers** panel (via More Tools &rarr; Layers). Elements that create independent composited stacking contexts are rendered as distinct 3D planes. Alternatively, inspect the element in the Styles pane; Chrome DevTools displays a small tooltip next to properties like `opacity: 0.99`, `isolation: isolate`, or `z-index: 1` indicating: *"Creates a stacking context"*.
</details>

<details>
<summary><strong>6. Explain how to use the DevTools Rendering drawer to debug Cumulative Layout Shift (CLS).</strong></summary>

Open DevTools, press `Esc` to open the drawer, navigate to the **Rendering** tab, and enable **Layout Shift Regions**. As you navigate, scroll, or interact with the webpage, any DOM element that abruptly shifts position without prior user input is instantly highlighted with a bright rectangular blue flash. The summary in the Performance panel further identifies the specific node and layout shift score.
</details>

<details>
<summary><strong>7. How does the Firefox CSS Grid Inspector differ from Chromium's implementation?</strong></summary>

Firefox's CSS Grid Inspector is widely considered an industry benchmark. Beyond displaying line numbers and track sizes, Firefox provides a mini interactive grid schematic in the Layout tab that highlights grid areas, lets you toggle line names vs line numbers independently, displays negative line numbers (`-1`, `-2`), and visualizes explicit vs implicit grid lines with distinct dashed stroke patterns.
</details>

<details>
<summary><strong>8. What does the "Rendered Fonts" section at the bottom of the Computed tab reveal?</strong></summary>

The "Rendered Fonts" section shows which font file was **actually rasterized on screen** by the operating system, rather than just what was listed in your `font-family` stack. If you declared `font-family: 'CustomFont', sans-serif;`, this panel reveals whether the browser successfully downloaded and used the network webfont, or if it fell back to a local system font (e.g., `Arial (Local file)`), helping identify font loading failures.
</details>

---

## 11. Practice Exercises

### Exercise 1: Live Interactive Value Adjustment (Easy)
**Task**: In any browser, inspect a paragraph `<p>`. Using only keyboard shortcuts in the Styles pane, increase its `line-height` by `0.2` and increase its `padding` by `20px` without typing the numbers manually.

<details>
<summary><strong>View Solution</strong></summary>

1. Select the `<p>` element in the DOM tree.
2. In the Styles pane, click the number inside `line-height: 1.2;`.
3. Press `Alt + Up Arrow` (macOS: `Option + Up Arrow`) twice to increment by `0.1` twice &rarr; `1.4`.
4. Click the number inside `padding: 10px;`.
5. Press `Shift + Up Arrow` twice to increment by `10px` twice &rarr; `30px`.
</details>

---

### Exercise 2: Freeze an Ephemeral Modal or Dropdown (Easy)
**Task**: Write the single-line JavaScript snippet in the DevTools Console to freeze the execution of a web page after a 2-second delay so you can inspect an ephemeral popover.

<details>
<summary><strong>View Solution</strong></summary>

```javascript
setTimeout(() => { debugger; }, 2000);
```
*Instructions*: Press Enter in Console, immediately open the popover or hover menu within 2 seconds. The debugger pauses execution, locking the DOM and hover states.
</details>

---

### Exercise 3: Identify Why an Element is Not Vertically Centered (Medium)
**Task**: An engineer writes `display: flex; justify-content: center; align-items: center;` on a `.parent` div, but the child button remains anchored at the top of the viewport. How do you diagnose this in DevTools within 10 seconds?

<details>
<summary><strong>View Solution</strong></summary>

1. Hover over `.parent` in the Elements panel or inspect its Computed pane.
2. Look at the computed `height`.
3. In 99% of cases, `.parent` has `height: auto`, which collapses to the height of the child button itself.
4. *Fix*: Add `min-height: 100vh;` or define an explicit container height.
</details>

---

### Exercise 4: Audit and Measure Unused CSS (Medium)
**Task**: Outline the exact steps to generate a CSS code coverage report in Google Chrome to determine what percentage of a vendor CSS bundle is dead code on page load.

<details>
<summary><strong>View Solution</strong></summary>

1. Open DevTools (`F12`).
2. Press `Ctrl + Shift + P` (macOS: `Cmd + Shift + P`) to open the Command Menu.
3. Type `Coverage` and select **Show Coverage**.
4. Click the reload icon (Start instrumenting coverage and reload page).
5. Review the table: locate the `.css` URL, inspect the **Unused Bytes** bar (red indicates unused rules; cyan indicates executed rules).
6. Double-click the file to inspect the red/green gutter annotations line-by-line.
</details>

---

### Exercise 5: Verify Contrast Accessibility Compliance (Hard)
**Task**: An interface uses text color `#94a3b8` on background `#f8fafc`. Use DevTools to check its contrast ratio, identify if it fails WCAG AA standards, and select an accessible replacement directly using the Color Picker.

<details>
<summary><strong>View Solution</strong></summary>

1. Inspect the text element in DevTools Elements pane.
2. In the Styles pane, click the color swatch square next to `color: #94a3b8;`.
3. The Color Picker opens. Look at the **Contrast ratio** line:
   - It displays `~2.8:1` with a red prohibition sign (fails WCAG AA 4.5:1 requirement).
4. Expand the contrast accordion in the picker. Two curve lines appear across the color palette (the top curve is AA 4.5:1, the bottom is AAA 7:1).
5. Click the eyedropper or click any color swatch beneath the AA line (e.g., `#64748b` giving `4.6:1`).
</details>

---

### Exercise 6: Trace Composited Layers & Paint Flashing (Hard)
**Task**: An infinite spinning loading icon is causing high CPU fan usage on mobile devices. How do you confirm whether the spinner is causing CPU repaints or GPU composited transforms?

<details>
<summary><strong>View Solution</strong></summary>

1. Open DevTools &rarr; `Esc` &rarr; **Rendering** panel &rarr; enable **Paint flashing**.
2. If the icon flashes green constantly, it is updating CPU layout/paint properties (like `top`, `left`, `margin`, or `box-shadow`).
3. Open More Tools &rarr; **Layers**.
4. Check if the loading spinner has its own composite layer. If not, refactor its animation:
```css
/* Refactor to GPU-only compositor animation */
.spinner {
  will-change: transform;
  animation: spin 1s linear infinite;
}

@keyframes spin {
  to { transform: rotate(360deg); }
}
```
5. With `transform`, paint flashing stops completely, and the Layers panel shows the spinner operating on its dedicated hardware texture.
</details>

---

## 12. Summary / Cheat Sheet

```
+-------------------------------------------------------------------------------------------------+
| CSS DEVTOOLS MASTER COMMANDS & SHORTCUTS                                                        |
+--------------------------+-------------------------------------+--------------------------------+
| Tool / Action            | Access / Path                       | Primary Purpose                |
+--------------------------+-------------------------------------+--------------------------------+
| Inspect Element          | Ctrl + Shift + C / Cmd + Opt + C   | Pick live element from canvas  |
| Command Palette          | Ctrl + Shift + P / Cmd + Shift + P | Run any internal tool command  |
| Toggle Drawer            | Esc                                 | Open Console, Rendering, Layers|
| Force Pseudo-Classes     | Styles pane -> :hov button          | Trigger :hover, :focus-visible |
| Grid & Flex Overlay      | DOM Tree -> [grid] / [flex] badge   | View tracks, lines, gaps       |
| Emulate Dark Mode        | Rendering Drawer -> prefers-color   | Test dark theme CSS            |
| Paint Flashing           | Rendering Drawer -> Paint Flashing  | Identify CPU repaints (green)  |
| CSS Code Coverage        | Command Palette -> "Show Coverage"  | Identify dead/unused CSS bytes |
| Stacking Contexts / GPU  | More Tools -> Layers                | Inspect 3D GPU composite layers|
+--------------------------+-------------------------------------+--------------------------------+
```

---

## 13. Further Reading

- [Chrome DevTools: CSS Features Reference](https://developer.chrome.com/docs/devtools/css/)
- [Firefox Developer Tools: Page Inspector & Grid](https://firefox-source-docs.mozilla.org/devtools-user/page_inspector/)
- [Safari Web Inspector: Elements Tab & Styles](https://developer.apple.com/documentation/safari-developer-tools)
- [web.dev: Inspect CSS Grid](https://web.dev/articles/learn-css-grid)
- [web.dev: Debug layout shifts](https://web.dev/articles/debug-layout-shifts)

---

[Previous: 03 - Ways to Add CSS](03-ways-to-add-css.md) | [Section Index](README.md) | [Next: 05 - Tooling: Stylelint, Prettier & Lightning CSS](05-tooling-stylelint-prettier-lightningcss.md)
