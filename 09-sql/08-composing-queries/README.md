# Composing Queries

Building queries out of other queries: subqueries, CTEs, recursion and set operations.

| Note | What it covers |
|---|---|
| [Subqueries](./00-subqueries.md) | Scalar, row and table subqueries, IN/ANY/ALL, and subquery vs join. |
| [Correlated Subqueries](./01-correlated-subqueries.md) | Subqueries that reference the outer query, and their performance behavior. |
| [EXISTS and NOT EXISTS](./02-exists.md) | Existence checks, semi-joins and why NOT EXISTS beats NOT IN. |
| [Common Table Expressions](./03-ctes.md) | WITH clauses for readability, materialization behavior and CTE vs subquery. |
| [Recursive CTEs](./04-recursive-ctes.md) | Hierarchies, graphs, sequence generation and termination. |
| [Set Operations](./05-set-operations.md) | UNION, UNION ALL, INTERSECT and EXCEPT/MINUS. |


Next folder: [Window Functions](../09-window-functions/README.md)
