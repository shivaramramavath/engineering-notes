# Cookies and Sessions

- **NestJS Version**: 11.x
- **Node.js Baseline**: >= 20.0.0
- **Module**: 02-fundamentals
- **Prerequisites**: [07-request-data](./07-request-data.md), [08-response-handling](./08-response-handling.md), [00-prerequisites/04-http-and-rest-basics](../00-prerequisites/04-http-and-rest-basics.md)

---

## 1. Context & Purpose

Stateful authentication, session persistence, shopping cart management, and CSRF protection rely heavily on **HTTP Cookies** and **Server-Side Sessions**. While stateless JWT tokens stored in browser `localStorage` are vulnerable to Cross-Site Scripting (XSS) attacks, storing authentication credentials in `HttpOnly`, `Secure`, `SameSite=Strict` cookies provides robust security against client-side script theft.

NestJS integrates smoothly with both `cookie-parser` and session managers (`express-session` with Redis/Connect-Redis stores). This guide explains how to read and set signed cookies, configure distributed server-side sessions, build custom cookie decorators, and avoid critical security vulnerabilities.

---

## 2. Theoretical Foundations

### 2.1 Cookies
A cookie is a small key-value pair stored by the user's browser:
- **`HttpOnly`**: Inaccessible to JavaScript via `document.cookie` (mitigates XSS).
- **`Secure`**: Transmitted only over encrypted TLS (HTTPS) connections.
- **`SameSite`**: Controls whether cookies are sent on cross-site requests (`Strict`, `Lax`, `None`), mitigating Cross-Site Request Forgery (CSRF).
- **Signed Cookies**: HMAC-signed cookies appended with a cryptographic signature (e.g. `s:value.signature`) to detect client-side tampering.

### 2.2 Server-Side Sessions
Sessions store user state on the server (e.g., Redis or database), issuing only a cryptographically signed session ID (`connect.sid`) to the client's cookie. On subsequent requests, the server loads the session data from the store based on this ID.

---

## 3. Architecture & Internal Mechanics

```
Client Browser
      │
      ├─> GET /auth/login (Credentials)
      │
      ▼
NestJS Application
      │
      ├─> Validates user credentials
      ├─> Creates Session record in Redis { userId: "123", role: "admin" }
      ├─> Encrypts/Signs session ID -> sid_abc123
      │
      ▼
HTTP Response Header
Set-Cookie: connect.sid=s%3Asid_abc123; HttpOnly; Secure; SameSite=Lax; Path=/
      │
      ▼
Client Browser stores cookie
      │
      ├─> GET /users/me (Cookie: connect.sid=s%3Asid_abc123)
      │
      ▼
Session Middleware
      │
      ├─> Extracts & verifies signature of connect.sid
      ├─> Fetches { userId: "123" } from Redis
      ├─> Attaches session to req.session
      │
      ▼
Controller Handler: req.session.userId === "123"
```

---

## 4. Concrete Implementation

### 4.1 Cookie Parsing & Signed Cookies

Install `cookie-parser`:
```bash
npm install --save cookie-parser
npm install --save-dev @types/cookie-parser
```

Configure in `main.ts`:
```typescript
// src/main.ts
import { NestFactory } from '@nestjs/core';
import cookieParser from 'cookie-parser';
import { AppModule } from './app.module';

async function bootstrap() {
  const app = await NestFactory.create(AppModule);

  // Provide secret for signed cookies
  app.use(cookieParser(process.env.COOKIE_SECRET || 'super-secret-key'));

  await app.listen(3000);
}
bootstrap();
```

---

### 4.2 Setting & Reading Cookies in Controllers

```typescript
// src/auth/auth.controller.ts
import { Controller, Post, Get, Req, Res, UnauthorizedException } from '@nestjs/common';
import { Request, Response } from 'express';

@Controller('auth')
export class AuthController {
  @Post('login')
  login(@Res({ passthrough: true }) res: Response) {
    const sessionToken = 'jwt-or-session-token-12345';

    // Set secure, signed cookie
    res.cookie('auth_token', sessionToken, {
      httpOnly: true,
      secure: process.env.NODE_ENV === 'production',
      sameSite: 'lax',
      signed: true, // Signs cookie using COOKIE_SECRET
      maxAge: 7 * 24 * 60 * 60 * 1000, // 7 days in ms
      path: '/',
    });

    return { message: 'Logged in successfully' };
  }

  @Get('profile')
  getProfile(@Req() req: Request) {
    // Read signed cookie
    const token = req.signedCookies['auth_token'];
    if (!token) {
      throw new UnauthorizedException('Authentication cookie missing or tampered');
    }

    return { user: 'Alice', token };
  }

  @Post('logout')
  logout(@Res({ passthrough: true }) res: Response) {
    res.clearCookie('auth_token', { path: '/' });
    return { message: 'Logged out successfully' };
  }
}
```

---

### 4.3 Custom `@Cookies()` Param Decorator

Create a clean param decorator to extract cookies directly:

```typescript
// src/common/decorators/cookies.decorator.ts
import { createParamDecorator, ExecutionContext } from '@nestjs/common';
import { Request } from 'express';

export const Cookies = createParamDecorator(
  (data: { name: string; signed?: boolean }, ctx: ExecutionContext) => {
    const req = ctx.switchToHttp().getRequest<Request>();
    if (!data) return req.cookies;

    return data.signed
      ? req.signedCookies?.[data.name]
      : req.cookies?.[data.name];
  },
);
```

Usage in controller:
```typescript
@Get('cart')
getCart(@Cookies({ name: 'cart_id' }) cartId: string) {
  return { cartId: cartId || 'guest_cart' };
}
```

---

