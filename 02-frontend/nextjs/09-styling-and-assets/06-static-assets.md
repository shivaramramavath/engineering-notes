# Static Assets

Static assets are files served as-is: images, PDFs, downloads, icons, `robots.txt`, verification files. This note covers where they live in a Next.js project, how they are cached, and when to use something other than the `public/` folder.

> Verified against the Next.js 16.4 docs (public folder, images, metadata files).

## Where files can live

| Location | URL | Processed? | Typical use |
|---|---|---|---|
| `public/` | `/filename` | No. Served as-is | PDFs, downloads, verification files, images you reference by path |
| Next to your code, **imported** (`import img from "./a.png"`) | Hashed `/_next/static/...` | Yes: hashed, optimized, dimensions known | Images and fonts your components use |
| `app/` metadata files | Special URLs | Built into metadata | `favicon.ico`, `icon.png`, `robots.txt`, `sitemap.xml`, `manifest`, Open Graph images |
| External storage / CDN | Another origin | By that service | User uploads, large media |
| `/_next/static/` | Generated | Yes | Your own JS, CSS, imported assets (do not put files here yourself) |

## The `public/` folder

Files in `public/` at the project root are served from the base URL (`/`):

```text
public/
├── avatars/me.png        → /avatars/me.png
├── brochure.pdf          → /brochure.pdf
├── .well-known/
│   └── security.txt      → /.well-known/security.txt
└── google1234.html       → /google1234.html
```

```tsx
import Image from "next/image";

export function Avatar() {
  return <Image src="/avatars/me.png" alt="Portrait" width={64} height={64} />;
}

export function Brochure() {
  return <a href="/brochure.pdf" download>Download the brochure</a>;
}
```

Rules:

- **Do not include `public` in the URL.** `public/a.png` is `/a.png`.
- Names must not collide with your routes. A `public/about.png` is fine; a file that matches a page path is ambiguous.
- Files are looked up at **build time and server start**. Files added to `public/` while a production server is running are not guaranteed to be served. Never use `public/` for user uploads; use object storage (see [File Upload](../08-route-handlers-and-proxy/04-file-upload.md)).
- Keep secrets out of `public/`. Everything in it is world-readable.

## Caching

Next.js cannot know whether a `public/` file changed, so it cannot cache it aggressively. The default header is:

```text
Cache-Control: public, max-age=0
```

That means browsers revalidate on every use. For large or rarely changing files, set your own headers in `next.config.ts`:

```ts
import type { NextConfig } from "next";

const nextConfig: NextConfig = {
  async headers() {
    return [
      {
        source: "/downloads/:path*",
        headers: [{ key: "Cache-Control", value: "public, max-age=31536000, immutable" }],
      },
    ];
  },
};

export default nextConfig;
```

Only use `immutable` for files whose **URL changes when the content changes** (for example `report-2026-10.pdf`). If you replace a file without renaming it, visitors keep the old copy.

By contrast, **imported assets** are hashed automatically and cached for a long time by Next.js, which is a major reason to import images used by components instead of putting them in `public/`.

## Import or `public/`?

| Question | Import it | Put in `public/` |
|---|---|---|
| Used by a component as an image? | **Yes**: gets size, blur placeholder, hashed URL | Only if you need a stable URL |
| Referenced by an external system by exact URL (email, partner, `.well-known`)? | No | **Yes** |
| A downloadable file linked from content? | Possible | **Yes** (simple) |
| Changes often and must be fresh? | Hashing handles it | Add a versioned filename or no-cache header |
| User-generated? | No | **No**; use object storage |

## Special files: use the metadata conventions

For files with a standard purpose, prefer the dedicated conventions in `app/` over dropping files in `public/`:

| File | Convention | Why |
|---|---|---|
| Favicon / app icons | `app/favicon.ico`, `app/icon.png`, `app/apple-icon.png` (or generate with `icon.tsx`) | Next.js adds the correct `<link>` tags |
| `robots.txt` | `app/robots.ts` (or static `app/robots.txt`) | Can be generated from config |
| `sitemap.xml` | `app/sitemap.ts` | Can be generated from your data |
| Web app manifest | `app/manifest.ts` | Typed and generated |
| Social share images | `app/opengraph-image.png` or `.tsx` | Wired into metadata automatically |

They remain static by default unless they use request-time APIs. See [Metadata and SEO](../10-seo-and-metadata/README.md).

## Serving files from a Route Handler

