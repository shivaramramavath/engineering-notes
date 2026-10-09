# Passport and the Local Strategy

Passport is the most widely used authentication library in the Node ecosystem. It doesn't do authentication itself; it provides a uniform shape for **strategies**: small modules that know how to verify one kind of credential (username/password, JWT, Google OAuth, GitHub OAuth...). `@nestjs/passport` wraps it so strategies become injectable classes and guards.

This note builds the classic **email + password login** with the local strategy. Hashing is in [the next note](./03-password-hashing.md); tokens come in [JWT](./04-jwt.md).

Prerequisites: [Authentication architecture](./01-authentication-architecture.md), [guards](../../03-core-concepts/01-request-pipeline/04-guards.md).

## Do you need Passport?

| Use Passport when | Skip it when |
|-------------------|--------------|
| You'll support several login methods (local, Google, GitHub, SAML...) | You only verify a Bearer JWT: a ~20-line custom guard ([guards](../../03-core-concepts/01-request-pipeline/04-guards.md)) is simpler |
| You want a consistent, well-known structure | You want fewer dependencies and full control |
| An ecosystem strategy already exists for your provider | The strategy is thin and you'd rather own the logic |

Passport adds a layer of indirection (and some Express-era assumptions). It's a good fit for multi-provider login; for JWT-only APIs it's optional.

## Install

```bash
npm i @nestjs/passport passport passport-local
npm i -D @types/passport-local
```

Each strategy is its own package (`passport-local`, `passport-jwt`, `passport-google-oauth20`, ...), each with its own types package where needed.

## How it fits together

```text
POST /auth/login { email, password }
        │
        ▼
 AuthGuard('local')  ──►  LocalStrategy.validate(email, password)
        │                          │  returns user or throws
        │                          ▼
        │                    AuthService.validateUser (check hash)
        ▼
  req.user = the user
        │
        ▼
 AuthController.login(@Request() req)  ──►  issue token/session
```

A strategy's `validate()` return value becomes **`req.user`**; throwing `UnauthorizedException` (or returning falsy) makes the guard respond `401`.

## The local strategy

```ts
// auth/strategies/local.strategy.ts
import { Injectable, UnauthorizedException } from '@nestjs/common';
import { PassportStrategy } from '@nestjs/passport';
import { Strategy } from 'passport-local';
import { AuthService } from '../auth.service';

@Injectable()
export class LocalStrategy extends PassportStrategy(Strategy) {
  constructor(private readonly auth: AuthService) {
    super({ usernameField: 'email' });          // default field names are `username` / `password`
  }

  async validate(email: string, password: string) {
    const user = await this.auth.validateUser(email, password);
    if (!user) throw new UnauthorizedException('Invalid credentials');
    return user;                                 // → req.user
  }
}
```

```ts
// auth/guards/local-auth.guard.ts
import { Injectable } from '@nestjs/common';
import { AuthGuard } from '@nestjs/passport';

@Injectable()
export class LocalAuthGuard extends AuthGuard('local') {}
```

```ts
// auth/auth.service.ts
@Injectable()
export class AuthService {
  constructor(
    private readonly users: UsersService,
    private readonly hasher: PasswordHasher,        // see password hashing
  ) {}

  async validateUser(email: string, password: string) {
    const user = await this.users.findCredentialsByEmail(email.toLowerCase());

    // Always run a hash comparison, even when the user doesn't exist (see below)
    const hash = user?.passwordHash ?? DUMMY_HASH;
    const ok = await this.hasher.verify(hash, password);

    if (!user || !ok || !user.active) return null;
    const { passwordHash, ...safe } = user;         // never let the hash travel further
    return safe;
  }
}
```

```ts
// auth/auth.controller.ts
@Controller('auth')
export class AuthController {
  constructor(private readonly auth: AuthService) {}

  @Public()                                         // opt out of the global JWT guard
  @UseGuards(LocalAuthGuard)
  @Post('login')
  @HttpCode(200)
  login(@Request() req) {
    return this.auth.login(req.user);               // issue tokens (next notes)
  }
}
```

```ts
// auth/auth.module.ts
@Module({
  imports: [UsersModule, PassportModule],
  providers: [AuthService, LocalStrategy, PasswordHasher],
  controllers: [AuthController],
})
export class AuthModule {}
```

The strategy must be a **provider** in the module, or `AuthGuard('local')` fails with "Unknown authentication strategy 'local'".

## Details that matter

### Body validation and `LocalAuthGuard`

