# Security Checklist

A practical, scannable checklist for web apps and Node services, with pointers to the notes that explain each item. Use it in design reviews, before releases, and when reviewing pull requests.

It is a **baseline**, not a guarantee. Threat-model anything that handles money, health data, or credentials in more depth.

## Prerequisites

Read the topic notes first: [XSS](./01_xss.md), [CSRF](./02_csrf.md), [Prototype pollution](./03_prototype-pollution.md), [Input validation](./04_input-validation.md), [Dependency security](./05_dependency-security.md).

---

## 1. Input and Output

- [ ] All external input is **validated server-side** against a schema; unknown fields rejected ([Input validation](./04_input-validation.md))
- [ ] Database access uses **parameterized queries / ORM bindings**, never string concatenation
- [ ] No `exec` with user input; use `execFile` with argument arrays
- [ ] File paths from users are **resolved and checked** against a base directory
- [ ] Output is **encoded for its context**; no `innerHTML` / `dangerouslySetInnerHTML` with untrusted data ([XSS](./01_xss.md))
- [ ] Rich text is sanitized with a maintained sanitizer (e.g. DOMPurify)
- [ ] Request body size, upload size, array length, and pagination limits are enforced
- [ ] No unsafe recursive merge/set with user-controlled keys ([Prototype pollution](./03_prototype-pollution.md))

## 2. Authentication and Sessions

- [ ] Passwords hashed with a **slow, salted algorithm** (argon2, bcrypt, or scrypt), never plain or fast hashes (MD5/SHA-1/SHA-256 alone)
- [ ] Session/auth cookies set with **`HttpOnly`, `Secure`, `SameSite`**
- [ ] Session IDs and tokens come from a **cryptographically secure** source (`crypto.randomBytes`, `crypto.randomUUID`), never `Math.random`
- [ ] Sessions expire; logout invalidates them server-side; IDs rotate on login
- [ ] **Rate limiting / lockout** on login, password reset, and OTP endpoints ([Rate limiting](../23_real-world-patterns/09_rate-limiting.md))
- [ ] Sensitive actions (email/password change, payments) require re-authentication or step-up
- [ ] Generic error messages for login failures ("invalid credentials"), not "user not found"
- [ ] Secret comparisons use `crypto.timingSafeEqual`

```js
import { scrypt, randomBytes, timingSafeEqual } from 'node:crypto';
import { promisify } from 'node:util';
const scryptAsync = promisify(scrypt);

export async function hashPassword(password) {
  const salt = randomBytes(16);
  const hash = await scryptAsync(password, salt, 64);
  return `${salt.toString('hex')}:${hash.toString('hex')}`;
}

export async function verifyPassword(password, stored) {
  const [saltHex, hashHex] = stored.split(':');
  const expected = Buffer.from(hashHex, 'hex');
  const actual = await scryptAsync(password, Buffer.from(saltHex, 'hex'), 64);
  return timingSafeEqual(actual, expected);
}
```

