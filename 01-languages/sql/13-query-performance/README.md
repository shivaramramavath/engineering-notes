# Query Performance

How queries execute and how to make slow ones fast.

| Note | What it covers |
|---|---|
| [How Queries Execute](./00-query-execution.md) | Parse, plan and execute: the lifecycle of a query. |
| [EXPLAIN](./01-explain.md) | EXPLAIN and EXPLAIN ANALYZE usage and options in PostgreSQL and MySQL. |
| [Reading Execution Plans](./02-reading-plans.md) | Plan nodes, scan types and spotting problems in a plan. |
| [Join Algorithms](./03-join-algorithms.md) | Nested loop, hash join and merge join, and when each is chosen. |
| [Statistics and Cardinality](./04-statistics-and-cardinality.md) | How the planner estimates rows and what happens when it is wrong. |
| [Sargable Queries](./05-sargable-queries.md) | Writing predicates that can use indexes. |
| [Query Optimization](./06-query-optimization.md) | Common rewrites, anti-patterns and a checklist for faster queries. |
| [Debugging Slow Queries](./07-debugging-slow-queries.md) | An end-to-end workflow: find, measure, explain, fix, verify. |


Next folder: [Transactions](../14-transactions/README.md)
