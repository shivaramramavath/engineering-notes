# OAuth 2.0 and OpenID Connect

Letting users sign in with Google, GitHub, Microsoft, and other identity providers instead of managing yet another password.

## Two protocols, two jobs

| Protocol                  | Purpose                                                  | Question it answers                                    |
| ------------------------- | -------------------------------------------------------- | ------------------------------------------------------ |
| **OAuth 2.0**             | **Authorization** — delegated access                     | "May this app read my GitHub repos / Google Calendar?" |
| **OpenID Connect (OIDC)** | **Authentication** — identity, built on top of OAuth 2.0 | "Who is this user?"                                    |

OAuth 2.0 alone does **not** tell you who the user is; it hands out access tokens for an API. OIDC adds an **ID token** (a JWT) containing identity claims like `sub`, `email`, and `name`. "Sign in with Google" is OIDC.

---

## The four roles

```
Resource Owner       → the user
Client               → your app
Authorization Server → Google/GitHub login + consent screen + token endpoint
Resource Server      → the API holding the user's data (Google APIs, GitHub API)
```

---

## Authorization Code Flow with PKCE

The recommended flow for web apps, SPAs, and mobile apps.

```
1. User clicks "Sign in with Google" on your app
2. Your app redirects the browser to Google:
     /authorize?client_id=...&redirect_uri=...&response_type=code
               &scope=openid email profile&state=<random>
               &code_challenge=<hash>&code_challenge_method=S256
3. User logs in at Google and approves the requested scopes
4. Google redirects back:  https://yourapp.com/auth/google/callback?code=abc&state=<random>
5. Your SERVER verifies `state`, then POSTs to Google's token endpoint:
     code + client_secret + code_verifier  →  access token + ID token (+ refresh token)
6. Your server reads the user's identity, finds or creates a local user, starts a session
```

Key idea: the browser only ever sees a short-lived, single-use **code**. The tokens are exchanged **server-to-server**, so they never pass through the browser's URL bar or history.

### The three protections

| Parameter                                 | Prevents                                                                                                                |
| ----------------------------------------- | ----------------------------------------------------------------------------------------------------------------------- |
| `state`                                   | **CSRF** — an attacker forcing you to complete _their_ login. Random value stored in the session and compared on return |
| `code_challenge` / `code_verifier` (PKCE) | **Code interception** — a stolen code is useless without the original verifier                                          |
| `redirect_uri` exact match                | **Open redirect / code theft** — register exact URIs with the provider; never use wildcards                             |

