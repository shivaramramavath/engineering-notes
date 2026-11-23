# Data Transfer Object (DTO) Pattern in TypeScript

> Architectural pattern for decoupling persistence and domain models from transport representations using TypeScript interfaces, utility types, mapped types, and branded validation.

| Metadata | Details |
| :--- | :--- |
| **Category** | Type-Safe APIs |
| **Difficulty** | Intermediate |
| **Prerequisites** | [04-objects-and-interfaces/00-interfaces.md](../04-objects-and-interfaces/00-interfaces.md), [07-utility-types/00-partial-required-readonly.md](../07-utility-types/00-partial-required-readonly.md), [15-runtime-validation/01-schema-validation.md](../15-runtime-validation/01-schema-validation.md) |
| **TypeScript Baseline** | TS 4.5+, TS 5.0+ |
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

A **Data Transfer Object (DTO)** is an object designed solely to carry data between processes, network boundaries, or application layers. In a fullstack TypeScript architecture, domain entities (often backed by database ORMs such as Prisma, Drizzle, or TypeORM) contain sensitive fields, database-generated timestamps, circular references, and internal business state that should never leak over the public network boundary.

Directly exposing database models to clients creates tight coupling, security vulnerabilities (such as mass assignment vulnerabilities where attackers inject `isAdmin: true` or overwrite `id`), and breaks API backward compatibility whenever the database schema changes.

TypeScript allows us to model DTOs with compile-time mathematical precision:
- **Projection**: Using `Pick`, `Omit`, and mapped types to slice domain entities into input and output shapes.
- **Transformation**: Converting database primitive types (e.g., SQL `timestamptz` or `Date`) into serializable wire representations (ISO 8601 strings).
- **Encapsulation**: Preventing unintended mutation and accidental exposure of domain logic.

```
+------------------+          DTO Transformation          +------------------+
|  Domain Entity   |  --------------------------------->  |   Response DTO   |
| (Database Model) |  Omit<Entity, 'hashedPassword'>     |   (Wire Format)  |
| - id: bigint     |                                      | - id: string     |
| - passwordHash   |                                      | - email: string  |
| - internalFlags  |                                      | - createdAt: str |
+------------------+                                      +------------------+
```

---

## 2. Core Concepts

### DTO vs Domain Model vs View Model
- **Domain Model**: Rich object or interface modeling business entities, containing database-specific types (`Date`, `Decimal`, `bigint`), internal identifiers, and relation methods.
- **Request DTO**: The minimal validated payload expected from an external client (e.g., `CreateUserDTO`, `UpdateUserDTO`). Fields like IDs and timestamps are omitted.
- **Response DTO**: The serialized shape delivered to the client, guaranteeing that internal secrets (password hashes, soft-delete flags, internal tenant IDs) are removed and non-JSON-native types are serialized.
- **View Model**: Client-side UI representation often enriched with display state (`isSelected`, `isPending`).

### Type Transformation Utilities
TypeScript provides native utility types to derive DTOs cleanly:
```typescript
interface UserEntity {
  id: string;
  email: string;
  passwordHash: string;
  twoFactorSecret: string | null;
  role: 'admin' | 'member';
  createdAt: Date;
  updatedAt: Date;
}

// Request DTO: client cannot set id, dates, or passwordHash
export type CreateUserRequestDTO = Pick<UserEntity, 'email' | 'role'> & {
  passwordPlainText: string;
};

// Response DTO: excludes security secrets, formats Date to string
export type UserResponseDTO = Omit<UserEntity, 'passwordHash' | 'twoFactorSecret' | 'createdAt' | 'updatedAt'> & {
  createdAt: string; // ISO 8601 string
  updatedAt: string;
};
```

---

## 3. How It Works Internally

The TypeScript compiler treats types structurally. When mapping a domain entity to a DTO, structural subtyping rules govern assignability.

```
       Source: UserEntity
     +--------------------+
     | id: string         |
     | email: string      |
     | passwordHash: str  |
     +--------------------+
               |
               | Type Checker: CheckAssignability
               | If target is UserResponseDTO:
               |   Does target have passwordHash? No.
               |   Excess Property Checks apply only on object literals!
               v
     +--------------------+
     | id: string         |
     | email: string      |
     +--------------------+
       Target: UserResponseDTO
```