`AuthGuard('local')` runs **before** pipes (guards precede pipes in the [pipeline](../../03-core-concepts/01-request-pipeline/01-pipeline-overview-and-execution-order.md)), so your login DTO's `ValidationPipe` rules haven't run when `validate()` is called. If the body is missing `email` or `password`, Passport responds `400 Missing credentials`. It also means `validate()` can receive non-string values if a client sends JSON objects. Coerce or check types before using them, especially if values later reach a [NoSQL filter](../05-mongoose/03-repositories.md).

### Generic errors and equal timing (account enumeration)

Attackers probe a login endpoint to learn which emails are registered. Two leaks to close:

1. **Messages:** return the same `401 Invalid credentials` for "no such user", "wrong password", and (usually) "disabled account".
2. **Timing:** skipping the expensive hash check for unknown users makes them respond measurably faster. Verify against a fixed **dummy hash** (generated once with your real algorithm and parameters) when the user doesn't exist, as in the service above.

Registration and "forgot password" endpoints leak the same information; respond identically ("if that email exists, we've sent a message").

### Return only safe data

`validate()` populates `req.user`. Strip `passwordHash` and other secrets, and keep `req.user` small. Downstream code (and logs) will see it.

### Sessions off by default

`@nestjs/passport` runs strategies **without** Passport sessions unless you enable them (`PassportModule.register({ session: true })` plus serializers). For token-based APIs, keep sessions off. For session login, see [session authentication](./07-session-authentication.md).

## Login rate limiting

Login is the prime brute-force target. Add per-IP **and** per-account throttling (for example the Throttler module) and consider progressive delays or temporary lockouts, balanced against attackers locking real users out ([rate limiting](../../07-production/01-security/04-rate-limiting-and-brute-force-protection.md)). Log failures (without logging the submitted password) and alert on spikes.

## Customizing guard responses

Override `handleRequest` to control errors or add logic:

```ts
@Injectable()
export class LocalAuthGuard extends AuthGuard('local') {
  handleRequest<TUser>(err: unknown, user: TUser | false) {
    if (err || !user) throw new UnauthorizedException('Invalid credentials');
    return user;
  }
}
```

## Other strategies follow the same pattern

```ts
export class JwtStrategy extends PassportStrategy(Strategy /* passport-jwt */) { /* validate(payload) */ }
export class GoogleStrategy extends PassportStrategy(Strategy /* passport-google-oauth20 */) { /* ... */ }
```

Each extends `PassportStrategy(Strategy, 'optional-name')`, returns a user from `validate()`, and is used via `AuthGuard('name')`. Next: [JWT](./04-jwt.md) and [social login](./09-social-login.md).

## Testing

- Unit-test `AuthService.validateUser` with a fake `UsersService` and hasher: valid credentials, wrong password, unknown user (still calls the hasher), disabled user.
- E2E: `POST /auth/login` with good/bad credentials, missing fields (400), and verify the response never includes the hash ([E2E testing](../01-testing/06-e2e-testing.md)).

## Common mistakes

- **Strategy not registered as a provider**, giving "Unknown authentication strategy".
- **Different messages or timing** for unknown user vs wrong password.
- **Leaking `passwordHash`** into `req.user`, responses, or logs.
- **Assuming `ValidationPipe` ran** before the local strategy.
- **No rate limiting on login.**
- **Forgetting `@Public()`** when a global auth guard is active, so login itself requires authentication.
- **Comparing passwords with `===`** or hashing with a fast hash instead of [Argon2/bcrypt](./03-password-hashing.md).
- **Using Passport sessions accidentally** in a stateless API (or forgetting to enable them for a session API).
- **Returning 200 with an error body** instead of 401.

## Debugging

- `Unknown authentication strategy "local"`: `LocalStrategy` isn't in `providers`, or isn't imported/instantiated in the module that uses the guard.
- `401` for valid credentials: check `usernameField` (default `username`), that the body is JSON with the expected keys, and your hash verification (algorithm/params).
- `400 Missing credentials`: the request body lacks the configured fields (or `Content-Type` isn't JSON).
- Global guard blocks login: add `@Public()` to the route.
- `req.user` undefined in the handler: the guard isn't applied to that route.

## Quick Summary

- Passport strategies verify credentials; `validate()` returns the user (becomes `req.user`) or throws `401`.
- `@nestjs/passport` turns strategies into providers and `AuthGuard('name')` into guards; use it for multiple login methods, skip it for JWT-only if you prefer a custom guard.
- Local strategy: configure `usernameField`, call `AuthService.validateUser`, strip secrets, return generic errors, and compare against a dummy hash for unknown users.
- Guards run before pipes: validate types yourself. Add `@Public()` to login and rate-limit it.

## Next

[Password hashing →](./03-password-hashing.md)