### 4.4 Distributed Sessions with Redis (`express-session` & `connect-redis`)

Install dependencies:
```bash
npm install --save express-session connect-redis redis
npm install --save-dev @types/express-session
```

Configure distributed session store in `main.ts`:

```typescript
// src/main.ts
import { NestFactory } from '@nestjs/core';
import session from 'express-session';
import { RedisStore } from 'connect-redis';
import { createClient } from 'redis';
import { AppModule } from './app.module';

async function bootstrap() {
  const app = await NestFactory.create(AppModule);

  // Initialize Redis client for session storage
  const redisClient = createClient({ url: process.env.REDIS_URL || 'redis://localhost:6379' });
  await redisClient.connect();

  app.use(
    session({
      store: new RedisStore({ client: redisClient, prefix: 'sess:' }),
      secret: process.env.SESSION_SECRET || 'keyboard-cat-secret',
      resave: false,
      saveUninitialized: false,
      cookie: {
        httpOnly: true,
        secure: process.env.NODE_ENV === 'production',
        sameSite: 'lax',
        maxAge: 24 * 60 * 60 * 1000, // 24 hours
      },
    }),
  );

  await app.listen(3000);
}
bootstrap();
```

---

## 5. Edge Cases & Failure Modes

### 5.1 Cookie Size Limits
Browsers enforce a strict 4096-byte limit per cookie. Storing large JWTs or state arrays directly in cookies leads to silent cookie dropping by clients.
- **Remedy**: Store only small opaque identifiers in cookies and keep the payload in Redis or the database.

### 5.2 Secure Cookies on Localhost (HTTP)
Setting `secure: true` when testing on `http://localhost` causes browsers to drop the cookie completely.
- **Handling**: Use conditional security: `secure: process.env.NODE_ENV === 'production'`.

### 5.3 Fastify Cookie and Session Plugins
Fastify requires `@fastify/cookie` and `@fastify/session` rather than Express middleware. Attempting to use `cookie-parser` or `express-session` inside Fastify throws runtime compatibility errors.

---

## 6. Performance & Memory Implications

- **Memory Sessions in Production**: Using default `MemoryStore` in `express-session` leaks memory, does not share sessions across multiple cluster instances or containers, and drops all sessions upon server restart.
- **Redis Sessions**: Fetching session state on every incoming request introduces a network hop (~0.5ms–1ms). Keep session payloads lean.

---

## 7. Anti-Patterns & Common Mistakes

- **Defaulting to `MemoryStore` in Production**: Deploying to Kubernetes with in-memory sessions; requests hit different pods, logging users out unpredictably.
- **Missing `resave: false` and `saveUninitialized: false`**: Causes race conditions and writes empty sessions for every anonymous crawler, bloating the Redis database.
- **Omitting `HttpOnly`**: Leaving cookies accessible to client-side scripts, exposing authentication tokens to XSS attacks.

---

## 8. Production Checklist & Operational Readiness

- [ ] All sensitive authentication cookies configured with `HttpOnly: true` and `SameSite: 'lax'` or `'strict'`.
- [ ] `Secure: true` enabled in production.
- [ ] Redis session store configured with appropriate TTL expiration.
- [ ] Cookie secrets stored securely in environment variables (minimum 32 random bytes).
- [ ] `res.clearCookie` specifies the exact matching path and domain used when creating the cookie.

---

## 9. Migration & Version History

- **NestJS 7/8/9**: Standard integration via Express middleware.
- **NestJS 11.x**: Validated with Express 5 session handling and modern Redis 4.x / connect-redis 7.x clients.

---

## 10. Verification & Testing

Testing cookies with Supertest:

```typescript
import { Test, TestingModule } from '@nestjs/testing';
import { INestApplication } from '@nestjs/common';
import * as request from 'supertest';
import cookieParser from 'cookie-parser';
import { AuthController } from './auth.controller';

describe('Cookies Integration (e2e)', () => {
  let app: INestApplication;

  beforeAll(async () => {
    const moduleFixture: TestingModule = await Test.createTestingModule({
      controllers: [AuthController],
    }).compile();

    app = moduleFixture.createNestApplication();
    app.use(cookieParser('test-secret'));
    await app.init();
  });

  afterAll(async () => {
    await app.close();
  });

  it('sets signed auth cookie on login', async () => {
    const response = await request(app.getHttpServer())
      .post('/auth/login')
      .expect(200);

    const cookies = response.headers['set-cookie'];
    expect(cookies).toBeDefined();
    expect(cookies[0]).toContain('auth_token');
    expect(cookies[0]).toContain('HttpOnly');
  });
});
```

---

## 11. Comprehensive Examples

### Role-Based Session Guard

```typescript
// src/common/guards/session-auth.guard.ts
import {
  Injectable,
  CanActivate,
  ExecutionContext,
  UnauthorizedException,
} from '@nestjs/common';
import { Request } from 'express';

declare module 'express-session' {
  interface SessionData {
    userId: string;
    role: string;
  }
}

@Injectable()
export class SessionAuthGuard implements CanActivate {
  canActivate(context: ExecutionContext): boolean {
    const req = context.switchToHttp().getRequest<Request>();

    if (!req.session || !req.session.userId) {
      throw new UnauthorizedException('Active session required');
    }

    return true;
  }
}
```

---

## 12. Cross-References & Further Reading

- [07-request-data.md](./07-request-data.md) - Request decorators
- [08-response-handling.md](./08-response-handling.md) - Response headers and cookies
- [04-intermediate/06-authentication/01-auth-overview.md](../04-intermediate/06-authentication/01-auth-overview.md) - Authentication strategies
- [express-session Documentation](https://github.com/expressjs/session)
