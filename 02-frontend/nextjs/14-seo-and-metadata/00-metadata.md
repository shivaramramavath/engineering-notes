# Metadata

Metadata is the information in a page's `<head>` that search engines and browsers use: the title, description, canonical URL, robots directives, icons and share data. The App Router generates those tags from exports in your layouts and pages, so metadata lives next to the route it describes.

> Verified against the Next.js 16.4 `generateMetadata` reference and the Metadata and OG images guide.

## What it is

Two ways to define metadata, plus special files:

| Approach | Use when |
|---|---|
| **`export const metadata`** (static object) | The values do not depend on request or data |
| **`export async function generateMetadata()`** | Values depend on `params`, `searchParams`, parent metadata or fetched data |
| **File conventions** (`favicon.ico`, `opengraph-image`, `robots`, `sitemap`) | Icons, share images, crawl files; **file-based metadata has higher priority** and overrides the config exports |

Next.js always adds `<meta charset="utf-8">` and `<meta name="viewport" content="width=device-width, initial-scale=1">`. You can override the viewport.

Rules from the docs:

- Exports work in `layout.tsx` and `page.tsx`, **Server Components only**.
- You **cannot export both** `metadata` and `generateMetadata` from the same segment.
- If metadata does not depend on request information, prefer the static `metadata` object.
- Do not add `<title>` or `<meta>` tags manually in the root layout; the Metadata API handles streaming and de-duplication.

## Static metadata

```tsx
// app/layout.tsx
import type { Metadata } from "next";

export const metadata: Metadata = {
  metadataBase: new URL("https://acme.com"),
  title: {
    default: "Acme",
    template: "%s | Acme",
  },
  description: "Acme builds tools for builders.",
  alternates: { canonical: "/" },
};

export default function RootLayout({ children }: { children: React.ReactNode }) {
  return (
    <html lang="en">
      <body>{children}</body>
    </html>
  );
}
```

```tsx
// app/about/page.tsx
import type { Metadata } from "next";

export const metadata: Metadata = {
  title: "About",                         // renders "About | Acme"
  description: "Who we are and why we build Acme.",
  alternates: { canonical: "/about" },
};
```

### Titles

| Form | Behavior |
|---|---|
| `title: "About"` | Used as is, or inserted into the nearest parent `template` |
| `title: { default: "Acme" }` | Fallback for child segments that define no title |
| `title: { template: "%s \| Acme", default: "Acme" }` | Applies to **child** segments; `default` is required with a template |
| `title: { absolute: "About" }` | Ignores parent templates |

The docs' gotchas:

- A `template` applies to **children**, not to the segment that defines it. A template in `app/layout.tsx` does not affect a `title` in the same folder's `page.tsx`... because the layout is a different file in the same segment; use a layout one level up, or put `title.absolute` where you need it.
- `title.template` in a `page.js` has no effect (a page has no children).
- If a route defines no title, the closest parent's resolved title is used.

### `metadataBase` and URL composition

Fields that need absolute URLs (canonical, `openGraph.images`, `alternates.languages`) can take relative paths **if `metadataBase` is set**. Without it, a relative path is a **build error**. Set it once in the root layout:

```ts
metadataBase: new URL("https://acme.com")
```

| Field value | Resolved |
|---|---|
| `/` or `./` | `https://acme.com` |
| `payments`, `/payments`, `./payments`, `../payments` | `https://acme.com/payments` |
| `https://beta.acme.com/payments` | unchanged (absolute URLs ignore the base) |

`metadataBase` can include a subdomain or base path. Duplicate slashes are normalized. Derive it from an environment variable so previews and production differ:

```ts
metadataBase: new URL(process.env.NEXT_PUBLIC_SITE_URL ?? "http://localhost:3000"),
```

If a `generateMetadata` function uses `"use cache"`, its return value must be serializable and `URL` instances are not, so return `url.toString()` in that case.

## Dynamic metadata

```tsx
// app/blog/[slug]/page.tsx
import type { Metadata } from "next";
import { notFound } from "next/navigation";
import { getPost } from "@/app/lib/data";          // wrapped in React cache()

export async function generateMetadata(props: PageProps<"/blog/[slug]">): Promise<Metadata> {
  const { slug } = await props.params;
  const post = await getPost(slug);
  if (!post) notFound();

  return {
    title: post.title,
    description: post.excerpt,
    alternates: { canonical: `/blog/${slug}` },
    openGraph: { type: "article", publishedTime: post.publishedAt.toISOString() },
  };
}

export default async function Page(props: PageProps<"/blog/[slug]">) {
  const { slug } = await props.params;
  const post = await getPost(slug);                // same call: executed once
  if (!post) notFound();
  return <article><h1>{post.title}</h1>{/* ... */}</article>;
}
```

`PageProps<'/blog/[slug]'>` is the generated global helper ([Routes and Params](../13-typescript/01-routes-and-params.md)); the docs' longer form types the first argument as `{ params: Promise<...>; searchParams: Promise<...> }` and adds a second `parent: ResolvingMetadata` argument.

