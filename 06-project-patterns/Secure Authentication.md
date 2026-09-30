# 🔐 MERN Authentication — Secure Authentication Architecture

A production-oriented authentication architecture for MERN applications using **MongoDB, Redis, JWT, Argon2id, and secure HTTP cookies**.

The design separates identity, authorization, sessions, and temporary security credentials so each component has a clear responsibility.

---

# 1. Architecture Overview

```text
                           React
                             │
              ┌──────────────┴──────────────┐
              │                             │
       Access Token                  Refresh Token
       Memory only                  HttpOnly Cookie
       Short-lived                       │
              │                           │
              ▼                           ▼
        Normal APIs                 /auth/refresh
              │                           │
              ▼                           ▼
           Express                    Redis
              │                           │
              │                 ┌─────────┴─────────┐
              │                 │                   │
              ▼                 ▼                   ▼
          MongoDB            Sessions         Temporary Tokens
              │              │                 │
              │              ├─ Refresh        ├─ Password Reset
              │              └─ Devices        └─ Email Verification
              │
              └── User / Account Data
```

---

# 2. Core Security Principles

The system follows these principles:

1. **Passwords are never stored directly.**
2. **Access tokens are short-lived.**
3. **Access tokens stay in React memory.**
4. **Refresh tokens are stored in HttpOnly cookies.**
5. **Refresh tokens are rotated after every successful refresh.**
6. **Only refresh-token hashes are stored in Redis.**
7. **Each device/login gets an independent session.**
8. **Password-reset and email-verification tokens are separate credentials.**
9. **Temporary credentials always have TTLs.**
10. **Password reset revokes all existing sessions.**
11. **Refresh rotation must be atomic.**
12. **Sensitive credentials are never written to logs.**

---

# 3. Authentication Credentials

The application has four different credential types.

| Credential | Purpose | Storage | Lifetime |
|---|---|---|---|
| Access Token | API authorization | React memory | ~10 min |
| Refresh Token | Continue session | HttpOnly cookie | ~30 days |
| Password Reset Token | Reset password | Email → Redis hash | ~15 min |
| Email Verification Token | Verify email | Email → Redis hash | ~30 min |

Never reuse one credential for another purpose.

---

# 4. Data Ownership

## MongoDB

MongoDB stores **permanent account data**.

```text
users
│
├── _id
├── email
├── passwordHash
├── role
├── isEmailVerified
├── isActive
├── createdAt
└── updatedAt
```

## Redis

Redis stores **temporary authentication state**.

```text
session:{sessionId}

user:sessions:{userId}

password-reset:token:{tokenHash}

email-verification:{tokenHash}

rate-limit:...
```

## Browser

```text
React Memory
└── accessToken

HttpOnly Cookie
└── refreshToken
```

---

# 5. MongoDB User Model

```js
import mongoose from "mongoose";

const userSchema = new mongoose.Schema(
  {
    email: {
      type: String,
      required: true,
      unique: true,
      lowercase: true,
      trim: true,
      index: true,
    },

    passwordHash: {
      type: String,
      required: true,
    },

    role: {
      type: String,
      enum: ["user", "admin"],
      default: "user",
    },

    isEmailVerified: {
      type: Boolean,
      default: false,
    },

    isActive: {
      type: Boolean,
      default: true,
    },
  },
  {
    timestamps: true,
  }
);

export default mongoose.model("User", userSchema);
```

Never store:

```text
❌ Plaintext passwords
❌ Access tokens
❌ Refresh tokens
❌ Password reset tokens
❌ Email verification tokens
```

---

# 6. Password Security

Passwords should use **Argon2id**.

## Hash

```js
import argon2 from "argon2";

const passwordHash = await argon2.hash(password, {
  type: argon2.argon2id,
});
```

Store:

```text
passwordHash
    ↓
MongoDB
```

## Verify

```js
const valid = await argon2.verify(
  user.passwordHash,
  password
);

if (!valid) {
  throw new Error("Invalid credentials");
}
```

