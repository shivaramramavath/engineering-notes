# JWT

A JSON Web Token (JWT) is a compact, signed string that carries **claims** (who the user is, when the token expires). Any server holding the right key can verify it **without a database lookup**, which is why JWTs suit APIs, mobile clients, and microservices. The price: a JWT can't be taken back before it expires, so how you use it (short lifetimes, refresh tokens, careful verification) matters more than the format.

Prerequisites: [Authentication architecture](./01-authentication-architecture.md), [Passport](./02-passport-and-local-strategy.md), [guards](../../03-core-concepts/01-request-pipeline/04-guards.md).

## Anatomy

```text
eyJhbGciOiJIUzI1NiIs...  .  eyJzdWIiOiI0MiIsImV4cCI6...  .  SflKxwRJSMeKKF2QT4...
        header                        payload                      signature
```

- **Header:** algorithm and token type (`{"alg":"HS256","typ":"JWT"}`).
- **Payload:** claims, base64url-encoded **JSON that anyone can read**.
- **Signature:** proves the header+payload weren't altered and were produced by someone holding the key.

> **A JWT is signed, not encrypted.** Never put secrets, passwords, or sensitive personal data in the payload. Anyone who gets the token can decode it. (Encrypted tokens, JWE, exist but are rarely what you need.)

### Standard claims

| Claim | Meaning |
|-------|---------|
| `sub` | Subject: the user id |
| `iat` | Issued at (seconds since epoch) |
| `exp` | Expiration (seconds since epoch) |
| `nbf` | Not valid before |
| `iss` | Issuer: who created it |
| `aud` | Audience: who it's for |
| `jti` | Unique token id (useful for revocation/denylists) |

Add only what you need (`roles`, `tenantId`). Remember claims are **frozen until the token expires**: a role removed from the user stays in already-issued tokens.

## `@nestjs/jwt`

```bash
npm i @nestjs/jwt
```

```ts
// auth/auth.module.ts
@Module({
  imports: [
    UsersModule,
    PassportModule,
    JwtModule.registerAsync({
      global: true,                                   // JwtService injectable everywhere
      inject: [ConfigService],
      useFactory: (config: ConfigService) => ({
        secret: config.getOrThrow<string>('JWT_ACCESS_SECRET'),
        signOptions: { expiresIn: '15m', issuer: 'my-api', audience: 'my-app' },
      }),
    }),
  ],
  providers: [AuthService /* ... */],
})
export class AuthModule {}
```

Always configure the secret through async config ([timing trap](../../03-core-concepts/03-configuration/01-configuration-basics.md)) and [validate it at startup](../../03-core-concepts/03-configuration/02-configuration-validation.md) (for HS256 require at least 32 random bytes).

### Issuing

```ts
@Injectable()
export class AuthService {
  constructor(private readonly jwt: JwtService) {}

  async issueAccessToken(user: { id: string; roles: string[] }) {
    const payload = { sub: user.id, roles: user.roles };
    return this.jwt.signAsync(payload);               // iss/aud/exp from signOptions
  }
}
```

### Verifying

```ts
const payload = await this.jwt.verifyAsync(token, {
  algorithms: ['HS256'],                              // pin the allowed algorithm(s)
  issuer: 'my-api',
  audience: 'my-app',
});
```

`verifyAsync` checks the signature **and** `exp`/`nbf` (and `iss`/`aud` when you pass them), throwing on failure. **`decode()` does not verify anything**: it just base64-decodes. Never make an authorization decision from `decode()`.

## Guard: custom vs Passport

### Custom guard (no Passport)

```ts
@Injectable()
export class JwtAuthGuard implements CanActivate {
  constructor(private readonly jwt: JwtService, private readonly reflector: Reflector) {}

  async canActivate(ctx: ExecutionContext) {
    const isPublic = this.reflector.getAllAndOverride<boolean>(IS_PUBLIC_KEY, [ctx.getHandler(), ctx.getClass()]);
    if (isPublic) return true;

    const req = ctx.switchToHttp().getRequest();
    const [type, token] = req.headers.authorization?.split(' ') ?? [];
    if (type !== 'Bearer' || !token) throw new UnauthorizedException();

    try {
      req.user = await this.jwt.verifyAsync(token, { algorithms: ['HS256'] });
    } catch {
      throw new UnauthorizedException();
    }
    return true;
  }
}
```

Registered as `{ provide: APP_GUARD, useClass: JwtAuthGuard }` with `@Public()` for opt-outs ([guards](../../03-core-concepts/01-request-pipeline/04-guards.md)).

### Passport `jwt` strategy

```bash
npm i passport-jwt
npm i -D @types/passport-jwt
```

```ts
@Injectable()
export class JwtStrategy extends PassportStrategy(Strategy) {
  constructor(config: ConfigService) {
    super({
      jwtFromRequest: ExtractJwt.fromAuthHeaderAsBearerToken(),
      ignoreExpiration: false,
      secretOrKey: config.getOrThrow<string>('JWT_ACCESS_SECRET'),
      algorithms: ['HS256'],
      issuer: 'my-api',
      audience: 'my-app',
    });
  }

  validate(payload: { sub: string; roles: string[] }) {
    return { id: payload.sub, roles: payload.roles };       // → req.user
  }
}
```

Both work. The Passport version pays off if you also have other strategies; the custom guard has fewer moving parts. `validate()` is the place to optionally load the user from the database (to reject deleted/disabled accounts at the cost of one query per request).

