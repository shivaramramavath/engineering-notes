# 05. Zoom, Reflow and Text Spacing

| Metadata | Details |
| :--- | :--- |
| **Spec Level & Status** | WCAG 2.1 / 2.2 Level AA Standards (SC 1.4.10 Reflow & SC 1.4.12 Text Spacing) |
| **Browsers Tested** | Chrome 120+, Firefox 125+, Safari 17.4+ |
| **Tested With** | 400% Desktop Zoom (1280px $\rightarrow$ 320px viewport), Steve Faulkner's Text Spacing Bookmarklet |
| **Prerequisites** | `01-fundamentals/09-units.md`, `05-responsive-design/01-media-queries.md` |

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
Users with low vision frequently increase browser zoom up to **400%** or adjust operating system default font sizes to read digital content comfortably. Additionally, users with dyslexia and cognitive reading differences routinely inject custom user stylesheets that expand line height, letter spacing, and word spacing.

When web developers engineer rigid layouts utilizing fixed pixel widths (`width: 960px`), hardcoded container heights (`height: 300px`), or clipping overflow (`overflow: hidden`), two catastrophic failures occur:
1. **Two-Dimensional Scrolling (Reflow Failure)**: The page forces the user to scroll both vertically and horizontally across every line of text, increasing reading fatigue by orders of magnitude.
2. **Text Clipping & Truncation (Text Spacing Failure)**: Expanded text breaks out of fixed boxes or gets chopped off by `overflow: hidden`, causing critical instructions, buttons, and prices to vanish.

### The Solution
Two mandatory WCAG Level AA criteria govern responsive scaling and fluid text architecture:
- **WCAG SC 1.4.10 Reflow (Level AA)**: Content must reflow into a single column without requiring two-dimensional scrolling at a width equivalent to **320 CSS pixels** (which is exactly what a 1280px desktop screen becomes at 400% zoom).
- **WCAG SC 1.4.12 Text Spacing (Level AA)**: Interfaces must maintain full functionality and zero text truncation when users increase line height, paragraph spacing, letter spacing, and word spacing via user stylesheets.

---

## 2. Core Concepts

### 1. The 400% Zoom Reflow Threshold (SC 1.4.10)

```
STANDARD DESKTOP VIEWPORT (100% Zoom):
┌─────────────────────────────────────────────────────────────┐
│ 1280 CSS Pixels Width                                       │
│ Multi-column layout: Sidebar (280px) + Main Content (1000px)│
└─────────────────────────────────────────────────────────────┘
                               │
               User Zooms to 400% (Ctrl / Cmd +)
                               │
                               ▼
SCALED BROWSER VIEWPORT (400% Zoom):
┌───────────────────────────────┐
│ 320 CSS Pixels Width          │
│ All multi-column layouts MUST │
│ reflow into a single vertical │
│ column. Zero horizontal scroll│
│ permitted!                    │
└───────────────────────────────┘
```

- When a desktop user zooms in, the browser does not increase physical monitor resolution; it increases the CSS pixel scaling factor.
- At 400% zoom on a standard 1280px display, the browser viewport becomes exactly:
  $$\frac{1280\text{px}}{4.0} = 320\text{px}$$
- Therefore, **passing WCAG Reflow at 400% zoom is functionally identical to making your website render flawlessly on an iPhone 5 or narrow mobile screen (320px width)** without a horizontal scrollbar.

### 2. The Four Metrics of WCAG 1.4.12 (Text Spacing)
No loss of content or functionality can occur when a user stylesheet overrides the following four properties simultaneously:
1. **`line-height`**: Set to at least **$1.5\times$** the font size.
2. **`margin-bottom` on paragraphs**: Set to at least **$2\times$** the font size.
3. **`letter-spacing` (tracking)**: Set to at least **$0.12\times$** the font size.
4. **`word-spacing`**: Set to at least **$0.16\times$** the font size.

---

## 3. How It Works Internally

### Browser Zoom vs. Text-Only Zoom
Modern browsers support two distinct zooming modes:
1. **Page Zoom (Full Zoom - Default in Chrome, Edge, Safari, Firefox)**:
   - Scales the entire layout, effectively reducing the viewport's available CSS pixel dimensions ($1280\text{px} \rightarrow 320\text{px}$). Media queries (`@media (max-width: 600px)`) trigger automatically as zoom increases!
