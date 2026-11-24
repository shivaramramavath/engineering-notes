# Dynamic Metadata & JSON-LD Recipes (`generate-metadata-and-json-ld`)

> Advanced SEO architecture in Next.js App Router: Dynamic `generateMetadata`, OpenGraph image generation, JSON-LD structured data recipes, and canonical/alternate URL handling.

| Metadata | Details |
| :--- | :--- |
| **Next.js Version** | 15.2+ / 16.0 |
| **React Version** | React 19.0.0 |
| **Node Version** | Node.js 20+ LTS |
| **Router** | App Router (`app/`) |
| **SEO** | OpenGraph, Twitter Cards, Schema.org JSON-LD |
| **Tested With** | Next.js 15.2, Vitest 3.0, TypeScript 5.7 |

---

## 1. Concept Overview & Mental Model

### 1.1 The Role of Modern Next.js Metadata
In modern search engine and social indexing, static `<title>` and `<meta>` tags are insufficient. Search engine crawlers expect:
1. **Dynamic Metadata (`generateMetadata`)**: Asynchronously fetched page titles, descriptions, and canonical URLs matching dynamic database entities (`/products/[id]`).
2. **Dynamic Social Cards (`opengraph-image.tsx`)**: Auto-generated programmatic PNG cards.
3. **Structured Data (`JSON-LD`)**: Rich Schema.org snippets (Product, Article, Organization, BreadcrumbList) injected directly into HTML.

```
[ Incoming Crawler (Googlebot / Twitter) ]
                   │
                   ▼
[ generateMetadata() Async Execution ] ──> [ Fetches entity from DAL (deduplicated via cache()) ]
                   │
                   ├── Injects <title>, <meta name="description">
                   ├── Resolves canonical and alternate hreflang tags
                   └── Emits Schema.org <script type="application/ld+json">
```

---

## 2. Naive vs. Idiomatic Comparison

### Naive: Missing Canonical & Unsanitized JSON-LD
```tsx
// app/blog/[slug]/page.tsx (Naive - Vulnerable to XSS injection)
export default async function Page({ params }: any) {
  const post = await getPost(params.slug);
  return (
    <>
      <title>{post.title}</title>
      {/* VULNERABLE: Direct string interpolation inside script tag allows XSS! */}
      <script
        dangerouslySetInnerHTML={{
          __html: `{"@context": "https://schema.org", "headline": "${post.title}"}`,
        }}
      />
    </>
  );
}
```

### Idiomatic: Type-Safe `generateMetadata` & Safe JSON-LD
```tsx
// app/blog/[slug]/page.tsx (Idiomatic)
import type { Metadata } from 'next';
import { getPost } from '@/dal/posts';

export async function generateMetadata({
  params,
}: {
  params: Promise<{ slug: string }>;
}): Promise<Metadata> {
  const { slug } = await params;
  const post = await getPost(slug);

  if (!post) return { title: 'Post Not Found' };

  return {
    title: `${post.title} | Engineering Journal`,
    description: post.summary,
    alternates: {
      canonical: `https://app.com/blog/${slug}`,
    },
    openGraph: {
      title: post.title,
      description: post.summary,
      url: `https://app.com/blog/${slug}`,
      type: 'article',
      publishedTime: post.publishedAt.toISOString(),
    },
  };
}
```

---

## 3. Five Progressive Examples

### Example 1: Static Metadata in Root Layout
```tsx
// app/layout.tsx
import type { Metadata } from 'next';

export const metadata: Metadata = {
  metadataBase: new URL('https://mycompany.com'),
  title: {
    template: '%s | MyCompany',
    default: 'MyCompany — Cloud Infrastructure',
  },
  description: 'Enterprise serverless scaling platform.',
  robots: {
    index: true,
    follow: true,
  },
};
```

### Example 2: Safe Schema.org JSON-LD Recipe for Products
```tsx
// app/products/[id]/page.tsx
import { getProduct } from '@/dal/products';

