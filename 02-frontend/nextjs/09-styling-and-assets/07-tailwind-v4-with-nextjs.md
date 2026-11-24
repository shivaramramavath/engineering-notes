# Tailwind CSS v4 with Next.js (`tailwind-v4-with-nextjs`)

> Integrating Tailwind CSS v4 CSS-first architecture with Next.js App Router, `@theme` directives, Lightning CSS engine, layout shifts prevention, and dynamic `@vercel/og` social cards.

| Metadata | Details |
| :--- | :--- |
| **Next.js Version** | 15.2+ / 16.0 |
| **React Version** | React 19.0.0 |
| **Node Version** | Node.js 20+ LTS |
| **Router** | App Router (`app/`) |
| **Styling** | Tailwind CSS v4 + Lightning CSS |
| **Tested With** | Next.js 15.2, Tailwind v4.0, TypeScript 5.7 |

---

## 1. Concept Overview & Mental Model

### 1.1 The Tailwind CSS v4 Revolution
Tailwind CSS v4 is a ground-up rewrite using the Rust-based **Lightning CSS** engine.
- **No `tailwind.config.js`**: Replaced entirely by standard CSS directives (`@import "tailwindcss"`, `@theme`).
- **CSS-First Configuration**: Custom design tokens, fonts, and colors are defined directly in CSS using modern CSS variables.
- **10x Faster Build Times**: Integrates directly with Next.js Turbopack via PostCSS or native loaders.

```
[ app/globals.css ] ──(@import "tailwindcss")──> [ Lightning CSS Compiler (Rust) ]
                                                              │
                                                              ▼
                                              [ Optimized Production CSS (<15kB) ]
```

---

## 2. Naive vs. Idiomatic Comparison

### Naive: Legacy v3 Configuration
```js
// tailwind.config.js (Legacy v3 - Deprecated)
module.exports = {
  content: ['./app/**/*.{js,ts,jsx,tsx}'],
  theme: {
    extend: {
      colors: { brand: '#0284c7' },
    },
  },
  plugins: [],
};
```

### Idiomatic: Modern v4 CSS-First Configuration
```css
/* app/globals.css (Modern Tailwind v4) */
@import "tailwindcss";

@theme {
  --color-brand-primary: #0284c7;
  --color-brand-accent: #f59e0b;
  --font-display: var(--font-inter), sans-serif;
}
```

---

## 3. Five Progressive Examples

### Example 1: Installing Tailwind v4 in Next.js
```bash
npm install tailwindcss @tailwindcss/postcss postcss
```
```js
// postcss.config.mjs
export default {
  plugins: {
    '@tailwindcss/postcss': {},
  },
};
```

### Example 2: Font Optimization without Layout Shift (`next/font`)
```tsx
// app/layout.tsx
import { Inter } from 'next/font/google';
import './globals.css';

const inter = Inter({
  subsets: ['latin'],
  variable: '--font-inter',
  display: 'swap', // Zero Cumulative Layout Shift (CLS)
});

export default function RootLayout({ children }: { children: React.ReactNode }) {
  return (
    <html lang="en" className={inter.variable}>
      <body className="font-sans bg-slate-950 text-slate-100">{children}</body>
    </html>
  );
}
```

### Example 3: Dynamic Social Share Image with `@vercel/og`
```tsx
// app/api/og/route.tsx
import { ImageResponse } from 'next/og';

export const runtime = 'edge';

export async function GET(request: Request) {
  const { searchParams } = new URL(request.url);
  const title = searchParams.get('title') || 'Next.js 15 Masterclass';

  return new ImageResponse(
    (
      <div
        style={{
          display: 'flex',
          height: '100%',
          width: '100%',
          alignItems: 'center',
          justifyContent: 'center',
          backgroundColor: '#020617',
          color: '#ffffff',
          fontSize: 60,
          fontWeight: 800,
        }}
      >
        {title}
      </div>
    ),
    { width: 1200, height: 630 }
  );
}
```

### Example 4: Class Merging Utility (`cn`)
```ts
// lib/utils.ts
import { clsx, type ClassValue } from 'clsx';
import { twMerge } from 'tailwind-merge';

export function cn(...inputs: ClassValue[]) {
  return twMerge(clsx(inputs));
}
```

### Example 5: Responsive Image Optimization (`next/image`)
```tsx
// components/HeroBanner.tsx
import Image from 'next/image';

export function HeroBanner() {
  return (
    <div className="relative aspect-video w-full overflow-hidden rounded-2xl">
      <Image
        src="/hero.webp"
        alt="Platform Architecture"
        fill
        priority // Prevents LCP delay
        sizes="(max-width: 768px) 100vw, (max-width: 1200px) 80vw, 1200px"
        className="object-cover"
      />
    </div>
  );
}
```

---

