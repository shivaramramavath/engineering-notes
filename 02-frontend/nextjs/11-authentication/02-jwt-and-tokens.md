# JWT and Tokens

A JSON Web Token (JWT) is a compact, signed string that carries claims such as a user ID. It is the usual format for stateless sessions and for tokens issued by identity providers. This note explains what is inside one, what signing does and does not give you, and how to use `jose` safely in Next.js.

> Follows RFC 7519 (JWT), RFC 7515 (JWS), RFC 7516 (JWE) and the `jose` library documentation. The Next.js docs use `jose` for stateless sessions.

## What it is

```text
eyJhbGciOiJIUzI1NiJ9 . eyJ1c2VySWQiOiJ1XzEiLCJleHAiOjE3...  . 3kQ8…signature…
      header                     payload (claims)                signature
```

Each part is Base64URL-encoded JSON (the signature is raw bytes, encoded).

| Part | Example | Meaning |
|---|---|---|
| Header | `{"alg":"HS256"}` | How it is signed |
| Payload | `{"userId":"u_1","role":"user","iat":…,"exp":…}` | The claims |
| Signature | HMAC or RSA/ECDSA over header and payload | Proves nobody altered it |

Registered claims you will meet:

| Claim | Meaning |
|---|---|
| `iss` | Issuer: who created the token |
| `sub` | Subject: whom it is about (the user ID) |
| `aud` | Audience: who the token is for |
| `exp` | Expiration time (seconds since epoch) |
| `nbf` | Not valid before |
| `iat` | Issued at |
| `jti` | Unique token ID (useful for revocation lists) |

## Signed is not encrypted

| Format | Name | Guarantees |
|---|---|---|
| Signed (`SignJWT`) | JWS | **Integrity**: cannot be modified. **Not secret**: anyone can decode the payload |
| Encrypted (`EncryptJWT`) | JWE | Integrity **and** confidentiality |

Decoding a signed JWT needs no key:

```ts
JSON.parse(Buffer.from(token.split(".")[1], "base64url").toString());
```

So never put emails, phone numbers, or secrets in a signed token. The docs' guidance: the payload should hold the **minimum, unique user data** needed on later requests (user ID, role), and no PII or sensitive data. The Next.js guide names its helpers `encrypt` and `decrypt`, but with `SignJWT` and HS256 they sign and verify.

## Signing and verifying with `jose`

```ts
import "server-only";
import { SignJWT, jwtVerify, errors } from "jose";

const key = new TextEncoder().encode(process.env.SESSION_SECRET);   // HS256 needs a strong secret

export function sign(claims: Record<string, unknown>) {
  return new SignJWT(claims)
    .setProtectedHeader({ alg: "HS256" })
    .setIssuer("https://app.example.com")
    .setAudience("app")
    .setIssuedAt()
    .setExpirationTime("15m")
    .sign(key);
}

export async function verify(token: string) {
  try {
    const { payload } = await jwtVerify(token, key, {
      algorithms: ["HS256"],                 // pin it
      issuer: "https://app.example.com",
      audience: "app",
    });
    return payload;
  } catch (e) {
    if (e instanceof errors.JWTExpired) return null;   // expected; ask for a new one
    return null;                                       // invalid: treat as signed out
  }
}
```

Rules:

- **Pin `algorithms`.** Never accept whatever the header says (the classic `alg: none` and key-confusion attacks).
- **Always verify `exp`.** `jwtVerify` does when the claim is present, so set it when signing.
- Check `iss` and `aud` when more than one system issues or consumes tokens.
- Keep the secret out of source control; use at least 32 random bytes. Rotating it invalidates all sessions unless you verify against old and new keys.
- Symmetric (HS256): the same secret signs and verifies; fine when one server owns both. Asymmetric (RS256, ES256, EdDSA): a private key signs, a public key verifies; use when other services must verify tokens without being able to mint them.

## Encrypting a token (JWE)

When the payload must be hidden from the browser:

```ts
import { EncryptJWT, jwtDecrypt } from "jose";

const encKey = Buffer.from(process.env.SESSION_ENC_KEY!, "base64");   // exactly 32 bytes

export function seal(claims: Record<string, unknown>) {
  return new EncryptJWT(claims)
    .setProtectedHeader({ alg: "dir", enc: "A256GCM" })
    .setIssuedAt()
    .setExpirationTime("7d")
    .encrypt(encKey);
}

export async function unseal(token: string) {
  const { payload } = await jwtDecrypt(token, encKey);
  return payload;
}
```

Generate the key with `openssl rand -base64 32`. For cookie sessions, `iron-session` (named in the docs) does sealed cookies without you assembling this.

## Access tokens and refresh tokens

Short-lived and long-lived tokens split the risk:

| Token | Lifetime | Used for | Stored |
|---|---|---|---|
| **Access token** | Minutes | Authorizing API calls | `HttpOnly` cookie (or memory on the server) |
| **Refresh token** | Days to weeks | Getting a new access token | `HttpOnly` cookie, ideally server-side record |

```text
1. Login → issue access (15 min) + refresh (30 days)
2. Request with expired access token → server uses the refresh token
3. Verify the refresh token (and that it is not revoked) → issue a new access token
4. Rotate: also issue a new refresh token and invalidate the old one
5. If an old refresh token is reused → assume theft, revoke the whole family
```

