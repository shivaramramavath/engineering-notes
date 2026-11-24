# Authentication & Authorization Cheat Sheet

### Passport JWT Strategy
```typescript
@Injectable()
export class JwtStrategy extends PassportStrategy(Strategy) {
  constructor(config: ConfigService) {
    super({
      jwtFromRequest: ExtractJwt.fromAuthHeaderAsBearerToken(),
      ignoreExpiration: false,
      secretOrKey: config.getOrThrow('JWT_SECRET'),
    });
  }

  async validate(payload: { sub: string; role: string }) {
    return { id: payload.sub, role: payload.role };
  }
}
```

### Public Route Exemption
```typescript
export const IS_PUBLIC_KEY = 'isPublic';
export const Public = () => SetMetadata(IS_PUBLIC_KEY, true);
```
