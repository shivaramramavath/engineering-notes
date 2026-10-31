# CSRF

Cross-Site Request Forgery tricks a logged-in user's browser into sending a request they didn't intend, using credentials the browser attaches automatically. It only affects credentials that travel **automatically**, which means cookies.

**Prerequisites:** [02-threat-model.md](../01-fundamentals/02-threat-model.md), [02-cookie-sessions.md](./02-cookie-sessions.md)

---

## The attack

```text
1. You're logged in to bank.com (session cookie stored)
2. You visit evil.com, which contains:

   <form action="https://bank.com/transfer" method="POST">
     <input name="to" value="attacker"><input name="amount" value="1000">
   </form>
   <script>document.forms[0].submit()</script>

3. Your browser POSTs to bank.com and attaches bank.com's cookie
4. bank.com sees a valid session → performs the transfer
```

The attacker can't read the response (same-origin policy) or the cookie; they only need the browser to *send* it. So CSRF targets **state-changing** actions, not data theft.

---

## Does it affect this architecture?

| Credential | Sent how | CSRF-able? |
|---|---|---|
| Access JWT | `Authorization` header, set by your JS | **No**: a forged cross-site request can't add the header |
| Refresh cookie | Auto-attached by the browser | Yes in principle, but defended below |
| Session cookie (pure cookie-session design) | Auto-attached | **Yes**: needs full protection |

