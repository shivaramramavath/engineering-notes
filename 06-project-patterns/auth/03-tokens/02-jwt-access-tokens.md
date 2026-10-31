# JWT Access Tokens

The access token is a short-lived, signed JSON Web Token that the API verifies locally, with no database or Redis call. It carries just enough identity for the request: who the user is, which session it belongs to, and their role.

**Prerequisites:** [01-credential-types.md](../01-fundamentals/01-credential-types.md), [03-crypto-primitives.md](../01-fundamentals/03-crypto-primitives.md)

---

## Anatomy

A JWT is three base64url parts joined by dots:

```text
eyJhbGciOiJIUzI1NiJ9 . eyJzdWIiOiJ1MSIs... . 4xK2...
      header                  payload          signature
```

```json
// header
{ "alg": "HS256", "typ": "JWT", "kid": "k2" }

// payload (claims)
{
  "sub": "USER_ID",
  "sid": "SESSION_ID",
  "jti": "unique-token-id",
  "role": "user",
  "iss": "your-api",
  "aud": "your-frontend",
  "iat": 1754650000,
  "exp": 1754650600
}
```

The signature covers `header.payload`. Change one character of the payload and verification fails.

**Signed is not encrypted.** Anyone holding the token can read the payload. Put nothing in it you wouldn't show the user.

---

## Claims used here

| Claim | Meaning | Why |
|---|---|---|
| `sub` | Subject: the user id | Identifies who |
| `sid` | Session id (custom) | Links token to the Redis session, enabling per-session checks and "revoke this device" UX |
| `jti` | Unique token id | Tracing, optional deny-listing |
| `role` | Role (custom) | Cheap authorization without a DB hit |
| `iss` | Issuer | Reject tokens from other systems |
| `aud` | Audience | Reject tokens minted for another service |
| `iat` / `exp` | Issued / expires (seconds since epoch) | Lifetime |

`iss`, `aud` and `exp` are verified **only if you ask the library to**; see below.

---

## Issue

```bash
npm i jsonwebtoken
```

```js
import jwt from "jsonwebtoken";
import crypto from "crypto";

export function signAccessToken(user, sessionId) {
  return jwt.sign(
    { sid: sessionId, role: user.role },
    process.env.ACCESS_TOKEN_SECRET,
    {
      algorithm: "HS256",
      subject: user._id.toString(),
      jwtid: crypto.randomUUID(),
      issuer: "your-api",
      audience: "your-frontend",
      expiresIn: "10m",
    }
  );
}
```

`subject`, `jwtid`, `issuer`, `audience` and `expiresIn` are `jsonwebtoken` options that become the standard claims, so you don't hand-write them into the payload.

---

## Verify middleware

```js
export const authenticate = (req, res, next) => {
  const header = req.headers.authorization;
  if (!header?.startsWith("Bearer ")) {
    return res.status(401).json({ message: "Authentication required" });
  }

  try {
    req.user = jwt.verify(header.slice(7), process.env.ACCESS_TOKEN_SECRET, {
      algorithms: ["HS256"],        // pin the algorithm
      issuer: "your-api",
      audience: "your-frontend",
      clockTolerance: 5,            // seconds of clock skew allowed
    });
    next();
  } catch {
    res.status(401).json({ message: "Invalid or expired access token" });
  }
};
```

Verification checks signature, `exp`, `nbf`, and (because you passed them) `iss` and `aud`.

### Always pin `algorithms`

The header's `alg` is attacker-controlled input. Historically, servers that trusted it were tricked with `alg: "none"` (no signature) or by switching an RS256 token to HS256 and signing it with the public key (algorithm confusion). Passing an explicit `algorithms` allowlist closes both. See [07-jwt-algorithms-and-keys.md](./07-jwt-algorithms-and-keys.md).

### Return a clean 401, no details

Don't tell the client why a token failed ("signature invalid" vs "expired"). The frontend only needs to know to attempt a refresh. Log the reason server-side, without logging the token.

---

## How long should it live?

Access tokens cannot be revoked individually without storing state, so their lifetime **is** the revocation delay.

```text
Session deleted at 10:00:00
Access token issued at 09:55:00, exp 10:05:00
→ still accepted until 10:05:00
```

Short TTL (5-15 minutes) keeps that window small; the refresh flow makes the short TTL invisible to users. If an endpoint can't tolerate the window (changing email, deleting an account, admin actions), check Redis for the `sid` or re-authenticate the user ([step-up-auth](../05-mfa/05-step-up-auth.md)).

---

## Refresh interplay

```text
GET /api/orders  → 401 (expired)
        ↓
POST /api/auth/refresh (cookie)  → new access token (+ rotated refresh)
        ↓
Retry GET /api/orders
```

The frontend interceptor details are in [06-token-storage.md](./06-token-storage.md).

---

## Common mistakes

| Mistake | Consequence |
|---|---|
| Not passing `algorithms` | Algorithm confusion / `none` risk |
| Using `jwt.decode()` instead of `jwt.verify()` | `decode` doesn't check the signature at all |
| Long-lived access tokens (days) | Huge unrevocable window |
| Sensitive data in the payload | Readable by anyone with the token |
| Weak or short HS256 secret | Offline brute-forceable; use 256+ random bits |
| Skipping `iss`/`aud` checks | Tokens from other apps accepted |
| Trusting `role` for sensitive operations | Stale up to the token lifetime |
| Putting the JWT in `localStorage` | Readable by any XSS |

## Debugging

| Error (`jsonwebtoken`) | Meaning |
|---|---|
| `TokenExpiredError: jwt expired` | `exp` passed; client should refresh |
| `JsonWebTokenError: invalid signature` | Wrong secret/key, or token was altered |
| `JsonWebTokenError: jwt audience invalid` / `jwt issuer invalid` | Config mismatch between issuer and verifier |
| `JsonWebTokenError: invalid algorithm` | Token's `alg` isn't in your allowlist |
| `jwt must be provided` | Missing/empty header (or env secret undefined) |
| Token "expired" right after issue | Server clock skew; sync with NTP, add `clockTolerance` |

Paste a token into a decoder such as jwt.io only for **fake/dev** tokens, never production ones.

---

## Quick summary

- JWT = signed claims; readable by anyone, tamper-proof without the key.
- Use `sub`, `sid`, `role`, `iss`, `aud`, `exp`, and keep the payload small.
- Pin `algorithms`, verify `iss`/`aud`, use `verify` (never `decode`).
- 10-minute TTL: its length is your revocation delay.
- Keep it in memory, never in `localStorage`.

**Next:** [03-opaque-tokens.md](./03-opaque-tokens.md)
