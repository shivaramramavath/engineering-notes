# 03 · Structured Data

Machine-readable facts about a page (this is a product, this is an article, these are the breadcrumbs) that search engines can use for rich results. The format Next.js documents is JSON-LD rendered in a `<script>` tag.

> Verified against the Next.js 16.4 JSON-LD guide for the `<script>` pattern, XSS escaping and `schema-dts` typing. The Article, BreadcrumbList and Organization shapes below are general schema.org knowledge; which properties a search engine requires for a rich result is defined by that engine's docs, so validate before shipping.

## 1. What

JSON-LD is a block of JSON that describes the page using the schema.org vocabulary:

```html
<script type="application/ld+json">
{"@context":"https://schema.org","@type":"Product","name":"Widget"}
</script>
```

It is separate from visible HTML. It does not change how the page looks.

## 2. Why

- Search engines can show richer listings (price, rating, breadcrumbs, article info) when valid markup matches the visible content.
- It is not a ranking switch. It makes the page's meaning explicit; eligibility for rich results is the search engine's decision.

## 3. How

### 3.1 In a page

Render a native `<script>` in a Server Component. Do not use `next/script` for this; the docs show a plain tag.

```tsx
// app/products/[id]/page.tsx
export default async function Page(props: PageProps<'/products/[id]'>) {
  const { id } = await props.params
  const product = await getProduct(id)

  const jsonLd = {
    '@context': 'https://schema.org',
    '@type': 'Product',
    name: product.name,
    image: product.image,
    description: product.description,
    offers: {
      '@type': 'Offer',
      price: product.price,
      priceCurrency: 'USD',
      availability: 'https://schema.org/InStock',
    },
  }

  return (
    <section>
      <script
        type="application/ld+json"
        dangerouslySetInnerHTML={{
          __html: JSON.stringify(jsonLd).replace(/</g, '\\u003c'),
        }}
      />
      {/* page content */}
    </section>
  )
}
```

### 3.2 Why escape `<`

`dangerouslySetInnerHTML` inserts the string as raw HTML. If any field (a product name, a review) contains `</script><script>…`, it breaks out of the tag and runs. `JSON.stringify` does not escape `<`. Replacing it with `<` is still valid JSON that parsers decode back to `<`, and cannot close the tag. The docs recommend this, or a library such as `serialize-javascript`.

### 3.3 A reusable component

```tsx
// components/json-ld.tsx
export function JsonLd({ data }: { data: object }) {
  return (
    <script
      type="application/ld+json"
      dangerouslySetInnerHTML={{
        __html: JSON.stringify(data).replace(/</g, '\\u003c'),
      }}
    />
  )
}
```

Escaping in one place means no page can forget it.

### 3.4 Typing with `schema-dts`

```bash
npm install -D schema-dts
```

```tsx
import type { Product, WithContext } from 'schema-dts'

const jsonLd: WithContext<Product> = {
  '@context': 'https://schema.org',
  '@type': 'Product',
  name: product.name,
  description: product.description,
}
```

The compiler then rejects unknown property names and wrong value types. It checks the vocabulary, not whether a search engine requires a given property.

### 3.5 Other common types (general knowledge)

```tsx
// Article
const article: WithContext<Article> = {
  '@context': 'https://schema.org',
  '@type': 'Article',
  headline: post.title,
  datePublished: post.publishedAt.toISOString(),
  dateModified: post.updatedAt.toISOString(),
  author: { '@type': 'Person', name: post.author },
  image: [post.cover],
}

// BreadcrumbList
const crumbs: WithContext<BreadcrumbList> = {
  '@context': 'https://schema.org',
  '@type': 'BreadcrumbList',
  itemListElement: [
    { '@type': 'ListItem', position: 1, name: 'Home', item: 'https://acme.dev' },
    { '@type': 'ListItem', position: 2, name: 'Blog', item: 'https://acme.dev/blog' },
    { '@type': 'ListItem', position: 3, name: post.title },
  ],
}

// Organization (site-wide, root layout or home page)
const org: WithContext<Organization> = {
  '@context': 'https://schema.org',
  '@type': 'Organization',
  name: 'Acme',
  url: 'https://acme.dev',
  logo: 'https://acme.dev/logo.png',
}
```

(Import `Article`, `BreadcrumbList`, `Organization` from `schema-dts`.) Several blocks on one page are fine: render one `<JsonLd>` per object.

### 3.6 Where it renders and caching

- It is part of the page's HTML, so it is cached with the page. Under Cache Components, if the data comes from a cached function it is cached with it; if it depends on request data, it sits in the dynamic part and streams like other content.
- Because it lives in the body (or layout) rather than `<head>` metadata, it does not go through the Metadata API and has no streaming-metadata special case.

## 4. When

| Page | Markup |
|---|---|
| Product detail | `Product` with `Offer` |
| Blog or news post | `Article` (or `BlogPosting`) |
| Any nested page | `BreadcrumbList` |
| Home / about | `Organization` (once) |
| Marketing page with no matching type | Skip it |

## 5. Practical

1. Add the `JsonLd` component.
2. Build the object from the same data that renders the page.
3. Validate with Google's Rich Results Test and the Schema Markup Validator (schema.org).
4. Watch the enhancement reports in Search Console after deploy.

## Common mistakes

| Mistake | Effect | Fix |
|---|---|---|
| Markup that differs from visible content (fake ratings, wrong price) | Ignored, or a manual action | Generate from the same data; only mark up what users see |
| Not escaping `<` | Script injection from user-controlled fields | `.replace(/</g, '\\u003c')` |
| Using `next/script` | Unneeded loading logic | Plain `<script type="application/ld+json">` |
| Dates not ISO 8601 | Validation warnings | `toISOString()` |
| Relative URLs for images/links | Invalid values | Absolute URLs |
| Marking up every page with the same `Organization` | Noise | Once, site-wide |
| Hand-typed JSON strings | Typos | `schema-dts` types |

## Debugging

```text
Rich result not showing
  ├─ View source: is the <script type="application/ld+json"> present and valid JSON?
  ├─ Rich Results Test: errors (required property missing) or warnings?
  ├─ Does markup match what the page shows?
  └─ Valid but still no result → eligibility is the search engine's decision; give it time
```

## Quick Summary

- JSON-LD is a `<script type="application/ld+json">` rendered by a Server Component.
- Always escape `<` as `<` before `dangerouslySetInnerHTML`; centralize it in one component.
- Type objects with `schema-dts`; generate them from the same data as the page.
- Only mark up what is visible; validate with the Rich Results Test and Schema Markup Validator.

## Next

[15 · Next chapter](../README.md): see the repo root README for the folder name.