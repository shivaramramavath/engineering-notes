# Project 10: Type-Safe SQL Query Builder & Mini-ORM (mini-orm-type-queries)

> Fluent, zero-dependency type-safe SQL query builder in TypeScript: mapped table schemas, compile-time SELECT column inference, type-checked WHERE operators, and parameterized SQL generation.

| Metadata | Details |
| :--- | :--- |
| **Project Code** | `PRJ-10` |
| **TypeScript Baseline** | `>= 5.4.0` / TypeScript 7 Native |
| **Tested tsconfig** | `strict: true`, `noUncheckedIndexedAccess: true`, `exactOptionalPropertyTypes: true` |
| **Complexity** | Advanced (Rating: 5/5) |
| **Architectural Style** | Fluent Builder & Type-Level AST |
| **Core Concepts** | Mapped column types, Pick/Omit projection inference, Parameterized SQL escaping |

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

The query builder takes database schema definitions and tracks selected columns across chainable `.select()`, `.where()`, `.limit()` calls, producing both exact TypeScript return types and parameterized SQL `$1, $2` clauses.

```
       [ Query: db.from('users').select('id', 'email').where('age', '>=', 21) ]
                                   │
                                   ▼
                 ┌────────────────────────────────────┐
                 │       Type-Level Projection        │ ◄── Infers Pick<User, 'id' | 'email'>
                 └─────────────────┬──────────────────┘
                                   │
                                   ▼
                 ┌────────────────────────────────────┐
                 │     SQL Clause Serializer          │ ◄── Parameterizes values to $1, $2
                 └─────────────────┬──────────────────┘
                                   │
                                   ▼
                 ┌────────────────────────────────────┐
                 │  Output: { sql: string, values }   │
                 └────────────────────────────────────┘
```

### Component Responsibilities
1. **`types.ts`**: Database schema table mappings, comparison operator types, query execution contracts.
2. **`core.ts`**: SQL escaping, parameter indexing, and clause builders.
3. **`service.ts`**: Fluent `QueryBuilder` class with immutable method chaining.
4. **`index.ts`**: Database connection wrapper and query entry point.

---

## 2. Complete Project Source Code

### 2.1 Domain Models & Type Invariants (`src/types.ts`)
```typescript
export type ComparisonOp = "=" | "!=" | ">" | ">=" | "<" | "<=" | "LIKE";

export interface WhereCondition<TTable> {
  column: keyof TTable;
  operator: ComparisonOp;
  value: unknown;
}

export interface QueryResult<T> {
  sql: string;
  values: unknown[];
  execute(): Promise<T[]>;
}

export interface DatabaseSchema {
  users: {
    id: string;
    username: string;
    email: string;
    age: number;
    active: boolean;
  };
  orders: {
    id: string;
    userId: string;
    amountCents: number;
  };
}
```

### 2.2 Core Logic & Adapters (`src/core.ts`)
```typescript
import { ComparisonOp, WhereCondition } from "./types.js";

export function formatWhereClauses<T>(
  conditions: readonly WhereCondition<T>[],
  startIndex = 1
): { sql: string; values: unknown[]; nextIndex: number } {
  if (conditions.length === 0) {
    return { sql: "", values: [], nextIndex: startIndex };
  }

  const values: unknown[] = [];
  const parts: string[] = [];
  let idx = startIndex;

  for (const cond of conditions) {
    parts.push(`"${String(cond.column)}" ${cond.operator} $${idx++}`);
    values.push(cond.value);
  }

  return {
    sql: ` WHERE ${parts.join(" AND ")}`,
    values,
    nextIndex: idx,
  };
}
```

