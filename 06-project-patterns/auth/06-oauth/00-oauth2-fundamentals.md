# OAuth 2.0 Fundamentals

OAuth 2.0 is a framework for **delegated authorization**: letting one application act on a user's behalf at another service **without ever seeing the user's password**. "Let this app read my calendar" is OAuth. "Sign in with Google" is OAuth plus an identity layer on top (OpenID Connect, [next chapters](./02-oidc.md)).

**Prerequisites:** [00-authn-vs-authz.md](../01-fundamentals/00-authn-vs-authz.md), [06-jwt-vs-sessions.md](../04-sessions/06-jwt-vs-sessions.md)

---

## The problem it solves

```text
Before OAuth:   "Give MyApp your Google password so it can read your contacts."
                → full access, can't revoke one app, password shared with everyone

With OAuth:     User approves MyApp on Google's own page
                → MyApp gets a limited, revocable TOKEN. Never the password.
```

---

## The four roles

```text
┌──────────────────┐                         ┌──────────────────────┐
│  Resource Owner  │  approves access        │  Authorization Server │
│  (the user)      │ ──────────────────────► │  (Google, Auth0, ...) │
└──────────────────┘                         └──────────┬───────────┘
        │ uses                                           │ issues tokens
        ▼                                                ▼
┌──────────────────┐   access token         ┌──────────────────────┐
│  Client          │ ─────────────────────► │  Resource Server      │
│  (your app)      │                         │  (the API: Gmail,...) │
└──────────────────┘                         └──────────────────────┘
```

| Role | Who | In "Sign in with Google" |
|---|---|---|
| Resource owner | The user | The person logging in |
| Client | The app that wants access | **Your** backend/SPA |
| Authorization server (AS) | Authenticates the user, asks consent, issues tokens | Google's accounts service |
| Resource server (RS) | The API that accepts access tokens | Google's userinfo / APIs |

Which role is your app in? Depends on the situation:

| Situation | Your app is... |
|---|---|
| "Sign in with Google/GitHub" | **Client** |
| Your API called by your own React app, via your own login | Not OAuth: first-party authentication ([00-authn-vs-authz.md](../01-fundamentals/00-authn-vs-authz.md)); the hybrid design from earlier chapters |
| Third-party developers need access to your users' data | **Authorization server + resource server** |
| Internal services calling each other | RS (and AS if you issue the tokens): [06-machine-to-machine.md](./06-machine-to-machine.md) |

You rarely need to build an authorization server. If you do, use a proven one (Keycloak, Auth0, Ory Hydra, Cognito, `node-oidc-provider`) rather than writing it.

---

## Client types

| Type | Can keep a secret? | Examples | Authenticates to the AS with |
|---|---|---|---|
| **Confidential** | Yes | Server-side apps (Node/Express backend) | Client secret, private-key JWT, mTLS |
| **Public** | **No** | SPAs, mobile/desktop apps | Nothing (relies on PKCE) |

Anything shipped to a user's device (JavaScript bundle, mobile app) can be extracted, so a "client secret" there is not secret. A SPA that calls the AS directly is a public client. Putting a backend in the middle (the pattern in this repo) makes the OAuth client confidential.

---

## Tokens

