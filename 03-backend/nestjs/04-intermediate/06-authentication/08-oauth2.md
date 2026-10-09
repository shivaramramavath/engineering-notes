# OAuth 2.0

OAuth 2.0 is a framework for **delegated authorization**: it lets a user grant an application limited access to their data on another service **without giving that application their password**. ("Let this app read my calendar.") OpenID Connect (OIDC) builds on it to provide **authentication** ("sign in with Google"). Confusing the two is the root of many insecure "OAuth login" implementations.

This note is conceptual and practical: the roles, the flows you should use, the pitfalls. Implementing a login button with a provider is in [Social login](./09-social-login.md).

Prerequisites: [Authentication architecture](./01-authentication-architecture.md), [JWT](./04-jwt.md).

> OAuth and OIDC have detailed, evolving security guidance (see the OAuth 2.0 Security Best Current Practice and the OAuth 2.1 drafts). Treat this as an orientation, and rely on a maintained library or provider rather than hand-rolling flows.

## Roles

| Role | Who | Example |
|------|-----|---------|
| **Resource owner** | The user | A person with a Google account |
| **Client** | The app requesting access | Your Nest backend (or SPA/mobile app) |
| **Authorization server** | Authenticates the user, issues tokens | Google, GitHub, Auth0, Keycloak |
| **Resource server** | API holding the data | Google Calendar API |

In many setups your Nest app is the **client** (using a provider for login). Less often, your app is the **authorization server/resource server** (issuing tokens for third parties); that's a much bigger undertaking, and a dedicated server (Keycloak, an identity provider, or a maintained library) is usually wiser than building one.

## Core vocabulary

| Term | Meaning |
|------|---------|
| **Access token** | Credential the client sends to the resource server; opaque string or JWT; short-lived |
| **Refresh token** | Lets the client get new access tokens without the user; long-lived, must be stored securely ([refresh tokens](./05-refresh-tokens-and-logout.md)) |
| **Scope** | A named permission (`read:calendar`, `email`); the user consents to a set of scopes |
| **Client ID / secret** | Identifies the client; the secret authenticates *confidential* clients (servers). Public clients (SPAs, mobile) have no secret |
| **Redirect URI** | Where the authorization server sends the user back; must be pre-registered and matched **exactly** |
| **`state`** | Random value that ties the callback to the originating request (CSRF protection) |
| **ID token** (OIDC) | A signed JWT stating **who authenticated** (`sub`, `email`, `iss`, `aud`, `exp`, `nonce`) |

## Which flow to use

| Situation | Flow |
|-----------|------|
| Web app with a backend, or SPA/mobile (any user-interactive login) | **Authorization Code flow** (with **PKCE**) |
| Service-to-service, no user | **Client Credentials** |
| Input-constrained devices (TVs) | Device Authorization flow |
| Legacy | ~~Implicit flow~~ and ~~Resource Owner Password Credentials~~: **deprecated; don't use** |

Authorization Code with PKCE is the answer for nearly all user logins now, for confidential and public clients alike.

## Authorization Code flow (with PKCE)

```text
 User        Your app (client)                 Authorization server           Resource server
  │  click "Login"  │                                    │                            │
  │────────────────►│  1. redirect with:                 │                            │
  │                 │     client_id, redirect_uri, scope,│                            │
  │                 │     state, code_challenge (PKCE)   │                            │
  │◄────────────────┴───────────────────────────────────►│                            │
  │  2. user authenticates and consents                  │                            │
  │◄─────────────────────────────────────────────────────│                            │
  │  3. redirect back to redirect_uri?code=...&state=... │                            │
  │────────────────►│  4. verify state                   │                            │
  │                 │  5. POST /token: code, code_verifier, client_id(+secret)        │
  │                 │──────────────────────────────────►│                            │
  │                 │◄──── access token (+ refresh, id token) ──│                     │
  │                 │  6. call API with access token ───────────────────────────────►│
```

Key security properties:

- The browser only ever sees a **short-lived, single-use authorization `code`**, not tokens. The code is exchanged for tokens by a **back-channel** request.
- **`state`**: generate a random value, bind it to the user's browser session (cookie/session), and **verify it on return**. It defeats login CSRF (an attacker forcing the victim to complete the attacker's authorization).
- **PKCE** (Proof Key for Code Exchange): the client generates a random `code_verifier`, sends its hash (`code_challenge`) with the authorization request, and sends the verifier when redeeming the code. An attacker who steals the code can't redeem it without the verifier. Required for public clients and recommended for all.
- **Exact redirect URI matching**: register full URIs; never allow wildcards or open redirects, or attackers can steer codes to their own endpoint.
- **`nonce`** (OIDC): ties the ID token to the request, preventing replay.

## OAuth vs OIDC: authorization vs authentication

OAuth alone says "this token may call this API." It does **not** define how to learn *who the user is*. Using an access token as proof of identity (or calling a "get profile" endpoint and trusting it blindly) is the classic mistake.

**OpenID Connect** adds:

- the **`openid` scope** and an **ID token** (JWT) asserting identity,
- standard claims (`sub`, `email`, `email_verified`, `name`),
- a discovery document (`/.well-known/openid-configuration`) and **JWKS** endpoint for key discovery,
- a **UserInfo** endpoint.

