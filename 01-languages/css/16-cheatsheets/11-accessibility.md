# Cheatsheet 11: Accessibility

| Spec Level | WCAG 2.2 Level AA/AAA & CSS AAM 1.0 | Baseline Status | Widely Available |
|---|---|---|---|
| Browsers Tested | Chrome 120+, Firefox 125+, Safari 17.4+ | Primary Usage | Inclusive design, keyboard navigation, screen reader support, contrast |

---

## 1. Focus Visibility & Keyboard Navigation

Never suppress focus rings without providing an equal or superior `:focus-visible` replacement:

```css
/* Defective / Accessible Violation */
button:focus {
  outline: none; /* FORBIDDEN without replacement! Breaks keyboard accessibility */
}

/* Spec-Compliant Production Focus Pattern */
:focus-visible {
  outline: 3px solid #2563eb;
  outline-offset: 3px;
  border-radius: 4px;
}

/* Suppress default ring ONLY for mouse click while keeping keyboard ring */
:focus:not(:focus-visible) {
  outline: none;
}
```

---

## 2. WCAG 2.2 Color Contrast Ratios Matrix

Relative Luminance Formula:
$$L = 0.2126R + 0.7152G + 0.0722B$$
Contrast Ratio Formula:
$$\text{Ratio} = \frac{L_1 + 0.05}{L_2 + 0.05}$$

| Content Type | Minimum Ratio (AA) | Enhanced Ratio (AAA) | Notes |
|---|---|---|---|
| **Normal Text** (<18pt / <24px normal, or <14pt / <18.5px bold) | **4.5:1** | **7:1** | Body copy, table cells, form labels |
| **Large Text** ($\ge$18pt / $\ge$24px, or $\ge$14pt / $\ge$18.5px bold) | **3:1** | **4.5:1** | Section headings, banners |
| **UI Components & Graphical Objects** | **3:1** | — | Button borders, focus rings, form inputs, icons |
| **Incidental / Disabled Content** | No requirement | No requirement | Inactive buttons, decorative illustrations |

---

## 3. The Definitive `.sr-only` (Screen Reader Only) Snippet

Hides content visually from the screen while preserving full accessibility tree announcement for screen readers (NVDA, JAWS, VoiceOver):

```css
.sr-only {
  position: absolute;
  width: 1px;
  height: 1px;
  padding: 0;
  margin: -1px;
  overflow: hidden;
  clip-path: inset(50%);
  white-space: nowrap;
  border: 0;
}

/* Allow skip links to become visible on keyboard focus */
.sr-only-focusable:focus-visible {
  position: fixed;
  top: 1rem;
  left: 1rem;
  z-index: 9999;
  width: auto;
  height: auto;
  padding: 0.75rem 1.5rem;
  margin: 0;
  clip-path: none;
  background-color: #0f172a;
  color: #ffffff;
  border-radius: 6px;
  outline: 3px solid #38bdf8;
}
```

---

## 4. Touch Targets & Pointer Sizing (WCAG 2.5.8 & 2.5.5)

| Guideline | Target Size | Exceptions |
|---|---|---|
| **WCAG 2.2 Level AA (2.5.8)** | $\mathbf{24 \times 24\text{px}}$ minimum bounding box (or sufficient spacing) | Inline text links in a paragraph |
| **WCAG 2.2 Level AAA (2.5.5)** | $\mathbf{44 \times 44\text{px}}$ minimum bounding box | Essential controls |

```css
/* Expanding clickable hit-box without altering visual typography */
.small-icon-btn {
  position: relative;
  display: inline-flex;
  align-items: center;
  justify-content: center;
  width: 24px;
  height: 24px;
}

.small-icon-btn::after {
  content: "";
  position: absolute;
  inset: -10px; /* Expands hit target to 44x44px! */
}
```

---

## 5. Assistive Technology Guardrails

```css
/* 1. Windows High Contrast Mode Fix */
@media (forced-colors: active) {
  .badge, .card {
    border: 1px solid CanvasText; /* Guarantees boundary visibility */
  }
}

/* 2. Content Zoom Safety: Always define minimum text using relative units */
body {
  font-size: 1rem; /* Respects user's OS / browser base preference */
}

/* 3. Never use display: none on content needed by screen readers */
/* Use aria-hidden="true" for purely visual decorations */
.decorative-icon {
  speak: none;
}
```

---

[← Previous: Cheatsheet 10 - Modern Features](file:///e:/Learning/engineering-notes/01-languages/css/16-cheatsheets/10-modern-features.md) | [Next: Cheatsheet 12 - Common Bugs and Fixes →](file:///e:/Learning/engineering-notes/01-languages/css/16-cheatsheets/12-common-bugs-and-fixes.md)