## Algorithms and keys

| | HS256 (HMAC, shared secret) | RS256 / ES256 (asymmetric) |
|-|----------------------------|---------------------------|
| Keys | One secret to sign **and** verify | Private key signs; public key verifies |
| Who can mint tokens | Anyone who can verify | Only the holder of the private key |
| Fits | One service (or a few trusted ones) | Multiple services/third parties verifying; key rotation via JWKS |
| Ops | Simplest | Key generation, distribution, rotation |

Use HS256 with a long random secret when a single backend issues and verifies. Use asymmetric keys when other parties must verify tokens without being able to create them:

```ts
JwtModule.register({
  privateKey: readFileSync('private.pem'),
  publicKey: readFileSync('public.pem'),
  signOptions: { algorithm: 'RS256', expiresIn: '15m' },
});
```

(Load keys from secrets/config, not the repository. Add a `kid` header and publish a JWKS endpoint if you need rotation.) Keep access-token and refresh-token secrets **different**.

## Lifetimes

| Token | Typical lifetime | Notes |
|-------|------------------|-------|
| Access token | 5-15 minutes | Short because it can't be revoked |
| Refresh token | Days to weeks | Stored server-side, revocable ([next note](./05-refresh-tokens-and-logout.md)) |

A long-lived access token (days, "so users don't re-login") turns a leaked token into a long-lived breach. Fix it with refresh tokens, not long expiries.

## The revocation problem

A valid JWT is accepted until `exp`, even if the user logged out, was banned, or changed their password. Options, from least to most state:

1. **Short access lifetime** + revocable refresh tokens (the standard answer).
2. **Token version** (`tokenVersion` on the user, embedded in the token, compared per request): costs one lookup, revokes all tokens at once.
3. **Denylist** of `jti`s in Redis with TTL equal to the remaining lifetime: revokes individual tokens, costs a lookup per request.
4. **Switch to server-side sessions** if you need instant revocation everywhere ([session authentication](./07-session-authentication.md)).

Each state-adding option erodes the "stateless" benefit; choose deliberately.

## Where the token lives on the client

- **Authorization header** (`Bearer`): typical for mobile and API clients; in browsers usually held in memory, with the refresh token in an `httpOnly` cookie.
- **`httpOnly` cookie**: protects against XSS reading the token but needs CSRF defenses ([cookie authentication](./06-cookie-authentication.md)).
- **`localStorage`**: simple but any XSS steals it ([architecture trade-offs](./01-authentication-architecture.md)).

## Vulnerabilities to avoid

| Mistake | Consequence |
|---------|-------------|
| Accepting any `alg` (including `none`) | Attackers forge tokens. **Pin `algorithms`** |
| Weak/short/guessable secret | Offline brute-force of the signing key |
| Trusting `decode()` | No verification at all |
| Algorithm confusion (RS256 key used as an HS256 secret) | Forged tokens; pin the algorithm |
| No `exp` | Tokens valid forever |
| Skipping `iss`/`aud` checks in multi-service setups | A token for service A accepted by service B |
| Sensitive data in payload | Disclosed to anyone with the token |
| Treating claims as authoritative for permissions that change | Stale roles; re-check critical permissions server-side |
| Logging tokens | Leaks credentials |
| Same secret for access and refresh tokens | Cross-use of tokens |

## Testing

- Unit-test the guard: missing header, wrong scheme, tampered token, expired token (sign with `expiresIn: '-1s'`), wrong audience, valid token, `@Public()` bypass ([testing pipeline components](../01-testing/04-testing-pipeline-components.md)).
- E2E: login → call a protected route with the access token → expect 200; without or with a bad token → 401.

## Common mistakes

- **Long-lived access tokens.**
- **Unpinned algorithms** and missing `iss`/`aud` verification.
- **Using `decode()` for authentication.**
- **Secrets in code or short secrets**; reading them at import time.
- **Putting PII or secrets in the payload.**
- **Assuming logout invalidates a JWT.**
- **Using the JWT for permission data that changes** without a re-check.
- **Returning 403 for invalid tokens** (it's 401).
- **Putting tokens in URLs/query strings** (they leak via logs and referrers).

## Debugging

- `jwt expired`: normal after `exp`; client should refresh. Check server/client clock skew if tokens expire immediately.
- `invalid signature`: different secrets between issuing and verifying services/environments, or the token was altered.
- `jwt must be provided` / `jwt malformed`: header missing, wrong scheme, or extra whitespace/quotes.
- `invalid audience/issuer`: signOptions and verify options disagree.
- `secretOrPrivateKey must have a value`: the secret config is undefined (import-time read or missing env var).
- Inspect (don't trust) a token's contents with a JWT debugger or `jwt.decode`, never pasting real production tokens into third-party sites.

## Quick Summary

- A JWT is a signed (not encrypted) set of claims verified without a database lookup; keep payloads small and non-sensitive.
- Use `@nestjs/jwt` (`signAsync`/`verifyAsync`), pin algorithms, and check `exp`, `iss`, `aud`; never trust `decode()`.
- HS256 for a single trusting backend; RS256/ES256 when others must verify but not mint; separate secrets for access and refresh.
- Short access tokens (minutes) + revocable refresh tokens; add state (token version, denylist) only if you need faster revocation, or use sessions.
- Guard globally with `@Public()` opt-outs; mitigate XSS/CSRF depending on where the token is stored.

## Next

[Refresh tokens and logout →](./05-refresh-tokens-and-logout.md)