Crucially, **type erasure** means TypeScript cannot strip unwanted properties at runtime:
```typescript
function toUserDTO(user: UserEntity): UserResponseDTO {
  // @ts-expect-error - TS recognizes type compatibility if excess check is bypassed, but runtime object STILL HAS passwordHash!
  return user as unknown as UserResponseDTO; // SECURITY HOLE: passwordHash is still present on JSON.stringify!
}
```
A legitimate DTO transformation requires an explicit runtime mapping function or runtime schema parsing (e.g. Zod, ArkType) to physically reconstruct a clean object.

---

## 4. Syntax & API Reference

### Mapped Type Converters for JSON Serialization
Database entities frequently utilize `Date`, `bigint`, or custom classes. When sending over HTTP JSON, these must be transformed to primitives.
```typescript
export type SerializeToJson<T> = T extends Date
  ? string
  : T extends bigint
  ? string
  : T extends (...args: any[]) => any
  ? never
  : T extends object
  ? { [K in keyof T]: SerializeToJson<T[K]> }
  : T;
```

### Immutable DTOs
DTOs are data transport bags; once instantiated, they must be immutable to prevent cross-service race conditions:
```typescript
export type ImmutableDTO<T> = Readonly<{
  [K in keyof T]: T[K] extends object ? ImmutableDTO<T[K]> : T[K];
}>;
```

---

## 5. Practical Examples

### Example 1: Basic Request and Response DTO Definition
```typescript
interface ProductEntity {
  id: string;
  name: string;
  sku: string;
  costPriceCents: number;
  retailPriceCents: number;
  supplierId: string;
  isDeleted: boolean;
  createdAt: Date;
}

export type CreateProductDTO = {
  name: string;
  sku: string;
  retailPriceCents: number;
};

export type PublicProductDTO = Pick<ProductEntity, 'id' | 'name' | 'sku' | 'retailPriceCents'>;

export function mapToPublicProductDTO(entity: ProductEntity): PublicProductDTO {
  return {
    id: entity.id,
    name: entity.name,
    sku: entity.sku,
    retailPriceCents: entity.retailPriceCents
  };
}
```

### Example 2: Real-World Nested DTO with Schema Validation
```typescript
import { z } from 'zod';

export const AddressSchema = z.object({
  street: z.string().min(1),
  city: z.string().min(1),
  postalCode: z.string().regex(/^[0-9]{5}(-[0-9]{4})?$/)
});

export const CreateCustomerDTOSchema = z.object({
  fullName: z.string().min(2),
  email: z.string().email(),
  billingAddress: AddressSchema,
  shippingAddress: AddressSchema.optional()
});

export type CreateCustomerDTO = z.infer<typeof CreateCustomerDTOSchema>;
export type AddressDTO = z.infer<typeof AddressSchema>;
```

### Example 3: Edge Case - Recursive Nested DTOs (Comment Thread)
```typescript
interface CommentNodeEntity {
  id: string;
  authorId: string;
  content: string;
  internalFlags: number;
  replies: CommentNodeEntity[];
}

export interface CommentResponseDTO {
  id: string;
  content: string;
  replies: CommentResponseDTO[];
}

export function transformCommentTree(node: CommentNodeEntity): CommentResponseDTO {
  return {
    id: node.id,
    content: node.content,
    replies: node.replies.map(transformCommentTree)
  };
}
```

### Example 4: Failure Case - Direct Return Bypassing Runtime Stripping
```typescript
interface AccountEntity {
  id: string;
  username: string;
  secretToken: string;
}

interface AccountDTO {
  id: string;
  username: string;
}

function unsafeTransform(acc: AccountEntity): AccountDTO {
  // @ts-expect-error TS2352: Conversion of type 'AccountEntity' to type 'AccountDTO' may be a mistake if structural compatibility differs, but here structural typing allows it!
  // However, returning acc directly leaks `secretToken` to JSON.stringify!
  return acc; // Compiles because AccountEntity has id & username, but secretToken leaks!
}
```

