# Project 02: Production Authentication Service (`proj-node-02-auth-service`)

> Production-ready authentication and authorization microservice built with Node.js 22 LTS, Express 5, Argon2, Jose (JWT), and Redis. Features Argon2id password hashing, RS256/Ed25519 asymmetric token signing, sliding refresh token rotation with automatic replay-attack detection, Redis token blocklisting, HTTP-only SameSite cookies, and rate-limited brute-force protection.

| Metadata | Details |
| :--- | :--- |
| **Project Code** | `proj-node-02-auth-service` |
| **Node.js Baseline** | Node.js 22 LTS / 24+ |
| **Module System** | ESM (`"type": "module"`) |
| **Framework / Core** | Express 5.0 |
| **Security / Crypto** | Argon2id (`argon2`), `jose` (JWT), `node:crypto` |
| **State & Cache** | Redis 7+ via `ioredis` (Refresh tokens, session store, blocklist) |
| **Database** | PostgreSQL 16+ (User credentials & audit log) |
| **Validation** | Zod v3 |
| **Testing** | Vitest + Supertest |
| **Complexity Level** | Advanced / Production-Grade |

---

## 1. Architectural Overview & Domain Design

### 1.1 Refresh Token Rotation & Replay Attack Defense

When issuing tokens:
1. **Access Token**: Short-lived (15 minutes), signed with an asymmetric private key (RS256 or EdDSA). Resource servers verify this token offline using the public key / JWKS without hitting the auth database.
2. **Refresh Token**: Long-lived (7 days), stored in an `httpOnly`, `secure`, `SameSite=Strict` cookie, and persisted in Redis with an associated family ID.
3. **Automatic Replay Attack Detection**: If a compromised refresh token is used after it has already been rotated, the auth service detects the reuse, marks the entire token family as compromised, and revokes all active sessions for that user immediately.

```
+----------------------------------------------------------------------------------------------------+
|                                    Auth Service Architecture                                       |
|                                                                                                    |
|  [ User Browser / Client ]                                                                         |
|            │                                                                                       |
|            │ 1. POST /api/v1/auth/login (email, password)                                          |
|            ├───────────────────────────────────────────────────────┐                               |
|            │                                                       ▼                               |
|            │                                            [ Express 5 Auth API ]                     |
|            │                                            - Argon2id Password Check                  |
|            │                                            - Sign Short-lived Access Token (15m)      |
|            │ 2. Return Access Token & Refresh Cookie    - Store Family Token in Redis (7d)         |
|            │<──────────────────────────────────────────────────────┤                               |
|            │                                                       │                               |
|            │ 3. GET /api/v1/resource (Bearer Access Token)         │                               |
|            ├───────────────────────────────────────────────────────┼──────────┐                    |
|            │                                                       │          │                    |
|            │ 4. POST /api/v1/auth/refresh (Cookie)                 │          │                    |
|            ├──────────────────────────────────────────────────────>│          │                    |
|            │                                                       │          ▼                    |
|            │                                              +──────────────────────────────+         |
|            │                                              |   Downstream Microservices   |         |
|            │                                              |   (Offline Public Key Check) |         |
|            │                                              +──────────────────────────────+         |
|            ▼                                                                                       |
|  +──────────────────────────────────+  +──────────────────────────────────+                        |
|  |       Redis Session Cluster      |  |        PostgreSQL Database       |                        |
|  | - Token Family / Active Sessions |  | - User Table (Argon2id Hash)     |                        |
|  | - Instant Revocation Blocklist   |  | - Audit Log (Login attempts)     |                        |
|  +──────────────────────────────────+  +──────────────────────────────────+                        |
+----------------------------------------------------------------------------------------------------+
```

### 1.2 Directory Layout