export default async function ProductPage({ params }: { params: Promise<{ id: string }> }) {
  const { id } = await params;
  const product = await getProduct(id);

  const jsonLd = {
    '@context': 'https://schema.org',
    '@type': 'Product',
    name: product.name,
    description: product.description,
    image: product.imageUrl,
    offers: {
      '@type': 'Offer',
      price: (product.priceCents / 100).toFixed(2),
      priceCurrency: 'USD',
      availability: 'https://schema.org/InStock',
    },
  };

  return (
    <article>
      {/* Safe injection: JSON.stringify escapes XSS payloads */}
      <script
        type="application/ld+json"
        dangerouslySetInnerHTML={{ __html: JSON.stringify(jsonLd) }}
      />
      <h1>{product.name}</h1>
    </article>
  );
}
```

### Example 3: Dynamic Sitemap Generation (`sitemap.ts`)
```ts
// app/sitemap.ts
import { MetadataRoute } from 'next';
import { getAllProductIds } from '@/dal/products';

export default async function sitemap(): Promise<MetadataRoute.Sitemap> {
  const productIds = await getAllProductIds();

  const productUrls = productIds.map((id) => ({
    url: `https://mycompany.com/products/${id}`,
    lastModified: new Date(),
    changeFrequency: 'weekly' as const,
    priority: 0.8,
  }));

  return [
    { url: 'https://mycompany.com', lastModified: new Date(), priority: 1.0 },
    ...productUrls,
  ];
}
```

### Example 4: Dynamic `robots.txt` (`robots.ts`)
```ts
// app/robots.ts
import { MetadataRoute } from 'next';

export default function robots(): MetadataRoute.Robots {
  return {
    rules: {
      userAgent: '*',
      allow: '/',
      disallow: ['/admin/', '/api/'],
    },
    sitemap: 'https://mycompany.com/sitemap.xml',
  };
}
```

### Example 5: Internationalized Alternate Links (`hreflang`)
```ts
// app/[locale]/page.tsx
export async function generateMetadata({ params }: { params: Promise<{ locale: string }> }) {
  const { locale } = await params;

  return {
    alternates: {
      canonical: `https://mycompany.com/${locale}`,
      languages: {
        'en-US': 'https://mycompany.com/en',
        'es-ES': 'https://mycompany.com/es',
        'fr-FR': 'https://mycompany.com/fr',
      },
    },
  };
}
```

---

## 4. Under the Hood & Internals
Next.js resolves metadata sequentially from the root layout down to the leaf page component.
- The `generateMetadata` function of a layout runs first, followed by the page's `generateMetadata`.
- Properties are shallow-merged: leaf page values override parent values, while `title.template` dynamically inserts the child `%s`.
- Requests made inside `generateMetadata` (e.g. `getPost(slug)`) are deduplicated with identical queries in the page component via React `cache()`, causing zero database penalty.

---

## 5. Security & Safe Serialization
- **XSS Prevention in JSON-LD**: Never use template literal string interpolation inside `<script type="application/ld+json">`. Always use `JSON.stringify(object)`, which safely serializes user strings and prevents closing `</script>` tag breakout attacks.

---

## 6. Common Mistakes & Anti-Patterns (Table)

| Anti-Pattern | Root Cause | Observable Symptom | Correct Architecture |
| :--- | :--- | :--- | :--- |
| Missing `metadataBase` | Relative OG image URLs | Social cards fail to render on Twitter/LinkedIn | Define `metadataBase: new URL(...)` in layout |
| Double fetching in metadata & page | Omitting React `cache()` | Two identical SQL queries executed per request | Wrap DAL query in React `cache()` |
| String interpolation in JSON-LD | Careless scripting | Potential XSS injection | Use `JSON.stringify()` on sanitized object |
| Sync `params` in `generateMetadata` | Next.js 15+ breaking change | `[object Promise]` in titles | Await `params` in `generateMetadata` |

---

## 7. Performance & Optimization Guide
Because `generateMetadata` runs on the server before the `<head>` is flushed, keep metadata queries light. Fetch only required title and description columns (e.g. `select: { title: true, summary: true }`) rather than full entity bodies.

---

## 8. TypeScript & Strict Typing Recipes
```ts
import type { Metadata, ResolvingMetadata } from 'next';

