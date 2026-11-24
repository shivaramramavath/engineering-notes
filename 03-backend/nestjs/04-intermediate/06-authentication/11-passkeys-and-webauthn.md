# Passkeys and WebAuthn Authentication in NestJS

- **NestJS Version**: 11.x
- **Node.js Baseline**: >= 20.0.0
- **Module**: 04-intermediate / 06-authentication
- **Prerequisites**: [01-auth-overview](./01-auth-overview.md), [02-jwt-authentication](./02-jwt-authentication.md), [00-prerequisites/04-http-and-rest-basics](../../00-prerequisites/04-http-and-rest-basics.md)

---

## 1. Context & Purpose

Passwords represent the largest security vulnerability in modern web applications. They are susceptible to credential stuffing, brute force attacks, phishing, and server database breaches.

**Passkeys** (built on the W3C **WebAuthn** / FIDO2 standards) replace passwords with **asymmetric public-key cryptography**. Users authenticate using biometric sensors (Touch ID, Face ID, Windows Hello) or hardware security keys (YubiKeys). The user's private key never leaves their secure device hardware, making authentication entirely immune to phishing, server-side credential leaks, and interception attacks.

This guide demonstrates how to build a complete, production-ready WebAuthn / Passkeys authentication server in NestJS using `@simplewebauthn/server`.

---

## 2. Theoretical Foundations

Passkey authentication operates via a cryptographic **Challenge-Response** protocol:

### 2.1 Registration Ceremony
1. **Server generates challenge**: A cryptographically random buffer associated with the user session.
2. **Authenticator creates key pair**: Client browser calls `navigator.credentials.create()`. Device hardware creates a new public/private key pair unique to the server's domain (`rpID`).
3. **Server verifies & stores public key**: Authenticator returns signed attestation. Server verifies challenge and persists the `publicKey`, `credentialID`, and `counter` in the database.

### 2.2 Authentication Ceremony
1. **Server issues challenge**: Sends random challenge string.
2. **Authenticator signs challenge**: Client browser signs challenge with stored private key.
3. **Server verifies signature**: Server verifies signature using the user's stored `publicKey` and verifies that the `counter` has incremented (preventing cloned authenticators).

---

## 3. Architecture & Internal Mechanics

```
REGISTRATION FLOW:
Browser (Client)                                NestJS Server
   │                                                  │
   ├─ 1. POST /passkeys/register-options ─────────────>│
   │<─ 2. Returns Challenge & RP Options ──────────────┤ (Stores challenge in Redis)
   │                                                  │
   ├─ 3. navigator.credentials.create() (Biometrics)  │
   │                                                  │
   ├─ 4. POST /passkeys/register-verify (Attestation)─>│
   │                                                  │ Verify Challenge & Signature
   │                                                  │ Save Credential & PublicKey in DB
   │<─ 5. Return 200 OK (Registration Complete) ───────┤

AUTHENTICATION (LOGIN) FLOW:
Browser (Client)                                NestJS Server
   │                                                  │
   ├─ 1. POST /passkeys/auth-options ─────────────────>│
   │<─ 2. Returns Challenge & Allowed Credentials ─────┤ (Stores challenge in Redis)
   │                                                  │
   ├─ 3. navigator.credentials.get() (Biometrics)     │
   │                                                  │
   ├─ 4. POST /passkeys/auth-verify (Assertion) ──────>│
   │                                                  │ Verify Signature with PublicKey
   │                                                  │ Check Counter > Stored Counter
   │<─ 5. Issue JWT / Session Cookie ──────────────────┤
```

---

## 4. Concrete Implementation

### 4.1 Installing Dependencies

```bash
npm install --save @simplewebauthn/server
npm install --save-dev @simplewebauthn/types
```

### 4.2 Database Entity for Passkey Credentials

```typescript
// src/passkeys/entities/passkey-credential.entity.ts
export interface PasskeyCredential {
  id: string; // Base64URL credential ID
  userId: string;
  publicKey: Buffer;
  counter: number;
  deviceType: string;
  backedUp: boolean;
  transports?: string[];
  createdAt: Date;
}
```

---

### 4.3 Passkeys Service Implementation

