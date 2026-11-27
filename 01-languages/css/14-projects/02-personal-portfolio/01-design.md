# Design System & Specifications: Minimalist Engineer Bento Portfolio

## 1. Bento Grid Area Blueprint

The portfolio organizes 6 modular content cards into a 4-column × 3-row CSS Grid matrix on desktop viewports:

```
+-----------------------------------+-----------------+-----------------+
|                                   |                 |                 |
|  [Bio & Executive Introduction]   | [Tech Stack]    | [Status Pill]   |
|  (Span 2 Cols, Span 2 Rows)       | (Span 1 Col)    | (Span 1 Col)    |
|                                   +-----------------+-----------------+
|                                   | [Writing & Architecture Notes]    |
|                                   | (Span 2 Cols, Span 1 Row)         |
+-----------------------------------+-----------------------------------+
|  [Featured System: Distributed DB]| [Connect & PGP Key]               |
|  (Span 2 Cols, Span 1 Row)        | (Span 2 Cols, Span 1 Row)         |
+-----------------------------------+-----------------------------------+
```

---

## 2. Color Palette & Theming (Light-Dark System)

The portfolio supports zero-JS dark and light themes using CSS `light-dark()`:

```css
:root {
  color-scheme: light dark;
  --bg-page: light-dark(#f8fafc, #090d16);
  --bg-card: light-dark(#ffffff, #111827);
  --border-card: light-dark(#e2e8f0, #1f2937);
  --text-main: light-dark(#0f172a, #f9fafb);
  --text-sub: light-dark(#64748b, #9ca3af);
  --accent: light-dark(#2563eb, #38bdf8);
  --accent-glow: light-dark(rgba(37, 99, 235, 0.08), rgba(56, 189, 248, 0.12));
}
```

---

## 3. Responsive Breakpoint Strategy

| Breakpoint | Layout Strategy |
|---|---|
| **Wide Desktop (`> 1024px`)** | 4-column asymmetric Bento Grid (`grid-template-columns: repeat(4, 1fr)`) |
| **Tablet Viewport (`640px – 1023px`)** | 2-column balanced Grid (`repeat(2, 1fr)`), all cards span 1 or 2 cols |
| **Mobile Screen (`< 640px`)** | Single vertical column (`1fr`), natural order based on document hierarchy |
| **Print Output (`@media print`)** | Formatted 2-column linear document, suppression of shadows and glow effects |
