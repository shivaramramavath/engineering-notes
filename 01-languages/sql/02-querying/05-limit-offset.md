# LIMIT and OFFSET

`LIMIT` caps how many rows a query returns, and `OFFSET` skips rows before it starts. Together they are the simplest way to take "the first N" or "page 3".

## Syntax

```sql
SELECT name, salary
FROM employees
ORDER BY salary DESC
LIMIT 5;                  -- top 5

SELECT name, salary
FROM employees
ORDER BY salary DESC
LIMIT 5 OFFSET 10;        -- rows 11 to 15
```

Always pair them with `ORDER BY`. Without it, "the first 5 rows" means any 5 rows, and a different 5 next time. Add a unique tiebreaker so pages are stable (see [ORDER BY](./03-order-by.md)).

## Dialects

| | Syntax |
|---|---|
| PostgreSQL, MySQL | `LIMIT n OFFSET m` |
| MySQL shorthand | `LIMIT m, n` (offset first, then count) |
| SQL standard | `OFFSET m ROWS FETCH FIRST n ROWS ONLY` (supported by PostgreSQL, not MySQL) |
| SQL Server | `TOP n` or `OFFSET ... FETCH` |
| Oracle | `FETCH FIRST n ROWS ONLY` (12c and later) |

The MySQL `LIMIT m, n` form reverses the order you might expect, which is a common source of off-by-offset bugs. Prefer `LIMIT n OFFSET m`.

`FETCH FIRST ... WITH TIES` (PostgreSQL 13 and later) also returns rows that tie with the last one:

```sql
SELECT name, salary
FROM employees
ORDER BY salary DESC
FETCH FIRST 3 ROWS WITH TIES;   -- more than 3 rows if the 3rd place is tied
```

## How it works

`LIMIT` is the last step in the logical order (see [SELECT](./00-select.md)). The database must still produce and sort the rows before cutting them off, unless an index already provides the order, in which case it can stop early.

## OFFSET gets slower the deeper you go

```sql
SELECT * FROM events ORDER BY id LIMIT 20 OFFSET 100000;
```

The database reads and discards 100,000 rows to return 20. Cost grows with the offset, and rows inserted or deleted between page requests make pages shift, so users see duplicates or miss rows. For large or frequently changing data, use keyset pagination instead; see [Pagination](../10-advanced-queries/04-pagination.md).

## Common uses

- Top N: `ORDER BY score DESC LIMIT 10`.
- Peek at a table: `SELECT * FROM big_table LIMIT 10` (fine for exploring; `LIMIT` without `ORDER BY` here is acceptable).
- Existence-style check: `SELECT 1 FROM t WHERE ... LIMIT 1`.

## Common mistakes

- `LIMIT` without `ORDER BY` when order matters.
- Paging on a non-unique sort key.
- Using large `OFFSET` values on big tables.
- Mixing up MySQL's `LIMIT m, n` argument order.

## Quick summary

- `LIMIT n OFFSET m` works in PostgreSQL and MySQL; `FETCH FIRST` is the standard form (PostgreSQL only of the two).
- Always combine with a deterministic `ORDER BY`.
- `OFFSET` cost grows with its value; switch to keyset pagination for deep paging.

---
[← Previous: DISTINCT](./04-distinct.md) · [Next folder: Functions →](../03-functions/README.md)
