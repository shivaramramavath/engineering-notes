# 02 · Sitemap and Robots

Two small files that talk to crawlers: `sitemap.xml` lists URLs you want discovered, and `robots.txt` says which paths crawlers may fetch. Next.js serves both from special files in `app/`.

> Verified against the Next.js 16.4 docs for `sitemap.xml` and `robots.txt`. How search engines use these files is general SEO knowledge, not Next.js documentation.

## 1. What

| File | URL | Purpose |
|---|---|---|
| `robots.txt` / `robots.ts` | `/robots.txt` | Crawl rules and the sitemap location |
| `sitemap.xml` / `sitemap.ts` | `/sitemap.xml` | List of canonical URLs, with optional dates and alternates |

## 2. Why

- A sitemap helps crawlers find pages that are not well linked, such as new or deep content.
- `robots.txt` keeps crawlers away from areas that waste crawl budget or are not meant to be fetched (admin, API, internal search).
- Generating both from code keeps them in sync with your data and environment.

## 3. How

### 3.1 robots

Static: drop a plain `app/robots.txt`.

Generated:

```ts
// app/robots.ts
import type { MetadataRoute } from 'next'

export default function robots(): MetadataRoute.Robots {
  return {
    rules: {
      userAgent: '*',
      allow: '/',
      disallow: ['/private/', '/api/'],
    },
    sitemap: 'https://acme.dev/sitemap.xml',
  }
}
```

Output:

```text
User-Agent: *
Allow: /
Disallow: /private/
Disallow: /api/

Sitemap: https://acme.dev/sitemap.xml
```

Per-agent rules take an array:

```ts
rules: [
  { userAgent: 'Googlebot', allow: ['/'], disallow: '/private/' },
  { userAgent: ['Applebot', 'Bingbot'], disallow: ['/'] },
],
```

The `Robots` type also has `host` and, since v16.3.0, an `other` field for extra directives not covered by the standard keys. Check the type in your editor for the exact shape before using it.

Make it environment-aware so non-production never invites crawlers:

```ts
export default function robots(): MetadataRoute.Robots {
  if (process.env.VERCEL_ENV !== 'production') {
    return { rules: { userAgent: '*', disallow: '/' } }
  }
  return { rules: { userAgent: '*', allow: '/' }, sitemap: 'https://acme.dev/sitemap.xml' }
}
```

(`VERCEL_ENV` is only an example; use whatever variable your host exposes.)

### 3.2 `robots.txt` is not `noindex`

`Disallow` stops crawlers from fetching a URL. It does not guarantee the URL stays out of search results: a blocked page can still appear if other sites link to it, shown without a snippet. To keep a page out of results:

```tsx
export const metadata: Metadata = { robots: { index: false, follow: false } }
```

and **do not** also disallow that URL in `robots.txt`; the crawler must fetch the page to see the tag. For staging, protect with authentication as the real fix, and use both `noindex` and `Disallow: /` only as a backstop.

### 3.3 sitemap

```ts
// app/sitemap.ts
import type { MetadataRoute } from 'next'

export default async function sitemap(): Promise<MetadataRoute.Sitemap> {
  const posts = await getAllPosts()

  return [
    { url: 'https://acme.dev', lastModified: new Date(), changeFrequency: 'yearly', priority: 1 },
    ...posts.map((p) => ({
      url: `https://acme.dev/blog/${p.slug}`,
      lastModified: p.updatedAt,
    })),
  ]
}
```

Notes:

- Use absolute URLs; `metadataBase` does not apply here.
- Set `lastModified` to the real content change date. A sitemap that says everything changed today on every build teaches crawlers to ignore the field. (General practice.)
- Search engines may ignore `priority` and `changeFrequency`; including them is harmless.
- Only list canonical, indexable, 200-status URLs. Do not list redirected, `noindex` or blocked pages.

Alternates, images and videos are supported through fields on each entry:

```ts
{
  url: 'https://acme.dev/about',
  lastModified: new Date(),
  alternates: {
    languages: {
      es: 'https://acme.dev/es/about',
      de: 'https://acme.dev/de/about',
    },
  },
  images: ['https://acme.dev/about.png'],
}
```

Videos use a `videos` array with fields such as `title`, `thumbnail_loc`, `description`; see the `Sitemap` type for the full list.

### 3.4 Many URLs: `generateSitemaps`

A single sitemap file is limited to 50,000 URLs. Split with `generateSitemaps`:

```ts
// app/product/sitemap.ts
import type { MetadataRoute } from 'next'

