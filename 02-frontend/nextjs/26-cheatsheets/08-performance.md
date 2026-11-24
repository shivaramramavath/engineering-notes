# Module 26: Performance & Core Web Vitals Cheatsheet

> **Quick Reference**: `next/image`, `next/font`, `next/dynamic`, `@next/bundle-analyzer`, and RSC payload optimization techniques.

---

## 1. Core Web Vitals Targets & Fixes

| Metric | Target | Next.js Mechanism to Optimize |
| :--- | :--- | :--- |
| **LCP** (Largest Contentful Paint) | $\le 2.5\text{s}$ | `<Image priority>`, Server Component streaming shell, edge CDN caching. |
| **INP** (Interaction to Next Paint) | $\le 200\text{ms}$ | React 19 `startTransition`, debouncing URL state changes, dynamic code-splitting. |
| **CLS** (Cumulative Layout Shift) | $\le 0.1$ | `next/font` zero-layout-shift fallback, explicit width/height on `<Image>`. |

---

## 2. `next/image` Syntax Cheatsheet

```tsx
import Image from 'next/image';

// 1. Static Local Image (Automatic width/height/blurDataURL)
import heroImg from '@/public/hero.jpg';
<Image src={heroImg} alt="Hero banner" priority placeholder="blur" />

// 2. Remote Image with explicit dimensions
<Image
  src="https://images.unsplash.com/photo-123"
  alt="Scenic mountain"
  width={800}
  height={600}
  loading="lazy"
/>

// 3. Responsive Fill Container
<div className="relative w-full aspect-video">
  <Image
    src="https://cdn.example.com/cover.png"
    alt="Video cover"
    fill
    sizes="(max-width: 768px) 100vw, (max-width: 1200px) 50vw, 33vw"
    className="object-cover rounded-lg"
  />
</div>
```

---

## 3. `next/font` Zero-Layout-Shift Setup

```tsx
// app/layout.tsx
import { Inter, JetBrains_Mono } from 'next/font/google';

const inter = Inter({
  subsets: ['latin'],
  display: 'swap',
  variable: '--font-sans',
});

const mono = JetBrains_Mono({
  subsets: ['latin'],
  display: 'swap',
  variable: '--font-mono',
});

export default function RootLayout({ children }: { children: React.ReactNode }) {
  return (
    <html lang="en" className={`${inter.variable} ${mono.variable}`}>
      <body className="font-sans">{children}</body>
    </html>
  );
}
```

---

## 4. Dynamic Code Splitting with `next/dynamic`

```tsx
import dynamic from 'next/dynamic';

// Heavy chart library: disabled on server, loaded only when in viewport
const HeavyChart = dynamic(
  () => import('@/components/HeavyChart').then(mod => mod.HeavyChart),
  {
    loading: () => <div className="h-64 bg-gray-100 animate-pulse rounded" />,
    ssr: false, // Prevents SSR if component relies heavily on window/canvas
  }
);
```

---

## 5. Bundle Analyzer Configuration

```bash
npm install @next/bundle-analyzer
```

```typescript
// next.config.ts
import withBundleAnalyzer from '@next/bundle-analyzer';

const bundleAnalyzer = withBundleAnalyzer({
  enabled: process.env.ANALYZE === 'true',
});

export default bundleAnalyzer({
  // NextConfig options
});
```

Run analysis:
```bash
ANALYZE=true npm run build
```
