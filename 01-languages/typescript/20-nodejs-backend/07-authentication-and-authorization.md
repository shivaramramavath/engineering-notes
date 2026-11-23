# Authentication, Authorization, and Session Typing in TypeScript

> Typing security architectures in Node.js: JWT payload typing, branded token IDs, session cookie schemas, Role-Based Access Control (RBAC), and Attribute-Based Access Control (ABAC).

| Metadata | Details |
| :--- | :--- |
| **Category** | Node.js Backend Typing |
| **Difficulty** | Advanced |
| **Prerequisites** | [04-objects-and-interfaces/02-discriminated-unions.md](../04-objects-and-interfaces/02-discriminated-unions.md), [20-nodejs-backend/03-middleware.md](./03-middleware.md) |
| **TypeScript Baseline** | TS 5.0+, Node.js 20/22 LTS |
| **Tested tsconfig** | `"strict": true, "target": "ES2022", "module": "NodeNext"` |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & API Reference](#4-syntax--api-reference)
5. [Practical Examples](#5-practical-examples)
6. [Edge Cases, Pitfalls & Gotchas](#6-edge-cases-pitfalls--gotchas)
7. [Common Mistakes & Anti-Patterns](#7-common-mistakes--anti-patterns)
8. [Performance & Type-Checker Complexity](#8-performance--type-checker-complexity)
9. [Compiler Internals](#9-compiler-internals)
10. [Best Practices & Design Guidelines](#10-best-practices--design-guidelines)
11. [Real-World Case Study](#11-real-world-case-study)
12. [Interview Questions](#12-interview-questions)
13. [Exercises & Challenges](#13-exercises--challenges)

---

## 1. What & Why

Authentication (**AuthN**: *Who are you?*) and Authorization (**AuthZ**: *What are you allowed to do?*) represent the security backbone of any backend application. In dynamic or weakly-typed codebases, security vulnerabilities emerge from subtle type mismatches:
- **Untyped JWT Payloads**: Accessing `token.userId` when the token actually contains `token.sub` causes authentication checks to pass with `undefined` IDs.
- **Role String Typos**: Writing `if (user.role === 'admin')` while the database stores `'ADMIN'` leads to unauthorized privilege escalation or accidental lockout.
- **Permissive ABAC Rules**: Attribute-based rules that fail open due to loose boolean checking.

In TypeScript, we establish compile-time security guarantees:
- **Strict JWT Interfaces**: Guaranteeing standard claims (`sub`, `exp`, `iat`, `iss`).
- **Nominal Branded IDs**: Distinguishing `UserId` from `TenantId` so IDs cannot be accidentally transposed in database queries.
- **Exhaustive Permission Matrices**: Using mapped types to enforce that every role explicitly defines permissions for every resource.

```
+-----------------------------------------------------------+
|                   Type-Safe Auth Pipeline                 |
|                                                           |
|   Incoming HTTP Request (Bearer Header)                   |
|                      |                                    |
|                      v                                    |
|   verifyJwt(token, schema) -> ValidatedJwtPayload         |
|                      |                                    |
|                      v                                    |
|   checkPermission(user.role, 'documents:delete')          |
|   (Verified against compile-time Permission Matrix)       |
|                      |                                    |
|                      v                                    |
|   Authorized Execution or 403 Forbidden Response          |
+-----------------------------------------------------------+
```

---

## 2. Core Concepts

### 1. Strongly-Typed JWT Payload
A type-safe JSON Web Token claims structure:
```typescript
export interface JwtClaims {
  readonly sub: string;           // Subject (User ID)
  readonly iss: string;           // Issuer
  readonly exp: number;           // Expiration timestamp (seconds)
  readonly iat: number;           // Issued-at timestamp (seconds)
  readonly role: 'ADMIN' | 'MEMBER' | 'GUEST';
}
```

### 2. Role-Based Access Control (RBAC) Matrix
Using mapped types to guarantee that every role declares its allowed actions:
```typescript
export type Role = 'admin' | 'editor' | 'viewer';
export type Permission = 'post:create' | 'post:edit' | 'post:delete' | 'user:manage';

export const ROLE_PERMISSIONS: Record<Role, readonly Permission[]> = {
  admin: ['post:create', 'post:edit', 'post:delete', 'user:manage'],
  editor: ['post:create', 'post:edit'],
  viewer: []
};
```

### 3. Attribute-Based Access Control (ABAC)
Rules that depend on resource attributes and dynamic context (e.g. *A user can edit a post only if they are the author OR an admin*):
```typescript
export type Rule<TResource> = (user: AuthUser, resource: TResource) => boolean;
```

---

## 3. How It Works Internally

During token verification:
1. `jsonwebtoken` or `jose` verifies the cryptographic signature (RS256/EdDSA).
2. The claims object is validated against a Zod schema.
3. Flow analysis narrows `req.user` to `AuthUser`.
4. The authorization guard checks `hasPermission(req.user, 'resource:action')` using lookup arrays in $O(1)$ time.

---

## 4. Syntax & API Reference

### Nominal Branded ID Helper
```typescript
export type UserId = string & { readonly __brand: unique symbol };
export type TenantId = string & { readonly __brand: unique symbol };

export function makeUserId(id: string): UserId {
  return id as UserId;
}
```

---

## 5. Practical Examples

### Example 1: Basic JWT Verification with Zod Validation
```typescript
import jwt from 'jsonwebtoken';
import { z } from 'zod';

const JwtClaimsSchema = z.object({
  sub: z.string(),
  email: z.string().email(),
  role: z.enum(['ADMIN', 'USER']),
  exp: z.number(),
  iat: z.number()
});

export type ValidatedClaims = z.infer<typeof JwtClaimsSchema>;

export function verifyAccessToken(token: string, secret: string): ValidatedClaims {
  const decoded = jwt.verify(token, secret);
  return JwtClaimsSchema.parse(decoded);
}
```

### Example 2: Real-World Type-Safe RBAC Permission Guard Middleware
```typescript
import type { Request, Response, NextFunction } from 'express';

export function authorize(requiredPermission: Permission) {
  return (req: Request, res: Response, next: NextFunction): void => {
    const userRole = req.user?.role;
    if (!userRole) {
      res.status(401).json({ error: 'Unauthorized' });
      return;
    }

    const permissions = ROLE_PERMISSIONS[userRole] || [];
    if (!permissions.includes(requiredPermission)) {
      res.status(403).json({ error: 'Forbidden: Insufficient permissions' });
      return;
    }

    next();
  };
}
```

### Example 3: Edge Case - ABAC Policy Checker with Resource Ownership
```typescript
export interface Document {
  id: string;
  ownerId: string;
  isPublic: boolean;
}

export function canEditDocument(user: AuthUser, doc: Document): boolean {
  if (user.role === 'admin') return true;
  return doc.ownerId === user.id;
}
```

### Example 4: Failure Case - Transposing Branded IDs
```typescript
declare function fetchTenantUser(tenant: TenantId, user: UserId): Promise<void>;

declare const myUserId: UserId;
declare const myTenantId: TenantId;

// @ts-expect-error TS2345: Argument of type 'UserId' is not assignable to parameter of type 'TenantId'.
fetchTenantUser(myUserId, myTenantId);
```

### Example 5: Idiomatic Session Cookie Signature Verification
```typescript
import { z } from 'zod';

export const SessionCookieSchema = z.object({
  sessionId: z.string().uuid(),
  userId: z.string(),
  createdAt: z.number()
});

export function parseSessionCookie(rawCookie: string): z.infer<typeof SessionCookieSchema> | null {
  try {
    const json = Buffer.from(rawCookie, 'base64').toString('utf-8');
    return SessionCookieSchema.parse(JSON.parse(json));
  } catch {
    return null;
  }
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **JWT Expiration Precision**:
   In standard JWT specifications, the `exp` claim is in **seconds**, not milliseconds. Comparing `token.exp < Date.now()` directly will always evaluate to true, falsely treating all tokens as expired! Always compare `token.exp * 1000 < Date.now()`.
2. **The "None" Algorithm Exploit**:
   Older JWT libraries allowed tokens signed with `"alg": "none"`. Always specify allowed algorithms explicitly during verification: `jwt.verify(token, key, { algorithms: ['RS256'] })`.
3. **Session Revocation in Stateless JWTs**:
   Stateless JWTs cannot be revoked without maintaining a centralized revocation blacklist in Redis. For immediate revocation capability, use session identifiers stored in Redis.
4. **Case Sensitivity in Roles**:
   Ensure role enums are normalized (e.g. uppercase `'ADMIN'`). Mismatched case causes subtle permission bypasses.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Bad Code | Good Code | Compiler / Runtime Impact |
| :--- | :--- | :--- | :--- |
| **Comparing `exp` in Milliseconds** | `token.exp < Date.now()` | `token.exp * 1000 < Date.now()` | All valid tokens falsely rejected |
| **Loose Role Strings** | `role: string` | `role: 'admin' \| 'user'` | Typo bypasses access checks |
| **Unsigned Cookie Sessions** | Storing user ID directly in raw cookie | Cryptographically signed session cookie | Attackers forge cookie with arbitrary user IDs |
| **Permissive Default in ABAC** | Fallthrough returns `true` | Default to `false` (Deny by default) | Unauthorized access on unhandled edge cases |
| **Allowing Arbitrary JWT Algorithms** | `jwt.verify(token, key)` | `{ algorithms: ['RS256'] }` | Alg: none signature bypass vulnerability |

---

## 8. Performance & Type-Checker Complexity

- **RBAC Lookup Speed**: Checking array inclusion `ROLE_PERMISSIONS[role].includes(action)` or using a Set takes ~20 nanoseconds.
- **Cryptographic Verification**: Asymmetric RSA/ECDSA verification takes ~0.5ms of CPU time in Node.js.

---

## 9. Compiler Internals

When using branded types:
```typescript
type UserId = string & { readonly __brand: unique symbol };
```
Because the `unique symbol` is un-constructible by any regular string literal, TypeScript considers the types structurally incompatible, completely preventing transposition.

---

## 10. Best Practices & Design Guidelines

1. **Deny by Default**: Authorization logic must default to denying access unless an explicit rule permits it.
2. **Brand Domain IDs**: Prevent passing User IDs where Tenant IDs are expected.
3. **Enforce Allowed JWT Algorithms**: Restrict verification strictly to `RS256` or `EdDSA`.
4. **Validate Claims at Runtime**: Parse decoded tokens with Zod schemas.

---

## 11. Real-World Case Study

### Multi-Tenant Healthcare Authorization Engine
Enforcing HIPAA-compliant role and tenant boundaries on patient medical records:

```typescript
export interface MedicalRecord {
  id: string;
  tenantId: TenantId;
  patientId: string;
  assignedDoctorId: string;
}

export class MedicalAuthorizationEngine {
  public static canViewRecord(user: AuthUser, record: MedicalRecord): boolean {
    // Rule 1: Tenant boundary isolation (Zero cross-tenant access allowed)
    if (user.tenantId !== record.tenantId) return false;

    // Rule 2: Admins within the tenant have full view access
    if (user.role === 'ADMIN') return true;

    // Rule 3: Doctors can only view their own assigned patients
    if (user.role === 'DOCTOR') {
      return record.assignedDoctorId === user.id;
    }

    return false; // Deny by default
  }
}
```

---

## 12. Interview Questions

<details>
<summary>1. What is the difference between Authentication and Authorization?</summary>

Authentication (AuthN) verifies the identity of a user (*Who are you?*). Authorization (AuthZ) verifies what permissions or resources an authenticated user is permitted to access (*What are you allowed to do?*).
</details>

<details>
<summary>2. Why are Nominal Branded Types valuable for security IDs?</summary>

In plain TypeScript, all string aliases (`type UserId = string; type TenantId = string`) are structurally identical. A developer could accidentally write `db.query(userId, tenantId)` with swapped arguments. Nominal branding prevents this at compile time by adding unique phantom symbols.
</details>

<details>
<summary>3. What is the difference between RBAC and ABAC?</summary>

RBAC (Role-Based Access Control) assigns permissions based on static roles (`admin`, `editor`). ABAC (Attribute-Based Access Control) evaluates dynamic contextual attributes: resource attributes (ownership), user attributes, and environmental conditions (time, IP address).
</details>

<details>
<summary>4. Why is the JWT `exp` claim frequently misunderstood in JavaScript?</summary>

The standard UNIX timestamp in JWT `exp` is in **seconds**, whereas JavaScript's `Date.now()` is in **milliseconds**. Comparing them directly without multiplying `exp * 1000` causes tokens to appear expired immediately.
</details>

<details>
<summary>5. What is the "Deny by Default" security principle?</summary>

It specifies that access should be denied unless explicit authorization rules are satisfied. If an error occurs, an unhandled role is encountered, or a rule evaluates to undefined, the system must reject access.
</details>

<details>
<summary>6. How do you revoke a stateless JWT before its expiration date?</summary>

Stateless JWTs cannot be revoked without external state. Common solutions include: maintaining a short-lived token expiration (e.g. 15 minutes) with rotating refresh tokens, or storing revoked token IDs (JTI) in an in-memory Redis blacklist until their expiration time.
</details>

<details>
<summary>7. Why should JWT verification always specify the `algorithms` array?</summary>

To prevent algorithm confusion attacks where an attacker crafts a token with `"alg": "none"` or switches from asymmetric `RS256` to symmetric `HS256` signed with the server's public key.
</details>

<details>
<summary>8. How do you type a session cookie in TypeScript?</summary>

Define a strict Zod schema for the session payload, parse cookies using `cookieParser`, and validate with `SessionSchema.parse(req.cookies.session)`.
</details>

---

## 13. Exercises & Challenges

<details>
<summary>Exercise 1 (Easy): Basic Role Union Type</summary>

**Problem:** Define a union `UserRole` for `'SUPERADMIN' | 'MANAGER' | 'EMPLOYEE'`.

**Solution:**
```typescript
export type UserRole = 'SUPERADMIN' | 'MANAGER' | 'EMPLOYEE';
```
</details>

<details>
<summary>Exercise 2 (Easy): Branded Token Generator</summary>

**Problem:** Create a branded `ApiKey` type and constructor function.

**Solution:**
```typescript
export type ApiKey = string & { readonly __brand: unique symbol };
export function makeApiKey(key: string): ApiKey { return key as ApiKey; }
```
</details>

<details>
<summary>Exercise 3 (Medium): Exhaustive Role Permission Map</summary>

**Problem:** Write a mapped type ensuring every `UserRole` defines an array of permissions.

**Solution:**
```typescript
type Role = 'admin' | 'user';
type Action = 'read' | 'write' | 'delete';

export const Permissions: Record<Role, readonly Action[]> = {
  admin: ['read', 'write', 'delete'],
  user: ['read']
};
```
</details>

<details>
<summary>Exercise 4 (Medium): JWT Expiration Checker</summary>

**Problem:** Write a function `isTokenExpired(expSeconds: number): boolean`.

**Solution:**
```typescript
export function isTokenExpired(expSeconds: number): boolean {
  return expSeconds * 1000 <= Date.now();
}
```
</details>

<details>
<summary>Exercise 5 (Hard): Type-Safe Permission Guard Builder</summary>

**Problem:** Create a function `hasPermission<R extends Role>(role: R, action: Permission): boolean` that verifies against a typed matrix.

**Solution:**
```typescript
export function checkPermission(
  role: 'admin' | 'user',
  action: 'read' | 'write'
): boolean {
  const matrix: Record<'admin' | 'user', ('read' | 'write')[]> = {
    admin: ['read', 'write'],
    user: ['read']
  };
  return matrix[role].includes(action);
}
```
</details>

<details>
<summary>Exercise 6 (Hard): Attribute-Based Access Control Rule Engine</summary>

**Problem:** Implement an ABAC engine that evaluates an array of rules against a user and resource, requiring all rules to pass.

**Solution:**
```typescript
export type PolicyRule<U, R> = (user: U, resource: R) => boolean;

export class PolicyEngine<U, R> {
  constructor(private rules: PolicyRule<U, R>[]) {}

  public isAuthorized(user: U, resource: R): boolean {
    if (this.rules.length === 0) return false; // Deny by default
    return this.rules.every((rule) => rule(user, resource));
  }
}
```
</details>