type Props = {
  params: Promise<{ id: string }>;
  searchParams: Promise<{ [key: string]: string | string[] | undefined }>;
};

export async function generateMetadata(
  props: Props,
  parent: ResolvingMetadata
): Promise<Metadata> {
  const { id } = await props.params;
  const previousImages = (await parent).openGraph?.images || [];
  return { title: `Item ${id}`, openGraph: { images: [...previousImages] } };
}
```

---

## 9. Testing & Quality Verification

```ts
import { describe, it, expect } from 'vitest';
import { generateMetadata } from './page';

describe('generateMetadata', () => {
  it('generates correct title and canonical URL', async () => {
    const meta = await generateMetadata({ params: Promise.resolve({ slug: 'test-post' }) });
    expect(meta.title).toContain('Engineering Journal');
    expect(meta.alternates?.canonical).toBe('https://app.com/blog/test-post');
  });
});
```

---

## 10. When NOT to Use This Pattern
- Do not generate dynamic OpenGraph images for pages hidden behind authenticated paywalls or internal company intranets.

---

## 11. Frequently Asked Interview Questions

<details>
<summary>1. How does `generateMetadata` avoid double-fetching data required by the page component?</summary>
By using React's `cache()` function inside the Data Access Layer. Both `generateMetadata` and the Page component invoke the same function with the same parameters, and React deduplicates the execution so the database is queried only once per request.
</details>

<details>
<summary>2. What is `metadataBase` and why is it mandatory?</summary>
`metadataBase` defines the base URL used to resolve relative paths for OpenGraph images, canonical URLs, and alternate links. Without it, social cards and canonical URLs fail or produce warnings in production.
</details>

<details>
<summary>3. Why is JSON-LD preferred over Microdata for structured data?</summary>
JSON-LD separates data from presentation. It exists as a clean, decoupled JSON script tag in the `<head>` or `<body>`, making it easier to maintain, test, and validate than inline HTML Microdata attributes.
</details>

<details>
<summary>4. What is the role of `title.template` in root layout metadata?</summary>
It allows child pages to define their specific title (`title: 'Dashboard'`) while the root layout automatically appends branding (e.g. `Dashboard | MyCompany`).
</details>

<details>
<summary>5. How do you prevent XSS vulnerabilities when injecting JSON-LD?</summary>
Always serialize data using `JSON.stringify(object)` rather than manual string template concatenation.
</details>

<details>
<summary>6. How do `sitemap.ts` and `robots.ts` work in the App Router?</summary>
They are special file conventions in `app/` that return structured JavaScript objects, which Next.js automatically compiles and serves as valid XML and plain-text files with correct headers.
</details>

<details>
<summary>7. Can `generateMetadata` access request cookies or headers?</summary>
Yes, by calling `await cookies()` or `await headers()`. However, doing so will mark the route as dynamic (`ƒ`).
</details>

<details>
<summary>8. How do you configure Twitter Card metadata in Next.js?</summary>
Inside the `twitter` object of `Metadata`: `{ card: 'summary_large_image', title, description, images: ['/og.png'] }`.
</details>

---

## 12. Hands-on Practice Exercises

<details>
<summary>Exercise 1 (Easy): Configure root layout metadata</summary>
Define `metadataBase`, title template, and OpenGraph defaults in `app/layout.tsx`.
</details>

<details>
<summary>Exercise 2 (Medium): Create a dynamic product sitemap</summary>
Build `app/sitemap.ts` fetching all product slugs from a database.
</details>

<details>
<summary>Exercise 3 (Hard): Implement Schema.org Article JSON-LD</summary>
Write a blog post page that generates OpenGraph metadata and safe JSON-LD structured data.
</details>

---

## 13. Cross-References & Further Reading
- [Styling: Dynamic Images with @vercel/og](file:///E:/Learning/engineering-notes/02-frontend/nextjs/09-styling-and-assets/07-tailwind-v4-with-nextjs.md)
- [Official Next.js Metadata API](https://nextjs.org/docs/app/api-reference/functions/generate-metadata)