2. **Text-Only Zoom (Firefox "Zoom Text Only", Safari "Zoom Text")**:
   - Scales *only* the font size (`font-size`), leaving the viewport width constant.
   - If containers have fixed pixel heights (`height: 50px`), the enlarged text immediately overflows or clips. If containers use relative units (`min-height: auto`, `padding: 1rem`), containers expand gracefully.

---

## 4. Syntax & Usage

### 1. Relative Units for Resilient Layout Containers
Never use hardcoded pixel heights on elements containing text:

```css
/* ANTI-PATTERN: Breaks on zoom & text spacing */
.feature-card {
  width: 350px;
  height: 200px;
  overflow: hidden; /* Chopped text! */
}

/* PRODUCTION PATTERN: Fluid & Extensible */
.feature-card {
  inline-size: 100%;
  max-inline-size: 24rem; /* 384px in rem */
  min-block-size: min-content; /* Grows naturally with text */
  padding: 1.5rem;
  overflow: visible; /* Never clips text */
}
```

### 2. Single-Column Reflow at 320px
Use responsive CSS Grid or Flexbox with wrap to guarantee elements reflow before hitting 320px:

```css
.card-grid {
  display: grid;
  /* Automatically wraps down to a single column on 320px screens */
  grid-template-columns: repeat(auto-fit, minmax(min(100%, 280px), 1fr));
  gap: 1.5rem;
}
```

---

## 5. Practical Examples

### Example 1: Resilient Navigation Bar at 400% Zoom
Navigation bars with horizontal items inevitably cause horizontal scrolling at 400% zoom unless designed to wrap or collapse into a responsive menu:

```html
<header class="app-header">
  <a href="/" class="brand-logo">CloudMonitor</a>
  <nav class="nav-container" aria-label="Main Navigation">
    <ul class="nav-list">
      <li><a href="#dash">Dashboard</a></li>
      <li><a href="#alerts">Alerts</a></li>
      <li><a href="#metrics">Metrics</a></li>
      <li><a href="#billing">Billing</a></li>
    </ul>
  </nav>
</header>
```

```css
.app-header {
  display: flex;
  flex-wrap: wrap; /* Allows wrapping at 320px / 400% zoom */
  align-items: center;
  justify-content: space-between;
  padding: 1rem;
  background-color: #ffffff;
  border-bottom: 1px solid #e2e8f0;
  gap: 1rem;
}

.brand-logo {
  font-size: 1.25rem;
  font-weight: 700;
  color: #0f172a;
  text-decoration: none;
}

.nav-list {
  display: flex;
  flex-wrap: wrap; /* Items stack cleanly without horizontal scroll */
  gap: 0.75rem 1.25rem;
  list-style: none;
  padding: 0;
  margin: 0;
}

.nav-list a {
  color: #334155;
  text-decoration: none;
  font-weight: 500;
}

.nav-list a:hover {
  color: #0284c7;
}

.nav-list a:focus-visible {
  outline: 2px solid #0284c7;
  outline-offset: 3px;
}
```

### Example 2: Text Spacing Bookmarklet Resilience
Testing cards against WCAG 1.4.12 text spacing overrides:

```html
<div class="user-bio-card">
  <img src="/avatars/alex.webp" alt="Alex Rivera" class="bio-avatar">
  <div class="bio-content">
    <h3 class="bio-name">Alex Rivera</h3>
    <p class="bio-role">Lead Site Reliability Engineer</p>
    <p class="bio-description">
      Scaling Kubernetes clusters and automated CI/CD deployment pipelines across multi-cloud regions.
    </p>
  </div>
</div>
```

```css
.user-bio-card {
  display: flex;
  flex-direction: column;
  background: #ffffff;
  border: 1px solid #e2e8f0;
  border-radius: 0.75rem;
  padding: 1.5rem;
  gap: 1rem;
  max-inline-size: 400px;
}

.bio-avatar {
  inline-size: 4rem;
  block-size: 4rem;
  border-radius: 50%;
  object-fit: cover;
}

.bio-content {
  display: flex;
  flex-direction: column;
}

/* Ensure headings and paragraphs do not collapse on text-spacing overrides */
.bio-name {
  font-size: 1.25rem;
  font-weight: 700;
  margin-block-end: 0.25em;
}

.bio-role {
  font-size: 0.875rem;
  color: #64748b;
  margin-block-end: 0.75em;
}

.bio-description {
  /* Line-height must use unitless relative numbers */
  line-height: 1.5;
  color: #334155;
  margin: 0;
  /* Prevent word breaking from splitting single words unnaturally */
  overflow-wrap: break-word;
}

@media (min-width: 480px) {
  .user-bio-card {
    flex-direction: row;
    align-items: flex-start;
  }
}
```