```typescript
// src/passkeys/passkeys.service.ts
import { Injectable, BadRequestException, UnauthorizedException } from '@nestjs/common';
import {
  generateRegistrationOptions,
  verifyRegistrationResponse,
  generateAuthenticationOptions,
  verifyAuthenticationResponse,
  VerifiedRegistrationResponse,
  VerifiedAuthenticationResponse,
} from '@simplewebauthn/server';
import { Redis } from 'ioredis';
import { UsersService } from '../users/users.service';

const RP_NAME = 'Acme Corp';
const RP_ID = process.env.RP_ID || 'localhost';
const EXPECTED_ORIGIN = process.env.EXPECTED_ORIGIN || 'http://localhost:3000';

@Injectable()
export class PasskeysService {
  constructor(
    private readonly usersService: UsersService,
    private readonly redis: Redis,
  ) {}

  // 1. Generate Registration Options
  async getRegistrationOptions(userId: string) {
    const user = await this.usersService.findById(userId);
    const existingCredentials = await this.usersService.findCredentialsByUserId(userId);

    const options = await generateRegistrationOptions({
      rpName: RP_NAME,
      rpID: RP_ID,
      userID: Buffer.from(user.id),
      userName: user.email,
      attestationType: 'none',
      excludeCredentials: existingCredentials.map((c) => ({
        id: c.id,
        transports: c.transports as any,
      })),
      authenticatorSelection: {
        residentKey: 'preferred',
        userVerification: 'preferred',
      },
    });

    // Cache challenge in Redis with 5-minute expiration
    await this.redis.set(`reg_challenge:${userId}`, options.challenge, 'EX', 300);

    return options;
  }

  // 2. Verify Registration Response
  async verifyRegistration(userId: string, body: any): Promise<boolean> {
    const expectedChallenge = await this.redis.get(`reg_challenge:${userId}`);
    if (!expectedChallenge) {
      throw new BadRequestException('Registration challenge expired or missing');
    }

    const verification: VerifiedRegistrationResponse = await verifyRegistrationResponse({
      response: body,
      expectedChallenge,
      expectedOrigin: EXPECTED_ORIGIN,
      expectedRPID: RP_ID,
    });

    if (!verification.verified || !verification.registrationInfo) {
      throw new BadRequestException('Passkey registration verification failed');
    }

    const { credential, credentialDeviceType, credentialBackedUp } =
      verification.registrationInfo;

    // Persist credential in database
    await this.usersService.saveCredential({
      id: credential.id,
      userId,
      publicKey: Buffer.from(credential.publicKey),
      counter: credential.counter,
      deviceType: credentialDeviceType,
      backedUp: credentialBackedUp,
      transports: body.response.transports,
      createdAt: new Date(),
    });

    await this.redis.del(`reg_challenge:${userId}`);
    return true;
  }

  // 3. Generate Authentication (Login) Options
  async getAuthOptions(email: string) {
    const user = await this.usersService.findByEmail(email);
    const credentials = await this.usersService.findCredentialsByUserId(user.id);

    const options = await generateAuthenticationOptions({
      rpID: RP_ID,
      allowCredentials: credentials.map((c) => ({
        id: c.id,
        transports: c.transports as any,
      })),
      userVerification: 'preferred',
    });

    await this.redis.set(`auth_challenge:${user.id}`, options.challenge, 'EX', 300);
    return { options, userId: user.id };
  }

  // 4. Verify Authentication (Login) Response
  async verifyAuth(userId: string, body: any) {
    const expectedChallenge = await this.redis.get(`auth_challenge:${userId}`);
    if (!expectedChallenge) {
      throw new UnauthorizedException('Authentication challenge expired');
    }

    const credential = await this.usersService.findCredentialById(body.id);
    if (!credential) {
      throw new UnauthorizedException('Unknown authenticator credential');
    }

    const verification: VerifiedAuthenticationResponse = await verifyAuthenticationResponse({
      response: body,
      expectedChallenge,
      expectedOrigin: EXPECTED_ORIGIN,
      expectedRPID: RP_ID,
      credential: {
        id: credential.id,
        publicKey: credential.publicKey,
        counter: credential.counter,
      },
    });

    if (!verification.verified) {
      throw new UnauthorizedException('Cryptographic assertion failed');
    }

    // Update counter in database to prevent cloned devices
    await this.usersService.updateCredentialCounter(
      credential.id,
      verification.authenticationInfo.newCounter,
    );

    await this.redis.del(`auth_challenge:${userId}`);
    return { success: true };
  }
}
```

---

### 4.4 Passkeys Controller

```typescript
// src/passkeys/passkeys.controller.ts
import { Controller, Post, Body, UseGuards, Req } from '@nestjs/common';
import { PasskeysService } from './passkeys.service';
import { JwtAuthGuard } from '../auth/guards/jwt-auth.guard';
import { AuthService } from '../auth/auth.service';

@Controller('auth/passkeys')
export class PasskeysController {
  constructor(
    private readonly passkeysService: PasskeysService,
    private readonly authService: AuthService,
  ) {}

  // Authenticated user registers a new device passkey
  @UseGuards(JwtAuthGuard)
  @Post('register-options')
  async registerOptions(@Req() req: any) {
    return this.passkeysService.getRegistrationOptions(req.user.id);
  }

  @UseGuards(JwtAuthGuard)
  @Post('register-verify')
  async registerVerify(@Req() req: any, @Body() body: any) {
    return this.passkeysService.verifyRegistration(req.user.id, body);
  }

  // Passwordless Login
  @Post('login-options')
  async loginOptions(@Body('email') email: string) {
    return this.passkeysService.getAuthOptions(email);
  }

  @Post('login-verify')
  async loginVerify(@Body() { userId, response }: { userId: string; response: any }) {
    await this.passkeysService.verifyAuth(userId, response);
    // Issue standard JWT session token upon verified cryptographic proof
    return this.authService.issueJwtToken(userId);
  }
}
```

