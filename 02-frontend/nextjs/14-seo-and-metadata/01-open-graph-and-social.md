# 01 · Open Graph and Social

How a link looks when someone pastes it into Slack, X, LinkedIn or a chat app: title, description and a preview image. You control it with `openGraph` and `twitter` metadata, or with image files that Next.js turns into the right tags.

> Verified against the Next.js 16.4 docs for Metadata, `opengraph-image` / `twitter-image` and `ImageResponse`. How each platform renders cards is the platform's decision and changes; test with their own debuggers.

## 1. What

Social platforms do not render your page. They fetch the HTML, read `<meta property="og:*">` and `<meta name="twitter:*">` tags, and build a card from them. Next.js gives you two ways to emit those tags:

| Way | Where | Use when |
|---|---|---|
| Config fields | `metadata.openGraph`, `metadata.twitter` | Text fields and static image URLs |
| File convention | `opengraph-image.*`, `twitter-image.*` | The image lives in the repo or is generated per route |

## 2. Why

- A good card increases the chance a shared link gets clicked; a missing one shows a bare URL.
- Crawlers fetch images by absolute URL, so a relative path without `metadataBase` is a common silent failure.
- Generated images let every blog post or product get its own card without a designer.

## 3. How

### 3.1 Config fields

```tsx
// app/blog/[slug]/page.tsx
import type { Metadata } from 'next'

export async function generateMetadata(
  props: PageProps<'/blog/[slug]'>
): Promise<Metadata> {
  const { slug } = await props.params
  const post = await getPost(slug)

  return {
    title: post.title,
    description: post.summary,
    openGraph: {
      title: post.title,
      description: post.summary,
      url: `/blog/${slug}`,
      siteName: 'Acme',
      type: 'article',
      publishedTime: post.publishedAt.toISOString(),
      authors: [post.author],
      images: [{ url: post.cover, width: 1200, height: 630, alt: post.title }],
    },
    twitter: {
      card: 'summary_large_image',
      title: post.title,
      description: post.summary,
      images: [post.cover],
    },
  }
}
```

Points that matter:

- With `metadataBase` set in the root layout (see [00-metadata](./00-metadata.md)), relative `url` and image paths become absolute.
- `twitter.card: 'summary_large_image'` asks for the large image card. Without a `twitter` object, platforms that read Open Graph fall back to those tags.
- **`openGraph` is replaced, not merged.** Setting it in a page discards the layout's `openGraph` (including `siteName` and `locale`). Repeat what you need, or share a base object and spread it.

```tsx
// lib/seo.ts
export const baseOpenGraph = { siteName: 'Acme', locale: 'en_US' } as const

// page
openGraph: { ...baseOpenGraph, title: post.title, type: 'article' }
```

### 3.2 Static image files

Put a file next to a route segment and Next.js adds the tags:

```text
app/
├── opengraph-image.png        → / and every route without its own
├── twitter-image.png
└── blog/
    └── [slug]/
        ├── opengraph-image.png
        └── opengraph-image.alt.txt   → alt text for that image
```

| Rule | Detail |
|---|---|
| Extensions | `.jpg`, `.jpeg`, `.png`, `.gif` |
| Size limits | `opengraph-image` 8 MB, `twitter-image` 5 MB; larger files fail the build |
| Alt text | A sibling `opengraph-image.alt.txt` (or `twitter-image.alt.txt`) |
| Precedence | File-based metadata overrides the `openGraph.images` / `twitter.images` config |

The last row is the usual cause of "my `images` config is ignored": a file in the same segment (or a parent) wins.

### 3.3 Generated images with `ImageResponse`

Use a `.tsx` or `.ts` file; the default export returns an image `Response`.

