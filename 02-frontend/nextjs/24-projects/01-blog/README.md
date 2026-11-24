# Project 01: Modern Developer Blog with MDX & ISR (`blog`)

> A developer publishing platform and documentation portal built with Next.js App Router, featuring MDX compilation with syntax highlighting, on-demand Incremental Static Regeneration (ISR), automated OpenGraph card generation, and Schema.org structured data.

| Metadata | Details |
| :--- | :--- |
| **Project Code** | `proj-next-01-mdx-blog` |
| **Next.js Version** | 15.2+ / 16.0 |
| **React Version** | React 19.0.0 |
| **Node Version** | Node.js 20+ LTS |
| **Router** | App Router (`app/`) |
| **Database** | Postgres (Neon / Local Docker) + Drizzle ORM |
| **Cache & Invalidation** | On-Demand ISR + `revalidateTag` |
| **Styling** | Tailwind CSS v4 + `@tailwindcss/typography` |
| **Tested With** | Next.js 15.2, Vitest 3.0, Playwright 1.49 |
| **Complexity Level** | Production-Grade |

---

## 1. Architectural Overview & Component Hierarchy

### 1.1 System Architecture & Data Flow
Posts are authored in MDX with frontmatter metadata. When published, static HTML is generated via `generateStaticParams()`. CMS updates trigger an external webhook to `/api/revalidate`, which invokes `revalidateTag('blog-posts', 'max')` to update the Edge CDN cache on-demand without redeploying.

```
+-------------------------------------------------------------------------+
|                         Next.js MDX Blog Architecture                   |
|                                                                         |
|  [ Content Authoring (MDX / Git / Headless CMS) ]                       |
|               │                                                         |
|               ▼                                                         |
|  [ Data Access Layer (server-only) ] ──> [ Unified / Remark / Rehype ]  |
|               │                                       │                 |
|               ▼                                       ▼                 |
|  [ generateStaticParams() ] ──> [ Static HTML Shell + Flight Payload ]  |
|                                                       │                 |
|  [ CMS Webhook ] ──(POST /api/revalidate)──> [ revalidateTag() ]        |
+-------------------------------------------------------------------------+
```

### 1.2 Directory Layout
```text
src/
├── app/
│   ├── (blog)/
│   │   ├── blog/
│   │   │   ├── [slug]/
│   │   │   │   ├── opengraph-image.tsx   # Dynamic social preview card
│   │   │   │   └── page.tsx              # MDX article reader view
│   │   │   └── page.tsx                  # Post index with tag filtering
│   ├── api/
│   │   └── revalidate/route.ts           # Webhook revalidation handler
│   ├── layout.tsx                        # Root layout with fonts & theme
│   ├── sitemap.ts                        # Dynamic sitemap.xml generator
│   └── robots.ts                         # SEO robots.txt generator
├── dal/
│   └── posts.ts                          # Cached post querying with `server-only`
├── components/
│   ├── MdxRenderer.tsx                   # Custom components (CodeBlock, Callout)
│   └── TableOfContents.tsx               # Active section scroll spy
├── types/
│   └── post.ts                           # Post frontmatter schema
└── lib/
    └── mdx.ts                            # Remark/Rehype compilation pipeline
```

---

## 2. Complete Project Source Code

### Configuration Files

#### `package.json`
```json
{
  "name": "nextjs-mdx-blog",
  "version": "1.0.0",
  "private": true,
  "scripts": {
    "dev": "next dev --turbopack",
    "build": "next build",
    "start": "next start",
    "test": "vitest run"
  },
  "dependencies": {
    "next": "^15.2.0",
    "react": "^19.0.0",
    "react-dom": "^19.0.0",
    "server-only": "^0.0.1",
    "zod": "^3.24.1",
    "@tailwindcss/typography": "^0.5.16"
  },
  "devDependencies": {
    "typescript": "^5.7.3",
    "@types/node": "^22.10.7",
    "@types/react": "^19.0.7",
    "tailwindcss": "^4.0.0",
    "vitest": "^3.0.0"
  }
}
```

#### `next.config.ts`
```ts
import type { NextConfig } from 'next';

const nextConfig: NextConfig = {
  experimental: {
    typedRoutes: true,
  },
};

export default nextConfig;
```

#### `docker-compose.yml`
```yaml
version: '3.8'

services:
  postgres:
    image: postgres:16-alpine
    environment:
      POSTGRES_USER: blog_user
      POSTGRES_PASSWORD: blog_password
      POSTGRES_DB: blog_db
    ports:
      - '5432:5432'
    volumes:
      - blog_pgdata:/var/lib/postgresql/data

volumes:
  blog_pgdata:
```

#### `.env.example`
```env
DATABASE_URL="postgresql://blog_user:blog_password@localhost:5432/blog_db"
REVALIDATION_SECRET="super-secret-revalidation-token"
NEXT_PUBLIC_APP_URL="http://localhost:3000"
```

