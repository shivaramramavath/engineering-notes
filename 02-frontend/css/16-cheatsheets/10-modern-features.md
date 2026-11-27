# Cheatsheet 10: Modern CSS Features

| Spec Level | CSS Cascading 5, Nesting 1, Properties & Values 1, Anchor Positioning 1 | Baseline Status | Baseline 2023–2026 |
|---|---|---|---|
| Browsers Tested | Chrome 120+, Firefox 128+, Safari 17.4+ | Primary Usage | Cascade architecture, typed tokens, relational styling, popovers |

---

## 1. Native CSS Nesting (Baseline Widely Available)

```css
/* Standard Nested Component */
.card {
  background-color: #ffffff;
  padding: 1.5rem;

  /* Direct child element */
  h2 {
    font-size: 1.5rem;
    margin-block-end: 0.5rem;
  }

  /* Ampersand (&) for pseudo-classes */
  &:hover {
    box-shadow: 0 8px 16px rgba(0, 0, 0, 0.1);
  }

  /* Modifier class */
  &.featured {
    border: 2px solid gold;
  }

  /* Contextual parent matching */
  .dark-theme & {
    background-color: #1e293b;
  }

  /* Media query inside rule */
  @media (width >= 768px) {
    padding: 2.5rem;
  }
}
```

---

## 2. Cascade Layers (`@layer`) Cheat Sheet

Layers organize precedence into explicit tiers, defeating specificity wars without `!important`.

```css
/* 1. Define explicit ascending priority order */
@layer reset, base, components, utilities;

/* Lowest priority */
@layer reset {
  * { box-sizing: border-box; margin: 0; }
}

@layer base {
  h1 { font-size: 2rem; }
}

@layer components {
  /* High specificity inside components CANNOT beat low specificity in utilities! */
  .btn.btn-primary.large {
    background-color: blue;
  }
}

/* Highest priority */
@layer utilities {
  .bg-red {
    background-color: red; /* Easily overrides .btn.btn-primary.large! */
  }
}

/* Unlayered styles ALWAYS override @layer styles regardless of layer order! */
```

---

## 3. Typed CSS Variables (`@property`) Baseline 2024

Allows defining types, inheritance, and default initial values for custom properties, unlocking transitions on gradients and colors.

```css
@property --gradient-angle {
  syntax: "<angle>";
  inherits: false;
  initial-value: 0deg;
}

@property --brand-color {
  syntax: "<color>";
  inherits: true;
  initial-value: #2563eb;
}

/* Animating a gradient smoothly */
.rotating-card {
  background: conic-gradient(from var(--gradient-angle), #2563eb, #ec4899, #2563eb);
  transition: --gradient-angle 1s linear;
}

.rotating-card:hover {
  --gradient-angle: 360deg;
}
```

---

## 4. The `:has()` Relational Selector (Baseline 2023)

```css
/* 1. Parent conditional styling */
article:has(img) {
  grid-template-columns: 200px 1fr;
}

/* 2. Form state awareness */
form:has(:invalid) button[type="submit"] {
  opacity: 0.5;
  cursor: not-allowed;
}

/* 3. Sibling navigation hover effect (dims unhovered items) */
ul:has(li:hover) li:not(:hover) {
  opacity: 0.4;
  filter: blur(1px);
}
```

---

## 5. CSS Anchor Positioning (Baseline 2024 / 2026)

Positions a top-layer element (tooltip, dropdown) relative to an anchor anywhere in the DOM without requiring wrapping parent `position: relative`.

```css
/* 1. Designate the anchor */
.trigger-btn {
  anchor-name: --tooltip-anchor;
}

/* 2. Position the floating element */
.tooltip-popover {
  position: fixed;
  position-anchor: --tooltip-anchor;

  /* Align top of tooltip to bottom of anchor */
  top: anchor(bottom);
  left: anchor(center);
  transform: translateX(-50%);

  /* Fallback positioning if tooltip overflows screen */
  position-try-fallbacks: flip-block, flip-inline;
}
```

---

[← Previous: Cheatsheet 09 - Transitions](file:///e:/Learning/engineering-notes/01-languages/css/16-cheatsheets/09-transitions-and-animations.md) | [Next: Cheatsheet 11 - Accessibility →](file:///e:/Learning/engineering-notes/01-languages/css/16-cheatsheets/11-accessibility.md)
