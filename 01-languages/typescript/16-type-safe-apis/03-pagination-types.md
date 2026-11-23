# Pagination Types and Cursor-Based Contracts in TypeScript

> Modeling robust offset, page-based, and cursor-based pagination contracts, Relay connection specs, opaque tokens, and generic paginated responses in TypeScript.

| Metadata | Details |
| :--- | :--- |
| **Category** | Type-Safe APIs |
| **Difficulty** | Intermediate |
| **Prerequisites** | [06-generics/00-generic-functions.md](../06-generics/00-generic-functions.md), [16-type-safe-apis/01-request-response-types.md](./01-request-response-types.md) |
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

Pagination is an essential contract in distributed systems to prevent database memory exhaustion, control network packet sizing, and ensure snappy UI rendering. When dealing with millions of records, returning unbounded arrays leads to out-of-memory (OOM) crashes and sluggish mobile experiences.

Historically, APIs used simple offset/limit pagination (`?offset=100&limit=20`). However, in high-velocity datasets, offset pagination suffers from:
1. **$O(N)$ Database Degradation**: The database must scan and discard $N$ rows before returning the requested slice.
2. **Page Drift / Duplication**: Inserting or deleting records while a user scrolls causes items to appear twice or disappear entirely.

Modern production systems employ **Cursor-Based Pagination** and the **Relay Connection Specification**. By typing pagination parameters with TypeScript generics, branded opaque cursor tokens, and discriminated union result types, we can prevent runtime pagination bugs at compile time.

```
Offset-Based:
[Row 0] ... [Row 99 (Skipped in DB)] -> [Row 100 .. 120] (Slow, prone to drift)

Cursor-Based:
WHERE (created_at, id) < ('2026-11-20', 'uuid-99') LIMIT 20 (Indexed index seek, instant)
```

---

## 2. Core Concepts

### 1. Offset Pagination Contract
A traditional model tracking `page` (or `offset`), `pageSize` (or `limit`), `totalCount`, and `totalPages`.
```typescript
export interface OffsetPaginationParams {
  page?: number;
  limit?: number;
}

export interface OffsetPaginatedResult<T> {
  items: readonly T[];
  pagination: {
    currentPage: number;
    pageSize: number;
    totalItems: number;
    totalPages: number;
    hasNextPage: boolean;
    hasPrevPage: boolean;
  };
}
```

### 2. Cursor Pagination Contract
In cursor pagination, the client sends an opaque pointer (`cursor`) pointing to the last seen item, alongside a `limit` and traversal `direction`.
```typescript
export type Cursor = string & { readonly __brand: unique symbol };

export interface CursorPaginationParams {
  cursor?: Cursor;
  limit: number;
  direction?: 'forward' | 'backward';
}

export interface CursorPaginatedResult<T> {
  data: readonly T[];
  meta: {
    nextCursor: Cursor | null;
    prevCursor: Cursor | null;
    hasMore: boolean;
  };
}
```

### 3. Relay Connection Specification (GraphQL & REST)
Standardized by GraphQL Relay, structuring results into `edges`, `node`, and `pageInfo`:
```typescript
export interface Edge<T> {
  cursor: Cursor;
  node: T;
}

export interface PageInfo {
  hasNextPage: boolean;
  hasPreviousPage: boolean;
  startCursor: Cursor | null;
  endCursor: Cursor | null;
}

export interface Connection<T> {
  edges: readonly Edge<T>[];
  pageInfo: PageInfo;
  totalCount?: number;
}
```

---

## 3. How It Works Internally

The cursor must be completely opaque to the client. Internally, a cursor is an encoded representation of the sort key(s) (e.g., base64-encoded `created_at` timestamp + unique `id`).

```
Database Row: { id: "user_123", createdAt: 1774390000 }
                       |
               Buffer.from(JSON.stringify([row.createdAt, row.id])).toString('base64url')
                       |
Cursor String: "WzE3NzQzOTAwMDAsInVzZXJfMTIzIl0"
                       |
Branded Type: string & { readonly __brand: unique symbol }
```

By branding the cursor type in TypeScript, we disallow consumers from crafting invalid string pointers or passing raw database primary keys directly as cursors.

