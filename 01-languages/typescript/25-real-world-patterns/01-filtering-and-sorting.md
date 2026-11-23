# Type-Safe Filtering, Sorting, and Pagination in TypeScript

> Engineering type-safe query builders and API criteria in TypeScript: dynamic filtering operators, type-safe sort orders, cursor vs offset pagination, and Prisma/Drizzle-style query interfaces.

| Metadata | Details |
| :--- | :--- |
| **Category** | Real-World Patterns |
| **Difficulty** | Advanced |
| **Prerequisites** | [06-generics/00-generic-functions.md](../06-generics/00-generic-functions.md), [07-mapped-and-conditional-types/00-mapped-types.md](../07-mapped-and-conditional-types/00-mapped-types.md) |
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

In modern web applications, search and listing endpoints (e.g. `/api/users?filter[age][gte]=18&sort=-createdAt&limit=20`) are ubiquitous. However, untyped query implementations present critical vulnerabilities:
- **Runtime SQL/ORM Exceptions**: Sorting by a column that does not exist (`sort: "nonExistentField"`) causes database errors.
- **Operator Type Mismatches**: Applying numeric filter operators (`$gt`, `$lte`) on string columns or boolean flags.
- **SQL Injection & Schema Exposure**: Unvalidated filter objects passed directly into database query builders.

TypeScript's generic mapped and conditional types enable **fully end-to-end type-safe query APIs**:
- Filter criteria match only existing entity keys.
- Operators are strictly constrained by field types (e.g. `$gte` only for `number` and `Date`).
- Sort parameters are type-checked against entity property names and direction enums.

---

## 2. Core Concepts

### Generic Field Operator Mapping
Using mapped and conditional types, we can associate specific operators with property types:
- `string`: `$eq`, `$in`, `$contains`, `$startsWith`
- `number` / `Date`: `$eq`, `$in`, `$gt`, `$gte`, `$lt`, `$lte`
- `boolean`: `$eq`

```
+-------------------------------------------------------------------------+
|                  Type-Safe Query Criteria Generation                    |
+-------------------------------------------------------------------------+
|                                                                         |
|  Entity: { name: string; age: number; active: boolean }                 |
|                                |                                        |
|                                v                                        |
|             [ Mapped Filter Criteria Generation ]                       |
|                                |                                        |
|   +-- name:   StringFilterOperators   (eq, in, contains)                |
|   +-- age:    NumberFilterOperators   (eq, in, gt, gte, lt, lte)        |
|   +-- active: BooleanFilterOperators  (eq)                              |
|                                                                         |
|  Result: { age: { $gte: 21 }, name: { $contains: "smith" } } -> Valid!  |
|          { active: { $gt: 5 } } -> COMPILE ERROR!                       |
|                                                                         |
+-------------------------------------------------------------------------+
```

### Cursor vs. Offset Pagination
- **Offset Pagination**: Uses `page` and `limit`. Simple, but suffers from performance degradation ($O(N)$ scan in DB) and duplicate/missed rows during real-time writes.
- **Cursor Pagination**: Uses an opaque cursor (typically base64-encoded ID or timestamp) and `first`/`after`. Highly scalable and stable.

---

## 3. How It Works Internally

1. **Conditional Operator Mapping**: For each key `K in keyof T`, the mapped type evaluates `T[K] extends number ? NumberOperators : ...`.
2. **Key Filtering**: Sort keys are constrained to `keyof T`, preventing invalid property queries.
3. **Control Flow Invariance**: Downstream SQL query generators can safely map type-checked keys to parameterized SQL fragments.

---

## 4. Syntax & API Reference

### Generic Query Criteria Types
```typescript
export interface ScalarFilterOperators<T> {
  $eq?: T;
  $in?: T[];
  $ne?: T;
}

export interface StringFilterOperators extends ScalarFilterOperators<string> {
  $contains?: string;
  $startsWith?: string;
  $endsWith?: string;
}

export interface NumberFilterOperators extends ScalarFilterOperators<number> {
  $gt?: number;
  $gte?: number;
  $lt?: number;
  $lte?: number;
}

export type PropertyFilter<T> = T extends number
  ? NumberFilterOperators | number
  : T extends string
    ? StringFilterOperators | string
    : T extends boolean
      ? ScalarFilterOperators<boolean> | boolean
      : ScalarFilterOperators<T>;

export type EntityFilter<T> = {
  [K in keyof T]?: PropertyFilter<T[K]>;
};

export type SortDirection = "asc" | "desc";

export type EntitySort<T> = {
  [K in keyof T]?: SortDirection;
};
```

