# OpenID Connect (OIDC)

OAuth 2.0 tells an app what it may access; it deliberately says nothing about **who the user is**. OpenID Connect is a thin identity layer on top that adds a standard way to log users in: an **ID token** (a signed JWT about the authentication event), a **UserInfo** endpoint, standard scopes and claims, and **discovery**.

**Prerequisites:** [01-authorization-code-pkce.md](./01-authorization-code-pkce.md), [07-jwt-algorithms-and-keys.md](../03-tokens/07-jwt-algorithms-and-keys.md)

---

## What OIDC adds

```text
OAuth 2.0:   access token  → "this holder may call that API"
OIDC:        + id_token    → "Alice (sub=…) authenticated at 10:02 with method X, for client Y"
             + /userinfo   → profile claims
             + discovery   → machine-readable provider configuration
```

An OIDC request is an OAuth Authorization Code request that includes the scope **`openid`**. The token response then also contains `id_token`.

Standard scopes (each unlocks a set of claims):

| Scope | Claims |
|---|---|
| `openid` | `sub` (required to be OIDC at all) |
| `profile` | `name`, `given_name`, `family_name`, `picture`, `locale`, ... |
| `email` | `email`, `email_verified` |
| `phone` / `address` | `phone_number`, `address` |
| `offline_access` | Allows a refresh token (provider-specific nuances) |

---

## The ID token

A JWT signed by the provider, intended for **your app** (the client), not for calling APIs.

```json
{
  "iss": "https://accounts.google.com",
  "sub": "110169484474386276334",
  "aud": "YOUR_CLIENT_ID",
  "exp": 1754653600,
  "iat": 1754650000,
  "nonce": "n-0S6_WzA2Mj",
  "email": "alice@example.com",
  "email_verified": true,
  "name": "Alice Example",
  "picture": "https://..."
}
```

| Claim | Meaning | Use it to |
|---|---|---|
| `iss` | Issuer (the provider) | Verify it's the provider you expect |
| `sub` | **Stable, unique user id at this issuer** | Identify the user |
| `aud` | Intended audience = your `client_id` | Verify it was issued to *you* |
| `exp`, `iat` | Expiry / issued-at | Reject stale tokens |
| `nonce` | Your random value echoed back | Prevent replay of an old ID token |
| `auth_time`, `acr`, `amr` | When/how the user authenticated | Step-up decisions ([05-step-up-auth.md](../05-mfa/05-step-up-auth.md)) |
| `at_hash` | Hash of the access token | Bind tokens together |
| `email_verified` | Provider asserts the email was verified | Gate account linking ([04-account-linking.md](./04-account-linking.md)) |

**Key your users by `(iss, sub)`, never by email.** Emails can change, be reassigned, or be unverified; `sub` is permanent for that provider.

---

## Validating an ID token

Must be done by you, every time. A library helps, but know the checklist:

| Check | How |
|---|---|
| Signature | Verify with the provider's public key from its JWKS (select by `kid`) |
| Algorithm | Allowlist (e.g. `RS256`); never `none` |
| `iss` | Exact match with the expected issuer |
| `aud` | Contains **your** `client_id` (and if several audiences, `azp` equals your client id) |
| `exp` / `iat` | Not expired; small clock tolerance |
| `nonce` | Equals the one you stored for this login |
| Received over the token endpoint | Tokens received directly from the token endpoint over TLS from the right provider don't need extra signature trust, but verifying anyway is cheap and standard |

```js
import { createRemoteJWKSet, jwtVerify } from "jose";

const JWKS = createRemoteJWKSet(new URL(meta.jwks_uri));

const { payload } = await jwtVerify(idToken, JWKS, {
  issuer: meta.issuer,
  audience: process.env.GOOGLE_CLIENT_ID,
  algorithms: ["RS256"],
  clockTolerance: 5,
});

if (payload.nonce !== saved.nonce) throw new Error("nonce mismatch");
```

Cache the `JWKS` object (create it once per provider, not per request): `jose` caches fetched keys and refetches on an unknown `kid`.

