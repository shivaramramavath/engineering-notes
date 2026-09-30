# JWT and Tokens

Stateless authentication: after login, the server gives the client a signed token, and the client presents it on every request.

## What a JWT is

A **JSON Web Token** is three Base64URL-encoded parts joined by dots:

```
eyJhbGciOiJIUzI1NiJ9.eyJzdWIiOiI0MiIsInJvbGUiOiJ1c2VyIn0.Xk3f...
└──── header ────────┘└────────── payload ───────────────┘└ signature ┘
```

```json
// header
{ "alg": "HS256", "typ": "JWT" }

// payload (the "claims")
{ "sub": "42", "role": "user", "iat": 1760000000, "exp": 1760000900 }
```

The signature is computed over `header.payload` using a secret (or private key). If anyone changes a single character, verification fails.

### The single most important fact

> **A JWT is signed, not encrypted.**

Anyone can decode the payload (paste a token into jwt.io). The signature guarantees it wasn't **tampered with**, not that it's **secret**. Never put passwords, personal data, or anything sensitive in the payload.

### Standard claims

| Claim | Meaning |
|---|---|
| `sub` | Subject — the user ID |
| `iat` | Issued at |
| `exp` | Expiration time (always set this) |
| `nbf` | Not valid before |
| `iss` | Issuer — who created it |
| `aud` | Audience — who it's intended for |
| `jti` | Unique token ID (useful for revocation) |

---

## Signing algorithms

| Type | Algorithms | Key | When to use |
|---|---|---|---|
| **Symmetric** | HS256 | One shared secret signs *and* verifies | Single service, simplest setup |
| **Asymmetric** | RS256, ES256, EdDSA | Private key signs, public key verifies | Multiple services need to verify without being able to issue tokens |

For microservices, asymmetric signing means only the auth service holds the private key; every other service verifies with the public key.

---

## Install and configure

```bash
npm install jsonwebtoken
```

```js
// config/jwt.js
export const ACCESS_SECRET = process.env.JWT_ACCESS_SECRET;
export const REFRESH_SECRET = process.env.JWT_REFRESH_SECRET;

if (!ACCESS_SECRET || !REFRESH_SECRET) {
  throw new Error("JWT secrets are not configured");   // fail at startup, not at first login
}
```

Generate a strong secret (at least 32 random bytes):

```bash
node -e "console.log(require('crypto').randomBytes(64).toString('hex'))"
```

Use **different** secrets for access and refresh tokens, and load them from the environment (see `16-production/01-environment-management.md`).

---

## Signing and verifying

```js
import jwt from "jsonwebtoken";
import { ACCESS_SECRET } from "./config/jwt.js";

// sign
const token = jwt.sign(
  { sub: user.id, role: user.role },
  ACCESS_SECRET,
  { algorithm: "HS256", expiresIn: "15m", issuer: "my-api" }
);

// verify — throws if invalid, expired, or tampered with
try {
  const payload = jwt.verify(token, ACCESS_SECRET, {
    algorithms: ["HS256"],     // ALWAYS pin the algorithm
    issuer: "my-api",
  });
  console.log(payload.sub);
} catch (err) {
  // err.name: "TokenExpiredError" | "JsonWebTokenError" | "NotBeforeError"
}
```

`jwt.decode(token)` only decodes — it does **not** verify. Never make authorization decisions from a decoded-but-unverified token.

---

## Auth middleware

```js
// middleware/authenticate.js
import jwt from "jsonwebtoken";
import { ACCESS_SECRET } from "../config/jwt.js";

export function authenticate(req, res, next) {
  const header = req.headers.authorization;   // "Bearer <token>"

  if (!header?.startsWith("Bearer ")) {
    return res.status(401).json({ error: "Missing token" });
  }

  try {
    const payload = jwt.verify(header.slice(7), ACCESS_SECRET, {
      algorithms: ["HS256"],
      issuer: "my-api",
    });
    req.user = { id: payload.sub, role: payload.role };
    next();
  } catch (err) {
    const message = err.name === "TokenExpiredError" ? "Token expired" : "Invalid token";
    return res.status(401).json({ error: message });
  }
}

// usage
app.get("/api/me", authenticate, (req, res) => res.json(req.user));
```

Role checks (`requireRole("admin")`) are covered in `06-express/06-auth-and-authorization.md`.

---

## The access + refresh token pattern

A JWT can't easily be revoked before it expires. So make the important token **short-lived** and use a second token to get new ones.

| | Access token | Refresh token |
|---|---|---|
| Lifetime | 5–15 minutes | 7–30 days |
| Sent with | Every API request | Only to `/auth/refresh` |
| Stored server-side? | No (stateless) | **Yes** (so it can be revoked) |
| If stolen | Useful for minutes | Dangerous — must be detectable |

```
1. POST /auth/login     → access token (15m) + refresh token (7d)
2. GET  /api/...        → Authorization: Bearer <access>
3. access expires       → API returns 401 "Token expired"
4. POST /auth/refresh   → send refresh token, receive NEW access + NEW refresh
5. POST /auth/logout    → delete refresh token from the database
```

### Issuing tokens

```js
import crypto from "node:crypto";
import jwt from "jsonwebtoken";

function hashToken(token) {
  return crypto.createHash("sha256").update(token).digest("hex");
}

async function issueTokens(user, familyId = crypto.randomUUID()) {
  const accessToken = jwt.sign({ sub: user.id, role: user.role }, ACCESS_SECRET, {
    algorithm: "HS256", expiresIn: "15m", issuer: "my-api",
  });

  const refreshToken = jwt.sign({ sub: user.id, family: familyId }, REFRESH_SECRET, {
    algorithm: "HS256", expiresIn: "7d", issuer: "my-api", jwtid: crypto.randomUUID(),
  });

  // store a HASH of the refresh token, never the token itself
  await RefreshToken.create({
    userId: user.id,
    familyId,
    tokenHash: hashToken(refreshToken),
    expiresAt: new Date(Date.now() + 7 * 24 * 60 * 60 * 1000),
  });

  return { accessToken, refreshToken };
}
```