### Avoid duplicate fetches

`fetch` calls with the same arguments are **memoized across `generateMetadata`, `generateStaticParams`, layouts, pages and Server Components**. For database calls, wrap the function in React's `cache` so the page and its metadata share one query:

```ts
import { cache } from "react";
export const getPost = cache(async (slug: string) => db.posts.findFirst({ where: { slug } }));
```

`notFound()` and `redirect()` may be called inside `generateMetadata`.

### Extending the parent

```tsx
export async function generateMetadata(props: Props, parent: ResolvingMetadata): Promise<Metadata> {
  const previousImages = (await parent).openGraph?.images || [];
  return { openGraph: { images: ["/some-specific-page-image.jpg", ...previousImages] } };
}
```

## Merging and ordering

Metadata is evaluated from the root segment down to the page. Exports from several segments are **shallowly merged**; duplicate keys are **replaced**.

```ts
// app/layout.tsx
export const metadata = { title: "Acme", openGraph: { title: "Acme", description: "Acme is a..." } };

// app/blog/page.tsx
export const metadata = { title: "Blog", openGraph: { title: "Blog" } };
// <title>Blog</title>
// <meta property="og:title" content="Blog" />      (the layout's og:description is gone)
```

| Page sets | Result |
|---|---|
| `title` only | `title` replaced; the layout's `openGraph` is **inherited** |
| `openGraph` | The **entire** layout `openGraph` object is replaced |

To share nested fields, spread a common object:

```ts
// app/shared-metadata.ts
export const openGraphImage = { images: ["https://acme.com/og.png"] };

// app/about/page.tsx
import { openGraphImage } from "../shared-metadata";
export const metadata = { openGraph: { ...openGraphImage, title: "About" } };
```

## Common fields

| Field | Purpose | Notes |
|---|---|---|
| `title`, `description` | Search result title and snippet | Unique per page; lengths below |
| `alternates.canonical` | The preferred URL for this content | Relative OK with `metadataBase` |
| `alternates.languages` | `hreflang` alternates | `{ "en-US": "/en-US", "de-DE": "/de-DE" }` |
| `alternates.types` | Feed links | `{ "application/rss+xml": "https://acme.com/rss" }` |
| `robots` | Robots meta tag | `{ index, follow, googleBot: {...} }` |
| `openGraph`, `twitter` | Share previews | [Open Graph and Social](./01-open-graph-and-social.md) |
| `icons` | Favicons | File convention preferred |
| `manifest` | Web app manifest URL | |
| `verification` | Search console ownership tags | `{ google: "..." }` |
| `keywords`, `authors`, `creator`, `publisher`, `generator`, `applicationName`, `referrer`, `formatDetection`, `category` | Other `<meta>` tags | Keywords carry little or no SEO weight; optional |

```ts
export const metadata: Metadata = {
  robots: {
    index: true,
    follow: true,
    googleBot: { index: true, follow: true, "max-image-preview": "large", "max-snippet": -1 },
  },
};
// <meta name="robots" content="index, follow" />
// <meta name="googlebot" content="index, follow, max-image-preview:large, max-snippet:-1" />
```

### Practical writing guidance

These are common practice, not Next.js rules; search engines decide what to display:

| Element | Guidance |
|---|---|
| Title | Unique, descriptive, front-load the topic; roughly 50 to 60 characters before truncation |
| Description | A one or two sentence summary; roughly 150 characters; written for people |
| Canonical | One URL per piece of content; absolute or relative-with-base; consistent with `sitemap.ts` and internal links |
| `lang` | Set `<html lang="en">` in the root layout |

### `noindex` for pages that should not appear in search

```ts
export const metadata: Metadata = { robots: { index: false, follow: false } };
```

Apply it to private areas, thank-you pages, internal search results, and preview or staging deployments. Do **not** also block these URLs in `robots.txt`, or crawlers will never fetch the page to see the `noindex` ([Sitemap and Robots](./02-sitemap-and-robots.md)). Next.js also adds `noindex` automatically on `forbidden()` and `unauthorized()` responses.

## Viewport, theme color and color scheme

`viewport`, `themeColor` and `colorScheme` moved out of `metadata` in v14. Use the `viewport` export or `generateViewport`:

```ts
import type { Viewport } from "next";

export const viewport: Viewport = {
  width: "device-width",
  initialScale: 1,
  themeColor: [
    { media: "(prefers-color-scheme: light)", color: "#ffffff" },
    { media: "(prefers-color-scheme: dark)", color: "#0a0a0a" },
  ],
};
```

Check the `generateViewport` reference for the full option list; the exact `Viewport` fields are not reproduced from the docs here.

## Streaming metadata

For dynamically rendered pages, Next.js streams metadata separately: the initial UI is sent first, and the resolved tags are appended once `generateMetadata` finishes. (Added in v15.2.)

