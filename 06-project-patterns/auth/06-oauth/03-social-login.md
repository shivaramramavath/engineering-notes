# Social Login (Google and GitHub)

"Sign in with Google/GitHub" using the flow from the previous notes. Google is a standards-compliant OIDC provider; GitHub is OAuth 2.0 only, so the two differ in how you learn who the user is. This note shows both, the provider-specific setup, and how a provider identity becomes a local user.

**Prerequisites:** [01-authorization-code-pkce.md](./01-authorization-code-pkce.md), [02-oidc.md](./02-oidc.md)

---

## What to decide first

| Question | Typical answer |
|---|---|
| Which providers? | Where your users already are (Google for consumers, GitHub for developers, Microsoft for business) |
| Social-only or alongside passwords? | Alongside: offer both, allow linking ([04-account-linking.md](./04-account-linking.md)) |
| What data? | Minimum: stable id, email, name. Ask only for the scopes you need |
| MFA? | Decide whether provider login satisfies your MFA policy ([00-mfa-overview.md](../05-mfa/00-mfa-overview.md)) |

Users who sign in only with a provider have **no password**, so the user schema must not require `passwordHash`:

```js
passwordHash: { type: String, select: false },   // optional; absent for social-only accounts
```

and the login code must treat "no password hash" as "cannot sign in with a password" (still do the dummy-hash verify to keep timing uniform; see [02-register-and-login.md](../02-passwords/02-register-and-login.md)).

---

## Data model

```js
// models/oauth-account.model.js
const oauthAccountSchema = new mongoose.Schema({
  userId:            { type: mongoose.Schema.Types.ObjectId, ref: "User", required: true, index: true },
  provider:          { type: String, enum: ["google", "github"], required: true },
  providerAccountId: { type: String, required: true },     // OIDC `sub`, or GitHub numeric id
  email:             String,                                // snapshot at link time (informational)
  emailVerified:     Boolean,
  createdAt:         { type: Date, default: Date.now },
});

oauthAccountSchema.index({ provider: 1, providerAccountId: 1 }, { unique: true });
```

The **unique compound index** is the identity: one provider account maps to exactly one local user, enforced by the database, not by a racy "find then insert".

---

## Google

### Setup (Google Cloud Console)

1. Create a project → **APIs & Services → OAuth consent screen** (app name, support email, scopes `openid email profile`; "External" for general users; testing mode limits you to listed test users until published).
2. **Credentials → Create credentials → OAuth client ID → Web application**.
3. Add **Authorized redirect URIs**, which must match exactly, one per environment:

```text
http://localhost:4000/api/auth/oauth/google/callback
https://api.example.com/api/auth/oauth/google/callback
```

4. Copy client id and secret into environment variables ([01-env-and-secrets.md](../00-setup/01-env-and-secrets.md)).

Console labels move around; check Google's current docs if the UI differs.

### Flow

Use the generic code from [01-authorization-code-pkce.md](./01-authorization-code-pkce.md) with Google's discovery document (`https://accounts.google.com/.well-known/openid-configuration`). The ID token gives you everything:

```js
// payload from the validated ID token
const profile = {
  providerAccountId: payload.sub,
  email: payload.email,
  emailVerified: payload.email_verified === true,
  name: payload.name,
  picture: payload.picture,
};
```

Google specifics:

