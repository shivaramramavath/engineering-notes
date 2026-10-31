# OAuth Security

OAuth moves a user between several parties through browser redirects, so most attacks are about **tricking one party into trusting a message meant for another**. This note maps each attack to its defense: `state`, `nonce`, PKCE, redirect URI validation, and the rest of the checklist from the OAuth Security Best Current Practice (RFC 9700).

**Prerequisites:** [01-authorization-code-pkce.md](./01-authorization-code-pkce.md), [02-oidc.md](./02-oidc.md)

---

## Attack → defense map

| Attack | What happens | Defense |
|---|---|---|
| **Login CSRF** | Attacker makes the victim's browser complete the *attacker's* OAuth flow; victim ends up signed in to the attacker's account and may store data there | `state` bound to the browser |
| **Authorization code interception/injection** | Attacker steals a code (logs, referrer, malicious app) or injects their own into the victim's callback | PKCE (S256), `state`, one-time short-lived codes |
| **Redirect URI manipulation** | Code or token delivered to an attacker-controlled URL | Exact-match registered redirect URIs |
| **ID token replay** | An old/stolen ID token is presented again | `nonce`, `exp`, `aud`, `iss` checks |
| **Token substitution / confused deputy** | A token issued for app A is accepted by app B | Validate `aud`; never treat access tokens as identity |
| **Mix-up attack** | With several providers, attacker confuses which AS issued a response | Per-provider state, check `iss` (RFC 9207 `iss` response parameter when supported) |
| **Open redirect** | Your own `returnTo` redirect sends users to a phishing site | Allowlist relative paths |
| **Token leakage via URL/referrer/logs** | Tokens in query strings or fragments | Authorization code flow only; no tokens in URLs; `Referrer-Policy` |
| **Implicit-flow theft** | Tokens exposed in browser history/JS | Don't use implicit |
| **Client secret theft** | Secret extracted from a public client or repo | Keep it server-side, env/secret manager, rotate |
| **Over-broad scopes** | Compromise gives too much access | Least privilege |
| **Account takeover via linking** | Matching on unverified email | [04-account-linking.md](./04-account-linking.md) |
| **Refresh token theft** | Long-lived access to provider APIs | Encrypt at rest, rotation, sender-constrained tokens |

---

## `state`: the CSRF defense

`state` must be:

1. **Unguessable** (256-bit random).
2. **Bound to the user's browser**, not just stored somewhere: compare the callback's `state` against a cookie (or session) from the **same browser** that started the flow.
3. **Single-use** and **short-lived** (`GETDEL`, ~10 min).

```js
if (req.query.state !== req.cookies.oauth_state) return res.sendStatus(400);   // different browser
const saved = await redis.getDel(`oauth:state:${hashToken(req.query.state)}`);  // replay / expiry
```

A `state` that's merely stored server-side and not tied to the browser doesn't stop the attack: the attacker simply starts a flow, gets a valid `state`, and sends the victim the callback link. The cookie binding is what makes it fail. Remember `SameSite=Lax` on that cookie ([01-authorization-code-pkce.md](./01-authorization-code-pkce.md)).

`state` may also carry non-secret context (like `returnTo`), but keep that server-side keyed by the state rather than encoding it into the string.

---

## `nonce`: the ID-token replay defense

`nonce` goes in the authorization request and returns **inside the signed ID token**. If the token's `nonce` doesn't match what you stored for this login, the token wasn't minted for this request.

```js
if (payload.nonce !== saved.nonce) throw new Error("nonce mismatch");
```

`state` protects the *redirect*; `nonce` protects the *token*. They're different checks, so use both.

---

## PKCE

Always include `code_challenge` with `S256`, **even for confidential clients**. It also blocks "authorization code injection" where an attacker lands their own code on the victim's callback: the victim's verifier won't match the attacker's challenge. Requirement for public clients, best practice for all.

---

## Redirect URI validation

The AS must send the code only to **pre-registered** URIs, compared **exactly** (scheme, host, port, path, query handling per spec):

```text
Registered:  https://app.example.com/api/auth/oauth/google/callback

Reject:      https://app.example.com/api/auth/oauth/google/callback/..    (path tricks)
             https://app.example.com.evil.com/...                          (suffix)
             https://evil.com/?https://app.example.com/...                 (embedding)
             http://app.example.com/...                                    (downgrade)
             https://app.example.com/api/auth/oauth/google/callback?x=1    (unless allowed)
```

For you as a **client**:

- Register the narrowest URI(s); no wildcards (`*.example.com`), no "any path under".
- One URI per environment; don't reuse prod's client for localhost.
- Don't build the `redirect_uri` from the request's `Host` header; use configuration (`API_URL`).

For you as an **authorization server** (if you ever build one):

- Exact string matching against the registered list.
- Never redirect to an unregistered URI, not even with an error. Show an error page instead.
- Reject non-HTTPS URIs except loopback for native apps.

Wildcard or prefix matching was the cause of many real-world token-theft incidents, because an open redirect or user-content path anywhere under the allowed prefix can be chained to steal the code.

