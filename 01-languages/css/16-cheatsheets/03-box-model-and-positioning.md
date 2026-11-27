# Cheatsheet 03: Box Model and Positioning

| Spec Level | CSS Box Model Level 3 & Positioned Layout Level 3 | Baseline Status | Widely Available |
|---|---|---|---|
| Browsers Tested | Chrome 120+, Firefox 125+, Safari 17.4+ | Primary Usage | Geometry, flow, stacking context, coordinate pinning |

---

## 1. Box Model Sizing Modes

```
+-------------------------------------------------------+
| Margin (Transparent outer spacing)                    |
|   +-----------------------------------------------+   |
|   | Border (Solid/dashed outer rim)               |   |
|   |   +---------------------------------------+   |   |
|   |   | Padding (Inner space around content)  |   |   |
|   |   |   +-------------------------------+   |   |   |
|   |   |   | Content Box (Text, images)    |   |   |   |
|   |   |   +-------------------------------+   |   |   |
|   |   +---------------------------------------+   |   |
|   +-----------------------------------------------+   |
+-------------------------------------------------------+
```

| Mode | Declared `width: 300px; padding: 20px; border: 5px solid;` Total Rendered Width |
|---|---|
| `box-sizing: content-box` (Default) | $300 + 20 + 20 + 5 + 5 = \mathbf{350\text{px}}$ (Content width alone is 300px) |
| `box-sizing: border-box` (Standard Reset) | $\mathbf{300\text{px}}$ (Content shrinks to $300 - 40 - 10 = 250\text{px}$) |

```css
/* Universal Box-Sizing Inheritance Reset */
*, *::before, *::after {
  box-sizing: border-box;
}
```

---

## 2. Margin Collapse Rules & Defenses

Vertical margins between in-flow block elements collapse into a single margin equal to $\max(M_1, M_2)$ (or $M_{\text{pos}} - |M_{\text{neg}}|$ if signs differ).

### When Margins Collapse:
1. **Adjacent In-Flow Siblings**: Top margin of element meets bottom margin of preceding sibling.
2. **Parent & First/Last Child**: No border, padding, inline content, or BFC separating parent margin from child margin.
3. **Empty Blocks**: Element has `min-height: 0`, no height, no border, no padding, and no content.

### When Margins NEVER Collapse:
- Between horizontal margins (`margin-left`, `margin-right`).
- Elements inside Flexbox (`display: flex`) or Grid (`display: grid`) containers.
- Float elements or absolutely positioned elements.
- Across a Block Formatting Context (BFC) boundary (e.g. `display: flow-root`, `overflow: clip`).
- Separated by `1px` padding or border.

---

## 3. Block Formatting Context (BFC) Quick Reference

A BFC contains internal floats, prevents external margin collapse, and clears adjacent floats.

```css
/* Modern Preferred BFC Trigger */
.container {
  display: flow-root; /* Clean, semantic BFC without overflow clipping side-effects */
}

/* Alternative BFC Triggers */
overflow: hidden; | overflow: clip; | overflow: auto;
display: inline-block; | display: flex; | display: grid;
position: absolute; | position: fixed;
```

---

## 4. Positioning Schemes Matrix

| Scheme | Flow Removal | Containing Block Reference | Top/Right/Bottom/Left Effect | Common Real-World Use Case |
|---|---|---|---|---|
| `static` | In flow | Normal layout flow | Ignored (`top`, `z-index` have no effect) | Default baseline HTML layout |
| `relative` | In flow (space preserved) | Normal layout position | Visual offset without affecting surrounding geometry | Anchor point for absolute children; z-index host |
| `absolute` | Removed from flow | Nearest ancestor with `position != static` (or Initial CB / viewport) | Coordinates relative to containing block padding edge | Badges, tooltips, dropdown menus, modal overlays |
| `fixed` | Removed from flow | Viewport (or ancestor with `transform`, `filter`, `perspective`) | Pinned to screen coordinates during scrolling | Sticky top navbar, cookie consent banner, floating CTA |
| `sticky` | In flow until scroll boundary | Nearest scrolling ancestor | Behaves as relative until threshold, then behaves as fixed | Table header (`th`), alphabetical contacts list, sticky sidebar |

---

## 5. Sticky Positioning Checklist (Why Sticky Fails)

If `position: sticky; top: 0;` fails to stick:
- [ ] **Missing Threshold**: You declared `position: sticky` but forgot `top: 0` (or `bottom: 0`).
- [ ] **Overflow Trap**: An ancestor element has `overflow: hidden`, `overflow: auto`, or `overflow: scroll`.
- [ ] **Zero Remaining Height**: The parent container has the exact same height as the sticky element (sticky cannot leave its parent).
- [ ] **Table Display**: Sticky on `<tr>` does not work across all browsers; apply `position: sticky` directly to `<th>` or `<td>`.

---

## 6. Stacking Context Triggers

Stacking contexts determine `z-index` paint order. A child cannot render above a sibling if its parent has a lower stacking context.

```css
/* Stacking Context Triggers Cheat List */
1. Root element: <html>
2. Positioned with z-index: position: relative/absolute/fixed/sticky AND z-index != auto
3. Opacity: opacity < 1
4. CSS Transforms: transform != none
5. CSS Filters: filter != none
6. CSS Clip / Mask: clip-path, mask != none
7. Perspective: perspective != none
8. Mix-blend-mode: mix-blend-mode != normal
9. Containment: contain: paint, contain: layout, contain: strict
10. Isolation: isolation: isolate (explicit non-destructive trigger)
```

---

[← Previous: Cheatsheet 02 - Units and Values](file:///e:/Learning/engineering-notes/01-languages/css/16-cheatsheets/02-units-and-values.md) | [Next: Cheatsheet 04 - Flexbox →](file:///e:/Learning/engineering-notes/01-languages/css/16-cheatsheets/04-flexbox.md)