Deprecated and **not** to be used: the _Implicit flow_ (tokens in the URL fragment) and the _Password grant_ (your app handling the user's provider password).

---

## Scopes

Scopes say what your app is asking for. Request the **minimum**.

| Scope                | Gives you                               |
| -------------------- | --------------------------------------- |
| `openid`             | Required for OIDC — returns an ID token |
| `email`              | The user's email and `email_verified`   |
| `profile`            | Name, picture                           |
| `read:user` (GitHub) | Read profile data                       |

---

## Implementing it by hand (to understand it)

```js
// routes/google-auth.js
import { Router } from "express";
import crypto from "node:crypto";

const router = Router();

const AUTH_URL = "https://accounts.google.com/o/oauth2/v2/auth";
const TOKEN_URL = "https://oauth2.googleapis.com/token";
const REDIRECT_URI = "https://yourapp.com/auth/google/callback";

const base64url = (buf) => buf.toString("base64url");

router.get("/google", (req, res) => {
  const state = base64url(crypto.randomBytes(32));
  const verifier = base64url(crypto.randomBytes(32));
  const challenge = base64url(
    crypto.createHash("sha256").update(verifier).digest(),
  );

  // remember them in the session for the callback
  req.session.oauth = { state, verifier };

  const params = new URLSearchParams({
    client_id: process.env.GOOGLE_CLIENT_ID,
    redirect_uri: REDIRECT_URI,
    response_type: "code",
    scope: "openid email profile",
    state,
    code_challenge: challenge,
    code_challenge_method: "S256",
  });

  res.redirect(`${AUTH_URL}?${params}`);
});

router.get("/google/callback", async (req, res, next) => {
  try {
    const { code, state } = req.query;
    const saved = req.session.oauth;
    delete req.session.oauth; // single use

    if (!saved || state !== saved.state) {
      return res.status(400).send("Invalid state"); // possible CSRF
    }

    const tokenRes = await fetch(TOKEN_URL, {
      method: "POST",
      headers: { "Content-Type": "application/x-www-form-urlencoded" },
      body: new URLSearchParams({
        code,
        client_id: process.env.GOOGLE_CLIENT_ID,
        client_secret: process.env.GOOGLE_CLIENT_SECRET,
        redirect_uri: REDIRECT_URI,
        grant_type: "authorization_code",
        code_verifier: saved.verifier,
      }),
    });

    if (!tokenRes.ok) return res.status(401).send("Token exchange failed");
    const { id_token } = await tokenRes.json();

    // Received directly from Google's token endpoint over TLS, so the payload can be
    // trusted here. Still verify iss/aud/exp/signature in production — see the library below.
    const claims = JSON.parse(
      Buffer.from(id_token.split(".")[1], "base64url").toString(),
    );

    if (!claims.email_verified)
      return res.status(403).send("Email not verified");

    const user = await findOrCreateUser({
      provider: "google",
      providerId: claims.sub,
      email: claims.email,
    });

    req.session.regenerate((err) => {
      if (err) return next(err);
      req.session.userId = user.id;
      res.redirect("/dashboard");
    });
  } catch (err) {
    next(err);
  }
});

export default router;
```

Writing this yourself is educational; **in production use a library** (below) so signature validation, `iss`/`aud`/`nonce` checks, JWKS key rotation, and discovery are handled correctly.

---

## Using a library

### `openid-client` (standards-compliant OIDC)

```bash
npm install openid-client
```

```js
import * as client from "openid-client";

const config = await client.discovery(
  new URL("https://accounts.google.com"),
  process.env.GOOGLE_CLIENT_ID,
  process.env.GOOGLE_CLIENT_SECRET,
);

// login route
const codeVerifier = client.randomPKCECodeVerifier();
const codeChallenge = await client.calculatePKCECodeChallenge(codeVerifier);
const state = client.randomState();

const url = client.buildAuthorizationUrl(config, {
  redirect_uri: REDIRECT_URI,
  scope: "openid email profile",
  code_challenge: codeChallenge,
  code_challenge_method: "S256",
  state,
});
```

The API differs between major versions — check the docs for the version you install.

### Passport.js (strategy-based, very common)

```bash
npm install passport passport-google-oauth20
```

Passport wraps many providers behind one interface and works well with `express-session`. It's convenient but adds abstraction; understand the flow above first so you know what it's doing for you.

---

## Linking OAuth accounts to local users

Store the provider's stable ID, **not** just the email:

```js
// users table
{ id, email, ... }

// identities table
{ id, userId, provider: "google", providerId: "1098234...(the `sub` claim)" }
```

- `sub` never changes; emails can.
- **Account-takeover risk:** if you auto-link by email, only do so when the provider states `email_verified: true` — and be cautious with providers that allow unverified emails. Otherwise an attacker can register an account at the provider with _your victim's_ email address.

---

## Machine-to-machine: Client Credentials

No user involved — one service calling another:

```
POST /token
grant_type=client_credentials&client_id=...&client_secret=...&scope=orders:read
→ { "access_token": "...", "expires_in": 3600 }
```

Use this for backend services and cron jobs, never from browsers.

---

## Building your _own_ OAuth server?

Usually **don't**. Use a hosted or self-hosted identity provider (Auth0, Keycloak, Ory, Cognito, Clerk, Supabase Auth) — they handle the many edge cases that bite home-grown implementations.

---

## Common mistakes

| Mistake                                                                | Consequence / Fix                                         |
| ---------------------------------------------------------------------- | --------------------------------------------------------- |
| Skipping `state`                                                       | Login CSRF — always generate and verify                   |
| Not using PKCE                                                         | Code interception — use it, even for confidential clients |
| Wildcard `redirect_uri`                                                | Code theft — register exact URLs                          |
| Trusting an ID token without validating `iss`, `aud`, `exp`, signature | Forged identity — use a library                           |
| Treating an OAuth **access token** as proof of identity                | Use the OIDC **ID token** for authentication              |
| Client secret in frontend code                                         | Secrets belong on the server only                         |
| Auto-linking on unverified email                                       | Account takeover                                          |
| Requesting excessive scopes                                            | Users refuse; larger breach impact                        |

## Next

**`05-common-vulnerabilities.md`** catalogues the attacks that target applications like the ones you've now built.