---

## 4. Syntax & API Reference

### Discriminated Union of Pagination Requests
An endpoint may allow either offset OR cursor pagination, but never a mixed, conflicting payload:
```typescript
export type PaginationQuery =
  | {
      mode: 'offset';
      page: number;
      pageSize: number;
    }
  | {
      mode: 'cursor';
      cursor?: Cursor;
      limit: number;
    };
```

### Universal Infinite Page Iterator
Typing an asynchronous iterable that repeatedly fetches cursor pages until exhausted:
```typescript
export type PageFetcher<T> = (cursor?: Cursor) => Promise<CursorPaginatedResult<T>>;

export async function* iteratePages<T>(fetcher: PageFetcher<T>): AsyncGenerator<T, void, unknown> {
  let nextCursor: Cursor | null | undefined = undefined;
  let hasMore = true;

  while (hasMore) {
    const page: CursorPaginatedResult<T> = await fetcher(nextCursor ?? undefined);
    for (const item of page.data) {
      yield item;
    }
    nextCursor = page.meta.nextCursor;
    hasMore = page.meta.hasMore && nextCursor !== null;
  }
}
```

---

## 5. Practical Examples

### Example 1: Basic Generic Offset Response Factory
```typescript
export function createOffsetResponse<T>(
  items: readonly T[],
  total: number,
  page: number,
  limit: number
): OffsetPaginatedResult<T> {
  const totalPages = Math.ceil(total / limit) || 1;
  return {
    items,
    pagination: {
      currentPage: page,
      pageSize: limit,
      totalItems: total,
      totalPages,
      hasNextPage: page < totalPages,
      hasPrevPage: page > 1
    }
  };
}
```

### Example 2: Real-World Safe Base64 Cursor Encoder / Decoder
```typescript
export type Cursor = string & { readonly __brand: unique symbol };

export interface CursorPayload {
  sortValue: string | number;
  id: string;
}

export function encodeCursor(payload: CursorPayload): Cursor {
  const serialized = JSON.stringify(payload);
  const base64 = Buffer.from(serialized, 'utf-8').toString('base64url');
  return base64 as Cursor;
}

export function decodeCursor(cursor: Cursor): CursorPayload | null {
  try {
    const json = Buffer.from(cursor, 'base64url').toString('utf-8');
    const parsed = JSON.parse(json);
    if (typeof parsed === 'object' && parsed !== null && 'id' in parsed && 'sortValue' in parsed) {
      return parsed as CursorPayload;
    }
    return null;
  } catch {
    return null;
  }
}
```

### Example 3: Edge Case - Bi-Directional Relay Connection Builder
```typescript
export function buildRelayConnection<T extends { id: string; createdAt: Date }>(
  items: readonly T[],
  limit: number,
  hasExtraItem: boolean,
  isBackward: boolean
): Connection<T> {
  const edges: Edge<T>[] = items.map((item) => ({
    cursor: encodeCursor({ sortValue: item.createdAt.toISOString(), id: item.id }),
    node: item
  }));

  return {
    edges,
    pageInfo: {
      hasNextPage: !isBackward && hasExtraItem,
      hasPreviousPage: isBackward && hasExtraItem,
      startCursor: edges.length > 0 ? edges[0].cursor : null,
      endCursor: edges.length > 0 ? edges[edges.length - 1].cursor : null
    }
  };
}
```

### Example 4: Failure Case - Mixing Unbranded Strings into Cursor Contract
```typescript
declare function fetchCursorPage(cursor: Cursor): Promise<void>;

const rawUserQuery = "page_2";

// @ts-expect-error TS2345: Argument of type 'string' is not assignable to parameter of type 'Cursor'. Type 'string' is not assignable to type '{ readonly __brand: unique symbol; }'.
fetchCursorPage(rawUserQuery);
```

