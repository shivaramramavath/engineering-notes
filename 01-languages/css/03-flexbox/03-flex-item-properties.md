# Flex Item Properties

| Metadata | Details |
| :--- | :--- |
| **Level** | Core / Intermediate |
| **Prerequisites** | `03-flexbox/01-flexbox-mental-model.md`, `03-flexbox/02-flex-container-properties.md` |
| **Estimated Reading Time** | 24 minutes |
| **Status** | Complete |
| **Baseline Status** | Widely Available across all modern browsers (Baseline 2015) |
| **Browsers Tested** | Chrome 120+, Firefox 121+, Safari 17.2+ |
| **Tested With** | Automated CSS Validation Runner (`css_runner.js`) |

---

## Table of Contents
- [1 What & Why](#1-what--why)
- [2 Core Concepts](#2-core-concepts)
- [3 How It Works Internally](#3-how-it-works-internally)
- [4 Syntax & Usage](#4-syntax--usage)
- [5 Practical Examples](#5-practical-examples)
- [6 Best Practices](#6-best-practices)
- [7 Common Mistakes & Pitfalls](#7-common-mistakes--pitfalls)
- [8 Performance, Accessibility & Browser Support](#8-performance-accessibility--browser-support)
- [9 Comparison / When to Use What](#9-comparison--when-to-use-what)
- [10 Interview Questions](#10-interview-questions)
- [11 Practice Exercises](#11-practice-exercises)
- [12 Summary / Cheat Sheet](#12-summary--cheat-sheet)
- [13 Further Reading](#13-further-reading)

---

## 1 What & Why

While container properties establish the overarching flow and direction of a flex layout, **flex item properties** govern how individual children behave within that container:
- How much spare space an item can claim when the container expands (`flex-grow`).
- How much an item must shrink when the container is constricted (`flex-shrink`).
- What starting size the engine uses before expanding or shrinking (`flex-basis`).
- How an individual item aligns itself independently across the cross axis (`align-self`).
- What order it displays visually (`order`).

Understanding the relationship between `flex-grow`, `flex-shrink`, and `flex-basis`—and why the `flex` shorthand is universally recommended by the W3C—is fundamental to writing predictable, resilient component styling.

---

## 2 Core Concepts

### The Three Sizing Pillars

```
                    +---------------------------+
                    |        flex-basis         |
                    | (The Starting Main Size)  |
                    +-------------+-------------+
                                  |
            Is there spare space? | Is there a deficit?
           +----------------------+----------------------+
           |                                             |
           ▼                                             ▼
+---------------------+                       +---------------------+
|      flex-grow      |                       |     flex-shrink     |
| (Absorb Free Space) |                       | (Absorb Space Short)|
+---------------------+                       +---------------------+
```

1. **`flex-basis`**:
   - The initial main size of the flex item before any free space distribution or shrinkage occurs.
   - Defaults to `auto`. When `auto`, the engine checks the item's explicit `width` or `height` property. If none exists, it falls back to the item's intrinsic content size (`content`).
   - If set to `0` or `0%`, the item's intrinsic content size is completely ignored during growth distribution!

2. **`flex-grow`**:
   - A unitless proportionality factor determining how much of the container's **positive free space** (remaining unused space) this item claims.
   - Defaults to `0` (item does not grow beyond its `flex-basis`).
   - If three items each have `flex-grow: 1`, they share free space equally (1:1:1). If one item has `flex-grow: 2` and two have `flex-grow: 1`, the first gets $50\%$ of the spare space and the other two get $25\%$ each.

3. **`flex-shrink`**:
   - A unitless proportionality factor determining how much this item contracts when there is **negative free space** (the combined items exceed container size).
   - Defaults to `1` (all items shrink proportionally to prevent overflow).
   - If set to `0`, the item refuses to shrink below its `flex-basis` or min-content size, even if it forces the container to overflow!

### 4. `order`

- An integer controlling the item's visual display sequence within the container.
- Defaults to `0`. Items with lower values appear first (e.g. `order: -1`), while higher values appear last.
- Items with the same `order` value are sorted in source DOM order.

### 5. `align-self`

- Allows a single flex item to override the container's `align-items` declaration along the cross axis.
- Values: `auto` (inherit container `align-items`), `flex-start`, `flex-end`, `center`, `baseline`, `stretch`.

---

## 3 How It Works Internally

### The W3C `flex` Shorthand Golden Rule

The W3C CSS Flexbox specification **strongly recommends** authoring the `flex` shorthand rather than individual longhand properties.

When you use the shorthand with single values, the browser intelligently adjusts defaults:

| Shorthand Declaration | Equivalent Longhand Expansion | Behavior |
| :--- | :--- | :--- |
| `flex: initial` (Default) | `flex: 0 1 auto` | Doesn't grow; shrinks if needed; sizes to content/width. |
| `flex: auto` | `flex: 1 1 auto` | Fully fluid; grows and shrinks, but respects content size as base. |
| `flex: none` | `flex: 0 0 auto` | Rigid/Inflexible; refuses to grow or shrink. |
| `flex: 1` (Single unitless) | `flex: 1 1 0%` | **Equal width distribution!** Resets `flex-basis` to `0%` so all items grow strictly equally regardless of content size. |
| `flex: 200px` (Single length) | `flex: 1 1 200px` | Basis is 200px; expands and contracts around 200px. |

```
Difference between flex: 1 and flex: auto

flex: 1 (flex: 1 1 0%)
Total Container: 600px | Free Space: 600px - (0px + 0px + 0px) = 600px
+-------------------------+-------------------------+-------------------------+
| Item 1 (200px)          | Item 2 (200px)          | Item 3 (200px)          |
| "Short text"            | "Medium text here"      | "Very long text content"|
+-------------------------+-------------------------+-------------------------+
Result: Perfectly equal 200px columns!

flex: auto (flex: 1 1 auto)
Initial sizes: Item 1 = 80px, Item 2 = 120px, Item 3 = 250px (Total = 450px)
Spare space: 600px - 450px = 150px. Spare space divided by 3 = 50px each.
+-------------------+-----------------------+---------------------------------+
| Item 1 (130px)    | Item 2 (170px)        | Item 3 (300px)                  |
| (80px + 50px)     | (120px + 50px)        | (250px + 50px)                  |
+-------------------+-----------------------+---------------------------------+
Result: Unequal columns because content size was factored into basis!
```

---

## 4 Syntax & Usage

```css
.flex-item {
  /* The Universal Recommended Shorthand */
  flex: 1;                     /* flex: 1 1 0% */
  flex: auto;                  /* flex: 1 1 auto */
  flex: none;                  /* flex: 0 0 auto */
  flex: 2 1 300px;             /* grow: 2, shrink: 1, basis: 300px */

  /* Cross-Axis Self Alignment */
  align-self: auto | flex-start | flex-end | center | baseline | stretch;

  /* Visual Reordering */
  order: 0;                    /* default integer */
  order: -1;                   /* jumps to start */
  order: 99;                   /* jumps to end */
}
```

---

## 5 Practical Examples

### Example 1: Equal-Width Columns with `flex: 1`

```css
.stats-grid {
  display: flex;
  gap: 16px;
}

.stat-box {
  /* flex: 1 ensures each box gets exactly 33.33% width
     even if one box has a 7-digit number and another has 2 digits */
  flex: 1;
  background-color: #f8fafc;
  border: 1px solid #e2e8f0;
  border-radius: 8px;
  padding: 16px;
  text-align: center;
}
```

### Example 2: Fixed Sidebar with Fluid Content Area

```css
.app-shell {
  display: flex;
  min-height: 100vh;
}

.app-sidebar {
  /* Rigid: never grows, never shrinks, fixed at 260px */
  flex: 0 0 260px;
  background-color: #0f172a;
  color: #ffffff;
}

.app-content {
  /* Fluid: claims all remaining width */
  flex: 1;
  min-width: 0; /* Prevents overflow blowout */
  padding: 24px;
}
```

### Example 3: Edge Case — Breaking Cross-Axis Stretch with `align-self`

```css
.user-profile-header {
  display: flex;
  align-items: stretch; /* Default: items stretch to match height */
  gap: 20px;
  background-color: #ffffff;
  padding: 24px;
}

.user-bio {
  flex: 1;
}

/* We want the avatar to keep its intrinsic aspect ratio without stretching */
.user-avatar {
  flex: 0 0 80px;
  height: 80px;
  border-radius: 50%;
  align-self: center; /* Overrides container stretch */
}
```

### Example 4: Failure Case — Using `flex: 1` when Longhand `flex-grow: 1` was Intended

```css
/* FAILURE PATTERN */
.icon-row {
  display: flex;
  gap: 12px;
}

.icon-label {
  /* BUG: Developer wrote flex-grow: 1 expecting equal widths.
     Because flex-basis defaults to 'auto', items with long labels
     end up noticeably wider than items with short labels! */
  flex-grow: 1;
}

/* FIXED PATTERN */
.icon-label-fixed {
  /* flex: 1 expands to flex: 1 1 0%, guaranteeing equal widths! */
  flex: 1;
}
```

### Example 5: Visual Priority Badges with `order`

```css
.card-header {
  display: flex;
  align-items: center;
  gap: 12px;
}

.card-title {
  font-size: 1.25rem;
  font-weight: 700;
  order: 1; /* Normal title */
}

/* Put urgent alert badge at the visual start without modifying HTML */
.card-alert-badge {
  order: -1; /* Jumps before title */
  background-color: #fee2e2;
  color: #b91c1c;
  padding: 2px 8px;
  border-radius: 4px;
  font-size: 0.75rem;
}
```

---

## 6 Best Practices

1. **Always use the `flex` shorthand**: Avoid setting `flex-grow`, `flex-shrink`, and `flex-basis` individually; the shorthand sets omitted values intelligently.
2. **Use `flex: 1` for truly equal-width columns**: Never use `flex-grow: 1` if equal columns are required, as `flex-basis: auto` preserves content width disparities.
3. **Always pair `flex: 1` with `min-width: 0` on responsive content**: Avoids container blowout caused by long strings or code snippets.
4. **Use `flex: 0 0 [size]` for non-collapsible items**: Guarantees fixed dimensions for icons, sidebars, or action buttons.
5. **Use `align-self` sparingly for exceptions**: If multiple children need identical alignment, set `align-items` on the container instead.
6. **Never use `order` to fix flawed DOM architecture**: Screen readers and keyboard tab navigation ignore `order`, leading to accessibility violations.

---

## 7 Common Mistakes & Pitfalls

| Mistake | Why It Fails | Solution |
| :--- | :--- | :--- |
| Writing `flex-grow: 1` expecting equal widths | `flex-basis` stays `auto`, making columns unequal based on text length | Use `flex: 1` (which resets basis to `0%`) |
| Icon squishing when text grows | Default `flex-shrink: 1` forces icon to compress | Add `flex-shrink: 0` or `flex: none` to the icon |
| Flex item overflowing container | Default `min-width: auto` prevents item from shrinking past content | Add `min-width: 0` |
| Using `width` instead of `flex-basis` | `width` can be overridden or warped by flex shrinkage calculations | Set `flex-basis` or use `flex: 0 0 200px` |
| `order: -1` breaks keyboard tab sequence | Focus jumps erratically across screen because DOM order is unchanged | Keep DOM order aligned with visual expectations |
| Declaring `justify-self` on a flex item | `justify-self` is not supported in the Flexbox specification | Use `margin-left: auto` or `margin-right: auto` |

---

## 8 Performance, Accessibility & Browser Support

### Layout Computation Performance

- Modern CSS engines resolve flex factors using a two-pass constraint algorithm.
- Setting explicit `flex-basis: 0` or `flex-basis: [length]` is marginally faster for engines to compute than `flex-basis: content`, because the engine does not need to measure internal text nodes before calculating free space.

### Accessibility Standards (WCAG 1.3.2)

- **The `order` Anti-Pattern**: Manipulating `order` changes only the visual display tree. Assistive technologies (screen readers like NVDA, VoiceOver, JAWS) read exclusively from the accessibility tree, which mirrors the DOM tree.
- If a form submit button is placed first in the DOM but given `order: 99` to sit at the bottom visually, a screen reader will read the Submit button before reading the form fields! Always ensure the DOM sequence makes sense linearly.

---

## 9 Comparison / When to Use What

| Shorthand | Expansion | Grows? | Shrinks? | Base Size | Best Use Case |
| :--- | :--- | :--- | :--- | :--- | :--- |
| `flex: initial` | `0 1 auto` | No | Yes | Content width | Default. Shrinkable buttons or tags |
| `flex: auto` | `1 1 auto` | Yes | Yes | Content width | Fluid items where larger text gets more space |
| `flex: none` | `0 0 auto` | No | No | Content width | Rigid avatars, fixed icons, non-collapsible sidebars |
| `flex: 1` | `1 1 0%` | Yes | Yes | `0%` | **Equal width columns**, grid-like cards |
| `flex: 0 0 240px` | `0 0 240px` | No | No | `240px` | Fixed desktop sidebars, fixed media columns |

---

## 10 Interview Questions

<details>
<summary><strong>1. What is the crucial difference between <code>flex: 1</code> and <code>flex: auto</code>?</strong></summary>

`flex: 1` expands to `flex: 1 1 0%`, meaning the starting size (`flex-basis`) is $0\%$, and all available space is distributed equally among items regardless of their text length. In contrast, `flex: auto` expands to `flex: 1 1 auto`, which means the browser first measures each item's intrinsic content size, and then distributes whatever *remaining* space is left over, resulting in unequal column widths.
</details>

<details>
<summary><strong>2. Why does an avatar image or icon inside a flex row sometimes collapse or squish into an oval?</strong></summary>

Because all flex items default to `flex-shrink: 1`. When neighboring text content grows long and consumes available space, the container runs out of room and forces all shrinkable items to compress. Setting `flex-shrink: 0` (or `flex: none`) on the image prevents it from compressing.
</details>

<details>
<summary><strong>3. What does <code>min-width: 0</code> do on a flex item?</strong></summary>

By specification, flex items have a default `min-width: auto` (instead of `0`). This prevents an item from shrinking below the width of its content (such as a long unbroken URL or code snippet), causing horizontal overflow. Setting `min-width: 0` overrides this default and allows the item to shrink properly and truncate text with `text-overflow: ellipsis`.
</details>

<details>
<summary><strong>4. How is negative free space distributed among items when <code>flex-shrink</code> values differ?</strong></summary>

Unlike `flex-grow` (which distributes space based purely on the grow factor), `flex-shrink` weights shrinkage by **both the shrink factor AND the item's `flex-basis`**:
$$\text{Scaled Shrink Factor} = \text{flex-shrink} \times \text{flex-basis}$$
Larger items shrink more than smaller items with the same shrink factor, preventing small items from vanishing to zero immediately.
</details>

<details>
<summary><strong>5. Can you use <code>justify-self</code> on a flex item?</strong></summary>

No. `justify-self` is intentionally not supported in the Flexbox specification because items share a continuous main-axis line without independent tracks. To achieve individual main-axis positioning, use auto margins (`margin-left: auto` or `margin-right: auto`).
</details>

<details>
<summary><strong>6. What is the initial specification value of the <code>flex</code> property?</strong></summary>

The initial value is `flex: 0 1 auto` (equivalent to `flex: initial`), meaning the item does not grow, can shrink to prevent overflow, and bases its starting size on its `width`/`height` or intrinsic content.
</details>

<details>
<summary><strong>7. How does <code>order</code> affect the tab order for keyboard navigation?</strong></summary>

It has zero effect. Keyboard tab navigation follows the DOM tree order, completely ignoring CSS `order`. Altering visual order without updating DOM order creates an inaccessible keyboard trap/disconnect.
</details>

<details>
<summary><strong>8. What happens if a flex item has both an explicit <code>width: 300px</code> and <code>flex-basis: 200px</code>?</strong></summary>

`flex-basis` takes precedence over `width` (or `height` in column layouts). The browser will use `200px` as the starting hypothetical size for all flex calculations, ignoring the `300px` width declaration.
</details>

---

## 11 Practice Exercises

### Exercise 1 (Easy): Non-Squishing Icon
- **Task**: Prevent a 24px SVG icon from collapsing when adjacent text expands inside a flex row.

<details>
<summary>View Solution</summary>

```css
.list-item {
  display: flex;
  align-items: center;
  gap: 12px;
}

.item-icon {
  flex-shrink: 0; /* Prevents squishing */
  width: 24px;
  height: 24px;
}
```
</details>

### Exercise 2 (Easy): Equal-Width Two-Button Group
- **Task**: Ensure two buttons in a modal footer have identical widths regardless of label length.

<details>
<summary>View Solution</summary>

```css
.modal-footer {
  display: flex;
  gap: 12px;
}

.modal-footer button {
  flex: 1; /* Equal 50/50 width */
}
```
</details>

### Exercise 3 (Medium): Proportional 2:1 Split Layout
- **Task**: Create a main article area that takes 2/3 of the space and a sidebar that takes 1/3 of the space.

<details>
<summary>View Solution</summary>

```css
.layout-split {
  display: flex;
  gap: 24px;
}

.article-main {
  flex: 2 1 0%; /* Claims 2 shares */
}

.sidebar-aside {
  flex: 1 1 0%; /* Claims 1 share */
}
```
</details>

### Exercise 4 (Medium): Truncating Long Text Inside Flex Child
- **Task**: Correctly truncate an overflowing text string with an ellipsis inside a flex item without blowing out the container.

<details>
<summary>View Solution</summary>

```css
.user-card {
  display: flex;
  align-items: center;
  gap: 12px;
  width: 300px;
  border: 1px solid #e2e8f0;
  padding: 12px;
}

.user-info {
  flex: 1;
  min-width: 0; /* Crucial: allows item to shrink below content width */
}

.user-email {
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
}
```
</details>

### Exercise 5 (Hard): Custom Self-Aligned Item in Flex Container
- **Task**: In a row of navigation links aligned to `flex-start`, align a single "Sign In" button to the `flex-end` cross edge and another to `center`.

<details>
<summary>View Solution</summary>

```css
.nav-bar {
  display: flex;
  align-items: flex-start;
  height: 80px;
  gap: 16px;
  background-color: #f8fafc;
}

.nav-link {
  padding: 8px 12px;
}

.nav-sign-in {
  align-self: flex-end; /* Pushed to bottom of 80px bar */
}

.nav-logo {
  align-self: center;   /* Centered vertically */
}
```
</details>

### Exercise 6 (Hard): Dynamic Input Addon Group
- **Task**: Build an input group where a left prefix and right button are fixed-size, and the input field expands to fill the middle space.

<details>
<summary>View Solution</summary>

```css
.input-group {
  display: flex;
  width: 100%;
}

.input-prefix {
  flex: 0 0 40px;
  background: #e2e8f0;
  display: grid;
  place-items: center;
}

.input-field {
  flex: 1;
  min-width: 0;
  padding: 8px 12px;
  border: 1px solid #cbd5e1;
}

.input-submit-btn {
  flex: 0 0 auto;
  padding: 8px 16px;
  background: #2563eb;
  color: #ffffff;
  border: none;
}
```
</details>

---

## 12 Summary / Cheat Sheet

```css
/* Flex Shorthand Quick Reference */
flex: 1;               /* Equal widths: flex-grow: 1; flex-shrink: 1; flex-basis: 0%; */
flex: auto;            /* Fluid content-based: 1 1 auto; */
flex: none;            /* Rigid: 0 0 auto; (never grows, never shrinks) */
flex: 0 0 200px;       /* Fixed size: precisely 200px */

/* Alignment & Sizing */
align-self: center;    /* Overrides cross-axis alignment for this item */
min-width: 0;          /* Essential to allow flex child text truncation */
order: -1;             /* Moves item to visual start */
```

---

## 13 Further Reading

- [MDN Web Docs: Controlling ratios of flex items along the main axis](https://developer.mozilla.org/en-US/docs/Web/CSS/CSS_flexible_box_layout/Controlling_ratios_of_flex_items_along_the_main_axis)
- [W3C CSS Flexible Box Layout Module Level 1: Flex Item Properties](https://www.w3.org/TR/css-flexbox-1/#flex-item-properties)
- [web.dev: Understanding flex-grow and flex-shrink](https://web.dev/)
- [Josh W. Comeau: The Interactive Guide to Flexbox](https://www.joshwcomeau.com/css/interactive-guide-to-flexbox/)

---

[Previous: 02-flex-container-properties.md](file:///e:/Learning/engineering-notes/01-languages/css/03-flexbox/02-flex-container-properties.md) | [Section Index](file:///e:/Learning/engineering-notes/01-languages/css/03-flexbox/README.md) | [Next: 04-alignment-and-gap.md](file:///e:/Learning/engineering-notes/01-languages/css/03-flexbox/04-alignment-and-gap.md)