When you need control that `public/` cannot give (authorization, per-request headers, generated content), return the file from a handler:

```ts
// app/api/reports/[id]/route.ts
import { NextResponse } from "next/server";
import { auth } from "@/lib/auth";

export async function GET(_req: Request, ctx: RouteContext<"/api/reports/[id]">) {
  const session = await auth();
  if (!session?.user) return NextResponse.json({ error: "Unauthorized" }, { status: 401 });

  const { id } = await ctx.params;
  const bytes = await loadReportBytes(id, session.user.id);   // your storage
  if (!bytes) return NextResponse.json({ error: "Not found" }, { status: 404 });

  return new Response(bytes, {
    headers: {
      "Content-Type": "application/pdf",
      "Content-Disposition": `attachment; filename="report-${id}.pdf"`,
      "Cache-Control": "private, no-store",
    },
  });
}
```

For private files at scale, hand out **short-lived signed URLs** from your storage provider instead of streaming bytes through your server.

## Large media and CDNs

- **Video and large downloads**: host on object storage or a media service and link to it; do not commit large binaries to the repository or serve them from your app server.
- **Static export (`output: "export"`)**: `public/` is copied as-is, but there is no image optimization server, so configure a custom image `loader` or use `unoptimized`.
- **`assetPrefix`** in `next.config` serves `/_next/static` assets from a CDN domain. `public/` files are **not** moved by it, so you may need to serve them from the CDN separately or reference absolute URLs.
- **`basePath`**: when set, `next/image` and `next/link` add it for you, but a raw `<img src="/a.png">` or `<a href="/file.pdf">` does not. Prefix manually or use the components.

## Performance and hygiene

- Compress images before committing; the optimizer helps at request time but cannot fix an unnecessarily huge source.
- Prefer `.webp`/`.avif`/`.svg` sources where appropriate; use `.woff2` for fonts.
- Give assets meaningful, lowercase, hyphenated names (`team-photo.jpg`); avoid spaces and uppercase, which behave differently across operating systems and URLs.
- Delete unused files; everything in `public/` is deployed.
- Watch repository size; use Git LFS or external storage for big binaries.

## Debugging

| Symptom | Likely cause | Fix |
|---|---|---|
| 404 for `/public/a.png` | `public` included in the URL | Use `/a.png` |
| 404 only in production | File added after build, or case mismatch (Linux is case-sensitive) | Rebuild; match the exact filename case |
| File updated but browsers show the old one | Long cache with unchanged URL | Rename the file, or lower the cache lifetime |
| Image not found with `basePath` | Raw `<img>` ignores `basePath` | Use `<Image>`, or prefix manually |
| Slow repeat downloads of a big file | Default `max-age=0` revalidation | Add a long-lived `Cache-Control` for versioned filenames |
| User upload disappears after deploy | Wrote to `public/` or local disk | Use object storage |
| Wrong `favicon` | Both `app/favicon.ico` and a `public/favicon.ico` | Keep one, in `app/` |
| Asset blocked by CORS when embedded elsewhere | Missing CORS headers on fonts/files | Add headers in `next.config.ts` for those paths |

## Common mistakes

| Mistake | Fix |
|---|---|
| Using `public/` for user uploads | Object storage with signed URLs |
| Putting component images in `public/` by habit | Import them for hashing, sizes and blur |
| Including `public/` in the URL | Reference from `/` |
| Long `immutable` caching on files you overwrite | Use versioned filenames |
| Committing multi-hundred-MB media | External storage or LFS |
| Hand-writing `robots.txt` and `sitemap.xml` for dynamic sites | Use `robots.ts` and `sitemap.ts` |
| Mixed-case and spaced filenames | Lowercase, hyphenated names |
| Storing secrets in `public/` | Never; the folder is public |

## Quick Summary

- `public/` serves files at the site root, unprocessed, with `Cache-Control: public, max-age=0` by default.
- Imported assets are hashed, optimized and cached long-term; prefer them for images your components use.
- Use `app/` metadata conventions for favicons, `robots`, `sitemap`, manifest and social images.
- Never use `public/` for user uploads or secrets; use object storage and signed URLs.
- Add custom cache headers only for files whose URL changes with their content.
- `basePath` and `assetPrefix` affect raw `<img>`/`<a>` and `public/` files differently from Next components.

## Next

- [10 · SEO and Metadata](../10-seo-and-metadata/README.md)
- [Images](./04-images.md)
- [File Upload](../08-route-handlers-and-proxy/04-file-upload.md)