| Visitor | Behavior |
|---|---|
| Browsers and JS-capable crawlers (for example Googlebot) | Metadata streams into the document after the initial UI; the docs say they verified Googlebot interprets it correctly |
| **HTML-limited bots** that cannot run JavaScript (for example `facebookexternalhit`, Twitterbot, Slackbot, Bingbot) | Metadata **blocks** rendering and appears in `<head>` |
| Prerendered pages | No streaming; metadata is resolved at build time |

Next.js detects HTML-limited bots from the User-Agent. Override the list with `htmlLimitedBots` in `next.config`, or turn streaming off entirely:

```ts
const config: NextConfig = { htmlLimitedBots: /.*/ };   // disables streaming metadata; may lengthen responses
```

The docs call overriding it an advanced option; the default is enough for most apps.

## With Cache Components

With `cacheComponents` on, `generateMetadata` follows the same rules as components: runtime data (`cookies()`, `headers()`, `params`, `searchParams`) or uncached fetches **defer it to request time**.

- If other parts of the page are request-time too, the static shell prerenders and metadata streams with them.
- If the page is otherwise **fully prerenderable**, Next.js raises an error so the choice is explicit. Fix it either way:

```ts
// Metadata depends on data but not on runtime values: cache it
export async function generateMetadata() {
  "use cache";
  const { title, description } = await db.query("site-metadata");
  return { title, description };
}
```

If metadata genuinely needs runtime data, add a small dynamic marker (an `await connection()` inside its own `<Suspense>`) so the rest of the page can still prerender; the reference page has the full example. See [Cache Components](../06-caching/05-cache-components.md) and the linked `blocking-prerender-metadata-*` error pages for options.

## Unsupported in the Metadata API

Render these in the layout or page yourself: `<base>`, `<noscript>`, `<style>`, `<script>` (see [Structured Data](./03-structured-data.md) for JSON-LD), `<link rel="stylesheet">` (import the CSS instead). `<meta http-equiv>` should be expressed as HTTP headers via `redirect()`, Proxy or `headers` config. Resource hints use `ReactDOM.preload`, `preconnect`, `prefetchDNS` in a Client Component; `next/font`, `next/image` and `next/script` handle their own hints.

## Debugging

| Symptom | Likely cause | Fix |
|---|---|---|
| Build error about relative URLs in metadata | No `metadataBase` | Set it in the root layout |
| `generateMetadata` ignored | Exported from a Client Component, or both `metadata` and `generateMetadata` in one segment | Server Component only; export one |
| Title lacks the site suffix | Template defined in the same segment's layout while the page is in that segment, or no `default` | Check where the `template` sits; add `default` |
| `og:description` disappeared on one page | The page set `openGraph`, replacing the parent's object | Repeat the fields or spread shared values |
| Tags missing in view-source on a dynamic page | Streaming metadata puts them in the body for normal user agents | Inspect the DOM in DevTools; check as a crawler would (`curl -A "Twitterbot" URL`) |
| Social previews empty but browsers show tags | A bot got a stale cached response, or the page timed out | Re-scrape in the platform's debugger; check `htmlLimitedBots` |
| Duplicate metadata fetches | `generateMetadata` and page query separately | `React.cache` the loader |
| Cache Components error about `generateMetadata` | Runtime or uncached data in a prerenderable page | `"use cache"` the function, or add a dynamic marker |
| `URL` cannot be serialized | `use cache` function returned a `URL` | Return a string |
| Wrong canonical on paginated or filtered URLs | Canonical derived from the request URL | Set it explicitly to the intended page |

## Common mistakes

| Mistake | Fix |
|---|---|
| The same title and description on every page | Per-route metadata |
| Hand-written `<title>` in the root layout | Metadata API |
| Missing `metadataBase` | Root layout |
| Assuming `openGraph` merges field by field | It replaces the whole object |
| Putting `noindex` pages in `robots.txt` Disallow | Allow crawling, use the `noindex` tag |
| Canonical pointing at a different, redirecting or `noindex` URL | Self-referential, indexable canonical |
| Fetching the same data twice for metadata and page | `cache()` |
| Metadata in a Client Component | Move to a Server Component and import the client part |
| Keyword stuffing | Write for people |

## Quick Summary

- Export `metadata` (static) or `generateMetadata` (dynamic) from Server Component layouts and pages; file conventions override them.
- Set `metadataBase` once; use `title.template` with a `default` for site-wide suffixes.
- Segment metadata is shallowly merged root to leaf: nested fields like `openGraph` are replaced, not combined.
- Share data between `generateMetadata` and the page with `fetch` memoization or `React.cache`.
- Streaming metadata is automatic; HTML-limited bots still get blocking `<head>` tags.
- Use `noindex` (not `robots.txt`) to keep pages out of results; set a canonical on every indexable page.

## Next

- [Open Graph and Social](./01-open-graph-and-social.md)
- [Sitemap and Robots](./02-sitemap-and-robots.md)
- [Routes and Params](../13-typescript/01-routes-and-params.md)

Sources: [generateMetadata](https://nextjs.org/docs/app/api-reference/functions/generate-metadata), [Metadata and OG images](https://nextjs.org/docs/app/getting-started/metadata-and-og-images)