const PER_FILE = 50000

export async function generateSitemaps() {
  const total = await countProducts()
  const files = Math.ceil(total / PER_FILE)
  return Array.from({ length: files }, (_, id) => ({ id }))
}

export default async function sitemap(props: {
  id: Promise<string>
}): Promise<MetadataRoute.Sitemap> {
  const id = Number(await props.id)
  const start = id * PER_FILE

  const products = await getProducts({ offset: start, limit: PER_FILE })
  return products.map((p) => ({
    url: `https://acme.dev/product/${p.id}`,
    lastModified: p.updatedAt,
  }))
}
```

- Files are served at `/product/sitemap/0.xml`, `/product/sitemap/1.xml`, and so on.
- Since v16.0.0 the `id` is a **Promise**; `await` it. Older code that reads it synchronously breaks after upgrade. It arrives as a string, hence `Number(...)`.
- Next.js does not generate an index file for you. List the individual sitemap URLs in `robots.ts` (`sitemap: [url1, url2]`) or submit them in Search Console, or write your own sitemap index route.

### 3.5 Caching

`sitemap.ts` is cached by default, like other metadata routes, unless it uses request-time APIs. For content that changes, revalidate through your normal mechanism (`revalidateTag`, `revalidatePath`, or a `revalidate` segment config) rather than assuming it regenerates on each request.

## 4. When

| Need | Use |
|---|---|
| Small static site | `app/sitemap.xml` and `app/robots.txt` files |
| Content from a CMS or database | `sitemap.ts` |
| More than 50,000 URLs | `generateSitemaps` |
| Block non-production | Auth first; `robots.ts` returning `Disallow: /` as backstop |
| Keep a page out of search | `robots: { index: false }` metadata, not `Disallow` |

## 5. Practical

1. Create `app/robots.ts` and `app/sitemap.ts`.
2. Run the app and open `/robots.txt` and `/sitemap.xml`; read the output.
3. Confirm every sitemap URL returns 200, is canonical and is not `noindex`.
4. Submit the sitemap in Search Console and watch the coverage report.

## Common mistakes

| Mistake | Effect | Fix |
|---|---|---|
| Disallowing a page to remove it from search | May stay listed, and the `noindex` tag is never seen | Allow crawl, add `noindex` |
| Relative URLs in sitemap | Invalid entries | Absolute URLs |
| `lastModified: new Date()` for everything | Field ignored | Use real update times |
| Reading `id` synchronously in `generateSitemaps` consumer | Wrong value or type error in v16+ | `await props.id` |
| Over 50,000 URLs in one file | Rejected by crawlers | `generateSitemaps` |
| Staging indexed | Duplicate content, leaked pages | Auth, `noindex`, restrictive `robots` |
| Listing redirected or non-canonical URLs | Wasted crawl, mixed signals | Only canonical 200 URLs |

## Debugging

```text
Sitemap not picked up
  ├─ Open /sitemap.xml directly: valid XML, absolute URLs?
  ├─ Listed in robots.txt or submitted in Search Console?
  ├─ Stale after content change?   → revalidate; metadata routes are cached
  └─ URLs flagged "excluded"?      → check noindex, redirects, canonical mismatch
```

## Quick Summary

- `robots.ts` and `sitemap.ts` generate `/robots.txt` and `/sitemap.xml` with typed return values.
- `robots.txt` controls crawling; `noindex` controls indexing; never rely on `Disallow` to hide a page.
- Sitemaps need absolute, canonical, indexable URLs and honest `lastModified`.
- Split large sitemaps with `generateSitemaps`; `id` is a Promise since v16.
- Keep non-production environments out of search.

## Next

[03 · Structured Data](./03-structured-data.md)