### Example 5: Idiomatic TanStack Query Infinite Query Typing
```typescript
interface InfiniteArticlesQuery {
  pageParam?: Cursor;
}

export const fetchArticles = async ({ pageParam }: InfiniteArticlesQuery): Promise<CursorPaginatedResult<{ id: string; title: string }>> => {
  const url = new URL('/api/articles', 'https://example.com');
  if (pageParam) url.searchParams.set('cursor', pageParam);
  url.searchParams.set('limit', '20');

  const res = await fetch(url.toString());
  if (!res.ok) throw new Error('Network error');
  return res.json();
};
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **Total Count Computation Cost**:
   Computing `COUNT(*)` in SQL on tables with 50M+ rows requires an expensive sequential table scan. Never force `totalCount` as a required field in cursor-based pagination contracts. Make it optional.
2. **Deterministic Tie-Breaking**:
   Sorting only by `created_at` causes pagination instability when multiple rows share the identical millisecond timestamp. The cursor sort contract must always include a secondary deterministic unique column (e.g., `ORDER BY created_at DESC, id DESC`).
3. **Negative Offsets and Overflow**:
   Clients can send `page=-1` or `limit=99999999`. Always clamp bounds via validation schemas (e.g., `z.number().int().min(1).max(100)`).
4. **Empty Page Cursors**:
   When zero records match, `startCursor` and `endCursor` must be explicitly typed as `null` rather than omitted or empty string `""`.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Bad Code | Good Code | Compiler / Runtime Impact |
| :--- | :--- | :--- | :--- |
| **Exposing DB Primary Key as Cursor** | `cursor: string` containing raw integer ID `1054` | Opaque base64 branded cursor | Information disclosure; clients manipulate IDs directly |
| **Unbounded Query Limit** | `limit: number` without schema bound | `limit: z.number().int().max(100)` | Denial of Service (DoS) via `?limit=1000000` |
| **Mutable Paginated Array** | `items: T[]` | `readonly items: readonly T[]` | Consumers mutating local array corrupts pagination cache |
| **Non-Deterministic Sort** | `ORDER BY updated_at` | `ORDER BY updated_at DESC, id DESC` | Missing items or duplicate items on next cursor fetch |
| **Hardcoded Next Page Calculation** | `page + 1` without checking `hasNextPage` | Check `hasNextPage` or `nextCursor !== null` | Endless looping on the final page |
| **Leaking SQL Where Clauses** | `cursor: "id > 500 AND status = 'ACTIVE'"` | Secure server-side cursor parser | Direct SQL injection vulnerability |

---

## 8. Performance & Type-Checker Complexity

- **Generics Instantiation**: `Connection<T>` generates nested structural types `Edge<T>[]` and `PageInfo`. When chaining utility types on `Connection<T>`, keep the generic depth shallow to avoid memory spikes in TSServer.
- **Async Iterator Memory Footprint**: The `AsyncGenerator` pattern yields individual items lazily. This keeps garbage collection overhead flat, consuming constant $O(1)$ memory on the client even when streaming 100,000 items.

---

## 9. Compiler Internals

When using branded types for cursors:
```typescript
declare const __brandSymbol: unique symbol;
type Cursor = string & { readonly [__brandSymbol]: 'Cursor' };
```
The TypeScript checker evaluates intersection types `A & B`. When a regular string is passed, the compiler evaluates `isTypeAssignableTo(string, Cursor)`. Since `string` does not contain the `[__brandSymbol]` property, the compiler halts and emits `TS2345: Argument of type 'string' is not assignable to parameter of type 'Cursor'`.

---

## 10. Best Practices & Design Guidelines

1. **Default to Cursor Pagination for Feeds & Large Lists**: Use offset pagination only for administrative tables with fixed page jump UI.
2. **Brand Your Cursors**: Enforce opaque cursor semantics using TypeScript nominal branding.
3. **Always Include Direction & Limit**: Standardize query shape as `{ limit: number, cursor?: Cursor, direction?: 'forward' | 'backward' }`.
4. **Use Readonly Arrays**: Ensure `readonly items: readonly T[]` to protect immutability.

---

## 11. Real-World Case Study

### High-Volume GitHub-Style Notification Feed
A notification service with 200 writes/sec implements a cursor-paginated feed that remains stable during real-time event streaming:

```typescript
export interface NotificationItem {
  id: string;
  recipientId: string;
  title: string;
  read: boolean;
  createdAt: Date;
}