### Example 5: Idiomatic Mapper Class with Static Factories
```typescript
export class UserMapper {
  public static toDTO(entity: { id: string; email: string; passwordHash: string; createdAt: Date }): {
    id: string;
    email: string;
    createdAtIso: string;
  } {
    return Object.freeze({
      id: entity.id,
      email: entity.email,
      createdAtIso: entity.createdAt.toISOString()
    });
  }

  public static toDTOList<T extends { id: string; email: string; passwordHash: string; createdAt: Date }>(
    entities: readonly T[]
  ) {
    return entities.map(UserMapper.toDTO);
  }
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **The Structural Leak (Object Literal vs Variable Assignability)**:
   In TypeScript, assigning a variable containing excess fields to a type with fewer fields is completely valid because TypeScript uses structural subtyping:
   ```typescript
   const entity = { id: '1', name: 'Alice', secretKey: 'SECRET_123' };
   const dto: { id: string; name: string } = entity; // Permitted!
   console.log(JSON.stringify(dto)); // '{"id":"1","name":"Alice","secretKey":"SECRET_123"}' Leaks secretKey!
   ```
2. **Date Serialization Pitfall**:
   In JSON, `Date` objects serialize via `.toISOString()`, yielding strings. If your client expects `Date`, it receives `string`, breaking date arithmetic (`dto.createdAt.getTime()` fails at runtime). Always type DTO dates explicitly as `string` (or use branded date strings).
3. **BigInt Serialization**:
   `JSON.stringify` throws a `TypeError: Do not know how to serialize a BigInt` when attempting to stringify native `bigint`. DTOs must convert `bigint` to `string` or `number` before serialization.
4. **Undefined vs Null in Patches**:
   In `Partial<UpdateDTO>`, `undefined` means "do not update", while `null` means "clear this field". Confusing the two causes subtle data corruption in database updates.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Bad Code | Good Code | Compiler / Runtime Impact |
| :--- | :--- | :--- | :--- |
| **Direct Entity Exposure** | `return db.user.findFirst()` directly in API route | `return UserMapper.toDTO(user)` | Exposes password hashes and database internals over HTTP |
| **Cast-Based Stripping** | `return entity as unknown as UserDTO` | `const { secret, ...safe } = entity; return safe;` | Type check succeeds, but runtime payload contains all forbidden fields |
| **Date Object in Wire Type** | `interface DTO { date: Date; }` | `interface DTO { date: string; }` | Runtime type mismatch on client (`date.getFullYear` throws `TypeError`) |
| **Unconstrained Partial Update** | `function update(dto: Partial<Entity>)` | `function update(dto: UpdateUserRequestDTO)` | Allows clients to attempt updating primary keys (`id`) |
| **Shared Mutable DTO** | `function handle(dto: UserDTO) { dto.id = '2'; }` | `function handle(dto: Readonly<UserDTO>)` | Mutation side-effects across event listeners or middleware |
| **Over-Fetching Over the Wire** | Reusing one massive 50-field DTO for every list endpoint | Dedicated lightweight `UserSummaryDTO` | Bloated network payload and severe mobile bandwidth degradation |

---

## 8. Performance & Type-Checker Complexity

- **Instantiation Depth**: Deeply nested DTO types with recursive `SerializeToJson<T>` types can exceed TypeScript's max instantiation depth if circular references exist in the domain model.
- **Object Allocation in Mappers**: In high-throughput microservices (10,000 req/sec), creating intermediate DTO objects in loops can trigger V8 Garbage Collection pressure. For hot paths, explicitly constructed flat object literals with predefined hidden classes (monomorphic shapes) perform ~4x faster than dynamic destructuring (`const { secret, ...rest } = entity`).

---

## 9. Compiler Internals

During compilation, when checking DTO assignment:
1. **Excess Property Checks (EPC)**: The compiler's `checkDestructuringOrAssignment` activates EPC *only* when the right-hand side is an explicit object literal:
   ```typescript
   const bad: UserDTO = { id: '1', email: 'a@b.com', extra: 123 }; // TS2353: Object literal may only specify known properties
   ```
2. If the entity is a typed variable reference:
   ```typescript
   const entity: UserEntity = ...;
   const good: UserDTO = entity; // No EPC triggered. TS checks `isTypeAssignableTo(UserEntity, UserDTO)` -> TRUE.
   ```
   Because `UserEntity` contains all required properties of `UserDTO`, assignability succeeds. The compiler's type checker ignores runtime property filtering.

---

## 10. Best Practices & Design Guidelines

1. **Always Use Explicit Mapper Functions**: Never rely on type assertions (`as DTO`). Construct the DTO object explicitly field-by-field or use Zod `.strip()` parser.
2. **Segregate Read and Write Models**: Maintain separate `CreateXRequestDTO`, `UpdateXRequestDTO`, and `XResponseDTO`.
3. **Use Readonly by Default**: Wrap DTO types in `Readonly<T>` or define properties with `readonly`.
4. **Colocate DTOs in Shared Monorepo Packages**: Place DTO interfaces in `@repo/api-contracts` or `@repo/types` so frontend and backend consume the exact same types.

---

## 11. Real-World Case Study

### FinTech Payment Transaction DTO Layer
In a banking platform, a `TransactionEntity` holds ledger accounting codes, compliance fraud scores, and merchant reconciliation details.

```typescript
// Shared Contract Package
export interface TransactionEntity {
  id: string;
  accountId: string;
  amountCents: bigint;
  currency: 'USD' | 'EUR' | 'GBP';
  merchantInternalId: string;
  fraudRiskScore: number;
  clearingStatus: 'PENDING' | 'CLEARED' | 'FLAGGED';
  executedAt: Date;
}

