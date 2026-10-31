# Authorization Code Flow with PKCE

The flow behind every "Sign in with ..." button and every app that asks for access to your account elsewhere. The browser is sent to the authorization server (AS), the user approves, and the app receives a one-time **code** that it exchanges for tokens over a secure back channel. **PKCE** binds that code to the client that started the flow, so a stolen code is useless.

**Prerequisite:** [00-oauth2-fundamentals.md](./00-oauth2-fundamentals.md)

---

## The flow

```text
 User/Browser            Your backend (client)              Authorization Server
      │                          │                                   │
 1.   │ click "Sign in with X"   │                                   │
      │ ───────────────────────► │ generate state, nonce,            │
      │                          │ code_verifier → code_challenge    │
 2.   │ ◄─ 302 to AS /authorize ─┤  (state + verifier saved)         │
      │ ─────────────────────────────────────────────────────────►   │
 3.   │            user logs in, sees consent, approves              │
      │ ◄─────────── 302 to redirect_uri?code=...&state=... ──────── │
 4.   │ ─────────────────────────►│ check state                      │
      │                          │ POST /token  code + code_verifier │
 5.   │                          │ ─────────────────────────────────►│ verify challenge matches
      │                          │ ◄───── access / id / refresh ──── │
 6.   │ ◄─ session cookie ───────┤  (validate ID token, create session)
```

Steps 1-3 are the **front channel** (browser, visible). Steps 4-5 are the **back channel** (server to server). The code in step 3 is the only thing exposed in the URL, and it is useless without the verifier from step 1.

---

## What PKCE adds

PKCE (RFC 7636, "pixy") defends against **authorization code interception**: malware, a malicious app registered for the same custom URL scheme, a browser extension, or logs capturing the redirect URL.

```text
Start:   code_verifier  = random secret kept by the client (43-128 chars)
         code_challenge = BASE64URL( SHA-256(code_verifier) )
Send:    code_challenge + code_challenge_method=S256   in the /authorize request
Redeem:  send code_verifier in the /token request
AS:      hashes the verifier and compares with the stored challenge
```

An attacker who steals only the `code` can't redeem it: they don't have the verifier, and the challenge can't be reversed into it. Required for public clients and **recommended for all clients, including confidential ones** (OAuth 2.1 makes it mandatory). Always use `S256`; the `plain` method sends the secret in the clear.

```js
import crypto from "crypto";

const verifier  = crypto.randomBytes(32).toString("base64url");                       // 43 chars
const challenge = crypto.createHash("sha256").update(verifier).digest("base64url");
```

---

## Request parameters

### Authorization request (browser redirect)

```text
GET https://AS/authorize
  ?response_type=code
  &client_id=YOUR_CLIENT_ID
  &redirect_uri=https://api.example.com/api/auth/oauth/google/callback
  &scope=openid%20email%20profile
  &state=RANDOM_STATE
  &nonce=RANDOM_NONCE                 (OIDC)
  &code_challenge=CHALLENGE
  &code_challenge_method=S256
```

| Parameter | Role |
|---|---|
| `state` | Random value tying the callback to *this* browser/session (CSRF defense) |
| `nonce` | Random value echoed inside the ID token (replay defense) |
| `redirect_uri` | Must exactly match a pre-registered URI |
| `scope` | Requested permissions |

### Token request (back channel)

```http
POST https://AS/token
Content-Type: application/x-www-form-urlencoded

grant_type=authorization_code
&code=THE_CODE
&redirect_uri=https://api.example.com/api/auth/oauth/google/callback
&client_id=YOUR_CLIENT_ID
&client_secret=YOUR_SECRET            (confidential clients)
&code_verifier=THE_VERIFIER
```

---

## Implementation (Express, no OAuth library)

Writing it by hand once makes the protocol concrete. Production code often uses a maintained library (below).

```js
// services/oauth.service.js
import crypto from "crypto";
import { createRemoteJWKSet, jwtVerify } from "jose";

const providers = {
  google: {
    issuer: "https://accounts.google.com",
    authorizationEndpoint: "https://accounts.google.com/o/oauth2/v2/auth",
    tokenEndpoint: "https://oauth2.googleapis.com/token",
    jwksUri: "https://www.googleapis.com/oauth2/v3/certs",
    clientId: process.env.GOOGLE_CLIENT_ID,
    clientSecret: process.env.GOOGLE_CLIENT_SECRET,
    scope: "openid email profile",
  },
};
// Prefer reading these from the provider's discovery document: see 02-oidc.md

const redirectUri = (name) => `${process.env.API_URL}/api/auth/oauth/${name}/callback`;
```

