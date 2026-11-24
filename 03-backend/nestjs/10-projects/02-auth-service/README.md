# Project 02: Centralized Authentication & Token Service (`02-auth-service`)

> Enterprise Identity Provider (IdP) microservice: Argon2id password hashing, JWT Access/Refresh token rotation, Redis token revocation blacklist, and WebAuthn / Passkeys authentication.

| Metadata | Details |
| :--- | :--- |
| **Project Code** | `proj-nest-02-auth-service` |
| **NestJS Version** | NestJS 11+ |
| **Node.js Baseline** | Node.js >= 20.0.0 (Node.js 22 LTS) |
| **HTTP Platform** | Express 5 |
| **Database & Cache** | PostgreSQL 16 + Redis 7 (Token Blacklist & Rate Limiting) |
| **Auth Technologies** | Argon2id, RS256 JWT, @simplewebauthn/server (Passkeys) |
| **Testing** | Jest + Supertest |
| **Complexity Level** | Advanced Production Grade |

---

## 1. Architectural Overview & Domain Design

```
+----------------------------------------------------------------------------------------------------+
|                                    02-auth-service Flow                                            |
|                                                                                                    |
|  [ Client ] ---> [ POST /auth/login ]                                                              |
|                         |                                                                          |
|                         v                                                                          |
|             [ Verify Password: Argon2id ]                                                          |
|                         |                                                                          |
|                         v (Success)                                                                |
|             Generate RS256 JWT Pair:                                                               |
|             - Access Token (15m TTL)                                                               |
|             - Refresh Token (7d TTL, Hashed in DB)                                                 |
|                         |                                                                          |
|  [ Client ] ---> [ POST /auth/logout ]                                                             |
|                         |                                                                          |
|                         v                                                                          |
|             [ Blacklist Access Token in Redis ] (TTL = remaining exp)                              |
+----------------------------------------------------------------------------------------------------+
```

---

## 2. Complete Production Implementation

### 2.1 Refresh Token Rotation Service

```typescript
// src/auth/token.service.ts
import { Injectable, UnauthorizedException } from '@nestjs/common';
import { JwtService } from '@nestjs/jwt';
import { InjectRepository } from '@nestjs/typeorm';
import { Repository } from 'typeorm';
import { RefreshTokenEntity } from './entities/refresh-token.entity';
import * as argon2 from 'argon2';
import { randomBytes } from 'node:crypto';

@Injectable()
export class TokenService {
  constructor(
    private readonly jwtService: JwtService,
    @InjectRepository(RefreshTokenEntity)
    private readonly tokenRepo: Repository<RefreshTokenEntity>,
  ) {}

  async generateTokenPair(userId: string, role: string) {
    const accessToken = this.jwtService.sign(
      { sub: userId, role },
      { expiresIn: '15m' },
    );

    const rawRefreshToken = randomBytes(32).toString('hex');
    const hashedRefreshToken = await argon2.hash(rawRefreshToken);

    const tokenEntity = this.tokenRepo.create({
      userId,
      tokenHash: hashedRefreshToken,
      expiresAt: new Date(Date.now() + 7 * 24 * 60 * 60 * 1000),
    });
    await this.tokenRepo.save(tokenEntity);

    return {
      accessToken,
      refreshToken: `${tokenEntity.id}.${rawRefreshToken}`,
    };
  }

  async rotateTokenPair(compositeToken: string) {
    const [tokenId, rawToken] = compositeToken.split('.');
    if (!tokenId || !rawToken) throw new UnauthorizedException('Malformed token');

    const stored = await this.tokenRepo.findOne({ where: { id: tokenId } });
    if (!stored || stored.isRevoked || stored.expiresAt < new Date()) {
      throw new UnauthorizedException('Invalid or expired refresh token');
    }

    const matches = await argon2.verify(stored.tokenHash, rawToken);
    if (!matches) {
      // Possible reuse attack: Revoke all tokens for this user
      await this.tokenRepo.update({ userId: stored.userId }, { isRevoked: true });
      throw new UnauthorizedException('Token reuse detected. All sessions terminated.');
    }

    // Revoke old token and issue new pair (Rotation)
    stored.isRevoked = true;
    await this.tokenRepo.save(stored);

    return this.generateTokenPair(stored.userId, 'USER');
  }
}
```

---

## 3. Production Deployment & Verification

```bash
# Verify Auth Endpoint
curl -X POST http://localhost:3000/auth/login   -H "Content-Type: application/json"   -d '{"email":"admin@example.com","password":"StrongPassword123!"}'
```