---

## 5. Edge Cases & Failure Modes

### 5.1 Clone Detection via Sign Counters
If an attacker duplicates a hardware key, both keys will use the same public key but their counters will desynchronize. If `verification.authenticationInfo.newCounter <= storedCounter` and the counter is not zero, the server must reject the assertion immediately as a suspected cloned device.

### 5.2 Origin Mismatch in Staging vs Production
WebAuthn strictly validates `expectedOrigin` (`https://app.example.com`). If a request passes through a proxy or CDN modifying headers, or if the port is mismatched (`http://localhost:3000` vs `http://localhost`), verification fails cryptographically.

---

## 6. Performance & Memory Implications

- **Crypto Operations**: Verifying ECDSA/RSA signatures in Node.js uses native OpenSSL bindings via `crypto`, completing in < 1ms.
- **Challenge Storage**: Challenges are lightweight strings stored in Redis with 5-minute TTL, consuming minimal RAM.

---

## 7. Anti-Patterns & Common Mistakes

- **Storing Challenges in Node Memory**: Storing challenges in an in-memory `Map`. When running multiple instances behind a load balancer, verification requests fail if they hit a different instance. Always use Redis.
- **Permitting Insecure HTTP Origins**: WebAuthn requires HTTPS in production. Browsers reject `navigator.credentials` on insecure HTTP (except `localhost`).
- **Ignoring the Counter**: Failing to persist and update `credential.counter` negates WebAuthn's clone detection protections.

---

## 8. Production Checklist & Operational Readiness

- [ ] `RP_ID` and `EXPECTED_ORIGIN` configured via environment variables.
- [ ] Challenges persisted in Redis with short expiration (3–5 minutes).
- [ ] Counter increment validation enforced to prevent credential cloning.
- [ ] Multiple passkeys supported per user (e.g. Phone + Laptop).
- [ ] Fallback authentication method (recovery codes or email magic links) provided.

---

## 9. Migration & Version History

- **WebAuthn Level 2/3**: Standardized passkey syncing (iCloud Keychain, Google Password Manager).
- **SimpleWebAuthn 10.x+**: First-class Node 20/22 support with zero native C++ compilation dependencies.

---

## 10. Verification & Testing

Unit testing passkey option generation:

```typescript
import { Test, TestingModule } from '@nestjs/testing';
import { PasskeysService } from './passkeys.service';
import { UsersService } from '../users/users.service';

describe('PasskeysService', () => {
  let service: PasskeysService;
  let mockRedis: any;

  beforeEach(async () => {
    mockRedis = { set: vi.fn(), get: vi.fn(), del: vi.fn() };
    const mockUsers = {
      findById: vi.fn().mockResolvedValue({ id: 'u123', email: 'user@example.com' }),
      findCredentialsByUserId: vi.fn().mockResolvedValue([]),
    };

    const module: TestingModule = await Test.createTestingModule({
      providers: [
        PasskeysService,
        { provide: UsersService, useValue: mockUsers },
        { provide: 'Redis', useValue: mockRedis },
      ],
    }).compile();

    service = module.get<PasskeysService>(PasskeysService);
  });

  it('generates registration options with challenge', async () => {
    const options = await service.getRegistrationOptions('u123');
    expect(options.challenge).toBeDefined();
    expect(mockRedis.set).toHaveBeenCalled();
  });
});
```

---

## 11. Comprehensive Examples

### Client-Side Browser Trigger (Vanilla JS)

```javascript
// client/passkeys.js
import { startRegistration, startAuthentication } from '@simplewebauthn/browser';

// 1. Register Passkey
export async function registerPasskey() {
  const optionsRes = await fetch('/auth/passkeys/register-options', { credentials: 'include' });
  const options = await optionsRes.json();

  const regResponse = await startRegistration({ optionsJSON: options });

  await fetch('/auth/passkeys/register-verify', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(regResponse),
  });
  alert('Passkey registered successfully!');
}

// 2. Login with Passkey
export async function loginWithPasskey(email) {
  const optionsRes = await fetch('/auth/passkeys/login-options', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ email }),
  });
  const { options, userId } = await optionsRes.json();

  const authResponse = await startAuthentication({ optionsJSON: options });

  const verifyRes = await fetch('/auth/passkeys/login-verify', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ userId, response: authResponse }),
  });

  const { token } = await verifyRes.json();
  localStorage.setItem('token', token);
  window.location.href = '/dashboard';
}
```

---

## 12. Cross-References & Further Reading

- [01-auth-overview.md](./01-auth-overview.md) - Authentication architecture
- [02-jwt-authentication.md](./02-jwt-authentication.md) - JWT token issuance
- [FIDO Alliance Passkey Guidelines](https://fidoalliance.org/passkeys/)
- [SimpleWebAuthn Documentation](https://simplewebauthn.dev)
