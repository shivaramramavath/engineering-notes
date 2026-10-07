# 08 · Route Handlers and Proxy

How your app talks to the outside world: HTTP endpoints for other callers (`route.ts`), and code that runs before requests reach your routes (`proxy.ts`).

> Verified against the Next.js 16.4 documentation. In Next.js 16, `middleware.ts` was renamed to `proxy.ts` and runs on the Node.js runtime.

## Where each tool fits

```text
Browser / client / third party
        │
        ▼
   proxy.ts            ← optional: redirect, rewrite, headers, cheap checks (runs first)
        │
        ├──► page.tsx          ← HTML: Server Components read data
        ├──► route.ts          ← HTTP API, webhooks, files, non-HTML
        └──► Server Action     ← POST from your own forms (see 07)
```

| I need to... | Use |
|---|---|
| Change data from my own UI | [Server Action](../07-server-actions/README.md) |
| Read data for my own UI | Server Component |
| Expose an API, receive webhooks, return a file or feed | **Route Handler** |
| Redirect or rewrite based on cookies/headers, add headers everywhere | **Proxy** (or `next.config` for static rules) |
| Upload a file | Route Handler or Server Action (small), signed upload (large) |

## Reading order

| # | Note | Covers |
|---|---|---|
| 00 | [Route Handlers](./00-route-handlers.md) | `route.ts`, methods, dynamic params, caching, streaming, non-UI responses |
| 01 | [Request and Response](./01-request-and-response.md) | `NextRequest`, reading bodies, `NextResponse`, cookies, headers, CORS, status codes |
| 02 | [Proxy](./02-proxy.md) | `proxy.ts`, matchers, rewrites, headers, why it is not an auth system |
| 03 | [Webhooks](./03-webhooks.md) | Signature verification, idempotency, fast responses, CMS revalidation, callbacks |
| 04 | [File Upload](./04-file-upload.md) | Multipart, signed direct uploads, validation and security |

## The rules to remember

1. **Every Route Handler is a public endpoint.** Authenticate, validate, and keep error messages generic.
2. **A segment has `page` or `route`, never both.**
3. **Route Handlers are not cached by default;** only `GET` can be, and the mechanism depends on your caching model.
4. **Proxy is a convenience, not a security boundary.** Server Actions and handlers must check auth themselves.
5. **Verify webhooks on the raw body** and treat events as at-least-once.
6. **Never trust uploaded filenames or MIME types.**

## Next

[09 · Styling and Assets](../09-styling-and-assets/README.md): CSS, Tailwind, fonts, images and static assets.