Never use plain SHA-256 for passwords.

```js
// ❌ Do not do this
crypto
  .createHash("sha256")
  .update(password)
  .digest("hex");
```

---

# 7. Secure Random Token Generation

Authentication credentials must use a cryptographically secure random generator.

Never use:

```js
// ❌
Math.random();
```

Use Node.js `crypto`.

## Standard Token

```js
import crypto from "crypto";

const token = crypto
  .randomBytes(32)
  .toString("base64url");
```

This provides **256 bits of randomness**.

## Refresh Secret

```js
const refreshSecret = crypto
  .randomBytes(64)
  .toString("base64url");
```

This provides **512 bits of randomness**.

---

# 8. Token Hashing

Temporary credentials should not be stored in plaintext in Redis.

Create a reusable hash utility:

```js
import crypto from "crypto";

export const hashToken = (token) => {
  return crypto
    .createHash("sha256")
    .update(token)
    .digest("hex");
};
```

Example:

```js
const token = crypto
  .randomBytes(32)
  .toString("base64url");

const tokenHash = hashToken(token);
```

The relationship is:

```text
                 Random Token
                      │
                      │ SHA-256
                      ▼
                   Hash
                      │
                      ▼
                    Redis
```

The raw token is given to the client/email.

The hash is stored server-side.

---

# 9. Constant-Time Comparison

For security-sensitive token comparisons, use a constant-time comparison.

```js
import crypto from "crypto";

export const safeCompare = (
  receivedHash,
  storedHash
) => {
  const received = Buffer.from(
    receivedHash,
    "hex"
  );

  const stored = Buffer.from(
    storedHash,
    "hex"
  );

  if (received.length !== stored.length) {
    return false;
  }

  return crypto.timingSafeEqual(
    received,
    stored
  );
};
```

Usage:

```js
const receivedHash = hashToken(token);

const valid = safeCompare(
  receivedHash,
  storedHash
);

if (!valid) {
  throw new Error("Invalid token");
}
```

---

# 10. Access Token

The access token is a **short-lived JWT**.

Example payload:

```json
{
  "sub": "USER_ID",
  "sid": "SESSION_ID",
  "jti": "TOKEN_ID",
  "role": "user",
  "iss": "your-api",
  "aud": "your-frontend",
  "iat": 1754650000,
  "exp": 1754650600
}
```

## Generate

```js
import jwt from "jsonwebtoken";
import crypto from "crypto";

const accessToken = jwt.sign(
  {
    sub: user._id.toString(),
    sid: sessionId,
    jti: crypto.randomUUID(),
    role: user.role,
  },

  process.env.ACCESS_TOKEN_SECRET,

  {
    expiresIn: "10m",
    issuer: "your-api",
    audience: "your-frontend",
  }
);
```

---

# 11. Access Token Storage

Store the access token only in **React memory**.

```text
React
└── Memory
    └── accessToken
```

Do not persist it in:

```text
❌ localStorage
❌ sessionStorage
❌ IndexedDB
❌ persistent Zustand
❌ cookie
```

The advantage is that a page reload loses the access token.

React can then obtain a new access token through the refresh flow.

---

# 12. Access Token Verification

Normal API requests send:

```http
Authorization: Bearer ACCESS_TOKEN
```

Middleware:

```js
import jwt from "jsonwebtoken";

export const authenticate = (
  req,
  res,
  next
) => {
  try {
    const header =
      req.headers.authorization;

    if (!header?.startsWith("Bearer ")) {
      return res.status(401).json({
        message: "Authentication required",
      });
    }

    const token =
      header.substring(7);

    const payload = jwt.verify(
      token,
      process.env.ACCESS_TOKEN_SECRET,
      {
        issuer: "your-api",
        audience: "your-frontend",
      }
    );

    req.user = payload;

    next();

  } catch {
    return res.status(401).json({
      message: "Invalid or expired access token",
    });
  }
};
```

