# Images

Images are usually the heaviest part of a page and the most common cause of layout shift. The `<Image>` component from `next/image` extends the HTML `<img>` element to solve both: it resizes and re-encodes images on demand, serves the right size per device, lazy loads, and reserves space so the page does not jump.

> Verified against the Next.js 16.4 docs (Image component and Image Optimization guide).

## What `<Image>` gives you

| Feature | How |
|---|---|
| Right-sized images | Generates a `srcset` and serves a size that fits the device |
| Modern formats | Serves WebP by default (AVIF optional) based on the browser's `Accept` header |
| No layout shift | Requires `width`/`height` (or `fill`) so space is reserved |
| Lazy loading | Native lazy loading by default |
| Blur-up placeholders | Optional `placeholder="blur"` |
| Remote image resizing | Optimizes images from other hosts you explicitly allow |

Use plain `<img>` only for tiny decorative assets, SVG icons you inline, or when optimization is not wanted.

## Local images

### Static import (preferred)

```tsx
import Image from "next/image";
import profile from "./profile.png";

export default function Page() {
  return (
    <Image
      src={profile}
      alt="Portrait of the author"
      placeholder="blur"     // optional; blurDataURL is generated for you
    />
  );
}
```

With a static import, Next.js reads the file at build time and fills in `width`, `height` and `blurDataURL` for you. It also hashes the file content, so the optimized image can be cached for a long time and changes automatically when the file changes.

### From `public/`

```tsx
<Image src="/profile.png" alt="Portrait of the author" width={500} height={500} />
```

A path starting with `/` points into `public/`. You must give `width` and `height`, because Next.js does not read the file to find them.

### Images chosen at runtime

For filenames known only at render time, use a dynamic `import()` in a Server Component so you still get dimensions and the blur placeholder:

```tsx
async function PostImage({ filename, alt }: { filename: string; alt: string }) {
  const { default: image } = await import(`@/content/blog/images/${filename}`);
  return <Image src={image} alt={alt} />;
}
```

The path needs a **static prefix**, and every file under that prefix is bundled, so keep the directory specific.

## Remote images

```tsx
<Image
  src="https://s3.amazonaws.com/my-bucket/profile.png"
  alt="Portrait"
  width={500}
  height={500}
/>
```

Next.js cannot inspect remote files at build time, so you **must** provide `width` and `height` (or `fill`), and optionally `blurDataURL`.

You must also allow the host in `next.config.ts`. Be as specific as possible:

```ts
import type { NextConfig } from "next";

const nextConfig: NextConfig = {
  images: {
    remotePatterns: [
      {
        protocol: "https",
        hostname: "s3.amazonaws.com",
        port: "",
        pathname: "/my-bucket/**",
        search: "",
      },
    ],
  },
};

export default nextConfig;
```

Why strict: the optimizer fetches and transforms whatever URL it is allowed to. A wildcard host lets anyone use your server to optimize arbitrary images (cost and abuse). Notes from the docs:

- A remote host that responds with a **redirect** is followed without re-checking `remotePatterns` for the new location. Limit this with `maximumRedirects`.
- `domains` is deprecated; use `remotePatterns`.
- Optimizing images from private/local IP addresses is blocked by default (`dangerouslyAllowLocalIP: false`) to avoid SSRF. Enable it only on a trusted private network.
- `localPatterns` can restrict which local paths may be optimized.

## Sizing: `width`/`height`, `fill`, `sizes`

### Fixed size

```tsx
<Image src="/avatar.png" alt="" width={64} height={64} />
```

`width` and `height` set the **aspect ratio and the intrinsic size** (in pixels), not necessarily the displayed size. CSS can scale it; keep the ratio correct to avoid distortion:

```tsx
<Image src={hero} alt="" className="h-auto w-full" />
```

### `fill`: fill a container

When the size is determined by the parent (a card, a hero), use `fill`:

