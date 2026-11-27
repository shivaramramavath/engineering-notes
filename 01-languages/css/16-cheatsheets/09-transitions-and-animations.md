# Cheatsheet 09: Transitions and Animations

| Spec Level | CSS Transitions Level 2 & CSS Animations Level 2 | Baseline Status | Widely Available / Discrete Transitions Baseline 2024 |
|---|---|---|---|
| Browsers Tested | Chrome 120+, Firefox 125+, Safari 17.4+ | Primary Usage | Interactive feedback, state transitions, dialog entry/exit animations |

---

## 1. Compositor-Safe Performance Matrix

Animating properties that trigger Layout or Paint drops frame rates below 60fps / 120fps on mobile devices.

| Pipeline Stage | CSS Properties | Performance Cost | Recommendation |
|---|---|---|---|
| **Composite Only** (GPU Thread) | `transform`, `opacity` | **Sub-millisecond (Zero CPU)** | **Always use for motion & scale** |
| **Paint Only** (CPU Raster) | `background-color`, `color`, `box-shadow`, `border-color` | Medium CPU cost (re-rasters layer) | Acceptable for micro-interactions |
| **Layout + Paint** (Reflow) | `width`, `height`, `margin`, `padding`, `top`, `left` | **Disastrous (Forces recalculate style)** | **NEVER animate in production** |

---

## 2. Transition Shorthand & Easing Functions

```css
/* Order: property duration timing-function delay */
.element {
  transition: transform 250ms cubic-bezier(0.16, 1, 0.3, 1) 0ms,
              opacity 200ms ease-in-out;
}
```

### Standard Easing Reference

| Easing Name | Curve / Cubic-Bezier | Visual Feel |
|---|---|---|
| `linear` | `cubic-bezier(0, 0, 1, 1)` | Robotic, constant velocity (spinners only) |
| `ease-in` | `cubic-bezier(0.42, 0, 1, 1)` | Accelerating (objects leaving viewport) |
| `ease-out` | `cubic-bezier(0, 0, 0.58, 1)` | Decelerating (objects entering viewport) |
| `ease-in-out` | `cubic-bezier(0.42, 0, 0.58, 1)` | Smooth acceleration & deceleration (state toggle) |
| **Natural Spring Entry** | `cubic-bezier(0.16, 1, 0.3, 1)` | Snappy iOS-like deceleration |

---

## 3. CSS Animations Shorthand

```css
/* Order: name duration timing-function delay iteration-count direction fill-mode */
.modal {
  animation: modal-enter 300ms cubic-bezier(0.16, 1, 0.3, 1) 0s 1 normal forwards;
}

@keyframes modal-enter {
  from {
    opacity: 0;
    transform: scale(0.95) translateY(10px);
  }
  to {
    opacity: 1;
    transform: scale(1) translateY(0);
  }
}
```

---

## 4. Animating `display: none` and Native `<dialog>` (Baseline 2024)

Modern CSS allows transitioning discrete properties like `display` and animating entry from `display: none` using `@starting-style` and `transition-behavior: allow-discrete`.

```css
dialog {
  opacity: 0;
  transform: scale(0.95);
  display: none;
  
  /* Transition both continuous AND discrete properties */
  transition: opacity 300ms ease,
              transform 300ms ease,
              overlay 300ms allow-discrete,
              display 300ms allow-discrete;
}

/* Open State */
dialog[open] {
  opacity: 1;
  transform: scale(1);
  display: block;
}

/* Starting Style: Initial state when element transitions FROM display: none */
@starting-style {
  dialog[open] {
    opacity: 0;
    transform: scale(0.95);
  }
}

/* Backdrop Transition */
dialog::backdrop {
  background-color: rgb(0 0 0 / 0);
  transition: display 300ms allow-discrete,
              overlay 300ms allow-discrete,
              background-color 300ms ease;
}

dialog[open]::backdrop {
  background-color: rgb(0 0 0 / 0.5);
}

@starting-style {
  dialog[open]::backdrop {
    background-color: rgb(0 0 0 / 0);
  }
}
```

---

## 5. `prefers-reduced-motion` Production Reset

Always pair transitions with accessibility defense:

```css
@media (prefers-reduced-motion: reduce) {
  *, *::before, *::after {
    animation-duration: 0.01ms !important;
    animation-iteration-count: 1 !important;
    transition-duration: 0.01ms !important;
    scroll-behavior: auto !important;
  }
}
```

---

[← Previous: Cheatsheet 08 - Colors](file:///e:/Learning/engineering-notes/01-languages/css/16-cheatsheets/08-colors-and-gradients.md) | [Next: Cheatsheet 10 - Modern CSS Features →](file:///e:/Learning/engineering-notes/01-languages/css/16-cheatsheets/10-modern-features.md)