JWT verification checks:

```text
✓ Signature
✓ Expiration
✓ Issuer
✓ Audience
```

Normal API requests do **not** need Redis.

---

# 13. Refresh Token

A refresh token is an **opaque session credential**.

Use:

```text
sessionId.secret
```

Generate:

```js
const sessionId = crypto
  .randomBytes(32)
  .toString("base64url");

const secret = crypto
  .randomBytes(64)
  .toString("base64url");

const refreshToken =
  `${sessionId}.${secret}`;
```

Conceptually:

```text
S1.xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
│  │
│  └── Secret
│
└───── Session ID
```

The refresh token does not contain:

```text
❌ Email
❌ Password
❌ Role
❌ User object
❌ Permissions
```

---

# 14. Refresh Token Cookie

Store the refresh token in an HttpOnly cookie.

```js
res.cookie(
  "refreshToken",
  refreshToken,
  {
    httpOnly: true,
    secure: process.env.NODE_ENV === "production",
    sameSite: "strict",

    // Only send this cookie to refresh endpoint
    path: "/api/auth/refresh",

    maxAge:
      30 * 24 * 60 * 60 * 1000,
  }
);
```

### Cookie properties

| Property | Purpose |
|---|---|
| `HttpOnly` | JavaScript cannot access cookie |
| `Secure` | HTTPS only in production |
| `SameSite` | Reduces CSRF |
| `Path` | Limits where cookie is sent |
| `Max-Age` | Cookie lifetime |

If cross-site cookies are required, configure `SameSite=None; Secure` together with an appropriate CSRF defense.

---

# 15. Redis Session

A session represents **one authenticated login/device**.

Redis key:

```text
session:{sessionId}
```

Example:

```text
session:S1
```

Store:

```json
{
  "userId": "U1",
  "refreshTokenHash": "HASH",
  "status": "active",
  "createdAt": 1754650000000,
  "lastUsedAt": 1754651000000
}
```

Set a TTL:

```js
await redis.expire(
  `session:${sessionId}`,
  30 * 24 * 60 * 60
);
```

---

# 16. Create a Session

```js
const sessionId = crypto
  .randomBytes(32)
  .toString("base64url");

const refreshSecret = crypto
  .randomBytes(64)
  .toString("base64url");

const refreshToken =
  `${sessionId}.${refreshSecret}`;

const refreshTokenHash =
  hashToken(refreshSecret);

await redis.hSet(
  `session:${sessionId}`,
  {
    userId: user._id.toString(),
    refreshTokenHash,
    status: "active",
    createdAt: Date.now(),
    lastUsedAt: Date.now(),
  }
);

await redis.expire(
  `session:${sessionId}`,
  30 * 24 * 60 * 60
);
```

The raw refresh token goes to the browser.

Redis stores only:

```text
SHA256(refreshSecret)
```

---

# 17. Multiple Devices

Every login creates an independent session.

```text
User U1
│
├── Session S1
│     └── Laptop
│
├── Session S2
│     └── Phone
│
└── Session S3
      └── Tablet
```

Each has its own refresh token:

```text
S1 → R1
S2 → R2
S3 → R3
```

Therefore:

```text
Logout Laptop
      ↓
Delete S1
      ↓
Laptop ❌
Phone  ✅
Tablet  ✅
```

---

# 18. Track User Sessions

Maintain a Redis Set:

```text
user:sessions:{userId}
```

Example:

```text
user:sessions:U1

S1
S2
S3
```

When creating a session:

```js
await redis.sAdd(
  `user:sessions:${userId}`,
  sessionId
);
```

This makes **logout all devices** straightforward.

---

# 19. Refresh Token Verification

Browser automatically sends:

```text
S1.SECRET_A
```

Extract:

```js
const separator =
  refreshToken.indexOf(".");

if (separator === -1) {
  throw new Error(
    "Invalid refresh token"
  );
}

const sessionId =
  refreshToken.slice(0, separator);

const secret =
  refreshToken.slice(separator + 1);
```

