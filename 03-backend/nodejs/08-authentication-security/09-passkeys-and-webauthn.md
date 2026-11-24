# Passkeys & WebAuthn Server-Side in Node.js

> Implementing FIDO2 / WebAuthn passwordless authentication with `@simplewebauthn/server`: registration, authentication, attestation, and public key verification.

| Metadata | Details |
| :--- | :--- |
| **Node.js Baseline** | Node.js 22 LTS / 24+ |
| **Packages** | `@simplewebauthn/server` v10+ |
| **Standard** | W3C WebAuthn Level 3 / FIDO2 |

---

## 1. How Passkeys Work

Passkeys replace shared secrets (passwords) with public-key cryptography:
- **Client/Authenticator**: Generates a public/private keypair (TouchID, FaceID, YubiKey, Windows Hello). The private key never leaves the secure enclave.
- **Server**: Generates a cryptographic challenge, stores the public key, and verifies incoming signatures using `@simplewebauthn/server`.

---

## 2. Server-Side Registration Flow

```javascript
import { generateRegistrationOptions, verifyRegistrationResponse } from '@simplewebauthn/server';

// 1. Generate registration options for client
export async function getRegistrationOptions(user) {
  const options = await generateRegistrationOptions({
    rpName: 'Enterprise Node.js Backend',
    rpID: 'example.com',
    userID: Buffer.from(user.id),
    userName: user.email,
    attestationType: 'none',
    authenticatorSelection: {
      residentKey: 'preferred',
      userVerification: 'preferred',
    },
  });

  // Save options.challenge in user session with short TTL
  await sessionStore.set(`challenge:${user.id}`, options.challenge, 60);
  return options;
}

// 2. Verify registration response from client authenticator
export async function verifyRegistration(user, clientResponse) {
  const expectedChallenge = await sessionStore.get(`challenge:${user.id}`);

  const verification = await verifyRegistrationResponse({
    response: clientResponse,
    expectedChallenge,
    expectedOrigin: 'https://example.com',
    expectedRPID: 'example.com',
  });

  if (verification.verified && verification.registrationInfo) {
    const { credentialPublicKey, credentialID, counter } = verification.registrationInfo;

    // Persist passkey record in PostgreSQL
    await db.passkeys.create({
      userId: user.id,
      credentialId: Buffer.from(credentialID).toString('base64url'),
      publicKey: Buffer.from(credentialPublicKey).toString('base64url'),
      counter,
    });

    return { success: true };
  }

  throw new Error('Passkey verification failed');
}
```