---

## 5. Practical Examples

### Example 1: Type-Safe Filtering and Sorting for a User Entity
```typescript
import { EntityFilter, EntitySort } from "./query-types.js";

export interface UserEntity {
  id: string;
  username: string;
  age: number;
  isActive: boolean;
  createdAt: Date;
}

export interface FindUsersQuery {
  filter?: EntityFilter<UserEntity>;
  sort?: EntitySort<UserEntity>;
  limit?: number;
}

export const validQuery: FindUsersQuery = {
  filter: {
    username: { $contains: "admin" },
    age: { $gte: 18 },
    isActive: true,
  },
  sort: {
    createdAt: "desc",
    username: "asc",
  },
  limit: 25,
};
```

### Example 2: Generic In-Memory Filter Engine
```typescript
export function matchesFilter<T extends Record<string, any>>(
  item: T,
  filter: EntityFilter<T>
): boolean {
  for (const [key, condition] of Object.entries(filter)) {
    const val = item[key];
    if (condition === undefined) continue;

    if (typeof condition === "object" && condition !== null) {
      if ("$eq" in condition && val !== condition.$eq) return false;
      if ("$gte" in condition && typeof val === "number" && val < condition.$gte) return false;
      if ("$contains" in condition && typeof val === "string" && !val.includes(condition.$contains)) return false;
    } else {
      if (val !== condition) return false;
    }
  }
  return true;
}
```

### Example 3: Edge Case - Cursor-Based Pagination Contract
```typescript
export interface CursorPageParams {
  first?: number;
  after?: string; // Base64 cursor
}

export interface PageInfo {
  hasNextPage: boolean;
  hasPreviousPage: boolean;
  startCursor?: string;
  endCursor?: string;
}

export interface Edge<T> {
  cursor: string;
  node: T;
}

export interface Connection<T> {
  edges: Edge<T>[];
  pageInfo: PageInfo;
  totalCount: number;
}
```

### Example 4: Compiler Error & Operator Mismatch
```typescript
export const invalidQuery: FindUsersQuery = {
  filter: {
    // @ts-expect-error - TS2322: Type '{ $gt: number; }' is not assignable to type 'PropertyFilter<boolean>'.
    isActive: { $gt: 5 },
  },
};
```

