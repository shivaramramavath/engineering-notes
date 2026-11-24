# Module 26: Next.js Ecosystem & Core Architecture Cheatsheet

> **Quick Reference**: Comprehensive cheat sheet for Next.js 15+ & 16 architecture, runtime configurations, release baseline, and core project directory layout.

---

## 1. Release Baseline & Runtime Support

| Dimension | Supported Baseline |
| :--- | :--- |
| **Next.js Version** | Next.js 15.2+ / Next.js 16.0 (Current Generation) |
| **React Version** | React 19.0.0 (GA) / React 19 Canary |
| **Node.js LTS** | Node.js 20.x LTS / 22.x LTS (Minimum: 18.18.0) |
| **TypeScript** | TypeScript 5.7+ (`"moduleResolution": "bundler"`) |
| **Default Bundler** | Turbopack (`next dev --turbo`, `next build --turbo`) |
| **Default Styling** | Tailwind CSS v4 (`@import "tailwindcss";`) |

---

## 2. Project Directory Conventions (`src/` or Root)

```
my-next-app/
├── app/                  # App Router: file-system routing & RSC tree
│   ├── layout.tsx        # Root layout (persists state across navigation)
│   ├── page.tsx          # Home page route (/)
│   ├── loading.tsx       # Root Suspense fallback
│   ├── error.tsx         # Root Client Error Boundary ("use client")
│   ├── not-found.tsx     # 404 handler (triggered by notFound())
│   └── (routes)/         # Route segments and route groups
├── components/           # Reusable UI components (Button, Modal, etc.)
├── features/             # Feature-sliced domain modules (billing, auth)
├── lib/                  # Shared utilities (db.ts, dal.ts, utils.ts)
├── proxy.ts              # Network proxy / edge routing (Next.js 16 convention)
├── middleware.ts         # Edge routing middleware (Next.js 15 convention)
├── instrumentation.ts    # OpenTelemetry / runtime boot hook
├── next.config.ts        # Framework configuration
└── package.json          # Dependencies & scripts
```

---

## 3. Key Configuration Options (`next.config.ts`)

```typescript
import type { NextConfig } from 'next';

const nextConfig: NextConfig = {
  // Container deployment
  output: 'standalone',

  // Experimental flags
  experimental: {
    // Partial Prerendering
    ppr: true,
    // Dynamic IO / "use cache"
    dynamicIO: true,
    // Instrumentation hook
    instrumentationHook: true,
    // Server action security
    serverActions: {
      bodySizeLimit: '2mb',
      allowedOrigins: ['https://app.example.com'],
    },
  },

  // Image optimization domains / remote patterns
  images: {
    formats: ['image/avif', 'image/webp'],
    remotePatterns: [
      { protocol: 'https', hostname: 'images.unsplash.com' },
      { protocol: 'https', hostname: 'cdn.example.com' },
    ],
  },
};

export default nextConfig;
```

---

## 4. Edge vs Node.js Runtime Comparison

| Feature | Node.js Runtime (Default) | Edge Runtime |
| :--- | :--- | :--- |
| **Environment** | Full Node.js V8 execution | V8 isolate (Cloudflare Workers style) |
| **Cold Start** | ~150ms–300ms | ~5ms–15ms |
| **Node.js APIs** | Complete (`fs`, `net`, `child_process`) | Restricted subset (Web APIs only: `fetch`, `crypto`) |
| **DB Drivers** | Native TCP sockets (pg, mysql2, ioredis) | HTTP/WebSocket only (Neon, Upstash) |
| **Max Duration** | Up to 15m (serverless) / Unlimited (container)| 25s–30s limit |
| **Declaration** | `export const runtime = 'nodejs';` | `export const runtime = 'edge';` |
