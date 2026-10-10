# 14 · SEO and Metadata

How to tell search engines and social networks what each page is: titles and descriptions, share images, sitemaps, crawl rules and structured data, using the App Router's built-in Metadata APIs.

> Verified against the Next.js 16.4 documentation: Metadata and OG images, `generateMetadata`, `opengraph-image`, `sitemap.xml`, `robots.txt` and the JSON-LD guide. General SEO guidance (lengths, canonicals, indexing behavior) is common practice, not Next.js documentation, and search engines change their rules; check their own docs for anything critical.

## Start here: what do you want to control?

```text
Title, description, canonical, robots tag ──► 00-metadata
How links look when shared (OG, X cards) ──► 01-open-graph-and-social
Which URLs crawlers may fetch / should know ─► 02-sitemap-and-robots
Rich results (products, articles, crumbs) ──► 03-structured-data
```

## Reading order

| # | Note | Covers |
|---|---|---|
| 00 | [Metadata](./00-metadata.md) | `metadata` and `generateMetadata`, titles and templates, `metadataBase`, merging, canonical, streaming, Cache Components |
| 01 | [Open Graph and Social](./01-open-graph-and-social.md) | `openGraph`, `twitter`, `opengraph-image` files and generated images with `ImageResponse` |
| 02 | [Sitemap and Robots](./02-sitemap-and-robots.md) | `sitemap.ts`, `generateSitemaps`, `robots.ts`, `noindex` vs `Disallow` |
| 03 | [Structured Data](./03-structured-data.md) | JSON-LD in layouts and pages, XSS-safe output, typing with `schema-dts`, validation |

## How it works in one picture

```text
layout.tsx / page.tsx
  export const metadata  |  export async function generateMetadata()
        │  (Server Components only; merged root → leaf)
        ▼
  <head> tags: title, description, canonical, robots, og:*, twitter:*, icons
Special files in the app folder
  favicon.ico, icon.*, opengraph-image.*, twitter-image.*   (override config)
  robots.(txt|ts), sitemap.(xml|ts)                          (served at /robots.txt, /sitemap.xml)
JSON-LD
  <script type="application/ld+json"> rendered in the page   (you write it)
```

## The rules to remember

1. **Every indexable page has a unique title and description,** set at the page or layout level, not hard-coded in `<head>`.
2. **Set `metadataBase` once** in the root layout so relative URLs (images, canonicals) resolve to absolute ones.
3. **Metadata exports only work in Server Components,** and a segment cannot export both `metadata` and `generateMetadata`.
4. **Nested metadata fields are replaced, not deep-merged:** a page that sets `openGraph` discards the layout's whole `openGraph` object.
5. **`robots.txt` controls crawling, not indexing.** To keep a page out of search results use a `noindex` robots tag, and do not block it in `robots.txt`, or crawlers will never see the tag.
6. **Do not hand-write `<head>` tags in the root layout.** Use the Metadata API so Next.js can stream and de-duplicate them.
7. **Escape JSON-LD** before injecting it with `dangerouslySetInnerHTML` (replace `<` with `\u003c`).
8. **Keep staging and preview deployments out of search** with a `noindex` and a restrictive `robots`.
9. **Verify with real tools:** view source or DevTools for tags, a rich-results validator for structured data, and Search Console for coverage.

## Next

[15 · Next chapter](../README.md): see the repo root README for the folder name.