### Example 5: High-Performance Base64 Cursor Encoder / Decoder
```typescript
export function encodeCursor(id: string, timestamp: number): string {
  return Buffer.from(`${id}:${timestamp}`).toString("base64");
}

export function decodeCursor(cursor: string): { id: string; timestamp: number } {
  const decoded = Buffer.from(cursor, "base64").toString("utf-8");
  const [id, timeStr] = decoded.split(":");
  if (!id || !timeStr) throw new Error("Malformed pagination cursor");
  return { id, timestamp: Number(timeStr) };
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **SQL Column Injection via Unsanitized Keys**:
   Even if TypeScript checks sort keys statically (`keyof T`), an external HTTP client can pass `sort=passwordHash`. Always whitelist allowed sorting columns at runtime before injecting into SQL queries!
2. **Nullable and Optional Properties**:
   If a property is `age?: number`, `T[K]` includes `undefined`. Filter generators must account for `undefined` by wrapping values in `Exclude<T[K], undefined>`.
3. **Cursor Pagination Off-by-One**:
   When using `>=` on timestamps, multiple records created in the exact same millisecond can cause missed records. Always combine timestamp with a unique secondary key (e.g., `ORDER BY created_at DESC, id DESC`).

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Bad Practice | Good Practice | Problem / Diagnostic |
| :--- | :--- | :--- | :--- |
| **Loose Sort Keys** | `sort: string` | `sort: EntitySort<T>` | Database column error on invalid key |
| **Stringified Filter Values** | Passing `"18"` for numeric filters | Coerce to typed primitives | Silent comparison failures |
| **Offset in Massive Datasets** | `OFFSET 1000000 LIMIT 20` | Use cursor-based pagination | Database engine memory collapse |
| **Direct ORM Injection** | Passing raw HTTP query to Prisma/TypeORM | Validate through schema first | Prototype/operator injection |
| **Unbounded Limits** | Omitting `maxLimit` in pagination | Cap `limit` to maximum 100 | Memory exhaustion / DoS |
| **Ignoring Nullable Types** | Operator assuming non-null values | Handle `null` filter explicitly (`$isNull`) | Unhandled database null comparisons |

---

## 8. Performance & Type-Checker Complexity

- Mapped query criteria types evaluate in $O(K)$ where $K$ is the number of keys on the entity interface.
- Keep mapped filters shallow; avoid deeply recursive nested relation filters unless necessary to prevent compiler slowdowns.

---

## 9. Compiler Internals

When resolving `EntityFilter<UserEntity>`:
1. `ts.TypeChecker` evaluates distributive conditional types on each property.
2. It expands union branches into candidate object interfaces.
3. Hovering over `FindUsersQuery.filter` displays the fully expanded and constrained property map.

---

## 10. Best Practices & Design Guidelines

1. **Constrain Sort Keys to `keyof T`**: Never allow arbitrary string column queries.
2. **Use Cursor Pagination for High-Scale Data Feeds**: Avoid large offset scans.
3. **Sanitize Query Keys Against a Runtime Whitelist**: Do not rely on TypeScript types alone for security.
4. **Cap Page Sizes at 100**: Protect database and network bandwidth.

---

## 11. Real-World Case Study

A high-volume fintech transaction log with 40 million rows experienced database query timeouts whenever users navigated past page 50 using traditional `page` & `limit` offset pagination:
```sql
SELECT * FROM transactions ORDER BY created_at DESC OFFSET 50000 LIMIT 20; -- Took 8.4 seconds!
```

**Remediation**:
1. Migrated the API to type-safe cursor pagination:
   ```typescript
   export interface TransactionCursorParams {
     after?: string;
     limit: number;
   }
   ```
2. Replaced `OFFSET` with seek pagination:
   ```sql
   SELECT * FROM transactions WHERE (created_at, id) < ($1, $2) ORDER BY created_at DESC, id DESC LIMIT 20;
   ```
**Result**: Query latency dropped from **8.4 seconds** to **4.2 milliseconds** regardless of pagination depth.

---

## 12. Interview Questions

<details>
<summary>1. What is the advantage of cursor-based pagination over offset-based pagination?</summary>

Offset pagination (`OFFSET N LIMIT M`) requires the database engine to scan and discard $N$ rows, resulting in $O(N)$ query degradation as page numbers grow. Cursor pagination queries records directly indexed after the cursor point (`WHERE id > cursor LIMIT M`), executing in constant $O(1)$ index lookup time regardless of dataset size.
</details>

<details>
<summary>2. How can TypeScript ensure that filter operators match property types?</summary>

Using generic mapped and conditional types: `[K in keyof T]?: T[K] extends number ? NumberFilter : T[K] extends string ? StringFilter : ...`. This prevents developers from applying string-only operators (like `$contains`) to numbers or booleans.
</details>

<details>
<summary>3. Why is `sort: keyof T` still insufficient on its own for security?</summary>

TypeScript types only validate code at compile time. If an external client sends a malicious query (`?sort=passwordHash`), the runtime JavaScript will receive the string unless validated against a runtime whitelist or Zod schema.
</details>

<details>
<summary>4. What is a "phantom row" issue in offset pagination?</summary>

If a new record is inserted while a user is paginating through offset pages, all existing records shift down by one position. When the user requests the next page, they see the last item of the previous page again as a duplicate. Cursor pagination avoids this by indexing off stable record identifiers.
</details>

<details>
<summary>5. How does GraphQL Relay connection specification model pagination?</summary>

It models pagination via `edges` (each containing `node` and `cursor`), accompanied by `pageInfo` (holding `hasNextPage`, `hasPreviousPage`, `startCursor`, and `endCursor`).
</details>

<details>
<summary>6. How do you type multi-column sorting in TypeScript?</summary>

Using an array of typed sort instructions: `type MultiSort<T> = Array<{ field: keyof T; direction: "asc" | "desc" }>`.
</details>

<details>
<summary>7. How can you exclude non-filterable properties from a generic filter type?</summary>

Using `Omit` or key filtering with `as`:
`type FilterableKeys<T> = { [K in keyof T]: T[K] extends Function ? never : K }[keyof T];`
</details>

<details>
<summary>8. How do you handle case-insensitive filtering in typed query builders?</summary>

By adding an `$options?: "i"` modifier to string filter operator interfaces and translating it to `ILIKE` in PostgreSQL or case-insensitive regex in MongoDB.
</details>

---

## 13. Exercises & Challenges

<details>
<summary>Challenge 1 (Easy): Type-safe sort direction type</summary>

**Task**: Define a type `SortOrder<T>` that accepts an object with optional keys of `T` mapping to `"ASC" | "DESC"`.

**Solution**:
```typescript
export type SortOrder<T> = {
  [K in keyof T]?: "ASC" | "DESC";
};
```
</details>

<details>
<summary>Challenge 2 (Easy): Cursor encoder utility</summary>

**Task**: Write a type-safe function encoding an ID and Date into an opaque base64 string.

**Solution**:
```typescript
export function createCursor(id: string, date: Date): string {
  return Buffer.from(`${id}|${date.toISOString()}`).toString("base64");
}
```
</details>

<details>
<summary>Challenge 3 (Medium): Filter out functions from queryable keys</summary>

**Task**: Write a utility type `DataKeys<T>` that extracts only property keys that are not functions.

**Solution**:
```typescript
export type DataKeys<T> = {
  [K in keyof T]: T[K] extends (...args: any[]) => any ? never : K;
}[keyof T];
```
</details>

<details>
<summary>Challenge 4 (Medium): Type-Safe Prisma-Style Where Input</summary>

**Task**: Implement a `WhereInput<T>` type supporting `$AND` and `$OR` logical combinations.

**Solution**:
```typescript
export type WhereInput<T> = {
  [K in keyof T]?: T[K] | { $in?: T[K][]; $ne?: T[K] };
} & {
  $AND?: WhereInput<T>[];
  $OR?: WhereInput<T>[];
};
```
</details>

<details>
<summary>Challenge 5 (Hard): Type-Safe SQL Query Builder Fragment</summary>

**Task**: Implement a function `buildWhereClause<T>(filter: EntityFilter<T>)` that produces a parameterized SQL WHERE clause and parameter array.

**Solution**:
```typescript
import { EntityFilter } from "./query-types.js";

