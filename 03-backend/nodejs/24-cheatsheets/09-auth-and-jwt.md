# Cheatsheet 09: Authentication, Cookies & JWT Reference Card

> Production syntax reference for password hashing via Argon2id, asymmetric JWT signing with `jose`, and secure HTTP cookie configurations.

---

### 1. Argon2id Password Hashing
Argon2id is the gold-standard memory-hard password hashing algorithm (OWASP recommended):
```javascript
import argon2 from 'argon2';

// Hash Password
const hash = await argon2.hash('P@ssword123!', {
  type: argon2.argon2id,
  memoryCost: 65536,  // 64MB memory consumption
  timeCost: 3,        // 3 iterations
  parallelism: 1,     // 1 thread
});

// Verify Password
const isValid = await argon2.verify(hash, 'P@ssword123!'); // true or false

// Check if rehash needed when parameters are upgraded
if (argon2.needsRehash(hash, { memoryCost: 131072 })) {
  // Rehash with higher memory cost and update DB
}
```

---

### 2. JWT Signing & Verification via `jose`
Modern Node.js 22 LTS standard without legacy `jsonwebtoken` vulnerabilities:

```javascript
import * as jose from 'jose';

// Asymmetric Private/Public Keys
const privateKey = await jose.importPKCS8(process.env.RSA_PRIVATE_KEY, 'RS256');
const publicKey = await jose.importSPKI(process.env.RSA_PUBLIC_KEY, 'RS256');

// Sign JWT Access Token
const token = await new jose.SignJWT({ role: 'ADMIN', tenantId: 'tenant_1' })
  .setProtectedHeader({ alg: 'RS256' })
  .setSubject('usr_12345')
  .setIssuedAt()
  .setExpirationTime('15m') // 15-minute expiration
  .setAudience('https://api.example.com')
  .setIssuer('https://auth.example.com')
  .sign(privateKey);

// Verify JWT Access Token
const { payload } = await jose.jwtVerify(token, publicKey, {
  issuer: 'https://auth.example.com',
  audience: 'https://api.example.com',
});

console.log(payload.sub); // 'usr_12345'
```

---

### 3. Secure HTTP-Only Cookie Configuration
```javascript
res.cookie('refresh_token', refreshToken, {
  httpOnly: true,                                       // Inaccessible to client JS (prevents XSS theft)
  secure: process.env.NODE_ENV === 'production',       // Enforce HTTPS
  sameSite: 'strict',                                  // Mitigates CSRF attacks
  maxAge: 7 * 24 * 60 * 60 * 1000,                     // 7 days in milliseconds
  path: '/api/v1/auth/refresh',                        // Restrict cookie sending strictly to refresh endpoint
  domain: process.env.COOKIE_DOMAIN,
});

// Clear Cookie on Logout:
res.clearCookie('refresh_token', { path: '/api/v1/auth/refresh' });
```

---

### 4. Express Authentication Guard Middleware
```javascript
export async function authenticateToken(req, res, next) {
  const authHeader = req.headers['authorization'];
  const token = authHeader && authHeader.split(' ')[1]; // Extract Bearer token

  if (!token) {
    return res.status(401).json({ error: 'Missing authentication token' });
  }

  try {
    const { payload } = await jose.jwtVerify(token, publicKey);
    req.user = payload; // Attach claims to request
    next();
  } catch (err) {
    return res.status(403).json({ error: 'Invalid or expired token' });
  }
}
```