## 4. Under the Hood & Internals
Tailwind CSS v4 replaces JavaScript configuration parsing with native CSS `@theme` token emission. Lightning CSS parses the stylesheet, scans the JSX source files via Rust binary scanners, and compiles a deduplicated atomic stylesheet directly during the Turbopack build pass.

---

## 5. Security & Asset Safety
- **Remote Image Domains**: Never allow arbitrary remote domains in `next.config.ts`. Always declare explicit `remotePatterns` with HTTPS protocols and specific hostnames to prevent Server-Side Request Forgery (SSRF) and image optimization denial-of-service.

---

## 6. Common Mistakes & Anti-Patterns (Table)

| Anti-Pattern | Root Cause | Observable Symptom | Correct Architecture |
| :--- | :--- | :--- | :--- |
| Leaving `tailwind.config.js` | Mixing v3 and v4 | v4 ignores JS config, styles fail | Migrate variables to `@theme` in CSS |
| Missing `display: 'swap'` | Default font loading | Noticeable text jump / high CLS | Set `display: 'swap'` in `next/font` |
| Using raw `<img>` tags | Bypassing image optimizer | High LCP, uncompressed images | Use `<Image>` from `next/image` |
| Permissive `remotePatterns` | Wildcard hostname matching | Vulnerable to SSRF via image proxy | Restrict `remotePatterns` to trusted CDNs |

---

## 7. Performance & Optimization Guide
Tailwind v4's atomic compilation ensures that even a 200-page Next.js application rarely emits more than 15-20 kB of compressed CSS, guaranteeing optimal First Contentful Paint (FCP).

---

## 8. TypeScript & Strict Typing Recipes
```ts
import type { RemotePattern } from 'next/dist/shared/lib/image-config';

export const allowedImagePatterns: RemotePattern[] = [
  { protocol: 'https', hostname: 'images.unsplash.com' },
];
```

---

## 9. Testing & Quality Verification

```bash
# Verify CSS compiles cleanly
npx next build
```

---

## 10. When NOT to Use This Pattern
- If a project relies on legacy third-party Tailwind v3 plugins that have not yet published Tailwind v4 compatibility updates, use the v3 legacy compatibility layer.

---

## 11. Frequently Asked Interview Questions

<details>
<summary>1. How does Tailwind CSS v4 differ from v3?</summary>
v4 eliminates `tailwind.config.js` in favor of CSS-native configuration via `@theme`, runs on the Rust-based Lightning CSS engine for 10x faster compilation, and features automatic content detection.
</details>

<details>
<summary>2. How does `next/font` prevent Cumulative Layout Shift (CLS)?</summary>
`next/font` automatically downloads font files at build time and inlines CSS size-adjust metrics to match the fallback system font dimensions, completely eliminating layout shifts when custom fonts load.
</details>

<details>
<summary>3. What is `@vercel/og` used for?</summary>
It dynamically generates OpenGraph social preview images (1200x630) using Satori and Resvg inside an Edge runtime, rendering JSX directly into PNG images.
</details>

<details>
<summary>4. Why should `sizes` always be specified on `<Image fill>`?</summary>
Without `sizes`, Next.js assumes the image spans 100vw across all viewports and serves unnecessarily large image files to mobile screens.
</details>

<details>
<summary>5. How do you configure theme tokens in Tailwind v4?</summary>
Inside your main CSS file, use the `@theme` directive to declare CSS variables (e.g. `--color-brand: #0284c7;`).
</details>

<details>
<summary>6. How does Tailwind Merge prevent class collisions?</summary>
`twMerge('p-2', 'p-4')` resolves Tailwind class specificity conflicts, correctly emitting only `'p-4'` rather than allowing both padding rules to compete.
</details>

<details>
<summary>7. What image formats does `next/image` support by default?</summary>
AVIF and WebP, automatically negotiating the best compression format supported by the user's browser.
</details>

<details>
<summary>8. How do you secure the Next.js image optimizer?</summary>
Declare explicit `remotePatterns` in `next.config.ts` specifying allowed protocols, hostnames, and pathnames.
</details>

---

## 12. Hands-on Practice Exercises

<details>
<summary>Exercise 1 (Easy): Configure Tailwind v4 globals.css</summary>
Create a globals stylesheet using `@import "tailwindcss"` and define custom theme brand colors.
</details>

<details>
<summary>Exercise 2 (Medium): Create an OpenGraph dynamic image route</summary>
Build an `app/api/og/route.tsx` endpoint rendering dynamic post titles into social cards.
</details>

<details>
<summary>Exercise 3 (Hard): Eliminate CLS on responsive hero banner</summary>
Build an accessible responsive hero banner with `next/image` and `next/font` scoring 100 on Lighthouse.
</details>

---

## 13. Cross-References & Further Reading
- [Performance: Core Web Vitals Optimization](file:///E:/Learning/engineering-notes/02-frontend/nextjs/20-performance/README.md)
- [Official Tailwind CSS v4 Documentation](https://tailwindcss.com/docs/v4-beta)
