# Cheatsheet 12: Common Bugs and Fixes

| Spec Level | CSS Troubleshooting Guide | Baseline Status | Evergreen Solutions |
|---|---|---|---|
| Target Audience | Frontend Engineers & Technical Interviewees | Focus | 15 definitive solutions for daily CSS rendering bugs |

---

## The Top 15 CSS Rendering Bugs & Instant Fixes

### 1. Flex Child Won't Shrink / Text Overflow Ellipsis Fails
- **Symptom**: Long unbroken text, URLs, or tables blow out the flex container width.
- **Root Cause**: Flex items default to `min-width: auto` (min-content size).
- **The One-Liner Fix**:
  ```css
  .flex-child { min-width: 0; }
  ```

---

### 2. Sticky Element Refuses to Stick
- **Symptom**: `position: sticky; top: 0;` scrolls off screen like a normal element.
- **Root Cause**: An ancestor has `overflow: hidden`, `overflow: auto`, or missing `top` offset.
- **The Fix**:
  ```css
  /* Check every ancestor; remove overflow or set: */
  .ancestor { overflow: clip; } /* or ensure top: 0 is declared on the sticky item */
  .sticky-nav { position: sticky; top: 0; z-index: 10; }
  ```

---

### 3. Mysterious 3px–4px Gap Under `<img>` Tags
- **Symptom**: Blank strip below inline images inside cards or figure elements.
- **Root Cause**: Images are `inline` by default, sitting on the typographic baseline leaving room for descenders (g, y, p, q).
- **The One-Liner Fix**:
  ```css
  img { display: block; } /* or vertical-align: middle; */
  ```

---

### 4. Accidental Horizontal Scrollbar on Entire Page
- **Symptom**: Mobile site wiggles horizontally or desktop shows bottom scrollbar.
- **Root Cause**: `100vw` includes the scrollbar gutter width (approx 15px), or an absolute element leaks outside.
- **The Fix**:
  ```css
  /* Replace width: 100vw with width: 100% */
  .full-width-element { width: 100%; }
  /* Pin global container overflow */
  html, body { overflow-x: clip; }
  ```

---

### 5. Layout Shifts When Scrollbar Appears / Disappears
- **Symptom**: Navbars and centered page content jerk horizontally when navigating between short and long pages.
- **Root Cause**: Operating system vertical scrollbar taking up 15px of viewport width on long pages.
- **The One-Liner Fix**:
  ```css
  html { scrollbar-gutter: stable; }
  ```

---

### 6. Modal / Centered Card Header Gets Cropped on Small Screens
- **Symptom**: Using `place-content: center` cuts off the top of a tall modal; user cannot scroll up.
- **Root Cause**: Centering creates negative scroll offset above the top viewport boundary (`scrollTop < 0` is inaccessible).
- **The Fix**:
  ```css
  .modal-overlay {
    display: flex;
    overflow-y: auto;
  }
  .modal-card {
    margin: auto; /* Centers perfectly when space exists; scrolls gracefully when tall */
  }
  ```

---

### 7. Margin Collapse Creating Unwanted Outer Spacing
- **Symptom**: Child's `margin-top` pushes the parent container down away from the top of the page.
- **Root Cause**: In-flow block margins collapse through parent boundaries lacking border or padding.
- **The One-Liner Fix**:
  ```css
  .parent { display: flow-root; } /* Creates a clean Block Formatting Context */
  ```

---

### 8. `z-index` Not Working (Element Stays Behind Sibling)
- **Symptom**: Setting `z-index: 9999` fails to render above another element.
- **Root Cause**: Either element lacks positioning (`position: static` ignores `z-index`), or parent has a lower stacking context.
- **The Fix**:
  ```css
  /* Ensure positioning is active */
  .element { position: relative; z-index: 9999; }
  /* Or isolate stacking context on sibling parent */
  .parent { isolation: isolate; }
  ```

---

### 9. Sticky Header Covers Section Anchor `#links`
- **Symptom**: Clicking internal page anchor links jumps target heading behind fixed/sticky navbar.
- **Root Cause**: Browser scrolls anchor target to `scrollTop: 0` without accounting for fixed header height.
- **The One-Liner Fix**:
  ```css
  html { scroll-padding-top: 5rem; /* Match header height + buffer */ }
  ```

---

### 10. CSS Grid Columns Squishing Below Content Width
- **Symptom**: Auto-fit grid tracks shrink so small that words wrap character-by-character.
- **Root Cause**: `minmax(0, 1fr)` allows tracks to shrink to 0px.
- **The Fix**:
  ```css
  /* Enforce minimum sensible column size */
  .grid { grid-template-columns: repeat(auto-fit, minmax(min(100%, 280px), 1fr)); }
  ```

---

### 11. iOS 100vh Bottom Cutoff (CTA Under Address Bar)
- **Symptom**: Mobile Safari address bar covers bottom buttons of a `height: 100vh` hero section.
- **Root Cause**: `100vh` measures the screen without dynamic browser navigation chrome.
- **The One-Liner Fix**:
  ```css
  .hero { min-height: 100svh; /* Small Viewport Height */ }
  ```

---

### 12. Button Text Blurry / Jagged During CSS Transform
- **Symptom**: Micro-interaction hover scale causes blurry text or subpixel jitter.
- **Root Cause**: Hardware acceleration layer scaling without subpixel anti-aliasing hints.
- **The Fix**:
  ```css
  .card-hover {
    transform: translateZ(0);
    backface-visibility: hidden;
    will-change: transform;
  }
  ```

---

### 13. Double Borders Between Adjacent Grid or Table Cells
- **Symptom**: Adjacent 1px borders combine into an ugly 2px border line.
- **Root Cause**: Both siblings render border on touching edges.
- **The Fix**:
  ```css
  /* For tables */
  table { border-collapse: collapse; }
  /* For custom grid items */
  .grid-item { margin-inline-end: -1px; margin-block-end: -1px; }
  ```

---

### 14. Input Focus Outline Cut Off by `overflow: hidden`
- **Symptom**: Default or custom focus ring is clipped at element boundaries.
- **Root Cause**: Container has `overflow: hidden` which crops external `outline-offset`.
- **The Fix**:
  ```css
  /* Replace outline with inset box-shadow */
  input:focus-visible {
    outline: none;
    box-shadow: inset 0 0 0 2px #2563eb;
  }
  ```

---

### 15. Variable Font Rendering Heavier on MacOS than Windows
- **Symptom**: Typography looks overly thick and smeared on macOS Retina screens.
- **Root Cause**: Subpixel font smoothing differences across operating systems.
- **The Fix**:
  ```css
  html {
    -webkit-font-smoothing: antialiased;
    -moz-osx-font-smoothing: grayscale;
  }
  ```

---

[← Previous: Cheatsheet 11 - Accessibility](file:///e:/Learning/engineering-notes/01-languages/css/16-cheatsheets/11-accessibility.md) | [Back to Module 16 Index →](file:///e:/Learning/engineering-notes/01-languages/css/16-cheatsheets/README.md)
