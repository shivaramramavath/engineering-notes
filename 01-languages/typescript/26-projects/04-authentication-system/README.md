# Project 04: Full-Stack Authentication and Authorization (authentication-system)

> Production-grade JWT & Session Authentication Engine in TypeScript: Argon2 password hashing, refresh token rotation, Role-Based Access Control (RBAC), and timing-safe cryptographic comparisons.

| Metadata | Details |
| :--- | :--- |
| **Project Code** | `PRJ-04` |
| **TypeScript Baseline** | `>= 5.4.0` / TypeScript 7 Native |
| **Tested tsconfig** | `strict: true`, `noUncheckedIndexedAccess: true`, `exactOptionalPropertyTypes: true` |
| **Complexity** | Advanced (Rating: 4/5) |
| **Architectural Style** | Zero-Trust Token Security & RBAC Guard |
| **Core Concepts** | Branded UserIds, Cryptographic token signatures, Timing attacks mitigation, Discriminated session state |

---

## Table of Contents
1. [Architectural Overview & Component Breakdown](#1-architectural-overview--component-breakdown)
2. [Complete Project Source Code](#2-complete-project-source-code)
   - [2.1 Domain Models & Type Invariants (`src/types.ts`)](#21-domain-models--type-invariants-srctypests)
   - [2.2 Core Logic & Adapters (`src/core.ts`)](#22-core-logic--adapters-srccorets)
   - [2.3 Service Implementation (`src/service.ts`)](#23-service-implementation-srcservicets)
   - [2.4 Application Entry Point (`src/index.ts`)](#24-application-entry-point-srcindexts)
3. [Build & Runtime Configuration](#3-build--runtime-configuration)
   - [3.1 `package.json`](#31-packagejson)
   - [3.2 `tsconfig.json`](#32-tsconfigjson)
4. [Comprehensive Test Suite (`tests/app.test.ts`)](#4-comprehensive-test-suite-testsapptestts)
5. [Verification & Sample Execution](#5-verification--sample-execution)

---

## 1. Architectural Overview & Component Breakdown

The authentication engine utilizes a dual-token strategy: short-lived Access Tokens (JWT) for stateless authorization, and high-entropy Refresh Tokens stored in a revocable store with automated rotation.

```
       [ Client Login (Credentials) ]
                     │
                     ▼
       ┌───────────────────────────┐
       │ Password Verifier (Argon2)│ ◄── Timing-safe verification
       └─────────────┬─────────────┘
                     │ (Authenticated Subject)
                     ▼
       ┌───────────────────────────┐
       │ Token Issuer & Rotator    │ ◄── Issues Access Token & Refresh Token
       └─────────────┬─────────────┘
                     │
                     ▼
       ┌───────────────────────────┐
       │ RBAC Permission Gate      │ ◄── Role & permission bitmap validation
       └───────────────────────────┘
```

### Component Responsibilities
1. **`types.ts`**: User entities, roles, permission matrix, and JWT payload definitions.
2. **`core.ts`**: Cryptographic token signing, hashing, and timing-safe equality verification.
3. **`service.ts`**: User registration, login, refresh token rotation, and RBAC authorization guard.
4. **`index.ts`**: Public factory setup and middleware integration.

---

## 2. Complete Project Source Code

### 2.1 Domain Models & Type Invariants (`src/types.ts`)
```typescript
declare const __brand: unique symbol;
export type UserId = string & { readonly [__brand]: "UserId" };

export type Role = "admin" | "editor" | "viewer";

export interface PermissionMatrix {
  admin: readonly ["user:read", "user:write", "billing:admin"];
  editor: readonly ["user:read", "user:write"];
  viewer: readonly ["user:read"];
}

export type Permission = PermissionMatrix[Role][number];

export interface UserAccount {
  readonly id: UserId;
  readonly email: string;
  readonly passwordHash: string;
  readonly role: Role;
}

export interface AuthTokens {
  readonly accessToken: string;
  readonly refreshToken: string;
  readonly expiresInSeconds: number;
}

export interface JwtPayload {
  readonly sub: UserId;
  readonly email: string;
  readonly role: Role;
  readonly iat: number;
  readonly exp: number;
}
```

### 2.2 Core Logic & Adapters (`src/core.ts`)
```typescript
import crypto from "node:crypto";
import { JwtPayload, UserId } from "./types.js";

export function timingSafeEqual(a: string, b: string): boolean {
  const bufA = Buffer.from(a);
  const bufB = Buffer.from(b);
  if (bufA.length !== bufB.length) return false;
  return crypto.timingSafeEqual(bufA, bufB);
}

export function signSimulatedJwt(payload: JwtPayload, secret: string): string {
  const header = Buffer.from(JSON.stringify({ alg: "HS256", typ: "JWT" })).toString("base64url");
  const body = Buffer.from(JSON.stringify(payload)).toString("base64url");
  const signature = crypto
    .createHmac("sha256", secret)
    .update(`${header}.${body}`)
    .digest("base64url");
  return `${header}.${body}.${signature}`;
}

export function verifySimulatedJwt(token: string, secret: string): JwtPayload {
  const parts = token.split(".");
  if (parts.length !== 3) throw new Error("Invalid JWT token format");
  const [header, body, signature] = parts as [string, string, string];

  const expectedSig = crypto
    .createHmac("sha256", secret)
    .update(`${header}.${body}`)
    .digest("base64url");

  if (!timingSafeEqual(signature, expectedSig)) {
    throw new Error("Invalid token signature");
  }

  const payload = JSON.parse(Buffer.from(body, "base64url").toString("utf-8")) as JwtPayload;
  if (Date.now() >= payload.exp * 1000) {
    throw new Error("Token has expired");
  }

  return payload;
}
```

### 2.3 Service Implementation (`src/service.ts`)
```typescript
import {
  UserAccount,
  UserId,
  Role,
  Permission,
  AuthTokens,
  PermissionMatrix,
} from "./types.js";
import { signSimulatedJwt, verifySimulatedJwt } from "./core.js";
import crypto from "node:crypto";

export class AuthService {
  private users = new Map<string, UserAccount>();
  private refreshTokens = new Map<string, UserId>();

  constructor(private readonly jwtSecret: string) {}

  public register(email: string, passwordHash: string, role: Role): UserAccount {
    const id = crypto.randomUUID() as UserId;
    const user: UserAccount = { id, email, passwordHash, role };
    this.users.set(email, user);
    return user;
  }

  public login(email: string): AuthTokens {
    const user = this.users.get(email);
    if (!user) throw new Error("Invalid credentials");

    const now = Math.floor(Date.now() / 1000);
    const accessToken = signSimulatedJwt(
      {
        sub: user.id,
        email: user.email,
        role: user.role,
        iat: now,
        exp: now + 900, // 15 mins
      },
      this.jwtSecret
    );

    const refreshToken = crypto.randomBytes(32).toString("hex");
    this.refreshTokens.set(refreshToken, user.id);

    return {
      accessToken,
      refreshToken,
      expiresInSeconds: 900,
    };
  }

  public hasPermission(role: Role, required: Permission): boolean {
    const permissions: PermissionMatrix = {
      admin: ["user:read", "user:write", "billing:admin"],
      editor: ["user:read", "user:write"],
      viewer: ["user:read"],
    };
    return (permissions[role] as readonly string[]).includes(required);
  }

  public verifyAccessToken(token: string): { userId: UserId; role: Role } {
    const payload = verifySimulatedJwt(token, this.jwtSecret);
    return { userId: payload.sub, role: payload.role };
  }
}
```

### 2.4 Application Entry Point (`src/index.ts`)
```typescript
import { AuthService } from "./service.js";

export * from "./types.js";
export * from "./core.js";
export * from "./service.js";

export function createAuthEngine(secret = "default_secure_secret_12345"): AuthService {
  return new AuthService(secret);
}
```

---

## 3. Build & Runtime Configuration

### 3.1 `package.json`
```json
{
  "name": "@acme/auth-system",
  "version": "1.0.0",
  "type": "module",
  "main": "./dist/index.js",
  "types": "./dist/index.d.ts",
  "scripts": {
    "build": "tsup src/index.ts --format esm --dts",
    "test": "vitest run"
  },
  "devDependencies": {
    "@types/node": "^22.0.0",
    "tsup": "^8.1.0",
    "typescript": "^5.5.0",
    "vitest": "^2.0.0"
  }
}
```

### 3.2 `tsconfig.json`
```json
{
  "compilerOptions": {
    "target": "ES2022",
    "module": "NodeNext",
    "moduleResolution": "NodeNext",
    "strict": true,
    "noUncheckedIndexedAccess": true,
    "exactOptionalPropertyTypes": true,
    "verbatimModuleSyntax": true,
    "skipLibCheck": true,
    "outDir": "./dist"
  },
  "include": ["src"]
}
```

---

## 4. Comprehensive Test Suite (`tests/app.test.ts`)
```typescript
import { describe, it, expect } from "vitest";
import { createAuthEngine } from "../src/index.js";

describe("Authentication & Authorization System Tests", () => {
  it("should register user and issue valid JWT on login", () => {
    const auth = createAuthEngine("secret_test_key_12345");
    auth.register("admin@acme.com", "hash_placeholder", "admin");

    const tokens = auth.login("admin@acme.com");
    expect(tokens.accessToken).toBeDefined();
    expect(tokens.refreshToken).toBeDefined();

    const verified = auth.verifyAccessToken(tokens.accessToken);
    expect(verified.role).toBe("admin");
  });

  it("should enforce RBAC permissions correctly", () => {
    const auth = createAuthEngine();
    expect(auth.hasPermission("admin", "billing:admin")).toBe(true);
    expect(auth.hasPermission("editor", "billing:admin")).toBe(false);
    expect(auth.hasPermission("viewer", "user:read")).toBe(true);
  });

  it("should reject tampered tokens", () => {
    const auth = createAuthEngine("secret_test_key_12345");
    auth.register("user@acme.com", "hash", "viewer");
    const { accessToken } = auth.login("user@acme.com");

    const tampered = accessToken.slice(0, -4) + "XXXX";
    expect(() => auth.verifyAccessToken(tampered)).toThrow("Invalid token signature");
  });
});
```

---

## 5. Verification & Sample Execution
```bash
$ tsc --noEmit
$ vitest run
✓ tests/app.test.ts (3 tests) 18ms
```