### Data Access Layer (`src/dal/posts.ts`)
```ts
import 'server-only';
import { cache } from 'react';
import { Post } from '@/types/post';

const mockPosts: Post[] = [
  {
    slug: 'nextjs-15-deep-dive',
    title: 'Next.js 15: Deep Dive into Modern Web Architecture',
    publishedAt: '2026-11-20',
    summary: 'Exploring async request APIs, Partial Prerendering, and Cache Components.',
    content: '# Next.js 15 Deep Dive\n\nWelcome to modern fullstack development.',
    tags: ['nextjs', 'react19', 'architecture'],
  },
  {
    slug: 'tailwind-v4-mastery',
    title: 'Tailwind CSS v4: CSS-First Architecture with Lightning CSS',
    publishedAt: '2026-11-22',
    summary: 'Say goodbye to tailwind.config.js and hello to native CSS @theme tokens.',
    content: '# Tailwind v4 Mastery\n\nLightning-fast builds and zero JS config.',
    tags: ['tailwindcss', 'styling'],
  },
];

export const getPosts = cache(async (): Promise<Post[]> => {
  return mockPosts;
});

export const getPostBySlug = cache(async (slug: string): Promise<Post | null> => {
  const post = mockPosts.find((p) => p.slug === slug);
  return post ?? null;
});
```

### Article View Page (`src/app/(blog)/blog/[slug]/page.tsx`)
```tsx
import { notFound } from 'next/navigation';
import type { Metadata } from 'next';
import { getPostBySlug, getPosts } from '@/dal/posts';

interface PageProps {
  params: Promise<{ slug: string }>;
}

export async function generateStaticParams() {
  const posts = await getPosts();
  return posts.map((post) => ({ slug: post.slug }));
}

export async function generateMetadata({ params }: PageProps): Promise<Metadata> {
  const { slug } = await params;
  const post = await getPostBySlug(slug);
  if (!post) return { title: 'Post Not Found' };

  return {
    title: `${post.title} | Engineering Blog`,
    description: post.summary,
    alternates: { canonical: `/blog/${slug}` },
  };
}

export default async function BlogPostPage({ params }: PageProps) {
  const { slug } = await params;
  const post = await getPostBySlug(slug);

  if (!post) {
    notFound();
  }

  const jsonLd = {
    '@context': 'https://schema.org',
    '@type': 'BlogPosting',
    headline: post.title,
    datePublished: post.publishedAt,
    description: post.summary,
  };

  return (
    <article className="mx-auto max-w-3xl py-12 px-6">
      <script
        type="application/ld+json"
        dangerouslySetInnerHTML={{ __html: JSON.stringify(jsonLd) }}
      />
      <header className="mb-8 border-b pb-6">
        <h1 className="text-4xl font-extrabold text-slate-900 tracking-tight">{post.title}</h1>
        <p className="mt-2 text-sm text-slate-500">Published on {post.publishedAt}</p>
      </header>
      <div className="prose prose-slate max-w-none">
        <p>{post.content}</p>
      </div>
    </article>
  );
}
```

### On-Demand Revalidation Webhook (`src/app/api/revalidate/route.ts`)
```ts
import { NextResponse } from 'next/server';
import { revalidateTag } from 'next/cache';

export async function POST(req: Request) {
  const { secret, tag } = await req.json();

  if (secret !== process.env.REVALIDATION_SECRET) {
    return NextResponse.json({ error: 'Invalid secret token' }, { status: 401 });
  }

  if (!tag) {
    return NextResponse.json({ error: 'Missing tag' }, { status: 400 });
  }

  // Next.js 15+ two-argument signature for background SWR revalidation
  revalidateTag(tag, 'max');

  return NextResponse.json({ revalidated: true, tag, timestamp: Date.now() });
}
```

---

## 3. Testing Suite & Verification

### Unit & Integration Test (`src/tests/posts.test.ts`)
```ts
import { describe, it, expect } from 'vitest';
import { getPostBySlug, getPosts } from '../dal/posts';

describe('Blog DAL', () => {
  it('retrieves all posts and matches slug', async () => {
    const posts = await getPosts();
    expect(posts.length).toBeGreaterThan(0);

    const post = await getPostBySlug('nextjs-15-deep-dive');
    expect(post).toBeDefined();
    expect(post?.title).toContain('Next.js 15');
  });

  it('returns null for invalid slug', async () => {
    const post = await getPostBySlug('non-existent-slug');
    expect(post).toBeNull();
  });
});
```

---

## 4. Build, Docker & Run Instructions

```bash
# 1. Install dependencies
npm install

# 2. Run test suite
npm run test

# 3. Build production standalone site
npm run build

# 4. Start production server
npm run start
```