```tsx
// app/blog/[slug]/opengraph-image.tsx
import { ImageResponse } from 'next/og'
import { readFile } from 'node:fs/promises'
import { join } from 'node:path'

export const alt = 'Post cover'
export const size = { width: 1200, height: 630 }
export const contentType = 'image/png'

export default async function Image({
  params,
}: {
  params: Promise<{ slug: string }>
}) {
  const { slug } = await params
  const post = await getPost(slug)

  const font = await readFile(join(process.cwd(), 'assets/Inter-SemiBold.ttf'))

  return new ImageResponse(
    (
      <div
        style={{
          width: '100%',
          height: '100%',
          display: 'flex',
          flexDirection: 'column',
          justifyContent: 'center',
          padding: 80,
          background: '#0b0b0f',
          color: 'white',
          fontSize: 64,
        }}
      >
        <div style={{ display: 'flex' }}>{post.title}</div>
        <div style={{ display: 'flex', fontSize: 32, opacity: 0.7 }}>
          acme.dev
        </div>
      </div>
    ),
    {
      ...size,
      fonts: [{ name: 'Inter', data: font, style: 'normal', weight: 600 }],
    }
  )
}
```

Details to know:

- `params` is a Promise; `await` it, like in pages.
- Exports `alt`, `size` and `contentType` become the tag attributes. `size` of 1200×630 is the common card size.
- **Only a subset of CSS works**: flexbox layout (set `display: 'flex'` on containers with more than one child) and a limited set of properties. Grid and many advanced properties are not supported.
- Fonts are passed as data (here read from disk). Read them once at module scope if the file is large and the route is hot.
- Small logos can be inlined as base64 `data:` URLs in `<img src>`.
- By default the route is statically optimized at build time unless it uses request-time APIs.
- Several images per route: export `generateImageMetadata` returning `[{ id, size, alt, contentType }]`; the `id` arrives to the image function as a Promise.

### 3.4 HTML-limited bots

Metadata can stream after the first HTML for normal browsers. Bots that cannot run or wait for that (the docs call these HTML-limited bots, configurable with `htmlLimitedBots`) receive metadata in the initial HTML instead, so cards still work. You normally do nothing; if a custom crawler shows no tags, check that its user agent matches that list.

## 4. When

| Situation | Choose |
|---|---|
| One brand image for the whole site | One `opengraph-image.png` at the app root |
| Per-post or per-product cards from data | `opengraph-image.tsx` in the dynamic segment |
| Image already hosted on a CDN, URL known per page | `openGraph.images` in `generateMetadata` |
| Need different X card than OG | Add `twitter-image.*` or the `twitter` object |

## 5. Practical

Checklist for a shareable page:

1. `metadataBase` set once at the root.
2. `title` and `description` set (the card text falls back to them only if you do not set `openGraph` text yourself, so set both explicitly when you customize).
3. An image of about 1200×630 with `alt`.
4. `openGraph.url` or a canonical so shares point to one URL.
5. Test: view page source for `og:image` with an absolute URL, open that URL directly, then try the platform debuggers (Facebook Sharing Debugger, LinkedIn Post Inspector, X card tools where still available).

## Common mistakes

| Mistake | Effect | Fix |
|---|---|---|
| Relative image URL and no `metadataBase` | Build warning or broken preview | Set `metadataBase` |
| Page `openGraph` drops layout's fields | Missing `siteName`, `locale` | Spread a shared base object |
| Config `images` ignored | A `opengraph-image.*` file wins | Delete the file or use only one approach |
| `ImageResponse` with grid or unsupported CSS | Blank or broken image | Flexbox and supported properties only |
| Missing `display: 'flex'` on a multi-child div | Render error | Add it |
| Image over 8 MB (OG) or 5 MB (Twitter) | Build fails | Compress |
| Private or login-gated image URL | Crawler cannot fetch | Use a public URL |

## Debugging

```text
Card empty or wrong
  ├─ View source: is og:image present and absolute?   no → metadataBase / file location
  ├─ Open og:image URL in a private window: loads?    no → auth, 404, size
  ├─ Both a file and config images in the segment?    yes → the file wins
  └─ Platform shows an old card?                      → platforms cache; re-scrape in their debugger
```

## Quick Summary

- Social cards come from `og:*` and `twitter:*` tags; set them with `openGraph` / `twitter` or image files.
- `openGraph` replaces rather than merges; share a base object.
- `opengraph-image` files override config images; limits are 8 MB (OG) and 5 MB (Twitter).
- `ImageResponse` supports flexbox-style layouts only; `params` is a Promise.
- Always give crawlers absolute, public image URLs.

## Next

[02 · Sitemap and Robots](./02-sitemap-and-robots.md)