// Client-facing public DTO
export type PublicTransactionResponseDTO = Readonly<{
  transactionId: string;
  amountFormatted: string;
  currency: 'USD' | 'EUR' | 'GBP';
  status: 'PENDING' | 'COMPLETED';
  timestamp: string;
}>;

export function mapTransactionToPublicDTO(tx: TransactionEntity): PublicTransactionResponseDTO {
  const isCompleted = tx.clearingStatus === 'CLEARED';
  const dollars = (Number(tx.amountCents) / 100).toFixed(2);
  
  return Object.freeze({
    transactionId: tx.id,
    amountFormatted: `$${dollars}`,
    currency: tx.currency,
    status: isCompleted ? 'COMPLETED' : 'PENDING',
    timestamp: tx.executedAt.toISOString()
  });
}
```

---

## 12. Interview Questions

<details>
<summary>1. Why doesn't TypeScript prevent secret fields from leaking if I assign an Entity to a DTO variable?</summary>

TypeScript utilizes a structural type system based on duck typing. As long as the source object contains all properties required by the target type with compatible types, it is assignable. Excess Property Checks (EPC) only trigger on direct object literals, not when passing existing object references. Furthermore, TypeScript types are completely erased at runtime; no code is emitted to delete keys from the underlying JavaScript object.
</details>

<details>
<summary>2. How do you handle Date fields when transferring DTOs over JSON HTTP APIs?</summary>

JSON does not have a native Date type. `JSON.stringify(new Date())` produces an ISO 8601 string. The DTO contract must explicitly type the property as `string` (or a branded ISO string `type IsoDateString = string & { readonly __brand: unique symbol }`). If client components need `Date` methods, a client deserializer must parse the string back into a `Date`.
</details>

<details>
<summary>3. What is the difference between `Pick<T, K>` and `Omit<T, K>` when creating DTOs, and which is safer for security?</summary>

`Pick<T, K>` creates a whitelist of allowed keys, whereas `Omit<T, K>` creates a blacklist. For security-critical DTOs, `Pick` is vastly superior. If a developer subsequently adds a sensitive column (`isSuperAdmin`) to `UserEntity`, `Pick` will omit it by default, whereas `Omit` will accidentally leak the new field.
</details>

<details>
<summary>4. How can we ensure a DTO is deeply immutable at compile time?</summary>

By applying a recursive `DeepReadonly<T>` mapped type:
```typescript
type DeepReadonly<T> = T extends Function | boolean | number | string | symbol | null | undefined
  ? T
  : T extends Array<infer U>
  ? ReadonlyArray<DeepReadonly<U>>
  : { readonly [K in keyof T]: DeepReadonly<T[K]> };