export type NotificationFeedResponse = Connection<Omit<NotificationItem, 'recipientId'>>;

export async function fetchNotificationFeed(
  recipientId: string,
  first: number = 20,
  after?: Cursor
): Promise<NotificationFeedResponse> {
  const cursorData = after ? decodeCursor(after) : null;
  // Database executes indexed seek:
  // SELECT * FROM notifications WHERE recipient_id = $1 AND (created_at, id) < ($2, $3) ORDER BY created_at DESC, id DESC LIMIT 21;
  const rawRows: NotificationItem[] = []; // retrieved from DB
  const hasExtra = rawRows.length > first;
  const sliced = hasExtra ? rawRows.slice(0, first) : rawRows;

  return buildRelayConnection(sliced, first, hasExtra, false);
}
```

---

## 12. Interview Questions

<details>
<summary>1. Why is cursor-based pagination superior to offset-based pagination in relational databases?</summary>

Offset pagination requires the database engine to scan all previous $N$ rows and discard them, resulting in $O(N)$ execution time and heavy I/O overhead. Cursor-based pagination leverages indexed multi-column inequalities (`WHERE (created_at, id) < (cursor_date, cursor_id)`), turning the query into an $O(\log N)$ B-tree index seek. Furthermore, cursor pagination prevents duplicate or skipped items when rows are inserted or deleted during user traversal.
</details>

<details>
<summary>2. How do you implement a branded type for an opaque cursor in TypeScript?</summary>

```typescript
declare const CursorBrand: unique symbol;
export type Cursor = string & { readonly [CursorBrand]: 'Cursor' };

export function makeCursor(raw: string): Cursor {
  return raw as Cursor;
}
```
This prevents plain strings from being accidentally supplied without going through the designated decoder/encoder.
</details>

<details>
<summary>3. What is the Relay Connection specification?</summary>

Relay Connection is a specification for cursor pagination that structures results into `edges` (each containing `node` and `cursor`) and a top-level `pageInfo` object (containing `hasNextPage`, `hasPreviousPage`, `startCursor`, and `endCursor`). It standardizes infinite scroll traversal across APIs.
</details>

<details>
<summary>4. What happens if you sort by a non-unique column in cursor pagination?</summary>

Pagination becomes non-deterministic. If multiple rows share the identical timestamp or status, the database engine does not guarantee the order among those identical rows. Consequently, a cursor pointing to that timestamp might skip rows or repeat rows across pages. You must always append a unique column (such as the primary key `id`) to the sort order.
</details>

<details>
<summary>5. How can you type an API that supports BOTH offset and cursor pagination without allowing invalid combinations?</summary>

Use a discriminated union:
```typescript
type PaginationRequest =
  | { type: 'offset'; page: number; limit: number }
  | { type: 'cursor'; cursor?: Cursor; limit: number };
```
This makes it a compile-time error to supply both `page` and `cursor`.
</details>

<details>
<summary>6. How do you handle total count efficiently in cursor pagination?</summary>

Avoid returning `totalCount` on every page request, as `COUNT(*)` requires an expensive full-table or index scan. If total counts are required by the UI, compute them asynchronously in a separate endpoint, or cache the approximate count using database catalog statistics (e.g. `pg_class.reltuples`).
</details>

<details>
<summary>7. How do you type an infinite scrolling React hook with TypeScript and cursor pagination?</summary>

Use generics on the page data:
```typescript
interface UseInfiniteResult<T> {
  items: T[];
  fetchNextPage: () => Promise<void>;
  hasNextPage: boolean;
  isFetchingNextPage: boolean;
}
```
</details>

<details>
<summary>8. Why should `items` in a paginated response type be `readonly T[]` rather than `T[]`?</summary>

Because paginated responses represent server state snapshots. Marking arrays as `readonly` prevents consuming code from accidentally modifying the array in place (`array.push()` or `array.sort()`), which would corrupt the state stored in caching layers like React Query or Apollo Client.
</details>

---

## 13. Exercises & Challenges

<details>
<summary>Exercise 1 (Easy): Create a Generic Paginated Response Interface</summary>

**Problem:** Define a generic interface `PaginatedList<T>` that holds an array of items, a total count, and a boolean indicating if more items exist.

**Solution:**
```typescript
export interface PaginatedList<T> {
  readonly items: readonly T[];
  readonly total: number;
  readonly hasMore: boolean;
}
```
</details>

<details>
<summary>Exercise 2 (Easy): Extract Item Type from Paginated Contract</summary>

**Problem:** Write a conditional type `ExtractPaginatedItem<T>` that extracts `TItem` from `PaginatedList<TItem>`.

**Solution:**
```typescript
type ExtractPaginatedItem<T> = T extends PaginatedList<infer TItem> ? TItem : never;
```
</details>

<details>
<summary>Exercise 3 (Medium): Strongly-Typed Multi-Column Cursor</summary>

**Problem:** Create a type-safe cursor encoder/decoder that handles composite sort keys: a date timestamp and a UUID.

**Solution:**
```typescript
export type CompositeCursor = string & { readonly __brand: 'CompositeCursor' };

