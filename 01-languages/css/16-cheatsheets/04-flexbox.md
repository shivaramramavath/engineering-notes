# Cheatsheet 04: Flexbox

| Spec Level | CSS Flexible Box Layout Module Level 1 | Baseline Status | Widely Available |
|---|---|---|---|
| Browsers Tested | Chrome 120+, Firefox 125+, Safari 17.4+ | Primary Usage | 1D linear layouts, alignment, dynamic content distribution |

---

## 1. Container Properties Cheat Sheet

```css
.flex-container {
  display: flex; /* or inline-flex */

  /* Main Axis Direction */
  flex-direction: row;            /* default: left-to-right (in LTR) */
  flex-direction: row-reverse;    /* right-to-left */
  flex-direction: column;         /* top-to-bottom */
  flex-direction: column-reverse; /* bottom-to-top */

  /* Multiline Wrapping */
  flex-wrap: nowrap;        /* default: shrink items on single line */
  flex-wrap: wrap;          /* wrap items to next line when out of space */
  flex-wrap: wrap-reverse;  /* wrap upwards */

  /* Shorthand: direction + wrap */
  flex-flow: row wrap;

  /* Main Axis Alignment */
  justify-content: flex-start;    /* pack items at start */
  justify-content: flex-end;      /* pack items at end */
  justify-content: center;        /* center items on main axis */
  justify-content: space-between; /* first item start, last item end, equal gap */
  justify-content: space-around;  /* equal space around items (half space at edges) */
  justify-content: space-evenly;  /* identical space between all items and edges */

  /* Cross Axis Alignment (Single Line) */
  align-items: stretch;     /* default: stretch items to container height */
  align-items: flex-start;  /* align items at top of cross axis */
  align-items: flex-end;    /* align items at bottom of cross axis */
  align-items: center;      /* vertically center items */
  align-items: baseline;    /* align items along their typographic baselines */

  /* Cross Axis Alignment (Multi-Line: Requires flex-wrap: wrap) */
  align-content: stretch | flex-start | flex-end | center | space-between | space-evenly;

  /* Spacing between items */
  gap: 1.5rem;              /* shorthand: row-gap column-gap */
  row-gap: 1rem;
  column-gap: 2rem;
}
```

---

## 2. Flex Item Sizing: `flex: grow shrink basis`

```
flex: <flex-grow> <flex-shrink> <flex-basis>
```

| Shorthand | Expands To | Behavior & Usage |
|---|---|---|
| `flex: 0 1 auto` | Default Flexbox item | Does not grow; shrinks if container is crowded; basis matches content width |
| `flex: 1` | `flex: 1 1 0%` | Grows and shrinks equally from a 0px base; guarantees identical column widths |
| `flex: auto` | `flex: 1 1 auto` | Grows and shrinks proportionally, but starts from content size (larger items stay wider) |
| `flex: none` | `flex: 0 0 auto` | Rigid, non-growing, non-shrinking element (fixed icon, badge, sidebar) |

### Individual Item Properties

```css
.flex-item {
  /* Growth factor when positive free space exists */
  flex-grow: 1;

  /* Shrink factor when negative free space exists */
  flex-shrink: 1;

  /* Initial main size before free space is distributed */
  flex-basis: 250px; /* px, rem, %, or auto */

  /* Override container align-items for this specific item */
  align-self: auto | flex-start | flex-end | center | baseline | stretch;

  /* Visual display order */
  order: 2; /* default 0; negative values render first (DOM order unchanged) */
}
```

---

## 3. The 3 Golden Flexbox Recipes

### Recipe 1: Perfect Bi-Directional Centering
```css
.modal-overlay {
  display: flex;
  justify-content: center; /* Main axis center */
  align-items: center;     /* Cross axis center */
  min-height: 100dvh;
}
```

### Recipe 2: Sticky Footer (CSS-Only)
```css
body {
  display: flex;
  flex-direction: column;
  min-height: 100vh;
}

main {
  flex-grow: 1; /* Absorbs all remaining vertical space pushing footer to bottom */
}
```

### Recipe 3: Auto-Margin Push (Navbar / Card Footer)
```css
.navbar {
  display: flex;
  align-items: center;
  gap: 1rem;
}

.nav-links {
  margin-inline-start: auto; /* Absorbs all free space, pushing links to far right */
}
```

---

## 4. The Critical Flexbox Gotcha: `min-width: auto`

By default in CSS Flexbox, all flex items have `min-width: auto` (or `min-height: auto` in column layout). This prevents flex items from shrinking smaller than their minimum intrinsic content width (e.g. unbroken URLs, code blocks, or nested tables).

```css
/* Flex child blowing out the container layout? */
.flex-item {
  min-width: 0; /* Unlocks flex-shrink; allows text-overflow: ellipsis to work */
}
```

---

[← Previous: Cheatsheet 03 - Box Model](file:///e:/Learning/engineering-notes/01-languages/css/16-cheatsheets/03-box-model-and-positioning.md) | [Next: Cheatsheet 05 - CSS Grid →](file:///e:/Learning/engineering-notes/01-languages/css/16-cheatsheets/05-grid.md)
