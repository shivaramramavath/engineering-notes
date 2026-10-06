# Cross-Site Scripting (XSS)

XSS happens when an attacker's text ends up running as **JavaScript in your user's browser, on your origin**. Once that happens, the script can do almost anything the user can: read the page, call your APIs with the user's session, change what they see, or capture what they type.

The root cause is always the same: **untrusted data was treated as code or markup**.

## Prerequisites

- [DOM](../14_dom-and-browser/01_dom.md) and [Element manipulation](../14_dom-and-browser/03_element-manipulation.md)
- [Browser storage](../14_dom-and-browser/08_browser-storage.md)

---

## The Three Types

| Type | Where the payload lives | Example |
|---|---|---|
| **Stored** | Saved on the server (comment, profile name), served to other users | A comment containing markup is rendered for every visitor |
| **Reflected** | Part of the request (query string), echoed in the response | `/search?q=<payload>` rendered into the page |
| **DOM-based** | Never touches the server; client-side JS reads it (`location`, `postMessage`) and writes it into the DOM unsafely | `el.innerHTML = location.hash.slice(1)` |

The fix differs by where the data lands, not by type.

---

## The Core Bug

```js
// Vulnerable: user input is parsed as HTML
const name = new URLSearchParams(location.search).get('name');
document.querySelector('#greeting').innerHTML = `Welcome, ${name}!`;
```

If `name` contains an HTML tag with an event handler, the browser parses it and runs the handler. The page never "decided" to run script; you handed the parser attacker-controlled markup.

```js
// Safe: treated as text, never parsed
document.querySelector('#greeting').textContent = `Welcome, ${name}!`;
```

---

## Dangerous Sinks to Know

Anything that turns a string into HTML or code is a sink:

| Sink | Safer alternative |
|---|---|
| `innerHTML`, `outerHTML`, `insertAdjacentHTML` | `textContent`, `createElement` + `append` |
| `document.write` | Never use |
| `eval`, `new Function`, `setTimeout("string")` | Pass functions; don't evaluate strings |
| `el.href = userUrl`, `location = userUrl` | Validate the URL scheme first (see below) |
| `el.setAttribute('onclick', ...)`, inline handlers | `addEventListener` |
| Framework escape hatches: `dangerouslySetInnerHTML` (React), `v-html` (Vue), `[innerHTML]` (Angular, sanitized by default) | Avoid, or sanitize first |
| Server templates with escaping disabled (`<%- %>`, `{{{ }}}`, `| safe`) | Use the escaping form |

Treat a `// eslint` rule or code-review flag on these as a prompt to justify each use.

---

## Defense 1: Output Encoding (the primary fix)

Encode data **for the context where it's placed**. HTML body, attribute, JavaScript, URL, and CSS contexts each need different encoding.

- Use the platform's safe APIs (`textContent`, `setAttribute` for plain attributes, DOM building).
- Rely on **auto-escaping** template engines and frameworks. React, Vue, and Angular escape interpolated values by default; the vulnerability returns when you opt out.
- If you must hand-escape for HTML text:

```js
const escapeHtml = (s) =>
  String(s)
    .replaceAll('&', '&amp;')
    .replaceAll('<', '&lt;')
    .replaceAll('>', '&gt;')
    .replaceAll('"', '&quot;')
    .replaceAll("'", '&#39;');
```

This is correct for HTML text and quoted attribute values only. It does **not** make a string safe inside a `<script>` block, an unquoted attribute, or a URL. Prefer a vetted library or framework over writing your own.

### URLs are a separate problem

A `javascript:` URL in an `href` runs code on click, and HTML-escaping doesn't stop it. Validate the scheme:

```js
function safeUrl(input) {
  try {
    const url = new URL(input, location.origin);
    return ['http:', 'https:', 'mailto:'].includes(url.protocol) ? url.href : '#';
  } catch {
    return '#';
  }
}
link.href = safeUrl(userSuppliedUrl);
```

Use an **allowlist** of schemes, never a blocklist.

---

## Defense 2: Sanitize When You Genuinely Need HTML