Now:

```text
sessionId = S1
secret    = SECRET_A
```

Find the session:

```js
const session =
  await redis.hGetAll(
    `session:${sessionId}`
  );
```

Check existence:

```js
if (!session.userId) {
  throw new Error(
    "Invalid refresh token"
  );
}
```

Check status:

```js
if (session.status !== "active") {
  throw new Error(
    "Session revoked"
  );
}
```

Hash the supplied secret:

```js
const receivedHash =
  hashToken(secret);
```

Compare:

```js
const valid = safeCompare(
  receivedHash,
  session.refreshTokenHash
);

if (!valid) {
  throw new Error(
    "Invalid refresh token"
  );
}
```

If everything matches:

```text
✓ Session exists
✓ Session active
✓ Secret matches
✓ Refresh token valid
```

---

# 20. Refresh Token Rotation

Never reuse the same refresh token indefinitely.

Initial:

```text
R1
```

Refresh:

```text
R1
 │
 ▼
Verify
 │
 ▼
Invalidate R1
 │
 ▼
Generate R2
```

Generate a new secret:

```js
const newSecret =
  crypto
    .randomBytes(64)
    .toString("base64url");
```

Hash it:

```js
const newHash =
  hashToken(newSecret);
```

Update Redis:

```js
await redis.hSet(
  `session:${sessionId}`,
  {
    refreshTokenHash: newHash,
    lastUsedAt: Date.now(),
  }
);
```

Create the new token:

```js
const newRefreshToken =
  `${sessionId}.${newSecret}`;
```

Set it as the new cookie.

Result:

```text
R1 ❌
R2 ✅
```

Next refresh:

```text
R2 → R3
```

Only the latest refresh token should be valid for that session.

---

# 21. Refresh Token Reuse Detection

Suppose:

```text
Current refresh token = R3
```

An attacker has:

```text
Old refresh token = R2
```

Attacker sends R2.

Server calculates:

```text
hash(R2)
```

Redis contains:

```text
hash(R3)
```

Therefore:

```text
hash(R2) ≠ hash(R3)
```

The token is invalid.

A conservative response is to revoke that session:

```js
await redis.del(
  `session:${sessionId}`
);
```

The affected device must authenticate again.

---

# 22. Refresh Race Condition

This is one of the most important implementation details.

Suppose two requests arrive simultaneously:

```text
Request A → R1
Request B → R1
```

A naive implementation:

```text
GET Redis
    ↓
Compare
    ↓
SET Redis
```

can allow both requests to observe R1 as valid.

### Required behavior

```text
Check R1
   ↓
If current
   ↓
Replace R1 → R2
   ↓
Only one request succeeds
```

Make this operation **atomic** using a Redis Lua script or another carefully designed atomic mechanism.

---

# 23. Complete Refresh Flow

```text
Access Token Expired
        │
        ▼
POST /auth/refresh
        │
        ▼
HttpOnly Refresh Cookie
        │
        ▼
Extract sessionId + secret
        │
        ▼
Redis Session Lookup
        │
        ▼
Check Session Status
        │
        ▼
Hash Secret
        │
        ▼
Constant-Time Comparison
        │
        ├── Invalid
        │      ↓
        │   Reject / Revoke
        │
        └── Valid
               │
               ▼
        Atomic Token Rotation
               │
               ├── New Refresh Token
               │
               └── New Access Token
```

---

# 24. Logout

Extract the session:

```js
const [sessionId] =
  refreshToken.split(".");
```

Delete it:

```js
await redis.del(
  `session:${sessionId}`
);
```

Remove it from the user's session set:

```js
await redis.sRem(
  `user:sessions:${userId}`,
  sessionId
);
```

Clear the cookie:

```js
res.clearCookie(
  "refreshToken",
  {
    httpOnly: true,
    secure: true,
    sameSite: "strict",
    path: "/api/auth/refresh",
  }
);
```