### Step 1: start

```js
export async function startLogin(req, res) {
  const p = providers[req.params.provider];
  if (!p) return res.sendStatus(404);

  const state = crypto.randomBytes(32).toString("base64url");
  const nonce = crypto.randomBytes(32).toString("base64url");
  const verifier = crypto.randomBytes(32).toString("base64url");
  const challenge = crypto.createHash("sha256").update(verifier).digest("base64url");

  // Server-side record, single-use, short-lived
  await redis.set(
    `oauth:state:${hashToken(state)}`,
    JSON.stringify({ provider: req.params.provider, nonce, verifier, returnTo: safeReturnTo(req.query.returnTo) }),
    { EX: 600 }
  );

  // Bind the flow to THIS browser: the callback must come back with the same cookie
  res.cookie("oauth_state", state, {
    httpOnly: true,
    secure: process.env.NODE_ENV === "production",
    sameSite: "lax",                       // NOT strict: the callback is a cross-site top-level redirect
    path: `/api/auth/oauth`,
    maxAge: 10 * 60 * 1000,
  });

  const url = new URL(p.authorizationEndpoint);
  url.search = new URLSearchParams({
    response_type: "code",
    client_id: p.clientId,
    redirect_uri: redirectUri(req.params.provider),
    scope: p.scope,
    state,
    nonce,
    code_challenge: challenge,
    code_challenge_method: "S256",
  }).toString();

  res.redirect(url.toString());
}
```

`SameSite=Strict` would break this: the browser returns from the AS via a cross-site navigation, and a Strict cookie wouldn't be sent. `Lax` is correct for the state cookie.

### Step 2: callback

```js
export async function callback(req, res) {
  const { code, state, error } = req.query;
  const p = providers[req.params.provider];

  if (error) return res.redirect(`${env.FRONTEND_URL}/login?error=oauth_denied`);   // never reflect error_description raw
  if (!p || typeof code !== "string" || typeof state !== "string") return res.sendStatus(400);

  // 1. state must match the cookie from THIS browser
  if (state !== req.cookies.oauth_state) return res.sendStatus(400);
  res.clearCookie("oauth_state", { path: "/api/auth/oauth" });

  // 2. server record, single-use
  const raw = await redis.getDel(`oauth:state:${hashToken(state)}`);
  if (!raw) return res.sendStatus(400);
  const saved = JSON.parse(raw);
  if (saved.provider !== req.params.provider) return res.sendStatus(400);

  // 3. exchange the code (back channel)
  const tokenRes = await fetch(p.tokenEndpoint, {
    method: "POST",
    headers: { "Content-Type": "application/x-www-form-urlencoded", Accept: "application/json" },
    body: new URLSearchParams({
      grant_type: "authorization_code",
      code,
      redirect_uri: redirectUri(req.params.provider),
      client_id: p.clientId,
      client_secret: p.clientSecret,
      code_verifier: saved.verifier,
    }),
  });
  if (!tokenRes.ok) return res.sendStatus(502);
  const tokens = await tokenRes.json();

  // 4. validate the ID token (see 02-oidc.md)
  const JWKS = createRemoteJWKSet(new URL(p.jwksUri));
  const { payload } = await jwtVerify(tokens.id_token, JWKS, {
    issuer: p.issuer,
    audience: p.clientId,
    algorithms: ["RS256"],
  });
  if (payload.nonce !== saved.nonce) return res.sendStatus(400);

  // 5. map to a local user and create OUR session (see 04-account-linking.md)
  const user = await resolveOAuthUser(req.params.provider, payload);
  const { refreshToken } = await createSession(user.id, { ip: req.ip, userAgent: req.get("user-agent") });
  setRefreshCookie(res, refreshToken);

  // 6. back to the SPA; it calls POST /api/auth/refresh to obtain an access token
  res.redirect(`${env.FRONTEND_URL}${saved.returnTo || "/"}`);
}
```