Sometimes users legitimately submit rich text. Don't write a regex; use a maintained sanitizer such as **DOMPurify**, which parses the HTML and removes anything outside an allowlist.

```js
import DOMPurify from 'dompurify';

const clean = DOMPurify.sanitize(userHtml);           // sensible defaults
const strict = DOMPurify.sanitize(userHtml, { ALLOWED_TAGS: ['b', 'i', 'a', 'p'], ALLOWED_ATTR: ['href'] });
element.innerHTML = clean;
```

Rules of thumb:

- Sanitize **at the point of rendering** (or store the original and sanitize on output), using the same sanitizer version you keep updated.
- Don't modify the sanitized string afterwards, since later edits can reintroduce a hole.
- Where supported, the Sanitizer API (`element.setHTML()`) is intended to do this natively; check browser support before relying on it.

---

## Defense 3: Content Security Policy (defense in depth)

CSP is a response header telling the browser which scripts it may run. It limits damage when an XSS bug slips through; it does not replace encoding.

```http
Content-Security-Policy:
  default-src 'self';
  script-src 'self' 'nonce-r4nd0m';
  object-src 'none';
  base-uri 'none'
```

- Block **inline scripts** and `eval` by default; allow your own scripts via **nonces** (a fresh random value per response) or hashes.
- Avoid `'unsafe-inline'` in `script-src`: it removes most of the protection.
- Roll out with `Content-Security-Policy-Report-Only` first and watch the reports.

```js
// Express: generate a nonce per response
import crypto from 'node:crypto';
app.use((req, res, next) => {
  res.locals.nonce = crypto.randomBytes(16).toString('base64');
  res.setHeader('Content-Security-Policy',
    `default-src 'self'; script-src 'self' 'nonce-${res.locals.nonce}'; object-src 'none'; base-uri 'none'`);
  next();
});
```

**Trusted Types** (a CSP directive supported in Chromium-based browsers) goes further: it makes dangerous sinks like `innerHTML` reject plain strings unless they came from an approved policy.

---

## Cookies and Session Impact

```http
Set-Cookie: sid=abc; HttpOnly; Secure; SameSite=Lax
```

`HttpOnly` stops scripts from **reading** the cookie, so it limits session theft. It does **not** stop XSS from *using* the session (the script can still make authenticated requests). Likewise, tokens in `localStorage` are readable by any script on the page, so an XSS bug exposes them directly.

---

## Common Mistakes and Misconceptions

| Misconception / mistake | Reality |
|---|---|
| "I filter `<script>` so I'm safe" | Blocklists fail; many payloads need no `<script>` tag (event handlers, `javascript:` URLs, SVG) |
| "Input validation prevents XSS" | It helps, but encoding on output is the real fix; valid input can still be dangerous in the wrong context |
| "React makes XSS impossible" | Only by default; `dangerouslySetInnerHTML`, `href` with user URLs, and server-side injection still apply |
| "HttpOnly cookies stop XSS" | They only limit one consequence |
| Sanitizing on the client only | The server may serve the same data to other clients (mobile, email, admin panel) |
| Trusting data from your own API or database | If it originated from a user, it's untrusted at the sink |

---

## Debugging and Testing

- Search the codebase for the sinks above; each one needs a justification.
- Try harmless probe strings in every input and see how they appear in the page source *and* in the rendered DOM; look for unescaped `<`, `"`, and `'`.
- Check DevTools → Console for CSP violation reports.
- Add a lint rule (`no-unsanitized`, or your framework's equivalent) so risky patterns fail in CI. See [Testing](../21_testing/README.md) for adding regression tests when you fix a hole.

---

## Quick Summary

- XSS = untrusted data interpreted as code/markup in the browser.
- **Fix at the sink:** use `textContent` / auto-escaping, encode for the right context, validate URL schemes with an allowlist.
- Sanitize rich HTML with a maintained library (DOMPurify), never with a regex.
- **CSP** (nonce-based, no `unsafe-inline`) and **Trusted Types** limit blast radius.
- `HttpOnly` limits cookie theft, not what injected script can do.

**Next:** [CSRF](./02_csrf.md)
