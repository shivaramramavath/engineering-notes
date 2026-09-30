# Caching & Compression

Two techniques for making HTTP responses faster and cheaper to deliver: telling clients when they can reuse a previous response instead of re-fetching, and shrinking responses that do need to be sent.

## HTTP Caching

### `Cache-Control` — the primary caching header

```http
Cache-Control: public, max-age=3600
```

| Directive   | Meaning                                                                                                     |
| ----------- | ----------------------------------------------------------------------------------------------------------- |
| `public`    | Can be cached by any cache — browser, CDN, proxy                                                            |
| `private`   | Can only be cached by the end user's browser, not a shared cache (CDN) — for user-specific responses        |
| `no-cache`  | Can be cached, but must be revalidated with the server before each use (not "don't cache" despite the name) |
| `no-store`  | Must never be cached, anywhere, at all — for genuinely sensitive responses                                  |
| `max-age=N` | How many seconds the response can be reused without revalidation                                            |
| `immutable` | This exact response will never change — skip revalidation entirely, even on a hard refresh                  |

```js
// Express
res.set("Cache-Control", "public, max-age=3600");
```

### `no-cache` is a confusing name

```http
Cache-Control: no-cache
```

This does **not** mean "don't cache it" — it means "cache it, but check with the server (revalidate) before using the cached copy each time." For true "never cache this," use `no-store` instead.

---

## Revalidation: `ETag` and `Last-Modified`

Rather than re-downloading a full response just to check if it changed, a client can ask "has this changed since I last saw it?" — and get a cheap `304 Not Modified` if not.

### `ETag` — a fingerprint of the response

```http
HTTP/1.1 200 OK
ETag: "a1b2c3d4"
Cache-Control: no-cache
```

On the next request, the client sends it back:

```http
GET /api/products HTTP/1.1
If-None-Match: "a1b2c3d4"
```

If the content hasn't changed (same ETag), the server responds:

```http
HTTP/1.1 304 Not Modified
```

with **no body at all** — the client reuses its existing cached copy. This saves bandwidth while still guaranteeing correctness, unlike a long `max-age` that just blindly trusts the cache for a fixed duration.

### `Last-Modified` — a simpler, date-based alternative

```http
Last-Modified: Wed, 21 Oct 2026 07:28:00 GMT
```

```http
If-Modified-Since: Wed, 21 Oct 2026 07:28:00 GMT
```

Same idea as `ETag`, but based on a timestamp rather than a content fingerprint — less precise (can't detect two different versions saved within the same second), but simpler to generate for something like a static file.

---

## Setting long cache lifetimes safely: content-hashed filenames

```
app.js          →  app.a1b2c3d4.js
styles.css       →  styles.e5f6g7h8.css
```

```http
Cache-Control: public, max-age=31536000, immutable
```

If a build process embeds a content hash in the filename, a genuinely new version of the file gets a genuinely new URL — so it's safe to tell browsers to cache the old URL essentially forever (`max-age=31536000` ≈ one year, plus `immutable`), since that exact URL's content will never change. This is the standard modern approach for frontend build output, and ties directly into the Nginx docs' `03-static-files-and-compression.md`.

---

## Caching dynamic API responses

```js
app.get("/api/products", (req, res) => {
  res.set("Cache-Control", "public, max-age=300"); // 5 minutes — a reasonable tradeoff for a catalog that changes occasionally
  res.json(products);
});
```

Only cache responses that are safe to serve slightly stale, and never cache user-specific/authenticated data with `public` — see the Nginx docs' `04-caching.md` for the equivalent concern (and the same danger of accidentally caching one user's response for another) at the reverse-proxy layer.

---

## Compression

Compressing text-based responses (JSON, HTML, CSS, JS) before sending them reduces transfer size substantially — often 60-80% smaller.

```http
Accept-Encoding: gzip, br
```

```http
Content-Encoding: gzip
```

The client advertises what it can decompress (`Accept-Encoding`); the server picks one and confirms it in `Content-Encoding` (see `02-headers-and-content-negotiation.md` for the negotiation mechanism itself).

### In Express

```bash
npm install compression
```

```js
import compression from "compression";
app.use(compression());
```

### Nginx vs Express for compression

If Nginx (or another reverse proxy) sits in front of your app, **prefer compressing there** rather than in Express — Nginx's implementation is highly optimized C code and doesn't consume any of your Node process's CPU. Use Express's `compression` middleware mainly when there's no reverse proxy in front of the app at all. Full coverage of the Nginx side lives in the Nginx docs' `03-static-files-and-compression.md`.

### What not to compress

```js
app.use(
  compression({
    filter: (req, res) => {
      if (req.headers["x-no-compression"]) return false;
      return compression.filter(req, res);
    },
  }),
);
```

Already-compressed formats — images (JPEG, PNG), videos, ZIP files — gain nothing from further compression and just waste CPU attempting it. `compression`'s default filter already skips most of these based on `Content-Type`; the override above is mainly for edge cases like letting a specific client opt out entirely.

---

## Caching and compression together

```http
Cache-Control: public, max-age=31536000, immutable
Content-Encoding: gzip
ETag: "a1b2c3d4"
```

These are independent, complementary concerns: caching avoids re-fetching a response at all; compression shrinks it when it does need to be sent (or re-validated). A well-optimized static asset typically uses both — long-lived caching via content-hashed filenames, plus compression for whatever does get transferred (the first time, or after a genuine change).

## Common mistakes

- **Assuming `no-cache` means "don't cache"** — it means "cache, but revalidate every time"; use `no-store` for truly uncacheable responses.
- **Setting `public` on a user-specific/authenticated response** — risks a shared cache (a CDN, or Nginx's `proxy_cache`) serving one user's data to another.
- **Long cache lifetimes on filenames that don't change when content does** — without a content hash, a long `max-age` means users can be stuck with stale content until it expires.
- **Compressing in both Nginx and Express redundantly** — wastes Node CPU for no benefit once Nginx is already handling it; pick one layer.
- **Compressing already-compressed formats** (images, videos, zip files) — wasted CPU for negligible or even negative size difference.

## Quick summary

- `Cache-Control` directives (`public`/`private`/`no-cache`/`no-store`/`max-age`/`immutable`) control how and for how long a response can be reused
- `ETag`/`If-None-Match` (or `Last-Modified`/`If-Modified-Since`) allow cheap revalidation via `304 Not Modified` instead of re-sending a full response
- Content-hashed filenames make very long cache lifetimes safe, since a real change always produces a new URL
- Compress text-based responses (JSON/HTML/CSS/JS); skip already-compressed formats
- Prefer compressing at the reverse proxy (Nginx) over the app (Express) when one is already in front of your app

## Section complete

That covers HTTP itself — methods, status codes, headers, content negotiation, cookies, CORS, caching, and compression. **`06-express`** builds directly on all of this through the framework most Node APIs actually use.