React clears the in-memory access token.

---

# 25. Logout All Devices

Get all sessions:

```js
const sessionIds =
  await redis.sMembers(
    `user:sessions:${userId}`
  );
```

Delete them atomically/batched:

```js
const pipeline = redis.multi();

for (const sessionId of sessionIds) {
  pipeline.del(
    `session:${sessionId}`
  );
}

pipeline.del(
  `user:sessions:${userId}`
);

await pipeline.exec();
```

Result:

```text
Laptop ❌
Phone  ❌
Tablet ❌
```

---

# 26. Password Reset

Password reset uses a completely separate token.

Generate:

```js
const token =
  crypto
    .randomBytes(32)
    .toString("base64url");
```

Hash:

```js
const tokenHash =
  hashToken(token);
```

Store:

```js
await redis.set(
  `password-reset:token:${tokenHash}`,
  JSON.stringify({
    userId: user._id.toString(),
  }),
  {
    EX: 15 * 60,
  }
);
```

Email the raw token:

```text
https://yourapp.com/reset-password?token=TOKEN
```

Redis stores only:

```text
SHA256(TOKEN)
```

---

# 27. Password Reset Verification

Client sends:

```json
{
  "token": "TOKEN",
  "password": "NewStrongPassword"
}
```

Server:

```js
const tokenHash =
  hashToken(token);
```

Lookup:

```js
const data =
  await redis.get(
    `password-reset:token:${tokenHash}`
  );
```

If missing:

```text
❌ Invalid or expired token
```

If found:

```js
const { userId } =
  JSON.parse(data);
```

Then:

```text
Token valid
    ↓
Find User
    ↓
Argon2id(new password)
    ↓
Update MongoDB
    ↓
Delete reset token
    ↓
Revoke ALL sessions
```

---

# 28. Password Reset Security

After successful password reset:

```text
Session S1 ❌
Session S2 ❌
Session S3 ❌
```

The user must authenticate again.

This prevents previously issued sessions from remaining valid after the password has been changed.

---

# 29. Email Verification

Email verification also uses a separate random token.

Generate:

```js
const token =
  crypto
    .randomBytes(32)
    .toString("base64url");
```

Hash:

```js
const tokenHash =
  hashToken(token);
```

Store:

```js
await redis.set(
  `email-verification:${tokenHash}`,
  JSON.stringify({
    userId: user._id.toString(),
  }),
  {
    EX: 30 * 60,
  }
);
```

Email:

```text
https://yourapp.com/verify-email?token=TOKEN
```

---

# 30. Email Verification Flow

```text
Registration
     │
     ▼
Create User
     │
     ├── isEmailVerified = false
     │
     ▼
Generate Verification Token
     │
     ├──────────────► Email
     │
     └── SHA-256 ───► Redis
                       │
                       └── TTL 30 min
```

User clicks:

```text
Email
  ↓
React
  ↓
POST /auth/verify-email
  ↓
Hash token
  ↓
Redis lookup
  ↓
Get userId
  ↓
MongoDB
  ↓
isEmailVerified = true
  ↓
Delete Redis token
```

---

# 31. Remove Tokens from URLs

Reset and verification links contain sensitive credentials.

React should immediately remove the token from browser history/address bar:

```js
const token = new URLSearchParams(
  window.location.search
).get("token");

window.history.replaceState(
  {},
  document.title,
  "/reset-password"
);
```

The token should only remain temporarily in application memory.

Also use:

```http
Referrer-Policy: no-referrer
```

Avoid third-party analytics/scripts on sensitive reset pages.

Ensure reverse proxies and application logs don't capture reset/verification tokens.

---

# 32. Forgot Password Enumeration Protection

Never return:

```json
{
  "message": "Email does not exist"
}
```

Instead:

```json
{
  "message": "If an account exists for this email, a reset link has been sent."
}
```

The response should be consistent regardless of whether the account exists.

---

# 33. Rate Limiting

Protect authentication endpoints:

```text
POST /auth/register
POST /auth/login
POST /auth/refresh
POST /auth/forgot-password
POST /auth/reset-password
POST /auth/resend-verification
```

Use Redis-backed rate limiting.

Sensitive operations should have stricter limits.

```text
Login
    ↓
IP + account based limits

Forgot Password
    ↓
IP + email based limits

Reset Password
    ↓
IP + token based limits
```

---

# 34. CORS

Never combine credentials with wildcard origins.

```js
// ❌
cors({
  origin: "*",
  credentials: true,
});
```

Use an explicit frontend origin:

```js
app.use(
  cors({
    origin: process.env.FRONTEND_URL,
    credentials: true,
  })
);
```

---

# 35. Security Headers

Use Helmet:

```js
import helmet from "helmet";

app.use(helmet());
```

Also consider a strict Content Security Policy (CSP), especially for applications containing rich text or user-generated content.

---

# 36. Redis Security

Redis should never be publicly exposed.

```text
Internet
    │
    ▼
Node.js
    │
    ▼
Private Network
    │
    ▼
Redis
```

Production Redis should use appropriate:

```text
✓ Authentication / ACL
✓ TLS where appropriate
✓ Private networking
✓ Firewall/security groups
✓ High availability
✓ Monitoring
```

---

# 37. Logging Rules

Never log:

```text
❌ Passwords
❌ Access tokens
❌ Refresh tokens
❌ Reset tokens
❌ Verification tokens
❌ Authorization headers
❌ Sensitive cookie values
```

Bad:

```js
console.log(req.cookies.refreshToken);
```

Bad:

```js
console.log(req.query.token);
```

Use sanitized security logs instead:

```text
LOGIN_SUCCESS
LOGIN_FAILED
SESSION_CREATED
SESSION_REVOKED
PASSWORD_RESET
EMAIL_VERIFIED
REFRESH_REUSE_DETECTED
```

---

# 38. Final Storage Architecture

```text
                         APPLICATION
                              │
             ┌────────────────┼────────────────┐
             │                │                │
             ▼                ▼                ▼
          MongoDB           Redis           Browser
             │                │                │
             │                │                ├── Access JWT
             │                │                │     Memory
             │                │                │
             │                ├── Sessions     └── Refresh Token
             │                ├── Reset              HttpOnly Cookie
             │                ├── Verification
             │                └── Rate Limits
             │
             └── Users
                 Password Hash
                 Account State
```

---

# 39. Complete Authentication Lifecycle

## Registration

```text
Register
   ↓
Validate
   ↓
Argon2id password
   ↓
MongoDB User
   ↓
Email Verification Token
   ↓
Redis + Email
```

## Verification

```text
Email Link
   ↓
Token
   ↓
SHA-256
   ↓
Redis
   ↓
MongoDB
   ↓
isEmailVerified = true
```

## Login

```text
Email + Password
   ↓
MongoDB
   ↓
Argon2id Verify
   ↓
Create Session
   ↓
Create Refresh Token
   ↓
Redis
   ↓
HttpOnly Cookie
   ↓
Access JWT
   ↓
React Memory
```

## Normal API

```text
Access JWT
   ↓
Verify JWT
   ↓
Authorize
   ↓
Controller
   ↓
MongoDB
```

## Refresh

```text
Refresh Cookie
   ↓
Session Lookup
   ↓
Hash Secret
   ↓
Verify
   ↓
Atomic Rotation
   ↓
New Refresh Token
   +
New Access Token
```

## Logout

```text
Refresh Cookie
   ↓
Session ID
   ↓
Delete Redis Session
   ↓
Clear Cookie
   ↓
Clear Access Token
```

## Password Reset

```text
Forgot Password
   ↓
Random Token
   ├── Raw → Email
   └── Hash → Redis
            ↓
        15m TTL
            ↓
      Reset Password
            ↓
       Argon2id
            ↓
        MongoDB
            ↓
      Delete Token
            ↓
    Revoke All Sessions
```

