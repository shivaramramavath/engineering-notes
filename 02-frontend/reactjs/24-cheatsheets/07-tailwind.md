# Tailwind CSS v4 Cheatsheet

> Quick-reference guide for Tailwind CSS v4 CSS-first configuration, theme directives, flexbox, grid, and container queries.

---

## 1. CSS-First Setup (`src/index.css`)

In Tailwind v4, `tailwind.config.js` is replaced with CSS-native `@theme` and `@import` directives:

```css
@import "tailwindcss";

@theme {
  --color-brand-primary: #0284c7;
  --color-brand-accent: #f59e0b;
  --font-display: "Inter", sans-serif;
  --breakpoint-3xl: 1920px;
}
```

---

## 2. Essential Layout Utility Classes

| Category | Classes | Result |
| :--- | :--- | :--- |
| **Flexbox** | `flex items-center justify-between gap-4` | Aligned row with even spread |
| **Grid** | `grid grid-cols-1 md:grid-cols-3 gap-6` | Responsive 1 to 3 column grid |
| **Centering** | `grid place-items-center` or `flex items-center justify-center` | Perfect center in viewport/parent |
| **Absolute Center** | `absolute inset-0 m-auto` | CSS-grid / margin auto center |
| **Aspect Ratio** | `aspect-video` (16:9), `aspect-square` (1:1) | Fixed responsive ratio |

---

## 3. Dynamic Merging with `clsx` & `tailwind-merge`

```ts
import { clsx, type ClassValue } from 'clsx';
import { twMerge } from 'tailwind-merge';

export function cn(...inputs: ClassValue[]) {
  return twMerge(clsx(inputs));
}

// Prevents collision: `p-4` properly overrides `p-2`!
cn('p-2 bg-white', isLarge && 'p-4'); // => "bg-white p-4"
```