For "sign in with X" you want OIDC. When validating an ID token you must check: the signature (against the provider's JWKS), `iss` (expected issuer), `aud` (**your** client id), `exp`, and `nonce`. Prefer a library to do this. Identify the user by the stable **`sub` (together with `iss`)**, never by email alone ([social login](./09-social-login.md)).

## Access tokens: opaque vs JWT, and who validates

- **Opaque** tokens are validated by calling the authorization server's **introspection** endpoint (or looking them up).
- **JWT** access tokens can be validated locally by resource servers using the provider's public keys (JWKS), checking signature, `exp`, `iss`, `aud`, and scopes ([JWT](./04-jwt.md)).
- A resource server must verify the token was issued **for it** (`aud`) and has the **required scope** before serving data. This is authorization; map scopes to permissions ([authorization](../07-authorization/README.md)).

## Client Credentials (machine to machine)

```text
service A ──POST /token { grant_type=client_credentials, client_id, client_secret, scope }──► auth server
service A ◄── access token ──
service A ──► service B with Bearer token
```

No user involved. The secret must be protected (secret manager, rotation). Prefer this to shared static API keys when you need scoping, expiry, and rotation.

## Using OAuth as a Nest client

You typically don't implement the protocol; you use a library:

- **Passport strategies** (`passport-google-oauth20`, `passport-github2`, ...) for specific providers ([social login](./09-social-login.md)).
- **`openid-client`** (a certified OpenID Connect client library) for generic OIDC providers, with discovery, PKCE, and ID token validation done for you. Check its current docs for API specifics.
- A managed identity platform (Auth0, Okta, Keycloak, Cognito, Clerk, Firebase Auth, and similar), where your API just validates JWTs from the provider.

After the provider authenticates the user, **your app creates its own session or tokens** ([session](./07-session-authentication.md) or [JWT](./04-jwt.md)); the provider's tokens are used only to fetch profile data or call the provider's APIs.

If you call the provider's APIs later (calendar, repos), you must **store the provider's refresh token** (encrypted at rest), handle expiry and revocation, and request the **minimum scopes** needed.

## Validating provider tokens in your API (resource server role)

If your API accepts access tokens issued by an external provider:

```ts
// verify a provider-issued JWT with the provider's public keys (JWKS)
// using e.g. the `jose` library: jwtVerify(token, createRemoteJWKSet(new URL(jwksUri)), { issuer, audience })
```

Check `iss`, `aud`, `exp`, signature algorithm (pinned), and required scopes, and cache the JWKS with sensible refresh. A `kid` header selects the key; handle rotation. Libraries like `jose` handle this; don't parse and trust tokens by hand.

## Security checklist

- Authorization Code + **PKCE**; never implicit or password grant.
- Verify **`state`** (and `nonce` for OIDC); store it server-side/session-bound.
- **Exact** redirect URI registration; no open redirects in your own app.
- Keep **client secrets** server-side (secret manager); never ship them in SPAs/mobile apps.
- Validate ID tokens fully (`iss`, `aud`, `exp`, `nonce`, signature).
- Request **least-privilege scopes**; explain consent to users.
- Use **TLS** everywhere; keep tokens out of URLs/logs (the code in the redirect URL is single-use and short-lived, but still avoid logging it).
- Store provider refresh tokens **encrypted**; support revocation and re-consent.
- Link accounts by `(iss, sub)` and only trust **verified** emails.
- Rate limit and monitor the callback and token endpoints.

## Common mistakes

- **Treating OAuth as authentication** (using an access token, or an unverified profile fetch, as login proof) instead of OIDC ID tokens.
- **Skipping `state`** (login CSRF) or PKCE.
- **Loose redirect URI validation** (wildcards, prefix matching, open redirects).
- **Using implicit or password grants** because older tutorials do.
- **Embedding client secrets in frontend/mobile code.**
- **Not validating `aud`/`iss`** on tokens (accepting tokens meant for other apps).
- **Identifying users by email only** (account takeover).
- **Requesting broad scopes** "just in case".
- **Hand-rolling the protocol** instead of using a maintained library or provider.
- **Leaking tokens or codes in logs/referrers.**

## Debugging

- `redirect_uri_mismatch`: the URI in the request doesn't match the registered one exactly (scheme, host, port, path, trailing slash).
- `invalid_grant`: the code was already used or expired, the `code_verifier` doesn't match, or the redirect URI differs between the authorize and token requests.
- `invalid_client`: wrong client id/secret, or secret sent in the wrong way for this provider.
- `state` mismatch: the session/cookie holding the original state was lost (cross-site cookie settings, multiple tabs, different hostname between start and callback).
- ID token validation fails: clock skew, wrong `aud` (using another app's client id), stale JWKS cache, or wrong issuer URL.
- Use the provider's dashboard logs and your own structured logs (without secrets) to trace the flow.

## Quick Summary

- OAuth 2.0 = delegated **authorization**; OIDC adds **authentication** via ID tokens. Use OIDC for login.
- Use **Authorization Code + PKCE** for user logins, **Client Credentials** for service-to-service; avoid implicit and password grants.
- Verify `state`, `nonce`, exact redirect URIs, and validate ID tokens (`iss`, `aud`, `exp`, signature).
- Use libraries or identity providers rather than implementing flows yourself; create your own session/tokens after login.
- Request minimal scopes, protect secrets and provider refresh tokens, identify users by `(iss, sub)`.

## Next

[Social login →](./09-social-login.md)