### Example 3: Handling Extremely Long Words & URLs
Long URLs or strings of continuous characters will break through 320px containers and force horizontal scrolling unless wrapped:

```css
.prose {
  /* Automatically wraps long strings, URLs, and code tokens */
  overflow-wrap: break-word;
  word-break: normal;
  /* Soft hyphenation support for narrow columns */
  hyphens: auto;
}
```

### Example 4: Responsive Tables without Two-Dimensional Page Scroll
Data tables are one of the few exceptions in WCAG 1.4.10 where horizontal scrolling is permitted, **provided the scroll container is confined strictly to the table widget**, rather than causing the entire browser page to scroll horizontally:

```html
<div class="table-scroll-wrapper" tabindex="0" role="region" aria-label="Monthly Invoices">
  <table class="data-table">
    <thead>
      <tr>
        <th>Invoice ID</th>
        <th>Date</th>
        <th>Amount</th>
        <th>Status</th>
      </tr>
    </thead>
    <tbody>
      <tr>
        <td>#INV-2026-001</td>
        <td>2026-11-20</td>
        <td>$1,420.00</td>
        <td>Paid</td>
      </tr>
    </tbody>
  </table>
</div>
```

```css
/* Confine horizontal scrolling strictly to the table element */
.table-scroll-wrapper {
  inline-size: 100%;
  overflow-x: auto;
  border: 1px solid #e5e7eb;
  border-radius: 0.5rem;
  -webkit-overflow-scrolling: touch;
}

.table-scroll-wrapper:focus-visible {
  outline: 2px solid #2563eb;
  outline-offset: 2px;
}

.data-table {
  inline-size: 100%;
  min-inline-size: 450px; /* Forces table scrollbar inside wrapper on 320px screens */
  border-collapse: collapse;
}

.data-table th, .data-table td {
  padding: 0.75rem 1rem;
  text-align: start;
  border-bottom: 1px solid #f3f4f6;
}
```

### Example 5: Fluid Typography with Clamped Safe Boundaries
Ensure `clamp()` scales smoothly without exceeding readable or accessible limits on 320px viewports:

```css
:root {
  /* Clamped typography: min 1rem (16px) at 320px, max 1.5rem (24px) at 1200px */
  --fluid-h3: clamp(1rem, 0.85rem + 0.75vw, 1.5rem);
  --fluid-h1: clamp(1.75rem, 1.25rem + 2.5vw, 3rem);
}

h1 {
  font-size: var(--fluid-h1);
  line-height: 1.2;
}

h3 {
  font-size: var(--fluid-h3);
  line-height: 1.3;
}
```

---

## 6. Best Practices

1. **Design for 320px Width Baseline**: Treat 320 CSS pixels as your primary minimum breakpoint; if an interface works cleanly at 320px with zero horizontal scroll, it automatically passes 400% zoom reflow audits.
2. **Never Fix Container Heights on Text Elements**: Avoid `height: 250px`. Use `min-height` or let block padding (`padding-block: 1.5rem`) dictate container boundaries naturally.
3. **Use Unitless `line-height`**: Always specify line-height as a unitless multiplier (`line-height: 1.5`), not fixed pixels (`line-height: 24px`), so it scales proportionally when users increase font size.
4. **Apply `overflow-wrap: break-word` Globally**: Set `overflow-wrap: break-word` on `body` or typography containers to prevent long strings and links from breaking horizontal viewport bounds.
5. **Confine Essential Two-Dimensional Scrolling to Isolated Widgets**: Maps, data tables, code diff editors, and spreadsheets are permitted to scroll horizontally, but their overflow must be encapsulated in a focused scrollable container (`overflow-x: auto; max-width: 100%;`).
6. **Verify with the Official Text Spacing Bookmarklet**: Test pages using Steve Faulkner’s WCAG Text Spacing Bookmarklet to ensure no buttons or headings clip when text spacing is amplified.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Consequence | Correct Pattern |
| :--- | :--- | :--- |
| **Using `width: 100vw` with vertical scrollbars** | Viewport width includes scrollbar width; triggers an unwanted horizontal scrollbar. | Use `width: 100%` or `inline-size: 100%`. |
| **Setting fixed heights on cards with `overflow: hidden`** | Text spacing expansions or text zoom chops off bottom sentences and buttons. | Use `min-height` and `overflow: visible`. |
| **Hardcoding fixed pixel widths (`width: 400px`)** | Element overflows 320px viewports at 400% zoom, failing WCAG 1.4.10. | Use `max-inline-size: 100%` or `min(100%, 400px)`. |
| **Disabling viewport zoom via meta tag** | `user-scalable=no` or `maximum-scale=1` prevents low-vision users from zooming. | Never disable user zoom; allow full browser scaling. |
| **Using fixed pixel line-height (`line-height: 20px`)** | When font size scales to 32px, text lines crash into each other and overlap. | Use unitless multipliers: `line-height: 1.5`. |
| **Using `text-overflow: ellipsis` on critical instructional copy** | Chopped text hides essential information when text metrics expand. | Allow text to wrap onto multiple lines naturally. |