```tsx
<div className="relative aspect-video w-full overflow-hidden rounded-lg">
  <Image
    src="/cover.jpg"
    alt="Event cover"
    fill
    sizes="(max-width: 768px) 100vw, 50vw"
    className="object-cover"
  />
</div>
```

- The parent **must** have `position: relative` (or `fixed`/`absolute`); the image is absolutely positioned inside.
- Control cropping with CSS `object-fit` (`object-cover` crops to fill, `object-contain` fits inside).
- Give the parent a size (`aspect-video`, fixed height, grid cell), or it collapses to zero.

### `sizes`: tell the browser how big it will be

`sizes` describes the image's displayed width at different breakpoints so the browser can pick a candidate from the `srcset`.

```tsx
sizes="(max-width: 768px) 100vw, (max-width: 1200px) 50vw, 33vw"
```

Use `sizes` whenever the image is `fill` or CSS makes it responsive. **If you omit it, the browser assumes `100vw`** and may download an image far larger than needed. It also changes `srcset` generation: with `sizes`, you get a full responsive set; without it, a `fill` image is generated as if it spans the viewport, and a fixed-`width` image gets a small 1x/2x set.

## Loading priority: the LCP image

The biggest above-the-fold image is usually your Largest Contentful Paint (LCP) element and should load early.

```tsx
<Image src={hero} alt="Product hero" preload />          // adds a <link rel="preload"> in <head>
<Image src={hero} alt="Product hero" loading="eager" />   // loads immediately, no preload link
<Image src={hero} alt="Product hero" fetchPriority="high" />
```

- Next.js 16 added the **`preload`** prop and **deprecated `priority`**. Existing `priority` still appears in older code and tutorials; migrate to `preload`.
- Use `preload` only for a single, certain LCP image. The docs advise `loading="eager"` or `fetchPriority="high"` in most cases, and **not** combining `preload` with `loading` or `fetchPriority`.
- Everything else keeps the default `loading="lazy"`.
- Do not mark many images as high priority; it defeats the purpose.

## Placeholders

```tsx
<Image src={photo} alt="" placeholder="blur" />                        // static import: automatic
<Image src="https://..." alt="" width={800} height={600}
       placeholder="blur" blurDataURL="data:image/jpeg;base64,..." />  // remote: provide your own tiny image
<Image src={photo} alt="" placeholder="empty" />                       // default
```

Generate `blurDataURL` ahead of time for remote images (a ~10px version encoded as a data URL). Keep it small or you add to the HTML size.

## Quality, formats and caching

```ts
// next.config.ts
const nextConfig: NextConfig = {
  images: {
    qualities: [75],                       // allowed values for the `quality` prop
    formats: ["image/avif", "image/webp"], // order matters; first supported match wins
    minimumCacheTTL: 14400,                // seconds the optimized image is cached (default 4 hours)
  },
};
```

- **`quality`** (1–100, default 75). Since Next.js 16 the default `qualities` allow-list is `[75]`; other values passed to `quality` are **coerced to the nearest allowed value**, with a dev warning. Add the values you really use (`[50, 75, 100]`).
- **`formats`**: default is WebP. AVIF compresses smaller but takes about 50% longer to encode the first time. Each format is cached separately, so using both increases storage. If a CDN sits in front, it must forward the `Accept` header.
- **`minimumCacheTTL`**: the optimized image expires after the larger of this value and the source's `Cache-Control`. There is **no cache invalidation mechanism**, so keep it modest for images whose URL does not change (or change the `src`). Static imports are hashed, so their URLs change when content changes.

## SVG, GIF and special cases

- **SVG** is not optimized (it is already vector) and is blocked by default for security reasons. `dangerouslyAllowSVG` exists, but SVGs can contain scripts; if you enable it, keep the default `contentDispositionType` (`attachment`) and a strict `contentSecurityPolicy`. For your own SVG icons, import them as components or use plain `<img>`.
- **Animated images** are served in their original format.
- **`unoptimized`**: skips the optimizer for one image (`<Image unoptimized ... />`), useful for already-optimized sources.
- **Static export (`output: "export"`)** has no image optimization server; use a custom `loader` or `unoptimized`.
- **Custom loader**: `loader={({ src, width, quality }) => ...}` lets you point at an image CDN that does its own resizing.