Most Next.js apps with one backend do not need two tokens: a single cookie session with a database row, or a 7-day signed cookie, is enough. Add refresh tokens when you call **external APIs** on the user's behalf (an OAuth provider's access token expires and must be refreshed with its refresh token; see [OAuth](./03-oauth.md)).

## The revocation problem

A signed token is valid until `exp`, even if the user is deleted, demoted, or logs out. Options:

| Approach | Trade-off |
|---|---|
| Short `exp` plus refresh | Role changes take effect within minutes; needs refresh logic |
| Denylist of `jti` values (database or cache) | Check on every request, which gives up part of "stateless" |
| **Database session** whose ID sits in the cookie | Immediate revocation; one lookup per request (cache per render) |
| Version counter on the user (`tokenVersion` claim must match the DB) | One lookup, easy "log out everywhere" |

If you need any of these on every request, a database session is simpler than a JWT with a denylist.

## Where to keep tokens

| Place | Verdict |
|---|---|
| `HttpOnly` + `Secure` + `SameSite` cookie, set by the server | **Use this** |
| `localStorage` / `sessionStorage` | Readable by any script on the page; one XSS bug exposes the token |
| JavaScript-readable cookie | Same exposure |
| URL query string | Leaks through logs, history, `Referer` |
| Server memory or database only | Fine for server-to-server tokens (e.g. a provider's refresh token) |

A browser `fetch` to your own app sends the cookie automatically, so Client Components rarely need to touch tokens. When you must call another API from the browser, proxy it through a Route Handler or Server Action that attaches the credentials.

## Verifying tokens from other issuers

When an identity provider (Auth0, Clerk, Google, Cognito) issues the JWT, verify it with the provider's **public keys** (JWKS):

```ts
import { createRemoteJWKSet, jwtVerify } from "jose";

const JWKS = createRemoteJWKSet(new URL("https://YOUR_ISSUER/.well-known/jwks.json"));

export async function verifyProviderToken(token: string) {
  const { payload } = await jwtVerify(token, JWKS, {
    issuer: "https://YOUR_ISSUER/",
    audience: "YOUR_API_AUDIENCE",
  });
  return payload;
}
```

`createRemoteJWKSet` fetches and caches the key set and handles rotation. Find the JWKS URL in the provider's OpenID discovery document (`/.well-known/openid-configuration`). Always supply `issuer` and `audience`.

## JWT in the Next.js stack

| Place | How |
|---|---|
| Session cookie | `SignJWT` in a Server Action; verify in the DAL and Proxy |
| Proxy | `jwtVerify` works there (Proxy runs on the Node.js runtime in v16); do **not** hit the database from Proxy |
| Route Handler for an API | Read `Authorization: Bearer <token>` with `req.headers.get("authorization")`, verify, then authorize |
| Calling a backend | Server Component or Server Action fetches with the token; keep it off the client |

```ts
// app/api/items/route.ts
import { verify } from "@/app/lib/jwt";

export async function GET(req: Request) {
  const raw = req.headers.get("authorization")?.replace(/^Bearer /i, "");
  const claims = raw ? await verify(raw) : null;
  if (!claims) return new Response(null, { status: 401 });
  // authorize, then return data
}
```

## Debugging

| Symptom | Likely cause | Fix |
|---|---|---|
| `JWSSignatureVerificationFailed` | Wrong secret/key, tampered token | Compare secrets across environments |
| `JWTExpired` | Past `exp` | Re-issue or re-login |
| `JWTClaimValidationFailed` | `iss`, `aud` or `nbf` mismatch | Match the values used at signing |
| `Invalid Compact JWS` | Token truncated, cookie too large, extra whitespace | Check the cookie value length (4 KB cookie limit) |
| `"alg" (Algorithm) Header Parameter value not allowed` | Token alg is not in your `algorithms` list | Sign and verify with the same alg |
| `Key must be …` errors | Wrong key type or length (JWE needs exactly 32 bytes for A256GCM) | Generate the right key |
| Clock-related failures | Server clocks differ | Sync clocks; `jwtVerify` accepts `clockTolerance` |
| Cookie silently dropped | Cookie over 4 KB | Shrink the payload; store a session ID |

Decode a token for inspection with a local script, not by pasting production tokens into third-party sites.

## Common mistakes

| Mistake | Fix |
|---|---|
| Putting personal data in a signed JWT | Keep to ID and role |
| Not pinning `algorithms` | Always pass them |
| Skipping `exp` | Always expire; keep access tokens short |
| Treating the token as proof the user still exists or has the role | Re-check against the database for sensitive operations |
| Storing in `localStorage` | HttpOnly cookie |
| Same secret for everything | Separate keys per purpose |
| Hand-rolling HMAC or JWT parsing | Use `jose` |
| Using decoded-but-unverified claims | Only trust the result of `jwtVerify` |

## Quick Summary

- A JWT is header, payload and signature; signing prevents tampering but does not hide the payload.
- Use `jose`: sign with `SignJWT`, verify with `jwtVerify` and pinned `algorithms`, `exp`, and where relevant `iss` and `aud`.
- Encrypt (JWE) only if the browser must not read the claims.
- A stateless token cannot be revoked before `exp`; use short lifetimes, a version claim, or database sessions.
- Keep tokens in `HttpOnly` cookies; verify third-party tokens with their JWKS.

## Next

- [OAuth](./03-oauth.md)
- [Sessions and Cookies](./01-sessions-and-cookies.md)
- [Protecting Routes](./04-protecting-routes.md)
