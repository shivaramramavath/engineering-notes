# Session Authentication

With server-side sessions, the browser holds only a **random session id** (in a cookie). The server keeps the actual session data (the user id, roles, flags) in a store and looks it up on every request. Logout and revocation are trivial (delete the session), and nothing sensitive lives on the client. It's the classic, robust model for browser apps that talk to their own backend.

Prerequisites: [Authentication architecture](./01-authentication-architecture.md), [cookie authentication](./06-cookie-authentication.md) (attributes and CSRF apply in full), [Passport local strategy](./02-passport-and-local-strategy.md).

## How it works

```text
POST /auth/login  ──► verify password ──► create session {userId} in store ──► Set-Cookie: sid=<random id>
GET  /me  (Cookie: sid=...)  ──► middleware loads session from store ──► req.session.userId ──► handler
POST /auth/logout ──► destroy session in store ──► clear cookie
```

The cookie carries **no user data**, just an unguessable id. Compromising it requires stealing the cookie itself (XSS can't read it if `HttpOnly`; network capture is prevented by TLS).

## Setup with `express-session`

```bash
npm i express-session
npm i -D @types/express-session
```

```ts
// main.ts
import session from 'express-session';

app.set('trust proxy', 1);                       // required for Secure cookies behind a TLS-terminating proxy

app.use(
  session({
    name: 'sid',                                 // cookie name
    secret: config.getOrThrow<string>('SESSION_SECRET'),   // or an array to rotate secrets
    resave: false,                               // don't re-save unchanged sessions
    saveUninitialized: false,                    // don't create sessions for anonymous visitors
    rolling: false,                              // true = refresh the cookie expiry on every response
    cookie: {
      httpOnly: true,
      secure: process.env.NODE_ENV === 'production',
      sameSite: 'lax',
      maxAge: 1000 * 60 * 60 * 8,                // 8 hours
    },
    store: sessionStore,                         // see below: NOT the default in production
  }),
);
```

Key options:

| Option | Why |
|--------|-----|
| `secret` | Signs the session-id cookie. Keep it long and secret; supply an array (newest first) to rotate without logging everyone out |
| `resave: false` | Avoids needless writes and race conditions on concurrent requests |
| `saveUninitialized: false` | Only create a session once you store something (login), avoiding a session per bot/visitor and helping privacy/consent rules |
| `cookie.*` | Same attribute guidance as [cookie auth](./06-cookie-authentication.md) |
| `store` | Where sessions live (below) |

Register this **before** routes (and before Passport's `session()` if you use it). For Fastify use `@fastify/session` with `@fastify/cookie`; the concepts are the same, the API differs.

### The store must not be the default

The default `MemoryStore` leaks memory, loses all sessions on restart, and **doesn't work across multiple instances**. It's for development only. Use a shared store such as Redis:

```bash
npm i connect-redis ioredis
```

```ts
import { RedisStore } from 'connect-redis';        // named export in recent connect-redis versions; check yours
import Redis from 'ioredis';

const redis = new Redis(config.getOrThrow('REDIS_URL'));
const sessionStore = new RedisStore({ client: redis, prefix: 'sess:', ttl: 60 * 60 * 8 });
```

Redis gives fast lookups and automatic expiry via TTL. Other stores exist (SQL, Mongo); choose by what you already operate. Treat the store as critical infrastructure: if it's down, nobody can authenticate. See [Redis](../../05-advanced/01-caching/03-redis.md). (`connect-redis` and `ioredis`/`redis` client APIs have changed across versions; follow the current README.)

## Using sessions in handlers

Type the session data once:

```ts
// types/session.d.ts
import 'express-session';
declare module 'express-session' {
  interface SessionData {
    userId?: string;
    createdAt?: number;
  }
}
```

```ts
@Public()
@Post('login')
@HttpCode(200)
async login(@Body() dto: LoginDto, @Req() req: Request) {
  const user = await this.auth.validateUser(dto.email, dto.password);
  if (!user) throw new UnauthorizedException('Invalid credentials');

  // Prevent session fixation: issue a NEW session id on privilege change
  await new Promise<void>((resolve, reject) =>
    req.session.regenerate((err) => (err ? reject(err) : resolve())),
  );
  req.session.userId = user.id;
  req.session.createdAt = Date.now();
  return { id: user.id, email: user.email };
}

@Post('logout')
@HttpCode(204)
async logout(@Req() req: Request, @Res({ passthrough: true }) res: Response) {
  await new Promise<void>((resolve, reject) => req.session.destroy((err) => (err ? reject(err) : resolve())));
  res.clearCookie('sid');            // use the same cookie name/options as configured
}
```

A guard turns the session into `req.user`:

```ts
@Injectable()
export class SessionAuthGuard implements CanActivate {
  constructor(private readonly users: UsersService, private readonly reflector: Reflector) {}

  async canActivate(ctx: ExecutionContext) {
    if (this.reflector.getAllAndOverride<boolean>(IS_PUBLIC_KEY, [ctx.getHandler(), ctx.getClass()])) return true;

    const req = ctx.switchToHttp().getRequest<Request>();
    const userId = req.session?.userId;
    if (!userId) throw new UnauthorizedException();

    const user = await this.users.findActiveById(userId);       // fresh data; rejects disabled accounts
    if (!user) throw new UnauthorizedException();
    (req as any).user = user;
    return true;
  }
}
```

Register globally with `APP_GUARD` and `@Public()` for opt-outs, as in the [guards note](../../03-core-concepts/01-request-pipeline/04-guards.md). Store **minimal data** in the session (a user id, maybe a few flags) and load the rest per request so changes (roles, disabled accounts) take effect immediately.

## Session fixation

If the session id **doesn't change** when a user logs in, an attacker who planted a known session id in the victim's browser (before login) can reuse it after the victim authenticates. Defense: **regenerate the session id at login** (and on any privilege change such as password change or 2FA completion), as shown. Never accept a session id from a URL or request body.

## Timeouts

| Timeout | Meaning | How |
|---------|---------|-----|
| **Idle (sliding)** | Expire after inactivity | `rolling: true` + a modest `maxAge` (the cookie and store TTL refresh on activity) |
| **Absolute** | Expire N hours after login regardless of activity | Store `createdAt` in the session; reject if too old in the guard |
| **Remember me** | Longer lifetime on request | Set a longer `maxAge` at login only when asked; consider a separate persistent token |

Use both idle and absolute timeouts for sensitive apps. `rolling: true` extends sessions forever for an active user, which is why you pair it with an absolute limit.

## Passport sessions (optional)

If you use Passport's session support, you provide serialization to and from the session:

```ts
@Injectable()
export class SessionSerializer extends PassportSerializer {
  constructor(private readonly users: UsersService) { super(); }

  serializeUser(user: { id: string }, done: (err: Error | null, id?: string) => void) {
    done(null, user.id);                                   // what goes into the session
  }
  async deserializeUser(id: string, done: (err: Error | null, user?: unknown) => void) {
    done(null, await this.users.findActiveById(id));       // → req.user on each request
  }
}
```

plus `PassportModule.register({ session: true })`, and `app.use(passport.initialize()); app.use(passport.session());` after `express-session`. The local-strategy guard then needs `await super.logIn(request)` (override `canActivate` in `LocalAuthGuard`). Newer Passport versions regenerate the session on `req.logIn` and have changed `logout` to be callback-based, so follow the version-specific docs. Plain `req.session.userId` as shown above is often simpler and more transparent than Passport sessions when you only have one login method.

## Sessions and CSRF

Because the cookie is sent automatically, session auth **requires CSRF defenses** for state-changing requests ([cookie authentication](./06-cookie-authentication.md), [CSRF](../../07-production/01-security/03-csrf.md)): `SameSite=Lax/Strict`, CSRF tokens or a required custom header, no state changes on GET, strict CORS if the frontend is on another origin.

## Sessions vs JWT

| | Sessions | JWT access tokens |
|-|----------|-------------------|
| Revocation | **Instant** | Hard |
| Per-request cost | Store lookup | Signature check |
| Data freshness | Always current | Stale until expiry |
| Scaling | Needs shared store (Redis) | Stateless |
| Mobile/API clients | Awkward (cookies) | Natural |
| Multi-service verification | Needs a shared store or gateway | Easy with public-key verification |
| Browser security model | Cookie + CSRF | Header (XSS risk) or cookie (+ CSRF) |

For a browser app served alongside its API, sessions are usually the simplest secure choice. Reach for tokens when you serve mobile/third-party clients or many services.

## Operational concerns

- **Capacity:** one store entry per active session; set TTLs so abandoned sessions disappear.
- **Availability:** make the store highly available; decide what happens when it's unreachable (fail closed: treat as unauthenticated).
- **Session lists:** store user agent/IP at creation and maintain a per-user index of session ids to support "sign out of other devices" (a Redis set per user, cleaned on destroy).
- **Revoke on password change/reset:** delete all of a user's sessions except possibly the current (regenerated) one.
- **Don't store large objects** in sessions; each request loads and re-saves them.
- **Concurrency:** parallel requests can race on session writes; keep sessions mostly read-only after login.
- **Secret rotation:** supply `secret` as an array.

## Testing

```ts
const agent = request.agent(app.getHttpServer());
await agent.post('/auth/login').send({ email, password }).expect(200);
await agent.get('/me').expect(200);
await agent.post('/auth/logout').expect(204);
await agent.get('/me').expect(401);
```

Also test: session id **changes** across login (fixation), idle/absolute expiry (fake timers or short TTL), a user disabled after login is rejected, and logout invalidates the stored session ([E2E testing](../01-testing/06-e2e-testing.md)). Use an in-memory or test Redis store.

## Common mistakes

- **`MemoryStore` in production** (leaks, no scaling, lost on restart).
- **Not regenerating the session id on login** (fixation).
- **`saveUninitialized: true`** creating sessions for every visitor.
- **Missing `trust proxy`**, so `Secure` cookies aren't set behind a load balancer.
- **No CSRF protection.**
- **Weak or committed `secret`.**
- **Storing whole user objects** (stale data, large sessions) instead of an id.
- **Only an idle timeout** with `rolling`, so active sessions never expire.
- **Not destroying sessions on password change.**
- **Registering the session middleware after routes/Passport.**
- **Clearing the cookie but not destroying the server-side session.**

## Debugging

- Session lost on every request: cookie not being set/sent (`Secure` on HTTP, missing `credentials: 'include'`, `SameSite`), or `trust proxy` not set behind a proxy.
- Works locally, loses sessions in production with several instances: store isn't shared.
- Users logged out after deploys/restarts: `MemoryStore`, or a changed `secret` without rotation.
- `req.session` undefined: middleware not registered (or registered after the route).
- Session not saved after changes in async code: call `req.session.save()` when you respond manually or redirect immediately.
- Redis errors surfacing as random 500s: handle store connection errors and monitor the store.

## Quick Summary

- Sessions keep identity server-side; the cookie holds only an unguessable id, so logout/revocation are instant.
- Use `express-session` with a **shared store (Redis)**, `resave: false`, `saveUninitialized: false`, secure cookie flags, `trust proxy` behind proxies.
- **Regenerate the session id at login**; destroy server-side on logout and password change; set idle and absolute timeouts.
- A global session guard turns `req.session.userId` into `req.user`; store minimal data and reload fresh user data.
- Cookie-based, so **CSRF defenses are required**. Sessions suit browser apps; tokens suit mobile/multi-service APIs.

## Next

[OAuth 2.0 →](./08-oauth2.md)