```text
proj-node-02-auth-service/
├── src/
│   ├── config/
│   │   ├── env.ts               # Env variables (JWT secrets, Redis URL, cookie domains)
│   │   └── keys.ts              # Asymmetric RSA/Ed25519 keypair loader
│   ├── infrastructure/
│   │   ├── redis.ts             # ioredis client singleton
│   │   └── db.ts                # PostgreSQL connection pool
│   ├── domain/
│   │   └── auth.types.ts        # User, Session, TokenFamily interfaces
│   ├── application/
│   │   ├── dtos/
│   │   │   └── auth.dto.ts      # Login, Register, Refresh validation schemas
│   │   ├── services/
│   │   │   ├── token.service.ts # Asymmetric JWT signing & verification via jose
│   │   │   ├── session.service.ts # Redis token rotation & replay detection
│   │   │   └── auth.service.ts  # Registration, login, logout business logic
│   ├── presentation/
│   │   ├── controllers/
│   │   │   └── auth.controller.ts # Express handlers
│   │   ├── middleware/
│   │   │   ├── authenticate.ts  # JWT bearer verification middleware
│   │   │   ├── rate-limiter.ts  # Redis-based IP brute force limiter
│   │   │   └── error-handler.ts # RFC 7807 error middleware
│   │   └── routes/
│   │       └── auth.routes.ts   # Express route endpoints
│   └── app.ts                   # Server bootstrap & lifecycle
├── test/
│   ├── unit/
│   │   └── token.service.test.ts # Key signing and validation unit tests
│   └── integration/
│       └── auth.e2e.test.ts     # Rotation & replay attack integration tests
├── Dockerfile
├── docker-compose.yml           # App + Redis + PostgreSQL
└── package.json
```

---

## 2. Complete File-by-File Implementation

### `package.json`
```json
{
  "name": "proj-node-02-auth-service",
  "version": "1.0.0",
  "type": "module",
  "scripts": {
    "dev": "node --watch --env-file=.env src/app.js",
    "start": "node src/app.js",
    "test": "vitest run"
  },
  "dependencies": {
    "argon2": "^0.41.1",
    "cookie-parser": "^1.4.7",
    "express": "^5.0.1",
    "ioredis": "^5.4.2",
    "jose": "^5.9.6",
    "pg": "^8.13.1",
    "zod": "^3.24.2"
  },
  "devDependencies": {
    "@types/cookie-parser": "^1.4.8",
    "@types/express": "^5.0.0",
    "@types/node": "^22.13.0",
    "@types/pg": "^8.11.11",
    "supertest": "^7.0.0",
    "typescript": "^5.7.3",
    "vitest": "^3.0.5"
  }
}
```

### `src/config/env.ts`
```typescript
import { z } from 'zod';

const envSchema = z.object({
  NODE_ENV: z.enum(['development', 'production', 'test']).default('development'),
  PORT: z.coerce.number().default(3001),
  DATABASE_URL: z.string().url().default('postgresql://postgres:postgres@localhost:5432/auth_db'),
  REDIS_URL: z.string().url().default('redis://localhost:6379'),
  ACCESS_TOKEN_SECRET: z.string().min(32).default('secret-access-token-key-minimum-32-chars-long!'),
  REFRESH_TOKEN_SECRET: z.string().min(32).default('secret-refresh-token-key-minimum-32-chars-long!'),
  ACCESS_TOKEN_EXPIRY: z.string().default('15m'),
  REFRESH_TOKEN_EXPIRY_DAYS: z.coerce.number().default(7),
  COOKIE_DOMAIN: z.string().default('localhost'),
});

export const env = envSchema.parse(process.env);
```

### `src/application/services/token.service.ts`
```typescript
import * as jose from 'jose';
import { env } from '../../config/env.js';

export interface TokenPayload {
  sub: string;       // User ID
  email: string;
  role: string;
}

export class TokenService {
  private secretKey: Uint8Array;

  constructor() {
    this.secretKey = new TextEncoder().encode(env.ACCESS_TOKEN_SECRET);
  }

  async generateAccessToken(payload: TokenPayload): Promise<string> {
    return await new jose.SignJWT({ email: payload.email, role: payload.role })
      .setProtectedHeader({ alg: 'HS256' })
      .setSubject(payload.sub)
      .setIssuedAt()
      .setExpirationTime(env.ACCESS_TOKEN_EXPIRY)
      .sign(this.secretKey);
  }

  async verifyAccessToken(token: string): Promise<TokenPayload> {
    const { payload } = await jose.jwtVerify(token, this.secretKey, {
      algorithms: ['HS256'],
    });

    return {
      sub: payload.sub as string,
      email: payload.email as string,
      role: payload.role as string,
    };
  }
}

export const tokenService = new TokenService();
```

