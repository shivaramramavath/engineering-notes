# WHERE

`WHERE` decides which rows survive. The condition is evaluated for each row, and the row is kept only if the result is `TRUE`. Rows where the condition is `FALSE` *or `NULL`* are dropped, and that second part is where most `WHERE` bugs come from.

## Basics

```sql
SELECT name, salary
FROM employees
WHERE department = 'Sales'
  AND salary >= 50000;
```

Comparison operators: `=`, `<>` (or `!=`), `<`, `<=`, `>`, `>=`. `<>` is the standard form; `!=` works in both PostgreSQL and MySQL.

Combine conditions with `AND`, `OR` and `NOT`.

## AND binds tighter than OR

```sql
-- Probably not what you meant
WHERE department = 'Sales' OR department = 'Support' AND salary > 60000;

-- Parsed as: Sales (any salary) OR (Support AND salary > 60000)
```

Add parentheses whenever you mix `AND` and `OR`:

```sql
WHERE (department = 'Sales' OR department = 'Support')
  AND salary > 60000;
```

## NULL in conditions

`NULL` means "unknown", so any comparison with it yields `NULL`, which `WHERE` treats as not true.

```sql
WHERE department = NULL      -- never true, returns nothing
WHERE department <> 'Sales'  -- rows with NULL department are NOT returned
```

Use `IS NULL` / `IS NOT NULL`:

```sql
WHERE department IS NULL
```

To compare "equal, treating NULLs as equal", use a null-safe operator:

```sql
-- PostgreSQL
WHERE department IS NOT DISTINCT FROM 'Sales'

-- MySQL
WHERE department <=> 'Sales'
```

The full rules (three-valued logic, `NOT IN` with NULLs) are in [NULL Values](../01-fundamentals/02-null-values.md).

## Booleans

```sql
WHERE is_active              -- PostgreSQL boolean column
WHERE is_active = 1          -- MySQL (BOOLEAN is TINYINT(1))
WHERE NOT is_active          -- skips rows where is_active IS NULL
```

## Text comparison depends on the database

PostgreSQL compares text case-sensitively by default (`'sales' <> 'Sales'`). MySQL's default collations are case-insensitive, so `'sales' = 'Sales'` is true. The same query can return different rows on the two systems. Check the collation if results surprise you (see [Data Types](../01-fundamentals/01-data-types.md)).

## Dates and ranges

For timestamps, prefer half-open ranges over `BETWEEN`:

```sql
WHERE hired_on >= '2026-01-01'
  AND hired_on <  '2026-02-01'    -- all of January, no end-of-day edge case
```

## What WHERE can't do

- It can't use aggregates: `WHERE COUNT(*) > 5` is an error. Filtering on aggregates is `HAVING` (see [GROUP BY and HAVING](../07-aggregation/01-group-by-and-having.md)).
- It can't see `SELECT` aliases, because `WHERE` runs first (see [SELECT](./00-select.md)).

## Writing index-friendly conditions

Wrapping the column in a function usually prevents an index from being used:

```sql
WHERE extract(year from hired_on) = 2026   -- can't use a plain index on hired_on
WHERE hired_on >= '2026-01-01' AND hired_on < '2027-01-01'   -- can
```

This idea is called sargability, covered in [Sargable Queries](../13-query-performance/05-sargable-queries.md).

## Common mistakes

- `= NULL` instead of `IS NULL`.
- Mixing `AND` and `OR` without parentheses.
- Expecting `<> 'x'` to return rows where the column is NULL.
- Filtering a left-joined table in `WHERE`, which silently turns the join into an inner join (see [Outer Joins](../06-joins/02-outer-joins.md)).

## Quick summary

- A row is kept only when the condition is `TRUE`; `FALSE` and `NULL` both drop it.
- `AND` before `OR`; use parentheses.
- Use `IS NULL`, never `= NULL`.
- Use half-open ranges for dates and keep columns bare for index use.

---
[← Previous: SELECT](./00-select.md) · [Next: BETWEEN, IN and LIKE →](./02-between-in-like.md)