---

## Your own open redirect: `returnTo`

```js
export function safeReturnTo(value) {
  if (typeof value !== "string") return "/";
  // Only same-site relative paths: starts with one slash, not "//" or "/\"
  if (!/^\/(?![\/\\])/.test(value)) return "/";
  return value;
}
```

`//evil.com` and `/\evil.com` are interpreted by browsers as absolute URLs, so a check like `startsWith("/")` is not enough. Prefer mapping a small allowlist of known destinations.

---

## Token handling

| Token | Rule |
|---|---|
| Authorization code | Single-use, minutes of lifetime; redeem immediately; don't log the callback URL |
| Access token (provider's) | Use for API calls only; don't forward to other services; short-lived |
| Refresh token (provider's) | Store only if needed; encrypt at rest; revoke on unlink/logout ([revocation endpoint](./00-oauth2-fundamentals.md)) |
| ID token | Validate then discard; don't use as your session |
| Client secret | Server-side only; rotate; separate per environment |

Don't log full callback URLs, since they contain the `code` and `state`. Your reverse proxy's access log probably does: scrub query strings on the callback route.

Set `Referrer-Policy: no-referrer` (or `same-origin`) on pages reached after the callback, so the code isn't leaked to third-party resources.

---

## Callback hardening

```js
// Validate shape before using
if (typeof code !== "string" || code.length > 2048) return res.sendStatus(400);
if (typeof state !== "string" || state.length > 256) return res.sendStatus(400);

// Handle the error response without reflecting it
if (req.query.error) return res.redirect(`${FRONTEND_URL}/login?error=oauth_failed`);
```

- Rate-limit the callback and start routes ([00-rate-limiting.md](../08-hardening/00-rate-limiting.md)).
- Treat any failure (state mismatch, nonce mismatch, token exchange failure) as a generic failed login; log the specific reason server-side without secrets.
- Enforce a total time budget on the flow (state expiry).

---

## As an authorization server or resource server

If you issue or accept OAuth tokens:

| Role | Must do |
|---|---|
| **Resource server** | Verify signature, `iss`, **`aud`** (is this token for *me*?), `exp`, and **scope** for each endpoint; reject ID tokens used as access tokens |
| **AS** | PKCE for every code grant; exact redirect URIs; short-lived single-use codes; refresh token rotation for public clients; consent screen showing the client and scopes; revoke on logout/compromise |
| Both | TLS everywhere; no tokens in URLs; rate limits on token endpoint |

Sender-constrained tokens (DPoP, RFC 9449; mutual TLS, RFC 8705) make stolen tokens unusable by anyone without the private key, an advanced hardening step for high-value APIs.

---

## Checklist

- [ ] Authorization Code flow with PKCE `S256`; no implicit, no password grant
- [ ] `state`: random, browser-bound, single-use, expiring
- [ ] `nonce` checked in the ID token
- [ ] ID token validated: signature, `iss`, `aud`, `exp`, `nonce`, algorithm allowlist
- [ ] Exact registered redirect URIs; none built from request headers
- [ ] Users keyed by `(iss, sub)`; `email_verified` gates linking
- [ ] `returnTo` restricted to safe relative paths
- [ ] No tokens or codes in logs, URLs shared onward, or client storage
- [ ] Client secret server-side, per environment, rotatable
- [ ] Minimal scopes; provider refresh tokens encrypted and revocable
- [ ] Callback and start routes rate-limited

---

## Common mistakes

| Mistake | Consequence |
|---|---|
| `state` missing or only checked server-side without a browser binding | Login CSRF |
| Skipping `nonce` | ID-token replay |
| Prefix/wildcard redirect URI | Code theft via chained redirects |
| `startsWith("/")` for `returnTo` | Open redirect with `//evil.com` |
| Accepting any valid token from the provider (wrong `aud`) | Confused deputy |
| Treating an access token as login proof | Impersonation via tokens issued to other apps |
| Logging full callback URLs | Codes/state in logs |
| Reusing one OAuth client across prod and dev | Larger blast radius, redirect URI sprawl |

## Debugging

| Symptom | Cause |
|---|---|
| State mismatch only sometimes | Two tabs started flows, overwriting the single `oauth_state` cookie; use a per-flow cookie name or accept the latest only |
| State cookie missing on callback | `SameSite=Strict`, `Secure` on HTTP, cookie `Path`, or different API domain |
| `invalid_grant` after a long consent screen | Code expired; keep your exchange fast, don't retry a used code |
| Flow works with Google, fails with a second provider | Cookie/state keyed globally rather than per provider |

---

## Quick summary

- `state` (browser-bound) stops login CSRF; `nonce` stops ID-token replay; PKCE stops code theft/injection.
- Exact-match redirect URIs; never trust request-derived URLs, including your own `returnTo`.
- Validate ID tokens fully, key by `sub`, and check `aud` on every token you accept.
- Keep codes, tokens and secrets out of URLs, logs and client storage.

**Next:** [06-machine-to-machine.md](./06-machine-to-machine.md)