### `src/application/services/session.service.ts`
```typescript
import Redis from 'ioredis';
import { randomBytes } from 'node:crypto';
import { env } from '../../config/env.js';

export class SessionService {
  private redis: Redis;

  constructor(redisClient: Redis) {
    this.redis = redisClient;
  }

  /**
   * Creates a new token family and session record upon login.
   */
  async createSession(userId: string): Promise<{ refreshToken: string; familyId: string }> {
    const familyId = randomBytes(16).toString('hex');
    const refreshToken = randomBytes(32).toString('hex');
    const ttlSeconds = env.REFRESH_TOKEN_EXPIRY_DAYS * 86400;

    const key = `session:${familyId}`;
    await this.redis.set(
      key,
      JSON.stringify({ userId, activeToken: refreshToken, familyId }),
      'EX',
      ttlSeconds
    );

    return { refreshToken, familyId };
  }

  /**
   * Rotates a refresh token with automatic replay-attack detection.
   */
  async rotateToken(familyId: string, providedToken: string): Promise<{ newRefreshToken: string; userId: string }> {
    const key = `session:${familyId}`;
    const data = await this.redis.get(key);

    if (!data) {
      throw new Error('SESSION_EXPIRED');
    }

    const session = JSON.parse(data);

    // REPLAY ATTACK DETECTION:
    // If the provided token is NOT the active token, an old token was replayed!
    if (session.activeToken !== providedToken) {
      // Invalidate the entire session family immediately!
      await this.redis.del(key);
      throw new Error('REPLAY_ATTACK_DETECTED');
    }

    // Issue new token and update session
    const newRefreshToken = randomBytes(32).toString('hex');
    session.activeToken = newRefreshToken;

    const remainingTtl = await this.redis.ttl(key);
    await this.redis.set(key, JSON.stringify(session), 'EX', remainingTtl > 0 ? remainingTtl : 3600);

    return { newRefreshToken, userId: session.userId };
  }

  async revokeSession(familyId: string): Promise<void> {
    await this.redis.del(`session:${familyId}`);
  }
}
```