---

# 40. Token Comparison

```text
┌──────────────────────┬────────────────────────────┐
│ Access Token         │ Refresh Token              │
├──────────────────────┼────────────────────────────┤
│ JWT                  │ Opaque credential           │
│ Short-lived          │ Long-lived                  │
│ React memory         │ HttpOnly cookie             │
│ Authorization        │ Session continuation        │
│ Locally verified     │ Redis verified              │
│ Not rotated          │ Rotated                     │
│ No server state      │ Server-side session state   │
└──────────────────────┴────────────────────────────┘
```

---

# 41. Token Security Matrix

```text
┌──────────────────────┬──────────┬──────────┬───────────────┐
│ Credential           │ Random   │ Hashed   │ Single-use    │
├──────────────────────┼──────────┼──────────┼───────────────┤
│ Access JWT           │ JWT IDs  │ No       │ No            │
│ Refresh Secret       │ Yes      │ Redis    │ Yes/rotation  │
│ Reset Token          │ Yes      │ Redis    │ Yes           │
│ Verify Token         │ Yes      │ Redis    │ Yes           │
└──────────────────────┴──────────┴──────────┴───────────────┘
```

---

# 42. Production Security Checklist

## Credentials

- [ ] Argon2id password hashing
- [ ] Cryptographically secure random tokens
- [ ] SHA-256 hashing for temporary credentials
- [ ] Constant-time comparisons
- [ ] No plaintext credentials in Redis

## Access Token

- [ ] Short expiration
- [ ] Strong signing secret/key
- [ ] Issuer validation
- [ ] Audience validation
- [ ] Access token stored only in memory

## Refresh Token

- [ ] HttpOnly
- [ ] Secure
- [ ] SameSite configured correctly
- [ ] Restricted cookie path
- [ ] Refresh-token rotation
- [ ] Atomic rotation
- [ ] Reuse detection
- [ ] Redis TTL
- [ ] Session revocation

## Password Reset

- [ ] 256-bit random token
- [ ] Token hash stored in Redis
- [ ] 15-minute TTL
- [ ] Single-use
- [ ] Generic response
- [ ] Rate limiting
- [ ] Revoke all sessions after reset

## Email Verification

- [ ] 256-bit random token
- [ ] Token hash stored in Redis
- [ ] 30-minute TTL
- [ ] Single-use
- [ ] Resend rate limiting

## Application

- [ ] HTTPS
- [ ] Strict CORS
- [ ] Helmet
- [ ] CSP
- [ ] Input validation
- [ ] XSS protection
- [ ] CSRF protection where required
- [ ] No sensitive tokens in logs

## Infrastructure

- [ ] Redis not publicly accessible
- [ ] Redis authentication/ACL
- [ ] Redis TLS where appropriate
- [ ] MongoDB authentication
- [ ] Secrets outside Git
- [ ] Production monitoring
- [ ] Redis high availability

---

# 43. Final Design

```text
                         USER
                          │
                          ▼
                       MongoDB
                    Account Identity
                          │
              ┌───────────┴───────────┐
              │                       │
           LOGIN                 PASSWORD RESET
              │                       │
              ▼                       ▼
         Session S1              Reset Token
              │                       │
              ▼                       ▼
            Redis                   Redis
              │
              ▼
       Refresh Token R1
              │
              ▼
       HttpOnly Cookie
              │
              ▼
       Refresh → R2 → R3
              │
              ▼
        Access JWT
              │
              ▼
        React Memory
              │
              ▼
        Protected APIs
```

### The fundamental separation

```text
MongoDB
    → Who is the user?

Redis Session
    → Which login/device is active?

Refresh Token
    → Can this session be continued?

Access Token
    → Can this request access the API?

Reset Token
    → Can this password be changed?

Verification Token
    → Has this email been proven?
```

This separation keeps the authentication system **stateless where possible, stateful where necessary, and explicit about where every security credential lives**.