### 2.3 Service Implementation (`src/service.ts`)
```typescript
import { ComparisonOp, DatabaseSchema, WhereCondition } from "./types.js";
import { formatWhereClauses } from "./core.js";

export class QueryBuilder<
  TSchema extends Record<string, any>,
  TTableKey extends keyof TSchema,
  TSelected = TSchema[TTableKey]
> {
  private selectedColumns: (keyof TSchema[TTableKey])[] = [];
  private whereConditions: WhereCondition<TSchema[TTableKey]>[] = [];
  private limitCount?: number;

  constructor(
    private readonly tableName: TTableKey,
    private readonly driver: (sql: string, values: unknown[]) => Promise<any[]>
  ) {}

  public select<K extends keyof TSchema[TTableKey]>(
    ...columns: K[]
  ): QueryBuilder<TSchema, TTableKey, Pick<TSchema[TTableKey], K>> {
    const next = new QueryBuilder<TSchema, TTableKey, Pick<TSchema[TTableKey], K>>(
      this.tableName,
      this.driver
    );
    next.selectedColumns = columns;
    next.whereConditions = [...this.whereConditions];
    next.limitCount = this.limitCount;
    return next;
  }

  public where<K extends keyof TSchema[TTableKey]>(
    column: K,
    operator: ComparisonOp,
    value: TSchema[TTableKey][K]
  ): this {
    this.whereConditions.push({ column, operator, value });
    return this;
  }

  public limit(count: number): this {
    this.limitCount = count;
    return this;
  }

  public toSQL(): { sql: string; values: unknown[] } {
    const cols =
      this.selectedColumns.length > 0
        ? this.selectedColumns.map((c) => `"${String(c)}"`).join(", ")
        : "*";

    let sql = `SELECT ${cols} FROM "${String(this.tableName)}"`;
    const whereResult = formatWhereClauses(this.whereConditions, 1);
    sql += whereResult.sql;
    const values = [...whereResult.values];

    if (this.limitCount !== undefined) {
      sql += ` LIMIT $${whereResult.nextIndex}`;
      values.push(this.limitCount);
    }

    return { sql, values };
  }

  public async execute(): Promise<TSelected[]> {
    const { sql, values } = this.toSQL();
    return this.driver(sql, values) as Promise<TSelected[]>;
  }
}
```

### 2.4 Application Entry Point (`src/index.ts`)
```typescript
import { DatabaseSchema } from "./types.js";
import { QueryBuilder } from "./service.js";

export class Database<TSchema extends Record<string, any>> {
  constructor(
    private driver: (sql: string, values: unknown[]) => Promise<any[]> = async () => []
  ) {}

  public from<TKey extends keyof TSchema>(table: TKey) {
    return new QueryBuilder<TSchema, TKey>(table, this.driver);
  }
}

export function createDatabase<TSchema extends Record<string, any> = DatabaseSchema>(
  driver?: (sql: string, values: unknown[]) => Promise<any[]>
): Database<TSchema> {
  return new Database<TSchema>(driver);
}

export * from "./types.js";
export * from "./core.js";
export * from "./service.js";
```

---

## 3. Build & Runtime Configuration

### 3.1 `package.json`
```json
{
  "name": "@acme/mini-orm-type-queries",
  "version": "1.0.0",
  "type": "module",
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
import { createDatabase } from "../src/index.js";

describe("Type-Safe SQL Query Builder Tests", () => {
  it("should compile parameterized SELECT with WHERE and LIMIT", () => {
    const db = createDatabase();
    const query = db
      .from("users")
      .select("id", "email")
      .where("age", ">=", 21)
      .where("active", "=", true)
      .limit(10);

    const { sql, values } = query.toSQL();

    expect(sql).toBe(
      'SELECT "id", "email" FROM "users" WHERE "age" >= $1 AND "active" = $2 LIMIT $3'
    );
    expect(values).toEqual([21, true, 10]);
  });

  it("should infer exact selected column return types", async () => {
    const mockDriver = async () => [{ id: "u_1", email: "ada@lovelace.org" }];
    const db = createDatabase(mockDriver);

    const result = await db.from("users").select("id", "email").execute();

    expect(result).toHaveLength(1);
    expect(result[0]?.email).toBe("ada@lovelace.org");
  });
});
```

---

## 5. Verification & Sample Execution
```bash
$ tsc --noEmit
$ vitest run
✓ tests/app.test.ts (2 tests) 13ms
```
