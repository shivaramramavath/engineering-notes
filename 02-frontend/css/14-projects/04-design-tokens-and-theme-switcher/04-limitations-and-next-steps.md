# Limitations & Next Steps: Enterprise Design Tokens and Theme Switcher

## 1. Architectural Limitations & Production Edge Cases

1. **`@property` Support in Older WebKit Runtimes**:
   CSS `@property` registration enables smooth cross-color interpolation. In older environments lacking `@property`, color variables snap instantaneously rather than interpolating gradually, though layout and theming function identically.
2. **Relative Color Syntax (RCS) Fallback**:
   Generating dynamic shades via `oklch(from var(--brand-color) l c h / 0.2)` is supported in Baseline modern browsers (Chrome 119+, Safari 16.4+, Firefox 128+). For older engines, maintaining dedicated alpha token values is necessary.

---

## 2. Progressive Enhancements & Next Steps

1. **Style Dictionary Token Pipeline**:
   Exporting JSON token sources into CSS, Android XML, and iOS Swift variables automatically via CI/CD pipelines.
2. **Contrast-Adaptive Tokens via `@media (prefers-contrast: more)`**:
   Injecting higher contrast token overrides automatically when users configure accessibility OS preferences.