Notes on the design:

- Provider tokens are used **once** to learn who the user is. The app then issues **its own** session and tokens ([06-token-storage.md](../03-tokens/06-token-storage.md)). Storing provider access/refresh tokens is only necessary if you call the provider's APIs later; if you do, encrypt them like TOTP secrets ([01-totp.md](../05-mfa/01-totp.md)).
- **No token ever appears in a URL.** The redirect to the SPA carries nothing; the SPA obtains its access token via the normal refresh call using the HttpOnly cookie the callback just set.
- Check `MFA`: if the user has MFA enabled, the callback should create an MFA challenge rather than a session ([00-mfa-overview.md](../05-mfa/00-mfa-overview.md)). A provider login doesn't automatically satisfy your MFA policy unless you decide to trust the provider's `amr`.
- `safeReturnTo` must allow only relative paths ([05-oauth-security.md](./05-oauth-security.md)).

---

## Using a library

Hand-rolling teaches the protocol; libraries handle edge cases (discovery, JWKS rotation, clock skew, error formats):

| Library | Notes |
|---|---|
| `openid-client` (panva) | Certified OIDC client; its API changed substantially between major versions, so follow the docs for the version you install |
| `passport` + `passport-google-oauth20` / `passport-github2` | Familiar Express strategy model; verify the strategy handles PKCE and `state` as you need |
| Auth.js / NextAuth, Lucia-style helpers, Arctic | Higher-level, framework-oriented |
| Hosted IdP (Auth0, Clerk, Cognito, Keycloak) | Offloads the whole flow |

Whatever you choose, confirm it does **state, nonce, PKCE (S256), exact redirect URI and ID-token validation**; don't assume.

---

## Token response

```json
{
  "access_token": "ya29....",
  "expires_in": 3599,
  "refresh_token": "1//...",
  "scope": "openid https://www.googleapis.com/auth/userinfo.email ...",
  "token_type": "Bearer",
  "id_token": "eyJhbGciOi..."
}
```

Check `scope` for what was actually granted. `refresh_token` is often only returned on first consent or when the provider is asked for offline access (Google uses `access_type=offline`; check the provider's docs).

---

## Common mistakes

| Mistake | Consequence |
|---|---|
| No `state` or not bound to the browser | Login CSRF: the victim is signed in as the attacker |
| `state` stored in `localStorage`/memory of a different tab | Intermittent failures; tie it to a cookie + server record |
| `SameSite=Strict` on the state cookie | Callback arrives without it → always fails |
| PKCE skipped "because I have a secret" | Code injection/interception stays possible |
| `redirect_uri` differs between authorize and token requests | `redirect_uri_mismatch` / `invalid_grant` |
| Code reuse or slow exchange | `invalid_grant`: codes are single-use and expire in minutes |
| Using the access token as proof of identity | Token substitution attacks; validate the ID token |
| Putting tokens in the redirect to the SPA | Leaks via history, logs, referrer |
| Trusting the callback `error_description` | Reflected content / phishing text |

## Debugging

| Error | Meaning |
|---|---|
| `redirect_uri_mismatch` | URI in the request isn't an exact match to one registered at the provider (scheme, host, port, path, trailing slash) |
| `invalid_grant` | Code expired/used, wrong `redirect_uri`, or wrong `code_verifier` |
| `invalid_client` | Wrong client id/secret, or wrong client authentication method |
| `invalid_request` / `unsupported_response_type` | Missing or malformed parameter |
| `access_denied` | User declined, or app not allowed (e.g. test users only) |
| State mismatch in your app | Cookie not sent (SameSite/Secure/domain), cookie path, multiple tabs, expired record |
| Cookie not present on callback | `Path` too narrow, `Secure` on HTTP, API/frontend domain differences |

Inspect each hop in DevTools → Network ("Preserve log" on) to see the redirect chain.

---

## Quick summary

- Browser goes to the AS, user consents, your backend redeems a one-time `code` on the back channel.
- PKCE (`S256`) binds the code to the client that started the flow; use it always.
- `state` (bound to the browser) defeats login CSRF; `nonce` is checked in the ID token.
- Redeem the code, validate the ID token, then issue your own session; no tokens in URLs.

**Next:** [02-oidc.md](./02-oidc.md)
