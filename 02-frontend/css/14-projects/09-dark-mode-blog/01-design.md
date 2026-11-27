# Design System & Specifications: Technical Blog with Native Dark Mode

## 1. Dual-Theme Color Mapping (CSS `light-dark()`)

```css
:root {
  color-scheme: light dark;
  
  --bg-page: light-dark(#ffffff, #0d1117);
  --bg-surface: light-dark(#f6f8fa, #161b22);
  --border-subtle: light-dark(#d0d7de, #30363d);
  --text-main: light-dark(#1f2328, #e6edf3);
  --text-muted: light-dark(#656d76, #848d97);
  --accent-link: light-dark(#0969da, #2f81f7);
  --code-bg: light-dark(#f6f8fa, #161b22);
  --callout-bg: light-dark(#ddf4ff, rgba(56, 139, 253, 0.1));
  --callout-border: light-dark(#54aeff, rgba(56, 139, 253, 0.4));
}
```

---

## 2. Typographic Measure & Scale

- **Measure**: `max-width: 68ch;` ensuring between 55 and 75 characters per line across desktop monitors.
- **Base Font Size**: `1.125rem` (18px) with `line-height: 1.7` for reading comfort.
- **Headings**: `text-wrap: balance;` on H1, H2, and H3 elements.
- **Code Fences**: Monospace font stack with `overflow-x: auto; font-size: 0.875rem; border-radius: 8px;`.
