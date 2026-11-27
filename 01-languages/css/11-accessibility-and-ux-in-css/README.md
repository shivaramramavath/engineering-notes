# Module 11: Accessibility & UX in CSS

Welcome to **Module 11: Accessibility & UX in CSS** of the comprehensive CSS learning track. This module provides authoritative, production-grade guidance on engineering web user interfaces that conform to WCAG 2.1 / 2.2 AA and AAA criteria, respect user operating system preferences, and deliver inclusive, robust user experiences for everyone.

---

## 1. Module Overview

Accessibility in CSS is neither an afterthought nor a cosmetic layer; it is an architectural contract between the browser, assistive technologies, and the human user. CSS controls visual layout, visibility, color contrast, keyboard focus indicators, touch target bounding boxes, and system media adaptation. Poor CSS choices directly cause accessibility failures, such as hiding content from screen readers while keeping it visually visible, stripping keyboard focus rings, or causing catastrophic horizontal scrolling when users zoom to 400%.

This module covers the modern Web Platform APIs and best practices for creating accessible, high-performance web experiences.

---

## 2. Directory Structure & Notes

| Note | Title | Primary Focus |
| :--- | :--- | :--- |
| **[01-focus-visible-and-focus-styles.md](01-focus-visible-and-focus-styles.md)** | Focus Visible & Focus Indicators | `:focus-visible` vs `:focus`, `:focus-within`, WCAG 2.2 focus appearance, offset outlines, custom focus rings. |
| **[02-colour-contrast-and-colour-independence.md](02-colour-contrast-and-colour-independence.md)** | Colour Contrast & Independence | WCAG AA/AAA contrast ratios, APCA (Advanced Perceptual Contrast Algorithm), not relying on color alone, icons and borders. |
| **[03-prefers-color-scheme-contrast-and-reduced-data.md](03-prefers-color-scheme-contrast-and-reduced-data.md)** | System User Preferences | `prefers-color-scheme`, `prefers-contrast`, `prefers-reduced-data`, media queries for responsive user intent. |
| **[04-forced-colors-and-high-contrast-mode.md](04-forced-colors-and-high-contrast-mode.md)** | Forced Colors & High Contrast | Windows High Contrast Mode, `forced-colors: active`, system colors (`Canvas`, `ButtonText`), transparent border fallbacks. |
| **[05-zoom-reflow-and-text-spacing.md](05-zoom-reflow-and-text-spacing.md)** | Zoom, Reflow & Text Spacing | WCAG 1.4.10 Reflow (400% zoom without horizontal scroll), WCAG 1.4.12 Text Spacing overrides, relative unit typography. |
| **[06-touch-targets-and-hit-areas.md](06-touch-targets-and-hit-areas.md)** | Touch Targets & Hit Areas | WCAG 2.2 Target Size (Minimum 24x24px, Enhanced 44x44px), CSS padding expansion, pseudo-element touch extensions. |
| **[07-visually-hidden-content-patterns.md](07-visually-hidden-content-patterns.md)** | Visually Hidden Content Patterns | The `.sr-only` / `.visually-hidden` standard snippet, why `display: none` fails screen readers, accessible skip links. |
| **[08-readable-line-length-and-spacing.md](08-readable-line-length-and-spacing.md)** | Readable Line Length & Spacing | Measure (`ch` units, 45–75 characters), `line-height` ratios, paragraph spacing, dyslexia considerations. |
| **[09-print-and-alternative-media.md](09-print-and-alternative-media.md)** | Print & Alternative Media | `@media print`, page breaks (`break-inside: avoid`), expanding link URLs via `attr(href)`, removing non-essential UI. |
| **[10-css-and-screen-readers.md](10-css-and-screen-readers.md)** | CSS & Screen Readers | How CSS transforms the Accessibility Tree, generated content `content: "..." / "alt"`, list-style removal, uppercase pronunciation. |

---

## 3. Dependency Graph

```
                                 ┌────────────────────────┐
                                 │   WCAG 2.2 STANDARDS   │
                                 │  (POUR Core Principles)│
                                 └───────────┬────────────┘
                                             │
               ┌─────────────────────────────┼─────────────────────────────┐
               ▼                             ▼                             ▼
     ┌───────────────────┐         ┌───────────────────┐         ┌───────────────────┐
     │ 01-focus-visible  │         │ 02-colour-contrast│         │ 07-visually-hidden│
     │ Keyboard Focus    │         │ APCA & Contrast   │         │ Screen Reader Text│
     └─────────┬─────────┘         └─────────┬─────────┘         └─────────┬─────────┘
               │                             │                             │
               ▼                             ▼                             ▼
     ┌───────────────────┐         ┌───────────────────┐         ┌───────────────────┐
     │ 06-touch-targets  │         │ 03-user-prefs     │         │ 10-screen-readers │
     │ Minimum 44x44px   │         │ prefers-contrast  │         │ Accessibility Tree│
     └─────────┬─────────┘         └─────────┬─────────┘         └───────────────────┘
               │                             │
               ▼                             ▼
     ┌───────────────────┐         ┌───────────────────┐
     │ 05-zoom-reflow    │         │ 04-forced-colors  │
     │ 400% Zoom & rems  │         │ Windows HCM       │
     └───────────────────┘         └───────────────────┘
```

---

## 4. Learning Objectives

By completing this module, you will be able to:
1. Implement flawless keyboard navigation focus rings using `:focus-visible`, eliminating default outline stripping bugs while adhering to WCAG 2.2 Focus Appearance criteria.
2. Calculate and verify WCAG AA (4.5:1 / 3:1) and AAA (7:1 / 4.5:1) color contrast ratios, understand APCA perceptual lightness, and guarantee information independence from color alone.
3. Handle operating system media queries (`prefers-color-scheme`, `prefers-contrast`, `prefers-reduced-data`, `forced-colors`) to adapt interfaces to individual user needs.
4. Architect layouts that pass WCAG 1.4.10 Reflow at 400% desktop zoom and respect WCAG 1.4.12 user text spacing overrides without layout breakage.
5. Expand touch targets and hit areas to meet WCAG 2.2 Target Size (Minimum and Enhanced) without compromising visual design.
6. Apply the production `.sr-only` snippet correctly, implement accessible skip-links, and control the accessibility tree without accidental screen reader omissions.
7. Craft professional print stylesheets that output clean, ink-friendly documents with automatic hyperlink URL expansion.

---

| Previous Module | Next Module |
| :--- | :--- |
| [Module 10: Architecture & Methodologies](../10-architecture-and-methodologies/README.md) | [Module 12: Performance & Debugging](../12-performance-and-debugging/README.md) |
