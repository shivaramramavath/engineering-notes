# DISTINCT

`DISTINCT` removes duplicate rows from the result. It's easy to use and easy to overuse; in many queries it hides a deeper problem.

## Basics

```sql
SELECT DISTINCT department
FROM employees;
```

## It applies to the whole row

`DISTINCT` is not a function of the first column. It compares the entire select list:

```sql
SELECT DISTINCT department, salary FROM employees;
-- unique (department, salary) pairs, not unique departments
```

`NULL`s are treated as equal for this purpose, so all NULL departments collapse into a single row.

## COUNT(DISTINCT ...)

```sql
SELECT count(DISTINCT department) FROM employees;
```

`COUNT(DISTINCT x)` ignores NULLs, like all aggregates. Counting distinct values over a large table can be slow because it needs a sort or hash.

## ORDER BY with DISTINCT

Sorting happens after de-duplication, so PostgreSQL requires `ORDER BY` expressions to appear in the select list when you use `DISTINCT`:

```sql
-- PostgreSQL error: ORDER BY expressions must appear in select list
SELECT DISTINCT department FROM employees ORDER BY salary;
```

Think about which `salary` per department you would even mean.

## DISTINCT ON (PostgreSQL)

Keeps the first row for each key, according to `ORDER BY`. It's a compact way to get "latest row per group":

```sql
SELECT DISTINCT ON (department) department, name, salary
FROM employees
ORDER BY department, salary DESC;   -- highest-paid employee per department
```

The `ORDER BY` must start with the `DISTINCT ON` expressions. MySQL has no `DISTINCT ON`; use window functions instead (see [Top N per Group](../10-advanced-queries/00-top-n-per-group.md)).

## DISTINCT vs GROUP BY

For plain de-duplication, `SELECT DISTINCT a, b` and `SELECT a, b ... GROUP BY a, b` give the same rows, and the optimizer usually handles them alike. Use `GROUP BY` when you also aggregate, and `DISTINCT` when you're only removing duplicates.

## When DISTINCT is a warning sign

A frequent pattern: a join multiplies rows, and someone adds `DISTINCT` to make the duplicates go away.

```sql
-- Duplicates because an employee has many orders
SELECT DISTINCT e.name
FROM employees e
JOIN orders o ON o.employee_id = e.id;
```

Here the intent is "employees who have at least one order", which is a `EXISTS` question:

```sql
SELECT e.name
FROM employees e
WHERE EXISTS (SELECT 1 FROM orders o WHERE o.employee_id = e.id);
```

This is clearer and avoids the cost of building and de-duplicating the big intermediate result. If you reach for `DISTINCT` to fix unexpected duplicates, find out why the duplicates exist first.

## Common mistakes

- Thinking `DISTINCT` applies only to the first column.
- Using `DISTINCT` to paper over a join that multiplies rows.
- Expecting `ORDER BY` on an unlisted column to work with `DISTINCT` in PostgreSQL.

## Quick summary

- `DISTINCT` de-duplicates whole rows, treating NULLs as equal.
- PostgreSQL's `DISTINCT ON` gives one row per key; MySQL needs window functions.
- If you need `DISTINCT` to fix a join, reconsider the query (often `EXISTS`).

---
[← Previous: ORDER BY](./03-order-by.md) · [Next: LIMIT and OFFSET →](./05-limit-offset.md)
