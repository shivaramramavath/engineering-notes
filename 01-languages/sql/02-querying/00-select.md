# SELECT

`SELECT` is how you read data. Every query you write starts here, and most of the rest of SQL (filtering, joining, grouping) is more clauses around it. The mistake beginners make is reading a query top to bottom and assuming it runs in that order. It doesn't, and that explains a lot of confusing errors.

Examples in this folder use this table:

```sql
CREATE TABLE employees (
  id         integer PRIMARY KEY,
  name       text NOT NULL,
  department text,
  salary     numeric(10,2),
  hired_on   date NOT NULL,
  manager_id integer
);
```

## Basic form

```sql
SELECT name, department
FROM employees;
```

`SELECT` lists what to return (the projection), and `FROM` says where it comes from. The result is a new table; the original is untouched.

`SELECT` can also compute values:

```sql
SELECT name,
       salary * 12 AS annual_salary,
       upper(department) AS dept
FROM employees;
```

Both PostgreSQL and MySQL allow `SELECT` without `FROM` for quick checks:

```sql
SELECT 1 + 1, current_date;
```

## Aliases

`AS` renames a column or table in the result.

```sql
SELECT e.name AS employee_name
FROM employees AS e;
```

`AS` is optional, but keep it for column aliases. A missing comma between two column names silently becomes an alias (`SELECT name department` returns one column called `department`), which is a classic bug. Table aliases matter once you join (see [Join Fundamentals](../06-joins/00-join-fundamentals.md)).

## SELECT *

`SELECT *` is fine for exploring in a client. In application code it causes real problems:

- It returns columns you don't need, so more data crosses the network, and it prevents index-only scans (see [Covering Indexes](../12-indexes/03-covering-indexes.md)).
- Adding a column to the table changes what your code receives.
- With joins, duplicate column names collide.

List the columns you need.

## Logical execution order

You write `SELECT` first, but the database evaluates the clauses in a different logical order:

```text
FROM / JOIN      which tables, combined how
   ↓
WHERE            filter rows
   ↓
GROUP BY         form groups
   ↓
HAVING           filter groups
   ↓
SELECT           compute expressions, window functions, aliases
   ↓
DISTINCT         remove duplicate rows
   ↓
ORDER BY         sort
   ↓
LIMIT / OFFSET   cut the result
```

This is the logical order. The optimizer is free to execute differently as long as the result is the same.

Consequences you will hit:

```sql
-- Fails: the alias doesn't exist yet when WHERE runs
SELECT salary * 12 AS annual FROM employees WHERE annual > 100000;

-- Works: repeat the expression
SELECT salary * 12 AS annual FROM employees WHERE salary * 12 > 100000;

-- Works: ORDER BY runs after SELECT, so the alias is visible
SELECT salary * 12 AS annual FROM employees ORDER BY annual;
```

An alias is usable in `ORDER BY`. It is not usable in `WHERE`. For `GROUP BY` and `HAVING`, aliases are accepted by MySQL, while PostgreSQL accepts them in `GROUP BY` but not `HAVING`. Don't rely on either; repeat the expression or use a subquery or CTE.

## Identifiers and quoting

Quote identifiers when they contain spaces, are reserved words, or are mixed case.

| | PostgreSQL | MySQL |
|---|---|---|
| Identifier quote | `"Annual Salary"` | `` `Annual Salary` `` |
| String literal | `'text'` | `'text'` |
| Unquoted identifiers | folded to lowercase | case rules depend on OS and settings |

In PostgreSQL, `"Name"` and `name` are different columns. Avoid quoted mixed-case names and use `snake_case` everywhere, so you never need quotes.

Single quotes are always strings. Double quotes mean an identifier in PostgreSQL (and in standard SQL), so `WHERE name = "Ana"` is an error there: PostgreSQL looks for a column named `Ana`. MySQL accepts double quotes for strings by default, which hides the mistake until you move to PostgreSQL.

## Common mistakes

- Using `SELECT *` in code that ships.
- Using an alias in `WHERE` (see above).
- Forgetting that without `ORDER BY` the row order is not guaranteed (see [ORDER BY](./03-order-by.md)).
- Concatenating with `||` in MySQL: by default it is logical OR there. Use `CONCAT()`, which works in both.

## Quick summary

- `SELECT` picks columns and computes expressions; `FROM` picks the source.
- Logical order is FROM, WHERE, GROUP BY, HAVING, SELECT, DISTINCT, ORDER BY, LIMIT.
- Aliases exist only after `SELECT` runs, so `WHERE` can't see them but `ORDER BY` can.
- Name your columns instead of using `*` in real code.

---
[← Folder overview](./README.md) · [Next: WHERE →](./01-where.md)