For the hybrid design, an attacker triggering `POST /auth/refresh` cross-site gets nothing useful (can't read the new token) but could force a rotation and a session disruption. The defenses below stop that.

---

## Defense layer 1: SameSite cookies

```js
sameSite: "strict"   // or "lax"
```

With `SameSite=Strict`, the browser omits the cookie on any cross-site request, so the forged form above arrives unauthenticated. `Lax` additionally allows cross-site **top-level GET navigations**, which is why **state changes must never use GET**.

Limits of `SameSite` as your only defense:

- It's a **site** boundary. A vulnerable sibling subdomain (`evil-blog.example.com`) is *same-site* and can send the cookie.
- Older browsers ignore it.
- Deployments needing `SameSite=None` (cross-site frontend and API) lose it entirely.

Treat it as a strong baseline, not the only layer, for cookie-session apps.

---

## Defense layer 2: Verify where the request came from

### Fetch Metadata (modern, simple)

Browsers send `Sec-Fetch-Site` on every request, and scripts cannot forge it:

```js
export const csrfOriginCheck = (req, res, next) => {
  if (["GET", "HEAD", "OPTIONS"].includes(req.method)) return next();

  const site = req.get("sec-fetch-site");
  if (site && !["same-origin", "same-site", "none"].includes(site)) {
    return res.status(403).json({ message: "Cross-site request blocked" });
  }
  next();
};
```

### Origin header check

```js
const allowed = new Set([process.env.FRONTEND_URL]);

export const checkOrigin = (req, res, next) => {
  if (["GET", "HEAD", "OPTIONS"].includes(req.method)) return next();
  const origin = req.get("origin");
  if (!origin || !allowed.has(origin)) return res.status(403).json({ message: "Bad origin" });
  next();
};
```

Compare against an **exact allowlist**, never with `includes`/`endsWith` (`https://app.example.com.evil.com` passes a sloppy check). Missing `Origin` on a cookie-authenticated state-changing request is best rejected.

---

## Defense layer 3: CSRF tokens

Use when you rely on cookies in a way `SameSite` can't fully cover.

### Synchronizer token (stateful)

Store a random token in the server-side session; embed it in forms or return it from an endpoint; require it in a header on unsafe requests; compare.

```text
GET /csrf → { csrfToken }  (stored in session)
POST /x   header X-CSRF-Token: <token>  → server compares with session value
```

### Double-submit cookie (stateless)

The server sets a random token in a **readable** cookie; the frontend copies it into a header; the server checks cookie == header. A cross-site attacker can't read your cookie to copy it into the header.

The naive form is vulnerable if an attacker can *set* cookies on your domain (e.g. via a subdomain). The robust form signs the token and binds it to the session:

```js
import crypto from "crypto";

const sign = (value, sessionId) =>
  crypto.createHmac("sha256", process.env.CSRF_SECRET).update(`${sessionId}.${value}`).digest("base64url");

export function issueCsrfToken(res, sessionId) {
  const value = crypto.randomBytes(32).toString("base64url");
  const token = `${value}.${sign(value, sessionId)}`;
  res.cookie("csrf", token, { httpOnly: false, secure: true, sameSite: "strict", path: "/" });
  return token;
}

export function verifyCsrf(req, res, next) {
  if (["GET", "HEAD", "OPTIONS"].includes(req.method)) return next();

  const header = req.get("x-csrf-token");
  const cookie = req.cookies.csrf;
  const sessionId = req.session?.id;
  if (!header || !cookie || header !== cookie || !sessionId) return res.sendStatus(403);

  const [value, sig] = header.split(".");
  const expected = sign(value, sessionId);
  const ok = sig && sig.length === expected.length &&
             crypto.timingSafeEqual(Buffer.from(sig), Buffer.from(expected));
  return ok ? next() : res.sendStatus(403);
}
```

Frontend: read the `csrf` cookie and send it back as `X-CSRF-Token` on `POST/PUT/PATCH/DELETE`.

Libraries exist (for example `csrf-csrf` for double-submit); `csurf` is deprecated and shouldn't be used in new code. Check any library's current maintenance status and docs before adopting it.

---

## Defense layer 4: Custom header requirement

A request with a custom header (like `X-Requested-With` or `Content-Type: application/json`) from another origin triggers a CORS **preflight**. With strict CORS (exact `FRONTEND_URL`, no wildcard), the preflight fails and the browser never sends the real request. So for JSON APIs:

- Require `Content-Type: application/json` and reject form content types.
- Keep CORS to a single allowed origin.

This is a useful extra layer but shouldn't be the only one: misconfigured CORS silently removes it.

---

## What to apply

| Setup | Do |
|---|---|
| Hybrid design, SPA + API on the same site | `SameSite=Strict` + `Path`-scoped refresh cookie + strict CORS + Fetch-Metadata check |
| Cookie session (pure) | `SameSite=Lax/Strict` + CSRF token (or Fetch Metadata + Origin) + no state-changing GETs |
| Cross-site frontend/API (`SameSite=None`) | CSRF token **required** + Origin allowlist + strict CORS |
| Pure Bearer-token API (no cookies) | CSRF doesn't apply |

---

## Related but different

- **Login CSRF:** forcing a victim to log in as the *attacker*. Mitigated by the same tokens/`SameSite` on the login form.
- **CORS is not CSRF protection.** CORS controls whether a page can *read* a response; the forged request is still *sent* (for "simple" requests) and its side effects happen.
- **XSS defeats CSRF defenses.** Script running on your origin can read tokens and send same-origin requests. Fix XSS first ([02-threat-model.md](../01-fundamentals/02-threat-model.md)).

---

## Common mistakes

- Changing state with `GET` requests.
- Using `SameSite=None` without adding token/Origin defenses.
- Origin checks with `includes` or `endsWith`.
- Double-submit without signing/binding.
- Putting the CSRF token in a cookie only and never requiring it in a header.
- Assuming CORS blocks forged requests.
- Exempting "internal" endpoints such as logout or settings.

## Debugging

| Symptom | Cause |
|---|---|
| Legitimate POSTs get 403 | Frontend doesn't send `X-CSRF-Token`; token cookie not readable (`httpOnly: true` on the CSRF cookie) |
| Token mismatch after login | Token bound to an old session id; reissue after login/regenerate |
| Works in Postman, fails in browser | Postman sends no `Sec-Fetch-Site`/`Origin`; browser behavior differs, so test in a real browser |
| `Sec-Fetch-Site` check blocks your own app | Frontend on a different **site** than the API; use allowlist or tokens |

---

## Quick summary

- CSRF abuses automatically-sent credentials (cookies); Bearer-header tokens are immune.
- Baseline: `SameSite` + no state-changing GETs. Add Fetch Metadata/Origin checks.
- For cookie sessions or cross-site setups add signed, session-bound CSRF tokens.
- CORS isn't CSRF protection, and XSS bypasses everything.

**Next:** [06-jwt-vs-sessions.md](./06-jwt-vs-sessions.md)