## Patterns

### Responsive grid cards

```tsx
<ul className="grid grid-cols-2 gap-4 md:grid-cols-4">
  {products.map((p) => (
    <li key={p.id} className="relative aspect-square overflow-hidden rounded-lg">
      <Image src={p.image} alt={p.name} fill sizes="(max-width: 768px) 50vw, 25vw" className="object-cover" />
    </li>
  ))}
</ul>
```

### Light and dark variants

Render both and let CSS pick one (see [Theming](./03-theming.md)):

```tsx
<Image className="dark:hidden" src="/logo-light.svg" alt="Acme" width={120} height={32} />
<Image className="hidden dark:block" src="/logo-dark.svg" alt="Acme" width={120} height={32} />
```

### Art direction (different crops per breakpoint)

Use `getImageProps` from `next/image` to build the `srcset` values yourself inside a `<picture>` element.

### Background images

For a decorative background, either use `fill` with `-z-10`, or `getImageProps` and CSS `image-set()`.

## Accessibility

- **Always provide `alt`.** Describe the content for informative images; use `alt=""` for purely decorative ones so screen readers skip them.
- Do not put text inside images; use real text.
- Avoid autoplaying animated GIFs that cannot be paused.

## Debugging

| Symptom | Likely cause | Fix |
|---|---|---|
| `Invalid src prop ... hostname not configured` | Host not in `remotePatterns` | Add a specific `remotePatterns` entry and restart the dev server |
| `Image with src ... missing required "width"` | Remote or `public/` image without dimensions | Add `width` and `height`, or use `fill` |
| `fill` image invisible | Parent has no size or is not positioned | Add `relative` and a height or aspect ratio |
| Image stretched or squashed | Wrong aspect ratio | Match `width`/`height` to the real ratio; use `object-cover` |
| Huge image downloaded on mobile | Missing `sizes` | Add accurate `sizes` |
| Quality value ignored | Not in `qualities` | Add it to `images.qualities` |
| 400 Bad Request from `/_next/image` | Host blocked, or private-IP source | Check config; see `dangerouslyAllowLocalIP` caution |
| Image does not update after replacing the file | Optimizer cache (TTL) | Change the filename/`src`, or use a static import |
| AVIF not served behind a CDN | `Accept` header not forwarded | Configure the CDN |
| LCP image loads late | Lazy by default | `preload` or `loading="eager"` for that one image |

## Common mistakes

| Mistake | Fix |
|---|---|
| Using `<img>` for large content images | Use `<Image>` |
| Omitting `sizes` on `fill` or responsive images | Add `sizes` |
| `remotePatterns` with `hostname: "**"` | Allow specific hosts and paths |
| Using `priority` (deprecated in 16) | Use `preload`, or `loading="eager"` / `fetchPriority` |
| Marking every image as priority | One LCP image only |
| Forgetting a positioned, sized parent for `fill` | `relative` + height or aspect ratio |
| Empty or missing `alt` on meaningful images | Describe the image |
| Enabling SVG optimization casually | Keep SVG blocked unless you trust the source |
| Expecting instant cache invalidation | Version the URL |

## Quick Summary

- `<Image>` resizes, re-encodes, lazy loads and prevents layout shift.
- Prefer static imports (automatic size, blur and long-lived hashed caching); for remote images set `width`/`height` and a strict `remotePatterns`.
- Use `fill` with a positioned, sized parent, and always add `sizes` for responsive images.
- Next.js 16: use `preload` instead of `priority`; `qualities` defaults to `[75]`.
- Only the LCP image should load eagerly; everything else stays lazy.
- Always write meaningful `alt` text (or empty for decorative images).

## Next

- [Fonts](./05-fonts.md)
- [Static Assets](./06-static-assets.md)
- [Next Config](../00-setup/04-next-config.md)
