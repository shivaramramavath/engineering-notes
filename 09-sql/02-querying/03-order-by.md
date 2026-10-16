# ORDER BY

Without `ORDER BY`, a query's row order is not defined. It may look sorted today and change after an update, a vacuum, a new index or an upgrade. If order matters, say so explicitly.

## Basics

```sql
SELECT name, salary
FROM employees
ORDER BY salary DESC;
```

`ASC` is the default. Sort by several columns; later ones break ties in earlier ones:

```sql
ORDER BY department ASC, salary DESC
```

You can sort by an alias (see [SELECT](./00-select.md)), by an expression, or by a column that isn't in the select list:

```sql
SELECT name FROM employees ORDER BY salary DESC;
```

Sorting by column position (`ORDER BY 2`) works but breaks silently when the select list changes. Avoid it in real code.

## NULL ordering

This differs between the two databases.

| | PostgreSQL | MySQL |
|---|---|---|
| NULLs treated as | larger than any value | smaller than any value |
| `ORDER BY x ASC` | NULLs last | NULLs first |
| `ORDER BY x DESC` | NULLs first | NULLs last |
| `NULLS FIRST` / `NULLS LAST` | supported | not supported |

To get the same result in both, sort on a null flag first:

```sql
-- NULLs last, ascending, in both databases
ORDER BY (department IS NULL), department
```

In PostgreSQL you can simply write `ORDER BY department NULLS LAST`.

## Custom order

Use `CASE` to define your own order:

```sql
ORDER BY CASE status
           WHEN 'urgent' THEN 1
           WHEN 'normal' THEN 2
           ELSE 3
         END
```

## Make the order deterministic

If the sort key has ties, rows with equal values can come back in any order, and that order can change between runs. This matters most with `LIMIT` (see [LIMIT and OFFSET](./05-limit-offset.md)): pages can repeat or skip rows. Add a unique tiebreaker:

```sql
ORDER BY created_at DESC, id DESC
```

## Text sorting

Text sorts according to the column's collation, so `'a'`, `'B'` and `'é'` may order differently across databases, versions and settings. If sorted output looks wrong, check the collation.

## Performance

Sorting costs time and memory, and large sorts spill to disk. An index on the sort columns, in the same order and direction, lets the database read rows already sorted and stop early when combined with `LIMIT`. More in [Indexing Strategies](../12-indexes/05-indexing-strategies.md).

## Common mistakes

- Assuming an unsorted query returns rows in insert or primary key order.
- Sorting without a tiebreaker and then paginating.
- Expecting NULLs to sort the same way in PostgreSQL and MySQL.
- Using positional `ORDER BY 2` in maintained code.

## Quick summary

- No `ORDER BY`, no guaranteed order.
- Add a unique tiebreaker for stable results.
- NULL placement differs: PostgreSQL puts NULLs last in ascending order, MySQL puts them first.
- Indexes can supply the order and save the sort.

---
[← Previous: BETWEEN, IN and LIKE](./02-between-in-like.md) · [Next: DISTINCT →](./04-distinct.md)