---

## 8. Performance, Accessibility & Browser Support

### Testing Procedures
1. **Testing Reflow (WCAG 1.4.10)**:
   - Open Chrome / Firefox on a desktop monitor set to $1280\text{px}$ resolution.
   - Press `Ctrl +` (Windows) or `Cmd +` (Mac) until the zoom indicator reaches **400%**.
   - Audit the page: Ensure no horizontal scrollbar appears on the main `window`, all multi-column layouts reflow into a single column, and all content remains legible and operable.
2. **Testing Text Spacing (WCAG 1.4.12)**:
   - Execute the following JavaScript bookmarklet in the DevTools Console:
     ```javascript
     document.querySelectorAll('*').forEach(el => {
       el.style.setProperty('line-height', '1.5', 'important');
       el.style.setProperty('letter-spacing', '0.12em', 'important');
       el.style.setProperty('word-spacing', '0.16em', 'important');
     });
     document.querySelectorAll('p').forEach(p => {
       p.style.setProperty('margin-bottom', '2em', 'important');
     });
     ```
   - Verify that no text is clipped, overlapping, or cut off by container bounds.

---

## 9. Comparison / When to Use What

| Technique | Scope | Reflow Impact | Best Suited For |
| :--- | :--- | :--- | :--- |
| **Fluid Grid (`minmax(min(100%, 280px), 1fr)`)** | Layout Containers | 0 horizontal scroll at 320px | Product grids, card lists, article directories |
| **`overflow-x: auto` Wrapper** | Specific Widgets | Keeps page reflow intact | Complex data tables, code editors, maps |
| **Unitless `line-height: 1.5`** | Typography | Adapts to any font-size | All body copy, headings, and UI controls |
| **`clamp(min, preferred, max)`** | Dynamic Sizing | Scales safely within bounds | Hero headlines, fluid card padding |

---

## 10. Interview Questions

### Q1: How does WCAG 2.2 Success Criterion 1.4.10 (Reflow) define compliance, and why is 320 CSS pixels the benchmark?
<details>
<summary>Answer</summary>
WCAG 1.4.10 (Level AA) requires that content can be presented without loss of information or functionality, and without requiring scrolling in two dimensions (horizontal and vertical) for:
- Vertical scrolling content at a width equivalent to **320 CSS pixels**.
- Horizontal scrolling content at a height equivalent to **256 CSS pixels**.

320 CSS pixels is the official benchmark because it represents the viewport width when a user zooms to **400%** on a standard desktop monitor with a 1280 CSS pixel width ($\frac{1280}{4} = 320\text{px}$). Ensuring an application reflows cleanly at 320px guarantees that low-vision users utilizing 400% browser zoom can read the page using only vertical scrolling.
</details>

### Q2: What are the four specific text spacing overrides defined in WCAG Success Criterion 1.4.12?
<details>
<summary>Answer</summary>
Under WCAG 1.4.12, no loss of content or functionality may occur when a user stylesheet overrides the following four style metrics simultaneously:
1. **Line height (line spacing)**: Set to at least **1.5 times** the font size.
2. **Spacing following paragraphs (`margin-bottom`)**: Set to at least **2 times** the font size.
3. **Letter spacing (tracking)**: Set to at least **0.12 times** the font size.
4. **Word spacing**: Set to at least **0.16 times** the font size.
</details>

