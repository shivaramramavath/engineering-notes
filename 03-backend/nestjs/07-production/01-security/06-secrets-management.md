---
title: "Secrets Management and Cryptography in NestJS"
description: "Handling environment variables, HashiCorp Vault, AWS Secrets Manager, and cryptographic primitives in NestJS"
tags: [nestjs, security, secrets, vault, aws-secrets-manager, cryptography]
nestjs_version: "11.x"
node_version: ">=20.0.0"
express_version: "5.x"
---

# Secrets Management and Cryptography in NestJS

## Conceptual Overview
Managing cryptographic keys, database credentials, and third-party API tokens is critical for infrastructure security. Storing secrets in plain text files or environment variables on disk creates severe exposure risks.

This guide explores secrets management strategies in NestJS:
- Validating configuration using Joi / Zod with `@nestjs/config`.
- Dynamic secret retrieval from HashiCorp Vault or AWS Secrets Manager.
- Password hashing with **Argon2id**.
- Authenticated symmetric encryption using **AES-256-GCM**.

---

## Core Mechanisms & Architecture

### 1. Password Hashing: Argon2id
Argon2id is the winner of the Password Hashing Competition (PHC) and is recommended over bcrypt:
- Memory-hard algorithm resistant to GPU/ASIC cracking.
- Tunable memory cost, time cost, and parallelism.

### 2. Symmetric Encryption: AES-256-GCM
AES-GCM provides authenticated encryption, guaranteeing confidentiality and integrity through a Galois Authentication Tag.

---

## Implementation Details & NestJS APIs

### 1. Argon2 Hashing Service

```typescript
// src/security/hash.service.ts
import { Injectable } from '@nestjs/common';
import * as argon2 from 'argon2';

@Injectable()
export class HashService {
  async hashPassword(password: string): Promise<string> {
    return argon2.hash(password, {
      type: argon2.argon2id,
      memoryCost: 65536, // 64 MB
      timeCost: 3,
      parallelism: 4,
    });
  }

  async verifyPassword(hash: string, plainText: string): Promise<boolean> {
    return argon2.verify(hash, plainText);
  }
}
```

### 2. AES-256-GCM Encryption Service

```typescript
// src/security/cipher.service.ts
import { Injectable } from '@nestjs/common';
import { ConfigService } from '@nestjs/config';
import { createCipheriv, createDecipheriv, randomBytes } from 'node:crypto';

@Injectable()
export class CipherService {
  private readonly key: Buffer;

  constructor(private readonly config: ConfigService) {
    this.key = Buffer.from(this.config.getOrThrow<string>('ENCRYPTION_KEY_HEX'), 'hex');
  }

  encrypt(plainText: string): string {
    const iv = randomBytes(12); // 96-bit IV for AES-GCM
    const cipher = createCipheriv('aes-256-gcm', this.key, iv);

    let encrypted = cipher.update(plainText, 'utf8', 'hex');
    encrypted += cipher.final('hex');
    const tag = cipher.getAuthTag().toString('hex');

    return `${iv.toString('hex')}:${tag}:${encrypted}`;
  }

  decrypt(payload: string): string {
    const [ivHex, tagHex, encryptedText] = payload.split(':');
    const decipher = createDecipheriv('aes-256-gcm', this.key, Buffer.from(ivHex, 'hex'));
    decipher.setAuthTag(Buffer.from(tagHex, 'hex'));

    let decrypted = decipher.update(encryptedText, 'hex', 'utf8');
    decrypted += decipher.final('utf8');
    return decrypted;
  }
}
```

---

## Key Takeaways & Cheat Sheet

- Use **Argon2id** for password hashing.
- Use **AES-256-GCM** for symmetric encryption of sensitive fields at rest.
- Validate environment configuration at boot time using Joi or Zod.
- Never commit secrets to version control.

---

## Cross-References
- [01-security-fundamentals.md](./01-security-fundamentals.md) - Security foundations
- [06-authentication](../../04-intermediate/06-authentication/README.md) - Authentication patterns