```
</details>

<details>
<summary>5. How do you distinguish between "omitted" and "set to null" in an UpdateDTO?</summary>

In TypeScript, `field?: string` represents `string | undefined` (omitted from the request). If the API contract supports explicit nullification, type the field as `field?: string | null`. In the database handler: `if (dto.field === undefined)` do nothing; `if (dto.field === null)` set column to SQL `NULL`; `if (typeof dto.field === 'string')` update with the new value.
</details>

<details>
<summary>6. What is the performance implication of `const { secret, ...dto } = entity`?</summary>

Object rest destructuring creates a shallow clone of the object and invokes `Object.assign` or V8 runtime helper `_objectWithoutProperties`. For large objects in tight loops, this triggers de-optimizations and GC churn compared to returning a new literal `{ id: entity.id, name: entity.name }`.
</details>

<details>
<summary>7. How do you enforce compile-time validation that a DTO contains no functions or un-serializable symbols?</summary>

By passing the DTO through a conditional mapped type that asserts every property value extends JSON primitives (`string | number | boolean | null | Array<Json> | Record<string, Json>`), failing with `never` if functions or symbols are encountered.
</details>

<details>
<summary>8. Can you use Class-Validator and Class-Transformer with TypeScript DTOs? What are the tradeoffs?</summary>

Yes, using decorators (`@IsString()`, `@Expose()`). However, this requires enabling `"experimentalDecorators": true` and `"emitDecoratorMetadata": true`, increases bundle size, and does not work in pure ESM/bundler environments that strip types without emitting metadata. Modern TypeScript favors Zod or TypeBox schemas.
</details>

---

## 13. Exercises & Challenges

<details>
<summary>Exercise 1 (Easy): Create a Whitelist DTO Picker</summary>

**Problem:** Implement a generic function `pickDTO<T, K extends keyof T>(obj: T, keys: K[]): Pick<T, K>` that physically creates a new object containing only the allowed keys.

**Solution:**
```typescript
function pickDTO<T extends object, K extends keyof T>(obj: T, keys: readonly K[]): Pick<T, K> {
  const result = {} as Pick<T, K>;
  for (const key of keys) {
    if (key in obj) {
      result[key] = obj[key];
    }
  }
  return result;
}
```
</details>

<details>
<summary>Exercise 2 (Easy): ISO Date String Transformer Type</summary>

**Problem:** Write a mapped type `ConvertDatesToString<T>` that converts any property of type `Date` to `string`, keeping other types unchanged.

**Solution:**
```typescript
type ConvertDatesToString<T> = {
  [K in keyof T]: T[K] extends Date ? string : T[K];
};
```
</details>

<details>
<summary>Exercise 3 (Medium): Strict DTO Validator Without External Libs</summary>

**Problem:** Write a generic type `Exact<T, Shape>` that ensures an object literal passed to a function has no extra properties at the type-level, returning a compiler error if extra properties exist.

**Solution:**
```typescript
type Exact<T, Expected> = T extends Expected
  ? Exclude<keyof T, keyof Expected> extends never
    ? T
    : never
  : never;

function createDTO<T, Expected>(data: Exact<T, Expected>): Expected {
  return data as Expected;
}
```
</details>

<details>
<summary>Exercise 4 (Medium): Deep JSON Serializer Type</summary>

**Problem:** Write a recursive type `Jsonify<T>` that transforms `Date` to `string`, `bigint` to `string`, `Map<K, V>` to `Record<string, V>`, and removes function signatures.

**Solution:**
```typescript
type Jsonify<T> = T extends Date
  ? string
  : T extends bigint
  ? string
  : T extends (...args: any[]) => any
  ? never
  : T extends Map<string, infer V>
  ? Record<string, Jsonify<V>>
  : T extends Array<infer U>
  ? Array<Jsonify<U>>
  : T extends object
  ? { [K in keyof T as T[K] extends (...args: any[]) => any ? never : K]: Jsonify<T[K]> }
  : T;
```
</details>

<details>
<summary>Exercise 5 (Hard): Branded Safe DTO Builder</summary>

**Problem:** Create a type-safe DTO builder that tracks added keys at the type level, disallows building until all mandatory fields are supplied, and freezes the resulting object.

**Solution:**
```typescript
class DTOBuilder<TTarget, TCurrent = {}> {
  private data: Record<string, unknown> = {};

  public set<K extends keyof TTarget>(
    key: K,
    value: TTarget[K]
  ): DTOBuilder<TTarget, TCurrent & Record<K, TTarget[K]>> {
    this.data[key as string] = value;
    return this as any;
  }

  public build(this: DTOBuilder<TTarget, TTarget>): Readonly<TTarget> {
    return Object.freeze({ ...(this.data as TTarget) });
  }
}
```
</details>

<details>
<summary>Exercise 6 (Hard): Compile-Time Secret Leak Detector</summary>

**Problem:** Create a utility type `DisallowKeys<T, BannedKeys extends string>` that resolves to `never` if any property of `T` matches `BannedKeys`, producing an actionable compiler diagnostic.

**Solution:**
```typescript
type DisallowKeys<T, BannedKeys extends string> = keyof T & BannedKeys extends never
  ? T
  : {
      __ERROR__: 'Object contains forbidden sensitive keys';
      forbiddenKeys: keyof T & BannedKeys;
    };

type UserEntity = { id: string; passwordHash: string; email: string };
type BadDTO = DisallowKeys<UserEntity, 'passwordHash' | 'twoFactorSecret'>;
// BadDTO resolves to { __ERROR__: ...; forbiddenKeys: 'passwordHash' }
```
</details>
