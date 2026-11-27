# Module 02: Advanced Querying & Analytical SQL

Welcome to **Module 02: Advanced Querying & Analytical SQL**. This module represents the analytical engine of the SQL track, elevating your skills from basic filtering and single-table transformations to multi-table relational composition, window functions, hierarchical recursive graphs, semi/anti-joins, JSONB documents, and full-text search.

---

## 🎯 Module Overview & Objectives

By the end of this module, you will understand:
1. **Relational Join Mechanics**: INNER, LEFT, RIGHT, FULL OUTER, CROSS, and NATURAL joins, join associativity, Cartesian products, and self-referential hierarchies.
2. **Set Operations**: `UNION`, `UNION ALL`, `INTERSECT`, and `EXCEPT` / `MINUS`, multiset semantics, and deduplication costs.
3. **Subquery Quantifiers**: Scalar subqueries, correlated subqueries, `EXISTS`, `IN`, `ANY`, `SOME`, and `ALL`.
4. **Multi-Dimensional Grouping**: `GROUP BY`, `HAVING`, `GROUPING SETS`, `ROLLUP`, and `CUBE`.
5. **Window Functions Mastery**: The `OVER()` clause, `PARTITION BY`, ordering, dynamic window framing (`ROWS`, `RANGE`, `GROUPS`), ranking functions (`ROW_NUMBER`, `RANK`, `DENSE_RANK`, `NTILE`), and value offset functions (`LAG`, `LEAD`, `FIRST_VALUE`, `LAST_VALUE`).
6. **Common Table Expressions (CTEs)**: Non-recursive CTEs, CTE materialization controls in PostgreSQL 12+, and `WITH RECURSIVE` for tree and graph traversals.
7. **Advanced Relational Extensions**: `CROSS JOIN LATERAL` (correlated table-valued expressions), Semi-Joins and Anti-Joins, conditional aggregation (`FILTER` clause), semi-structured `JSON` / `JSONB` with GIN indexing, PostgreSQL array types, and native Full-Text Search (`tsvector` / `tsquery`).
8. **Writing Maintainable SQL**: Query refactoring, complexity management, and documentation standards.

---

## 📚 Module Curriculum

| Note | Title | Description |
|:---:|:---|:---|
| **01** | [Relational Joins](./01-joins.md) | INNER, LEFT, RIGHT, FULL, CROSS, NATURAL, join condition vs WHERE filter. |
| **02** | [Self Joins & Hierarchies](./02-self-joins.md) | Querying hierarchical and self-referential relations (manager-employee, parent-child). |
| **03** | [Set Operations](./03-set-operations.md) | UNION, UNION ALL, INTERSECT, EXCEPT / MINUS, set algebra vs bag semantics. |
| **04** | [Scalar & Correlated Subqueries](./04-subqueries.md) | Subqueries in SELECT, FROM, WHERE, correlation execution mechanics. |
| **05** | [Subquery Quantifiers: EXISTS, IN, ANY & ALL](./05-exists-in-any-all.md) | Universal vs existential quantifiers, NULL handling traps, optimizer rewrites. |
| **06** | [Aggregation & GROUP BY](./06-aggregation-and-group-by.md) | Grouping mechanics, aggregate functions (COUNT, SUM, AVG, MIN, MAX), HAVING filters. |
| **07** | [Multi-Dimensional Aggregation: GROUPING SETS, ROLLUP & CUBE](./07-grouping-sets-rollup-cube.md) | OLAP cube aggregation, subtotal hierarchies, and the GROUPING() function. |
| **08** | [Window Functions Foundations](./08-window-functions-basics.md) | OVER clause, PARTITION BY, ORDER BY, aggregate vs window distinctions. |
| **09** | [Window Framing: ROWS, RANGE & GROUPS](./09-window-framing.md) | Dynamic frame specifications, peer groups, running totals, and sliding windows. |
| **10** | [Ranking Window Functions](./10-ranking-functions.md) | ROW_NUMBER, RANK, DENSE_RANK, NTILE, resolving ties, pagination patterns. |
| **11** | [Value & Offset Window Functions](./11-value-window-functions.md) | LAG, LEAD, FIRST_VALUE, LAST_VALUE, NTH_VALUE, period-over-period variance. |
| **12** | [Common Table Expressions (CTEs)](./12-common-table-expressions.md) | Readable modular SQL, WITH clause, CTE optimization and materialization in PG 12+. |
| **13** | [Recursive CTEs](./13-recursive-ctes.md) | Anchor and recursive members, tree traversal, graph cycles, sequence generation. |
| **14** | [Lateral Joins](./14-lateral-joins.md) | CROSS JOIN LATERAL, correlated subqueries in FROM, top-N per category. |
| **15** | [Semi-Joins & Anti-Joins](./15-semi-joins-and-anti-joins.md) | Relational theory, EXISTS vs IN, finding missing records, performance. |
| **16** | [Conditional Aggregation & Pivot](./16-conditional-aggregation.md) | FILTER clause, CASE inside aggregates, pivoting rows into columns without vendor extensions. |
| **17** | [JSON & Semi-Structured Data](./17-json-and-semi-structured.md) | JSON vs JSONB, operators (`->`, `->>`, `@>`), jsonpath, and GIN indexing. |
| **18** | [Arrays & Composite Types](./18-arrays-and-composite-types.md) | Native PostgreSQL arrays, unnest(), array_agg(), containment operators. |
| **19** | [Full-Text Search](./19-full-text-search.md) | tsvector, tsquery, English stemming, stop words, GIN/GiST indexes, ts_rank. |
| **20** | [Writing Readable & Maintainable SQL](./20-writing-readable-sql.md) | Query decomposition, refactoring anti-patterns, maintainability standards. |

---

[Next: 01 - Relational Joins →](./01-joins.md)
