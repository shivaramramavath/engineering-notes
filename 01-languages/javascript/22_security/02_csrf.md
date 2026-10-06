# Cross-Site Request Forgery (CSRF)

CSRF tricks a logged-in user's **browser** into sending a request they didn't intend. It works because browsers automatically attach cookies to requests for a site, no matter which page triggered the request.

The attacker can't read the response; they only need the *side effect*: change an email, transfer money, delete an account.

## Prerequisites

- [HTTP fundamentals](../15_networking/01_http-fundamentals.md)
- [Headers and CORS](../15_networking/03_headers-and-cors.md)
- Cookies and sessions basics

---

## How the Attack Works

```text
1. User logs in to bank.example  → browser stores session cookie
2. User visits evil.example (another tab)
3. evil.example's page makes the browser send:
      POST https://bank.example/transfer   (auto-attaches bank.example cookies)
4. Server sees a valid session cookie → thinks the user did it
```

The server's mistake: **treating "request has a valid cookie" as "user intended this action."**

CSRF requires all of these:

1. A **cookie-based** (or other automatically attached) credential.
2. A **state-changing** request.
3. **Predictable parameters** the attacker can supply.

Remove any one and the attack fails.

---

## Defenses

### 1. SameSite cookies (first line of defense)

```http
Set-Cookie: sid=abc; HttpOnly; Secure; SameSite=Lax
```

| Value | Cookie sent on cross-site requests? |
|---|---|
| `Strict` | Never (even when following a link from another site) |
| `Lax` | Only on top-level navigations with safe methods (GET), not on cross-site POSTs, images, or fetches |
| `None` | Always (requires `Secure`) |

Modern browsers treat cookies with no `SameSite` attribute as `Lax`, but set it explicitly rather than relying on defaults, since behavior and edge cases (such as short-lived allowances for certain POST flows) have varied across browsers and versions. "Same-site" is also broader than "same-origin": subdomains of the same registrable domain count as the same site, so a vulnerable sibling subdomain can weaken this protection.

### 2. CSRF tokens (the classic fix)

The server issues an unpredictable token tied to the session; state-changing requests must include it somewhere an attacker's page can't read or set.

**Synchronizer token:** the server stores the token in the session and compares.

```js
import crypto from 'node:crypto';

// On rendering a form / bootstrapping an SPA
req.session.csrf ??= crypto.randomBytes(32).toString('hex');
res.render('form', { csrfToken: req.session.csrf });
```

```js
function verifyCsrf(req, res, next) {
  const sent = req.get('x-csrf-token') ?? req.body?._csrf;
  const expected = req.session.csrf;
  const ok = sent && expected &&
    sent.length === expected.length &&
    crypto.timingSafeEqual(Buffer.from(sent), Buffer.from(expected));
  if (!ok) return res.status(403).json({ error: 'Invalid CSRF token' });
  next();
}

app.post('/transfer', verifyCsrf, handler);
```

**Double-submit cookie:** a random value is set in a cookie *and* sent in a header/body; the server checks they match. It avoids server-side state but is weaker unless the value is signed/bound to the session and subdomains are trusted.

Notes:
- Tokens must be **unpredictable** (`crypto.randomBytes`, never `Math.random`).
- Send them in a **header or body**, never in the URL (they leak via logs and referrers).
- Prefer a maintained library over rolling your own. The once-popular `csurf` package for Express was deprecated; check current alternatives rather than assuming it's maintained.

### 3. Verify the request's origin

Reject state-changing requests whose `Origin` (or `Referer`) header isn't your own site. Modern browsers also send `Sec-Fetch-Site`, which tells you if the request is `same-origin`, `same-site`, or `cross-site`.

```js
function checkOrigin(req, res, next) {
  const origin = req.get('origin');
  if (origin && origin !== 'https://app.example.com') {
    return res.status(403).end();
  }
  next();
}
```

Use this as an extra layer alongside tokens/SameSite. Be deliberate about what you do when the header is missing.

### 4. Don't change state on GET

`GET /delete?id=5` can be triggered by an `<img>` tag. Safe methods must be read-only. Use `POST`/`PUT`/`PATCH`/`DELETE` for changes, and still protect them.

### 5. Require re-authentication for sensitive actions

Password changes, email changes, payments: ask for the password or a second factor. This protects even if other layers fail.

---

## Does CORS Stop CSRF? No.

This is the most common misconception.

- CORS controls whether a page may **read** a cross-origin response.
- A cross-site form `POST` (or "simple" request) is **still sent** with cookies; CORS doesn't block the request, only the reading of the response.
- Setting `Access-Control-Allow-Origin` too permissively with `Access-Control-Allow-Credentials: true` makes things *worse*.

Requiring `Content-Type: application/json` or a custom header does trigger a CORS preflight for cross-origin calls, which gives some incidental protection, but don't make that your only defense.

---

## APIs, SPAs, and Tokens

| Auth mechanism | CSRF-prone? | Other risk |
|---|---|---|
| Session cookie | **Yes** | Needs SameSite + token |
| `Authorization: Bearer <token>` set by your JS | No (browser doesn't auto-attach it) | If stored in `localStorage`, readable by XSS |
| Cookie with `SameSite=Strict/Lax` + HttpOnly | Mostly mitigated | Still add a token for sensitive endpoints |

CSRF and XSS interact: **any XSS bug defeats CSRF protection**, because injected script can read the token and make same-origin requests. See [XSS](./01_xss.md).

---

## Common Mistakes

| Mistake | Fix |
|---|---|
| Relying only on "we use JSON" | Add SameSite + token/Origin check |
| `SameSite=None` without need | Use `Lax` unless cross-site embedding is required |
| State-changing GET endpoints | Use non-safe methods |
| Predictable or reused tokens | Cryptographically random, per-session (or per-request) |
| Token only checked if present | Missing token must also be rejected |
| Skipping protection on "internal" admin routes | Admins are juicy targets; protect every state-changing route |
| Comparing tokens with `===` | Use `crypto.timingSafeEqual` (after a length check) |

---

## Debugging and Verification

- In DevTools → Application → Cookies, confirm `HttpOnly`, `Secure`, and `SameSite` values.
- In the Network tab, check which cookies are sent on cross-site requests, and look for rejected `403` responses on missing tokens.
- Write a test that sends a state-changing request **without** the token and with a **wrong** token, and asserts rejection. See [Integration testing](../21_testing/03_integration-testing.md).

---

## Quick Summary

- CSRF exploits **automatic cookie attachment**; the attacker triggers side effects, not data theft.
- Defend in layers: **SameSite cookies + CSRF tokens + Origin/Sec-Fetch-Site checks**, and never mutate state on GET.
- **CORS is not CSRF protection.**
- Bearer tokens in headers avoid CSRF but shift risk to XSS if stored in `localStorage`.
- Any XSS bug bypasses CSRF defenses.

**Next:** [Prototype Pollution](./03_prototype-pollution.md)