### Q3: Why is setting `width: 100vw` on root containers a common cause of WCAG Reflow failures?
<details>
<summary>Answer</summary>
`100vw` calculates the width of the viewport *including* the width of any visible vertical scrollbars.
In contrast, `width: 100%` calculates the width of the root container *excluding* the vertical scrollbar.

When a page has vertical content that exceeds the screen height, the browser injects a vertical scrollbar (typically 15–17px wide on Windows). If an element is set to `width: 100vw`, it expands beyond the available document width by the exact width of the scrollbar, forcing the browser to render an unnecessary horizontal scrollbar at the bottom of the page, directly violating WCAG 1.4.10.
</details>

### Q4: Why is using unitless numbers for `line-height` (e.g. `1.5`) mandatory for accessible typography?
<details>
<summary>Answer</summary>
When `line-height` is declared with a unit (e.g. `line-height: 24px` or `line-height: 1.5em` on a parent container):
- With `px`, the line height is fixed. If child text or user settings enlarge the font to 36px, the text collides and renders on top of itself within the 24px line box.
- With `em`, the line height is calculated once at the parent level based on the parent's font size and inherited down as a fixed pixel value to children.

With a **unitless number** (`line-height: 1.5`), each child element dynamically calculates its own line height by multiplying 1.5 against its *own* computed font size, guaranteeing adequate spacing across all headings, captions, and text zoom levels.
</details>

### Q5: What content types are officially exempt from the two-dimensional scrolling restriction in WCAG Reflow?
<details>
<summary>Answer</summary>
Content that inherently requires two-dimensional layout to preserve meaning is exempt from SC 1.4.10. Examples include:
1. **Complex Data Tables**: Multi-column spreadsheets where row/column intersections convey meaning.
2. **Geographical Maps & Diagrams**: Satellite maps, transit networks, and architectural blueprints.
3. **Video and Canvas Interfaces**: WebGL games and digital audio workstation timelines.
4. **Code Diffs & Syntax Blocks**: Side-by-side git diff comparisons where line wrapping destroys structural meaning.
Even for these exempt components, horizontal scrolling should be contained within the widget container (`overflow-x: auto`), rather than forcing the entire browser document to scroll horizontally.
</details>

### Q6: How does the CSS `overflow-wrap: break-word` property prevent reflow bugs?
<details>
<summary>Answer</summary>
By default, browser layout engines will not break unbroken strings of characters (such as long URLs, code tokens, or email addresses). On a narrow 320px viewport, an unbroken 40-character URL will push the boundary of its container and overflow off the right edge of the screen, creating a horizontal scrollbar.

`overflow-wrap: break-word` (or `word-break: break-word`) instructs the browser to calculate whether the word exceeds the container width; if it does, the browser breaks the word arbitrarily onto a new line, keeping the layout constrained within 320px.
</details>

### Q7: Why is `overflow: hidden` on text containers considered a severe accessibility hazard?
<details>
<summary>Answer</summary>
When developers set fixed dimensions on cards or buttons paired with `overflow: hidden`, any user who activates larger system fonts, 400% zoom, or WCAG text spacing overrides will have the excess text silently chopped off.
For example, a confirmation button reading "Transfer Funds" might display only "Transfer" or have its text cut in half vertically, leaving users unable to read or verify actions. Text containers should always use `min-height` and allow `overflow: visible` (or `overflow-y: auto` in fixed dialog viewports).
</details>

### Q8: How does `min(100%, 360px)` provide a more resilient layout than a fixed `width: 360px`?
<details>
<summary>Answer</summary>
`width: 360px` hardcodes the element to exactly 360 pixels. On a 320px viewport (400% zoom), the element is 40 pixels wider than the screen, triggering an immediate horizontal scrollbar.

`width: min(100%, 360px)` instructs the engine: "Take 360px if available, but if the viewport or parent container is narrower than 360px, shrink smoothly to 100% of the available width." This ensures the component never causes horizontal overflow.
</details>

---

## 11. Practice Exercises

### Exercise 1: Refactoring a Fixed-Dimension Card into a 400% Zoom Resilient Component
**Objective**: Refactor the following brittle, pixel-locked CSS snippet so that it reflows without horizontal scroll down to 320px and tolerates WCAG text spacing overrides without text truncation.