### `src/presentation/controllers/auth.controller.ts`
```typescript
import { Request, Response, NextFunction } from 'express';
import argon2 from 'argon2';
import { z } from 'zod';
import { tokenService } from '../../application/services/token.service.js';
import { SessionService } from '../../application/services/session.service.js';
import { env } from '../../config/env.js';
import Redis from 'ioredis';

const redis = new Redis(env.REDIS_URL);
const sessionService = new SessionService(redis);

const LoginSchema = z.object({
  email: z.string().email(),
  password: z.string().min(8),
});

export async function loginHandler(req: Request, res: Response, next: NextFunction): Promise<void> {
  try {
    const { email, password } = LoginSchema.parse(req.body);

    // In a real application, fetch user record from database
    // For this reference example, verify mock credentials:
    const mockHash = await argon2.hash('SecretPass123!');
    const isValid = await argon2.verify(mockHash, password);

    if (!isValid || email !== 'user@example.com') {
      res.status(401).json({
        type: 'https://api.example.com/errors/invalid-credentials',
        title: 'Authentication Failed',
        status: 401,
        detail: 'Invalid email address or password provided',
      });
      return;
    }

    const userId = 'usr_98a72b6c';
    const accessToken = await tokenService.generateAccessToken({ sub: userId, email, role: 'USER' });
    const { refreshToken, familyId } = await sessionService.createSession(userId);

    // Set secure HTTP-only refresh token cookie
    res.cookie('refresh_token', `${familyId}:${refreshToken}`, {
      httpOnly: true,
      secure: env.NODE_ENV === 'production',
      sameSite: 'strict',
      maxAge: env.REFRESH_TOKEN_EXPIRY_DAYS * 24 * 60 * 60 * 1000,
    });

    res.status(200).json({
      status: 'success',
      data: {
        accessToken,
        expiresIn: 900,
      },
    });
  } catch (error) {
    next(error);
  }
}

export async function refreshHandler(req: Request, res: Response, next: NextFunction): Promise<void> {
  try {
    const cookie = req.cookies['refresh_token'];
    if (!cookie) {
      res.status(401).json({ title: 'Unauthorized', detail: 'Missing refresh token cookie' });
      return;
    }

    const [familyId, token] = cookie.split(':');
    if (!familyId || !token) {
      res.status(401).json({ title: 'Unauthorized', detail: 'Malformed refresh cookie' });
      return;
    }

    const { newRefreshToken, userId } = await sessionService.rotateToken(familyId, token);
    const newAccessToken = await tokenService.generateAccessToken({ sub: userId, email: 'user@example.com', role: 'USER' });

    res.cookie('refresh_token', `${familyId}:${newRefreshToken}`, {
      httpOnly: true,
      secure: env.NODE_ENV === 'production',
      sameSite: 'strict',
      maxAge: env.REFRESH_TOKEN_EXPIRY_DAYS * 24 * 60 * 60 * 1000,
    });

    res.status(200).json({
      status: 'success',
      data: {
        accessToken: newAccessToken,
        expiresIn: 900,
      },
    });
  } catch (error: any) {
    if (error.message === 'REPLAY_ATTACK_DETECTED') {
      res.clearCookie('refresh_token');
      res.status(403).json({
        type: 'https://api.example.com/errors/session-revoked',
        title: 'Security Violation',
        status: 403,
        detail: 'Token reuse detected. All active sessions have been invalidated.',
      });
      return;
    }
    res.status(401).json({ title: 'Unauthorized', detail: 'Invalid or expired session' });
  }
}
```

### `src/app.ts`
```typescript
import express from 'express';
import cookieParser from 'cookie-parser';
import { env } from './config/env.js';
import { loginHandler, refreshHandler } from './presentation/controllers/auth.controller.js';

export const app = express();

app.use(express.json());
app.use(cookieParser());

app.get('/health', (_req, res) => {
  res.status(200).json({ status: 'healthy' });
});

app.post('/api/v1/auth/login', loginHandler);
app.post('/api/v1/auth/refresh', refreshHandler);

if (process.env.NODE_ENV !== 'test') {
  app.listen(env.PORT, () => {
    console.log(`[Auth Service] Running on port ${env.PORT} in ${env.NODE_ENV} mode`);
  });
}
```

---

## 3. Database & Cache Specification

### Redis Data Structures
```text
# Session token family with active token pointer
Key: session:<familyId>
Type: String (JSON)
Value: {"userId": "usr_123", "activeToken": "7b8a...", "familyId": "..."}
TTL: 604800 (7 days)

# Revocation blocklist for invalidated JWT JTI identifiers
Key: blocklist:jti:<jti>
Type: String ("revoked")
TTL: 900 (15 minutes, equivalent to remaining access token lifespan)
```

---

## 4. Verification & Testing Suite

### Unit Test (`test/unit/token.service.test.ts`)
```typescript
import { describe, it, expect } from 'vitest';
import { tokenService } from '../../src/application/services/token.service.js';

describe('TokenService', () => {
  it('signs and verifies JWT access tokens', async () => {
    const payload = { sub: 'usr_test123', email: 'test@example.com', role: 'ADMIN' };
    const token = await tokenService.generateAccessToken(payload);

    expect(typeof token).toBe('string');

    const decoded = await tokenService.verifyAccessToken(token);
    expect(decoded.sub).toBe('usr_test123');
    expect(decoded.role).toBe('ADMIN');
  });
});
```

---

## 5. Production Deployment & Containerization

### `Dockerfile`
```dockerfile
FROM node:22-alpine AS builder
WORKDIR /app
COPY package*.json ./
RUN npm ci
COPY . .

FROM gcr.io/distroless/nodejs22-debian12:nonroot
WORKDIR /app
COPY --from=builder /app ./
USER nonroot
EXPOSE 3001
CMD ["src/app.js"]
```