(This uses Node's built-in `scrypt` with sensible shape. In real projects, prefer a well-reviewed library and tune cost parameters to current guidance.)

## 3. Authorization

- [ ] Every endpoint checks **who** is calling and **whether they may** do this action *on this resource*
- [ ] Ownership checks prevent **IDOR** (changing `/orders/123` to `/orders/124` must not reveal someone else's data)
- [ ] Authorization is enforced **on the server**, not by hiding buttons in the UI
- [ ] Default deny: new routes require explicit permission
- [ ] Role/permission fields can't be set by clients (no mass assignment)

## 4. Browser-Facing Protections

- [ ] **CSRF** protection on cookie-authenticated, state-changing routes: SameSite + token/Origin check ([CSRF](./02_csrf.md))
- [ ] No state changes via `GET`
- [ ] **CSP** configured (nonces, no `unsafe-inline` for scripts); tested in report-only first
- [ ] **CORS** allows only specific trusted origins; never `*` together with credentials ([CORS](../15_networking/03_headers-and-cors.md))
- [ ] HTTPS everywhere, with **HSTS** enabled
- [ ] Security headers set (see below)
- [ ] Sensitive tokens not stored where XSS can trivially read them ([Browser storage](../14_dom-and-browser/08_browser-storage.md))

### Headers with Helmet (Express)

```js
import helmet from 'helmet';
app.use(helmet());   // sets a set of sensible default security headers
```

Common headers it covers or that you should verify:

| Header | Purpose |
|---|---|
| `Strict-Transport-Security` | Force HTTPS |
| `Content-Security-Policy` | Restrict script/resource sources (customize this one for your app) |
| `X-Content-Type-Options: nosniff` | Stop MIME-type sniffing |
| `Referrer-Policy` | Limit URL leakage |
| `Permissions-Policy` | Disable unneeded browser features |
| Frame protection (`frame-ancestors` in CSP, or `X-Frame-Options`) | Prevent clickjacking |

Helmet's defaults change between versions and a generic CSP rarely fits every app, so review the output rather than assuming.

## 5. Secrets and Configuration

- [ ] **No secrets in source control** (API keys, tokens, private keys); `.env` is git-ignored
- [ ] Secrets come from environment variables or a secret manager; config validated at startup ([Process and env](../16_nodejs/03_process-and-env.md))
- [ ] Different secrets per environment; rotate on suspected exposure
- [ ] Secrets never logged, never sent to the client, never embedded in frontend bundles
- [ ] Least-privilege credentials (DB user, cloud roles, API tokens)

If a secret is ever committed, **rotate it**. Deleting it from git history doesn't un-leak it.

## 6. Errors and Logging

- [ ] Production errors return **generic messages**; no stack traces or SQL errors to clients ([Production error handling](../10_error-handling/05_production-error-handling.md))
- [ ] Security-relevant events logged (login failures, permission denials, validation spikes)
- [ ] Logs exclude passwords, tokens, full card numbers, and other sensitive data
- [ ] Unhandled rejections and uncaught exceptions are handled and monitored
- [ ] `NODE_ENV=production` set in production (some frameworks enable verbose errors otherwise)

## 7. Dependencies and Supply Chain

- [ ] Lockfile committed; CI uses **`npm ci`** ([Dependency security](./05_dependency-security.md))
- [ ] `npm audit` (or equivalent) runs in CI; findings triaged
- [ ] Automated update PRs (Dependabot/Renovate) with passing tests required
- [ ] Unused dependencies removed; new ones reviewed (maintenance, name, size, install scripts)
- [ ] 2FA on package registry and source-hosting accounts

## 8. Server and Network

- [ ] Server-side requests to **user-supplied URLs** (webhooks, image fetchers) are restricted: allowlist hosts, block internal/private address ranges, limit redirects and response size (SSRF)
- [ ] Timeouts on outbound requests and database calls ([Timeout](../23_real-world-patterns/04_timeout.md))
- [ ] Node process doesn't run as root; containers use a non-root user
- [ ] Only necessary ports exposed; internal services not publicly reachable
- [ ] File uploads: size/type limits, generated filenames, stored outside the web root

---

## Using the Checklist

**In code review**, scan the diff for:

| Red flag | Likely issue |
|---|---|
| `innerHTML`, `eval`, `dangerouslySetInnerHTML` | XSS |
| String-built SQL or shell commands | Injection |
| `Object.assign`/custom merge with `req.body` | Mass assignment / pollution |
| New `app.get('/…/:id')` without ownership check | IDOR |
| `Math.random()` for tokens/IDs | Predictable secrets |
| `cors({ origin: true, credentials: true })` | Over-permissive CORS |
| Hard-coded keys or passwords | Secret exposure |
| `catch (e) { res.send(e.stack) }` | Information leak |

**In CI**, automate what you can: dependency audit, lint rules for dangerous sinks, secret scanning, and tests for auth/validation behavior ([Testing](../21_testing/README.md)). Security tests that assert *rejection* (missing token → 403, wrong user → 404/403, invalid body → 400) are cheap and valuable.

---

## Quick Summary

- Security is layered: **validate input, encode output, authenticate, authorize, protect the browser, manage secrets, handle errors safely, and control dependencies.**
- Most real-world breaches come from a short list: injection, broken access control, weak session handling, leaked secrets, and outdated dependencies.
- Enforce rules **server-side** and **by default**, so safe behavior doesn't depend on each developer remembering.
- Automate checks in CI and keep this list close to code review.

**Next:** Back to the [Security README](./README.md), or continue to [Real-World Patterns](../23_real-world-patterns/README.md).