- `email_verified` is `true` for Google accounts' own addresses.
- The `hd` claim (hosted domain) identifies Google Workspace accounts. To restrict sign-in to a company domain, **verify `hd` on the server**; the `hd` request parameter only pre-filters the account chooser and can be bypassed.
- `access_type=offline` + `prompt=consent` is how you request a refresh token if you need to call Google APIs later (you don't for login only).
- Don't request broad scopes (Drive, Gmail) for login; sensitive scopes trigger Google's app verification process.

---

## GitHub

GitHub's flow is **plain OAuth 2.0**: no ID token, no discovery. You get an access token and call the GitHub API to learn who the user is.

### Setup

GitHub → **Settings → Developer settings → OAuth Apps → New OAuth App**. Set the **Authorization callback URL** to your callback (exact). GitHub OAuth Apps allow limited redirect-URI flexibility; GitHub Apps are the newer model with finer permissions. For simple login, an OAuth App is fine.

### Endpoints and flow

```text
Authorize:  https://github.com/login/oauth/authorize
Token:      https://github.com/login/oauth/access_token     (send Accept: application/json)
User:       https://api.github.com/user
Emails:     https://api.github.com/user/emails              (needs scope user:email)
```

```js
const github = {
  authorizationEndpoint: "https://github.com/login/oauth/authorize",
  tokenEndpoint: "https://github.com/login/oauth/access_token",
  clientId: process.env.GITHUB_CLIENT_ID,
  clientSecret: process.env.GITHUB_CLIENT_SECRET,
  scope: "read:user user:email",
};
```

Always send `state`. GitHub's PKCE support is more recent than most providers'; check the current docs, and use PKCE if supported.

### Learn the identity

```js
const headers = { Authorization: `Bearer ${accessToken}`, Accept: "application/vnd.github+json" };

const user = await fetch("https://api.github.com/user", { headers }).then((r) => r.json());
const emails = await fetch("https://api.github.com/user/emails", { headers }).then((r) => r.json());

const primary = emails.find((e) => e.primary && e.verified);       // only trust verified

const profile = {
  providerAccountId: String(user.id),        // numeric, permanent: NOT user.login (can be renamed and reused)
  email: primary?.email ?? null,
  emailVerified: Boolean(primary),
  name: user.name ?? user.login,
  picture: user.avatar_url,
};
```

GitHub specifics:

- The profile `email` field may be `null` (private). Use `/user/emails` and pick the **primary and verified** one.
- Key accounts by the numeric `id`; usernames (`login`) change and can be claimed by someone else.
- Some users have no verified email at all; handle `email: null` gracefully (prompt for an email, then run your own verification).

---

## Turn a provider profile into a local user

```js
export async function resolveOAuthUser(provider, profile) {
  // 1. Known identity → that user
  const existing = await OAuthAccount.findOne({ provider, providerAccountId: profile.providerAccountId });
  if (existing) return User.findById(existing.userId);

  // 2. Not seen before → linking / registration rules live in 04-account-linking.md
  return linkOrCreate(provider, profile);
}
```

Step 1 never looks at the email; step 2 is where the dangerous decisions happen, covered next.

New social users arrive with `isEmailVerified` set from the provider's `email_verified` claim (true only when the provider asserts it), and need no verification email.

---

## Frontend

A plain link is enough, because the browser must navigate (not `fetch`) to the backend, which redirects to the provider:

```jsx
<a href={`${API_URL}/api/auth/oauth/google/start?returnTo=/dashboard`}>Continue with Google</a>
```

After the callback, the backend redirects to the SPA, which calls `POST /api/auth/refresh` to obtain an access token ([06-token-storage.md](../03-tokens/06-token-storage.md)). Follow each provider's branding rules for buttons.

---

## Other providers in one paragraph

- **Microsoft (Entra ID):** OIDC; multi-tenant issuer rules; `sub` is app-specific, `oid`/`tid` identify the user/tenant.
- **Apple:** OIDC with a signed-JWT client secret; sends the name **only on the first sign-in**; may provide a private relay email; required on iOS if you offer other social logins.
- **Facebook/LinkedIn/X:** each has quirks; use a maintained library or a hosted IdP rather than hand-rolling several providers.

A small provider config map, plus one generic callback, keeps adding providers cheap.

---

## Common mistakes

| Mistake | Consequence |
|---|---|
| Keying users by email or GitHub `login` | Takeover when they change or are recycled |
| Auto-linking to an existing account on unverified emails | Account takeover |
| Trusting GitHub's public profile email | Unverified/spoofable |
| Requesting sensitive scopes for login | Friction, verification burden, bigger breach impact |
| Redirect URI differs per environment but only one registered | `redirect_uri_mismatch` in staging |
| `hd` restriction applied client-side only | Bypass |
| No handling for provider users without email | Crashes/blocked sign-up |
| Making the user schema require `passwordHash` | Social-only users can't be created |
| Calling the provider API for every request | Use your own session after login |

## Debugging

| Symptom | Cause |
|---|---|
| Google: "Error 400: redirect_uri_mismatch" | Registered URI doesn't match character for character (also `http` vs `https`, port, trailing slash) |
| Google: "Access blocked: app has not completed verification" | App in testing mode and user isn't a test user |
| GitHub returns form-encoded body instead of JSON | Missing `Accept: application/json` on the token request |
| GitHub `email` is `null` | Use `/user/emails` with `user:email` scope |
| Works locally, fails in production | Env vars, redirect URIs, `API_URL` base, `trust proxy`/cookie `Secure` behind a load balancer |
| Infinite redirect loop | SPA calls `/refresh` before the cookie is readable at that path (check cookie `Path`/`SameSite` and domains) |

---

## Quick summary

- Google = OIDC (validate the ID token, use `sub`, `email_verified`, verify `hd` server-side). GitHub = OAuth 2.0 (call the API, use numeric `id`, pick the primary **verified** email).
- Store identities in `OAuthAccount` with a unique `(provider, providerAccountId)` index.
- Known identity → log in; unknown → apply careful linking rules.
- Register exact redirect URIs per environment; keep scopes minimal.

**Next:** [04-account-linking.md](./04-account-linking.md)
