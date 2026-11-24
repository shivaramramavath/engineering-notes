# Tailwind CSS v4 — Basic to Advanced

A comprehensive reference card for Tailwind CSS v4 in modern React 19 and Vite 6 projects.

> **Target Version:** Tailwind CSS v4.0+  
> **Major Architectural Shift:** Zero-config CSS-first engine, `@theme` directive, removal of `tailwind.config.js`  
> **Engine:** Oxide engine (written in Rust), 10x faster builds  

---

## Level 0 — Concepts

- **Zero-Config by Default**: Tailwind v4 removes `tailwind.config.js`. Themes, custom colors, fonts, and breakpoints are now declared directly inside your CSS file using the `@theme` directive.
- **CSS-First Theme Configuration**: Design tokens are declared as standard CSS custom properties under `@theme`.
- **First-Class Vite Integration**: Uses `@tailwindcss/vite` plugin directly without requiring separate PostCSS or Autoprefixer configuration files.

---

## Level 1 — Basic Setup & Usage

### 1.1 Installation with Vite 6
```bash
npm install tailwindcss @tailwindcss/vite
```

### 1.2 Vite Configuration
```ts
// vite.config.ts
import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';
import tailwindcss from '@tailwindcss/vite';

export default defineConfig({
  plugins: [react(), tailwindcss()],
});
```

### 1.3 CSS Entry Point
```css
/* src/index.css */
@import "tailwindcss";

/* In Tailwind v4, custom theme tokens are declared directly in CSS! */
@theme {
  --color-brand-primary: #2563eb;
  --color-brand-accent: #f59e0b;
  --font-display: "Inter", sans-serif;
  --radius-xl: 1rem;
}
```

### 1.4 Consuming Custom Tokens in React
```tsx
export function HeroCard() {
  return (
    <div className="p-6 bg-brand-primary text-white rounded-xl font-display shadow-lg">
      <h2 className="text-2xl font-bold">Tailwind v4 Engine</h2>
      <p className="mt-2 text-blue-100">Zero-config, CSS-first architecture.</p>
    </div>
  );
}
```

---

## Level 2 — Intermediate: Dark Mode & Dynamic Variants

```css
/* src/index.css */
@import "tailwindcss";

/* Declare custom dark-mode token overrides */
@layer base {
  :root {
    --bg-page: #ffffff;
    --text-primary: #1e293b;
  }
  .dark {
    --bg-page: #0f172a;
    --text-primary: #f8fafc;
  }
}
```

```tsx
export function ThemeCard() {
  return (
    <div className="bg-[var(--bg-page)] text-[var(--text-primary)] p-4 border rounded">
      Dynamic Theme Container
    </div>
  );
}
```

---

## Level 3 — Advanced: Arbitrary Values & Container Queries

```tsx
export function ResponsiveContainerCard() {
  return (
    // Container queries built natively into v4!
    <div className="@container p-4 border rounded">
      <div className="flex flex-col @md:flex-row items-center gap-4">
        <div className="w-16 h-16 bg-blue-500 rounded-full" />
        <p className="text-sm">Adapts based on parent container width, not screen size!</p>
      </div>
    </div>
  );
}
```

---

## Level 4 — Production Checklist & Anti-Patterns

| Anti-Pattern | Why It Breaks | Recommended Practice |
| :--- | :--- | :--- |
| **Creating a `tailwind.config.js` in v4 Projects** | Config files are ignored by v4 and create confusion. | Define all tokens in CSS using `@theme`. |
| **Using Dynamic String Interpolation (`text-${color}`)**| Tailwind scans source code statically at build time; dynamic strings are purged. | Use complete class names or mapping dictionaries (`const map = { blue: 'text-blue-600' }`). |
| **Installing `postcss` and `autoprefixer` Manually**| Redundant in Vite; `@tailwindcss/vite` handles parsing and prefixing automatically. | Rely on the official `@tailwindcss/vite` plugin. |
| **Overriding Specificity with `!important`** | Creates unmaintainable CSS specificity wars. | Use `tailwind-merge` (`twMerge`) to override utility classes cleanly. |