export function buildWhereClause<T extends Record<string, any>>(
  filter: EntityFilter<T>
): { sql: string; values: any[] } {
  const clauses: string[] = [];
  const values: any[] = [];
  let paramIndex = 1;

  for (const [key, condition] of Object.entries(filter)) {
    if (condition === undefined) continue;

    if (typeof condition === "object" && condition !== null) {
      if ("$eq" in condition) {
        clauses.push(`"${key}" = $${paramIndex++}`);
        values.push(condition.$eq);
      }
      if ("$gte" in condition) {
        clauses.push(`"${key}" >= $${paramIndex++}`);
        values.push(condition.$gte);
      }
    } else {
      clauses.push(`"${key}" = $${paramIndex++}`);
      values.push(condition);
    }
  }

  const sql = clauses.length > 0 ? `WHERE ${clauses.join(" AND ")}` : "";
  return { sql, values };
}
```
</details>

<details>
<summary>Challenge 6 (Hard): Generic GraphQL Connection Formatter</summary>

**Task**: Implement a generic utility function that transforms a raw array of records into a Relay-compliant `Connection<T>` object.

**Solution**:
```typescript
import { Connection, Edge } from "./challenge3.js";

export function toConnection<T extends { id: string }>(
  items: T[],
  totalCount: number,
  hasNext: boolean
): Connection<T> {
  const edges: Edge<T>[] = items.map((node) => ({
    cursor: Buffer.from(node.id).toString("base64"),
    node,
  }));

  return {
    edges,
    totalCount,
    pageInfo: {
      hasNextPage: hasNext,
      hasPreviousPage: false,
      startCursor: edges[0]?.cursor,
      endCursor: edges[edges.length - 1]?.cursor,
    },
  };
}
```
</details>