### Refresh token rotation with reuse detection

Every refresh **replaces** the refresh token. If an old one is presented again, someone has stolen it — kill the whole family.

```js
router.post("/refresh", async (req, res) => {
  const token = req.cookies.refreshToken;
  if (!token) return res.status(401).json({ error: "No refresh token" });

  let payload;
  try {
    payload = jwt.verify(token, REFRESH_SECRET, { algorithms: ["HS256"], issuer: "my-api" });
  } catch {
    return res.status(401).json({ error: "Invalid refresh token" });
  }

  const stored = await RefreshToken.findOne({ tokenHash: hashToken(token) });

  if (!stored) {
    // Valid signature but not in the DB → it was already used (rotated) or revoked.
    // Assume theft: revoke every token in this family.
    await RefreshToken.deleteMany({ familyId: payload.family });
    return res.status(401).json({ error: "Refresh token reuse detected" });
  }

  await stored.deleteOne();                       // one-time use

  const user = await User.findById(payload.sub);
  if (!user) return res.status(401).json({ error: "User not found" });

  const tokens = await issueTokens(user, payload.family);
  setRefreshCookie(res, tokens.refreshToken);
  res.json({ accessToken: tokens.accessToken });
});
```

---

## Where should the client store tokens?

| Storage | XSS can steal it? | CSRF risk? | Verdict |
|---|---|---|---|
| `localStorage` / `sessionStorage` | **Yes** — any injected script can read it | No | Avoid for refresh tokens |
| JS memory (variable) | Harder, but possible | No | Good for the short-lived access token |
| **httpOnly cookie** | **No** — JS can't read it | Yes, unless mitigated | Best for the refresh token |

A widely used compromise:

- **Access token** → returned in the JSON body, kept in memory by the frontend
- **Refresh token** → `httpOnly`, `Secure`, `SameSite` cookie, scoped to the refresh endpoint

```js
function setRefreshCookie(res, token) {
  res.cookie("refreshToken", token, {
    httpOnly: true,                             // not readable by JavaScript
    secure: process.env.NODE_ENV === "production",   // HTTPS only
    sameSite: "strict",                          // not sent on cross-site requests
    path: "/api/auth",                            // only sent to auth routes
    maxAge: 7 * 24 * 60 * 60 * 1000,
  });
}
```

This requires `cookie-parser` (`app.use(cookieParser())`). Cookie flags are explained in `05-http-web/03-cookies.md`.

---

## Logout and revocation

JWTs are stateless, so "logging out" a pure-JWT system doesn't invalidate anything already issued. Options:

1. **Delete the refresh token** server-side (shown above). The access token still works until it expires — a reason to keep it short.
2. **Denylist** access-token `jti` values in Redis with a TTL equal to the token's remaining life. Every request now needs a Redis lookup (which begins to resemble a session).
3. **Token version** on the user record: put `tokenVersion` in the JWT, increment it on "log out everywhere" or password change, and reject mismatches.

```js
router.post("/logout", async (req, res) => {
  const token = req.cookies.refreshToken;
  if (token) await RefreshToken.deleteOne({ tokenHash: hashToken(token) });
  res.clearCookie("refreshToken", { path: "/api/auth" });
  res.sendStatus(204);
});
```

---

## JWT vs sessions

| | JWT (stateless) | Session (stateful) |
|---|---|---|
| Server storage | None for access tokens | Session store (Redis) |
| Instant revocation | Hard | Easy — delete the session |
| Horizontal scaling | Trivial | Needs a shared store |
| Best for | APIs, mobile clients, service-to-service | Traditional web apps, browsers only |
| Size per request | Larger (token in header) | Small (session ID) |

Many teams reach for JWT by default when a plain session would be simpler and safer. See `03-sessions.md`.

---

## Common JWT vulnerabilities

| Vulnerability | Fix |
|---|---|
| **`alg: none`** — attacker strips the signature and sets the algorithm to none | Always pass `algorithms: [...]` to `verify` |
| **Algorithm confusion** — attacker signs with HS256 using your RS256 *public* key as the secret | Pin the algorithm; keep key types separate |
| Weak secret, brute-forced offline | 32+ random bytes |
| No `exp` | Always set `expiresIn` |
| Sensitive data in payload | It's readable by anyone |
| Token in `localStorage` + XSS | httpOnly cookie for refresh token, CSP (`07-helmet.md`) |
| Trusting the `role` claim forever | Short expiry; re-check the database for sensitive operations |

## Modern alternative: `jose`

`jsonwebtoken` is the classic choice. The `jose` package is a maintained, standards-focused alternative with first-class support for JWKS, EdDSA, and edge runtimes:

```js
import { SignJWT, jwtVerify } from "jose";

const secret = new TextEncoder().encode(process.env.JWT_ACCESS_SECRET);

const token = await new SignJWT({ role: "user" })
  .setProtectedHeader({ alg: "HS256" })
  .setSubject(user.id)
  .setIssuedAt()
  .setExpirationTime("15m")
  .sign(secret);

const { payload } = await jwtVerify(token, secret, { algorithms: ["HS256"] });
```

## Next

**`03-sessions.md`** covers the stateful alternative — often the better choice for browser-based apps.