| Token | Purpose | Format | Lifetime |
|---|---|---|---|
| **Authorization code** | Short-lived voucher, exchanged once for tokens | Opaque | ~1-10 min, single-use |
| **Access token** | Call the resource server | Opaque or JWT ([RFC 9068](https://www.rfc-editor.org/rfc/rfc9068)) | Minutes-hour |
| **Refresh token** | Get new access tokens | Opaque | Days-months |
| **ID token** (OIDC only) | Tells the *client* who logged in | JWT | Minutes |

The access token is intended for the resource server; the client should treat it as opaque. Your own first-party tokens from [the tokens chapter](../03-tokens/00-secure-random-tokens.md) follow the same ideas.

---

## Scopes

A scope is a named permission the client asks for and the user approves.

```text
scope=openid email profile                       (OIDC identity)
scope=https://www.googleapis.com/auth/calendar.readonly
scope=read:orders write:orders                   (your own API)
```

- Scopes are space-delimited strings in requests.
- Ask for the **minimum** you need; broad scopes reduce consent rates and increase breach impact.
- A scope limits what a token can do **in addition to** what the user themselves may do; the resource server must enforce both.
- The AS may grant fewer scopes than requested; the token response says what was actually granted.

---

## Grant types (flows)

| Grant | Use for | Status |
|---|---|---|
| **Authorization Code + PKCE** | User logs in/consents; web, SPA, mobile | **Default for everything user-facing** ([01-authorization-code-pkce.md](./01-authorization-code-pkce.md)) |
| **Client Credentials** | Machine-to-machine, no user | Current ([06-machine-to-machine.md](./06-machine-to-machine.md)) |
| **Refresh Token** | Renew access tokens | Current |
| **Device Authorization** (RFC 8628) | TVs/CLIs without a browser | Current |
| Implicit | Tokens straight in the URL fragment | **Deprecated**: tokens leak via history/referrer and can't be sender-bound |
| Resource Owner Password Credentials | App collects user's password and sends it to the AS | **Deprecated**: defeats OAuth's point, no MFA/consent |

OAuth 2.1 (consolidation draft) formally drops implicit and password grants, requires PKCE for every authorization-code flow, and requires exact redirect URI matching. Follow it today. The Security BCP (RFC 9700) is the canonical guidance.

---

## Endpoints

| Endpoint | Hosted by | Purpose |
|---|---|---|
| Authorization endpoint | AS | Browser redirect target: login + consent, returns a `code` |
| Token endpoint | AS | Back-channel: exchange code/credentials for tokens |
| Revocation endpoint (RFC 7009) | AS | Invalidate a token |
| Introspection endpoint (RFC 7662) | AS | RS asks "is this opaque token active?" |
| UserInfo endpoint | AS (OIDC) | Profile claims for the user |
| JWKS endpoint | AS | Public keys to verify JWTs |
| Redirect URI | **Client** | Where the AS sends the browser back |

Two channels:

```text
Front channel: browser redirects, visible to the user and to browser history/logs
               → carries only non-secret values (code, state)
Back channel:  server-to-server HTTPS (token endpoint)
               → carries secrets (client secret, tokens)
```

Design rule: tokens travel on the back channel, never in a URL.

---

## OAuth vs OIDC vs SAML vs "my own login"

| | Answers | Format | Typical use |
|---|---|---|---|
| OAuth 2.0 | "May this app access X?" | Access tokens | API delegation |
| OIDC | "Who is this user?" (+ OAuth) | ID token (JWT) + access token | Social login, SSO |
| SAML 2.0 | Who is this user? | XML assertions | Enterprise SSO (legacy but widespread) |
| First-party login | Who is this user? | Your session/JWT | Your own app |

**OAuth alone is not authentication.** An access token says "the holder may call this API", not "this is Alice". Using a bare access token as proof of identity is a classic vulnerability (a token issued to a different app can be replayed against yours). That is exactly the gap OIDC fills.

---

## Common misconceptions

- "OAuth is a login protocol." It is authorization; OIDC adds login.
- "The access token proves who the user is." Use the validated ID token's `sub`.
- "A client secret is safe in my SPA." It isn't; use PKCE and a backend.
- "Any redirect URI on my domain is fine." Must be an exact registered match ([05-oauth-security.md](./05-oauth-security.md)).
- "Scopes are the user's permissions." They limit the *client*; the resource server still checks the user's own rights.

---

## Quick summary

- OAuth delegates limited, revocable access without sharing passwords; roles: user, client, authorization server, resource server.
- Public clients can't keep secrets; use a backend (confidential client) and PKCE.
- Use Authorization Code + PKCE for users, Client Credentials for machines; avoid implicit and password grants.
- Tokens move on the back channel; scopes request minimal access.
- OAuth is authorization; OIDC adds authentication.

**Next:** [01-authorization-code-pkce.md](./01-authorization-code-pkce.md)