```css
/* BRITTLE CODE TO REFACTOR */
.pricing-card {
  width: 380px;
  height: 450px;
  overflow: hidden;
  line-height: 22px;
}
```

<details>
<summary>Solution</summary>

```css
/* REFACTORED RESILIENT CODE */
.pricing-card {
  /* Fluid width that never exceeds 380px but shrinks down to 100% on 320px screens */
  inline-size: 100%;
  max-inline-size: 23.75rem; /* 380px in rem */

  /* Flexible height that expands naturally with enlarged text */
  min-block-size: min-content;

  /* Never crop text */
  overflow: visible;

  /* Relative unitless line-height */
  line-height: 1.5;

  /* Ensure long words or tokens wrap gracefully */
  overflow-wrap: break-word;

  /* Add padding via relative units */
  padding: 1.5rem;
  background-color: #ffffff;
  border: 1px solid #e2e8f0;
  border-radius: 0.75rem;
}
```
</details>

### Exercise 2: Responsive Form Layout for 320px Viewports
**Objective**: Build a multi-input form row (First Name, Last Name) that displays side-by-side on wide screens, but stacks automatically on 320px viewports without horizontal scrolling.

<details>
<summary>Solution</summary>

```html
<form class="user-form">
  <div class="form-row">
    <div class="form-group">
      <label for="first-name">First Name</label>
      <input type="text" id="first-name" class="form-input">
    </div>
    <div class="form-group">
      <label for="last-name">Last Name</label>
      <input type="text" id="last-name" class="form-input">
    </div>
  </div>
</form>
```

```css
.user-form {
  inline-size: 100%;
  max-inline-size: 600px;
}

.form-row {
  display: flex;
  flex-wrap: wrap; /* Stacks inputs automatically when space is tight */
  gap: 1rem;
}

.form-group {
  /* Flex basis of 240px ensures inputs stack into a single column at 320px */
  flex: 1 1 240px;
  display: flex;
  flex-direction: column;
  gap: 0.375rem;
}

.form-group label {
  font-size: 0.875rem;
  font-weight: 600;
  color: #1e293b;
}

.form-input {
  inline-size: 100%;
  padding: 0.625rem 0.875rem;
  border: 1.5px solid #64748b;
  border-radius: 0.375rem;
  font-size: 1rem;
}

.form-input:focus-visible {
  border-color: #0284c7;
  outline: 2px solid #0284c7;
  outline-offset: 2px;
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **WCAG SC 1.4.10 (Reflow)**: Content must reflow without two-dimensional scrolling at a width equivalent to **320 CSS pixels** (400% zoom on 1280px display).
- **WCAG SC 1.4.12 (Text Spacing)**: No content loss when line height is $1.5\times$, paragraph margin is $2\times$, letter spacing is $0.12\times$, and word spacing is $0.16\times$.
- **No Hardcoded Heights**: Never use `height: 300px` on text containers; use `min-height` and `overflow: visible`.
- **Unitless Line-Height**: Always declare `line-height: 1.5` as a unitless multiplier.
- **Word Wrapping**: Apply `overflow-wrap: break-word` to prevent URLs and long strings from causing horizontal scrollbars.
- **Table Containment**: Isolate wide data tables in scrollable wrapper containers (`overflow-x: auto`) rather than scrolling the main page.
- **Avoid `100vw`**: Use `width: 100%` to prevent vertical scrollbars from triggering horizontal overflow.

---

## 13. Further Reading
- [W3C WCAG 2.2: Understanding Success Criterion 1.4.10 Reflow](https://www.w3.org/WAI/WCAG22/Understanding/reflow.html)
- [W3C WCAG 2.2: Understanding Success Criterion 1.4.12 Text Spacing](https://www.w3.org/WAI/WCAG22/Understanding/text-spacing.html)
- [Steve Faulkner: Text Spacing Bookmarklet](https://cdpn.io/stevef/debug/YLMqbo)
- [Adrian Roselli: Reflow and Text Spacing Testing](https://adrianroselli.com/2019/04/reflow-and-text-spacing.html)
- [web.dev: Responsive Web Design Basics](https://web.dev/responsive-web-design-basics/)

---

| Previous Note | Next Note |
| :--- | :--- |
| [04. Forced Colors and High Contrast Mode](04-forced-colors-and-high-contrast-mode.md) | [06. Touch Targets and Hit Areas](06-touch-targets-and-hit-areas.md) |