export interface CompositeCursorData {
  timestampMs: number;
  id: string;
}

export function packCursor(data: CompositeCursorData): CompositeCursor {
  return Buffer.from(`${data.timestampMs}:${data.id}`).toString('base64url') as CompositeCursor;
}

export function unpackCursor(cursor: CompositeCursor): CompositeCursorData | null {
  const raw = Buffer.from(cursor, 'base64url').toString('utf-8');
  const [timeStr, id] = raw.split(':');
  const timestampMs = Number(timeStr);
  if (isNaN(timestampMs) || !id) return null;
  return { timestampMs, id };
}
```
</details>

<details>
<summary>Exercise 4 (Medium): Build an Async Page Collector</summary>

**Problem:** Write a helper function `collectAllPages<T>(fetcher: (cursor?: Cursor) => Promise<CursorPaginatedResult<T>>): Promise<T[]>` that aggregates all pages into a single flat array.

**Solution:**
```typescript
export async function collectAllPages<T>(
  fetcher: (cursor?: Cursor) => Promise<CursorPaginatedResult<T>>
): Promise<T[]> {
  const all: T[] = [];
  let currentCursor: Cursor | null | undefined = undefined;

  do {
    const page: CursorPaginatedResult<T> = await fetcher(currentCursor ?? undefined);
    all.push(...page.data);
    currentCursor = page.meta.nextCursor;
  } while (currentCursor);

  return all;
}
```
</details>

<details>
<summary>Exercise 5 (Hard): Relay Connection Normalizer</summary>

**Problem:** Implement a function `normalizeConnection<T>(conn: Connection<T>): { entities: Record<string, T>; cursorOrder: Cursor[] }` requiring that every node contains a unique `id: string`.

**Solution:**
```typescript
export function normalizeConnection<T extends { id: string }>(
  connection: Connection<T>
): { entities: Record<string, T>; cursorOrder: Cursor[] } {
  const entities: Record<string, T> = {};
  const cursorOrder: Cursor[] = [];

  for (const edge of connection.edges) {
    entities[edge.node.id] = edge.node;
    cursorOrder.push(edge.cursor);
  }

  return { entities, cursorOrder };
}
```
</details>

<details>
<summary>Exercise 6 (Hard): Type-Safe Windowed Cursor Builder</summary>

**Problem:** Create a higher-order function that wraps a database query returning $N + 1$ items, automatically computing `hasNextPage`, slicing the extra item off the results, and returning a typed Connection.

**Solution:**
```typescript
export function createConnectionWrapper<T extends { id: string }>(
  getCursor: (item: T) => Cursor
) {
  return function toConnection(rows: readonly T[], requestedLimit: number): Connection<T> {
    const hasNextPage = rows.length > requestedLimit;
    const items = hasNextPage ? rows.slice(0, requestedLimit) : rows;

    const edges: Edge<T>[] = items.map((item) => ({
      cursor: getCursor(item),
      node: item
    }));

    return {
      edges,
      pageInfo: {
        hasNextPage,
        hasPreviousPage: false,
        startCursor: edges.length > 0 ? edges[0].cursor : null,
        endCursor: edges.length > 0 ? edges[edges.length - 1].cursor : null
      }
    };
  };
}
```
</details>