Multi-tenant providers (e.g. Microsoft's `common` endpoint) return a tenant-specific `iss`; validate it against the pattern/`tid` rules in their docs rather than a single fixed string.

---

## ID token vs access token

| | ID token | Access token |
|---|---|---|
| Audience | **Your client** | Resource server (an API) |
| Purpose | Prove the user authenticated | Call APIs |
| Send to APIs? | **No** | Yes |
| Contains identity? | Yes | Maybe (JWT) or opaque |
| Validate in | Your client/backend after login | The resource server |

Never send an ID token to an API as a bearer credential; and never accept someone else's access token as proof of login. Each token has one audience.

---

## UserInfo endpoint

An authenticated call returning claims for the access token's user:

```js
const info = await fetch(meta.userinfo_endpoint, {
  headers: { Authorization: `Bearer ${tokens.access_token}` },
}).then((r) => r.json());
```

Its `sub` **must** match the ID token's `sub`; check it if you use both. Often the ID token already has everything you need for login, so UserInfo is optional; use it for claims that are too large or too fresh for the ID token.

---

## Discovery

Providers publish their configuration at a well-known URL:

```text
{issuer}/.well-known/openid-configuration
e.g. https://accounts.google.com/.well-known/openid-configuration
```

```json
{
  "issuer": "https://accounts.google.com",
  "authorization_endpoint": "https://accounts.google.com/o/oauth2/v2/auth",
  "token_endpoint": "https://oauth2.googleapis.com/token",
  "userinfo_endpoint": "https://openidconnect.googleapis.com/v1/userinfo",
  "jwks_uri": "https://www.googleapis.com/oauth2/v3/certs",
  "scopes_supported": ["openid", "email", "profile"],
  "id_token_signing_alg_values_supported": ["RS256"],
  "code_challenge_methods_supported": ["S256"]
}
```

```js
const meta = await fetch(`${ISSUER}/.well-known/openid-configuration`).then((r) => r.json());
if (meta.issuer !== ISSUER) throw new Error("Issuer mismatch");   // always compare to what you configured
```

Benefits: no hard-coded endpoints, automatic key discovery, and support for any compliant provider (Google, Microsoft, Okta, Auth0, Keycloak) with the same code. Cache the document (and refresh periodically). Fetch it from a **configured** issuer URL only; never from a URL supplied by a user or token.

OAuth-only providers (GitHub) have no discovery or ID tokens ([03-social-login.md](./03-social-login.md)).

---

## Other flows and terms

- **`prompt=login` / `max_age`:** force re-authentication or limit authentication age, the OIDC counterpart of step-up.
- **`login_hint`:** pre-fill the account.
- **Logout:** RP-initiated logout and back-channel logout exist for SSO; ending *your* session doesn't end the provider's.
- **Hybrid/implicit flows:** return tokens through the front channel; avoid them. Use code + PKCE.
- **SSO:** one provider session shared by several apps; each app still gets its own ID token with its own `aud`.

---

## Common mistakes

| Mistake | Consequence |
|---|---|
| Identifying users by email | Account takeover when emails change or are unverified |
| Skipping `aud` / `iss` / `nonce` checks | Token substitution and replay |
| Decoding the ID token without verifying the signature | Anyone can forge identity |
| Treating an access token as an ID token | Token confusion across apps |
| Hard-coding the signing key | Breaks on key rotation; use JWKS |
| Trusting `email` without `email_verified` | Pre-hijacking/impersonation |
| Using the ID token as your API/session credential | Wrong audience, wrong lifetime |
| Not handling provider key rotation | Sudden "invalid signature" |

## Debugging

| Symptom | Cause |
|---|---|
| `JWTClaimValidationFailed: unexpected "aud" claim value` | Wrong `client_id`, or testing with a token minted for another client |
| `unexpected "iss"` | Issuer string differs (trailing slash, tenant) |
| `no applicable key found in the JSON Web Key Set` | Wrong `jwks_uri`, or provider rotated keys and you cached too long |
| `nonce` undefined | Provider didn't echo it: you didn't send it, or it's an access-token-only flow |
| `email` missing | `email` scope not requested/granted |
| `sub` differs between logins | You're using different client ids/providers; `sub` is per provider (and sometimes per client, with pairwise ids) |

Decode a **test** token at jwt.io to inspect claims; don't paste real production tokens.

---

## Quick summary

- OIDC = OAuth + `openid` scope → ID token, UserInfo, discovery.
- The ID token is for your client: verify signature, `iss`, `aud`, `exp`, `nonce`.
- Identify users by `(iss, sub)`; treat `email` as a mutable attribute, trusted only with `email_verified`.
- Use discovery and JWKS instead of hard-coded endpoints and keys.

**Next:** [03-social-login.md](./03-social-login.md)
