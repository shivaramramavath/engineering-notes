# BETWEEN, IN and LIKE

Three shorthand predicates for the filters you write most: a range, a list, and a text pattern.

## BETWEEN

```sql
SELECT name, salary
FROM employees
WHERE salary BETWEEN 40000 AND 60000;
-- same as: salary >= 40000 AND salary <= 60000
```

Both ends are included. That makes `BETWEEN` risky for timestamps:

```sql
-- Misses everything after midnight on the 31st
WHERE created_at BETWEEN '2026-01-01' AND '2026-01-31'

-- Safer
WHERE created_at >= '2026-01-01' AND created_at < '2026-02-01'
```

Use `BETWEEN` for integers and dates, where the inclusive bounds are what you mean. The low value must come first: `BETWEEN 60000 AND 40000` returns nothing.

## IN

```sql
WHERE department IN ('Sales', 'Support', 'Marketing')
```

This is shorthand for a chain of `OR` equality checks. `IN` also accepts a subquery:

```sql
WHERE id IN (SELECT manager_id FROM employees)
```

(see [Subqueries](../08-composing-queries/00-subqueries.md)).

### The NOT IN trap

If the list or subquery contains a `NULL`, `NOT IN` returns no rows at all:

```sql
-- If any manager_id is NULL, this returns an empty result
SELECT name FROM employees
WHERE id NOT IN (SELECT manager_id FROM employees);
```

`x NOT IN (1, NULL)` evaluates to `NULL` for any `x` not equal to 1, and `WHERE` drops it. Fix it by filtering NULLs out of the subquery or, better, using `NOT EXISTS` (see [EXISTS and NOT EXISTS](../08-composing-queries/02-exists.md)).

## LIKE

```sql
WHERE name LIKE 'A%'      -- starts with A
WHERE name LIKE '%son'    -- ends with son
WHERE name LIKE '%an%'    -- contains an
WHERE name LIKE 'J_n'     -- J, any one character, n
```

`%` matches any number of characters (including none) and `_` matches exactly one.

To match a literal `%` or `_`, escape it. The portable way is an explicit escape character:

```sql
WHERE discount_label LIKE '50!%' ESCAPE '!'
```

### Case sensitivity

| | PostgreSQL | MySQL |
|---|---|---|
| `LIKE` | case-sensitive | depends on collation (usually case-insensitive) |
| Case-insensitive match | `ILIKE` | default behavior |

### Regular expressions

```sql
-- PostgreSQL
WHERE name ~ '^(Ana|Ben)'

-- MySQL
WHERE name REGEXP '^(Ana|Ben)'
```

Syntax and features differ, so keep regex out of portable code.

## Performance notes

- `LIKE 'abc%'` (prefix match) can use a B-tree index, subject to collation and operator class settings in PostgreSQL.
- `LIKE '%abc'` and `LIKE '%abc%'` cannot use a normal B-tree index and scan the whole table. In PostgreSQL a trigram index (`pg_trgm`) fixes this; see [Extensions](../18-postgresql/03-extensions.md). For real text search, use full-text search instead.

## Common mistakes

- `NOT IN` with a nullable subquery.
- `BETWEEN` on timestamps.
- Assuming `LIKE` is case-insensitive everywhere.
- Leading wildcards on large tables.

## Quick summary

- `BETWEEN` is inclusive on both ends; avoid it for timestamps.
- `IN` is a compact list of equalities; `NOT IN` plus NULL returns nothing.
- `LIKE` uses `%` and `_`; case rules differ between PostgreSQL and MySQL.
- Only prefix patterns are index-friendly.

---
[← Previous: WHERE](./01-where.md) · [Next: ORDER BY →](./03-order-by.md)
