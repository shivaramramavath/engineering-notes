# SQL Curriculum Progress Tracker (SQL_PROGRESS.md)

**Curriculum Scope:** `01-languages/sql/`  
**Total Target Modules:** 15 (00 through 14) + Track Root  
**Overall Completion:** **40% (Modules 00-04 Complete, Modules 05-14 Pending)**

---

## Overall Module Status

| Module | Target Notes / Files | Complete | Status |
|:---|:---:|:---:|:---|
| **Track Root** | 12 root files/templates | 12 | Complete |
| **00-setup** | 7 notes + README | 8 | Complete |
| **01-fundamentals** | 16 notes + README | 17 | Complete |
| **02-querying** | 20 notes + README | 21 | Complete |
| **03-database-design** | 12 notes + README | 13 | Complete |
| **04-transactions-and-concurrency** | 11 notes + README | 12 | Complete |
| **05-indexing-and-performance** | 14 notes + README | 0 | Pending |
| **06-programmability** | 10 notes + README | 0 | Pending |
| **07-administration-and-security** | 11 notes + README | 0 | Pending |
| **08-dialects-and-engines** | 11 notes + README | 0 | Pending |
| **09-sql-from-applications** | 8 notes + README | 0 | Pending |
| **10-real-world-patterns** | 13 notes + README | 0 | Pending |
| **11-projects** | 11 projects (44 files) + README | 0 | Pending |
| **12-practice-labs** | 150+ graded exercises across modules | 0 | Pending |
| **13-interview** | 10 notes + README | 0 | Pending |
| **14-cheatsheets** | 11 cheatsheets + README | 0 | Pending |

---

## Detailed File Checklist

### Track Root Documentation
- [x] `README.md`
- [x] `INDEX.md`
- [x] `ROADMAP.md`
- [x] `GLOSSARY.md`
- [x] `CONTRIBUTING.md`
- [x] `SQL_FACTS.md`
- [x] `SQL_CONVENTIONS.md`
- [x] `SQL_AUDIT.md`
- [x] `SQL_DECISIONS.md`
- [x] `SQL_COMPLETION_PLAN.md`
- [x] `_templates/concept-template.md`
- [x] `_templates/project-template.md`

### Module 00: Setup & RDBMS Architecture (`00-setup/`)
- [x] `00-setup/README.md`
- [x] `00-setup/01-what-is-sql-and-the-relational-model.md`
- [x] `00-setup/02-rdbms-landscape-and-dialects.md`
- [x] `00-setup/03-installing-postgresql-mysql-sqlite.md`
- [x] `00-setup/04-clients-and-tools.md`
- [x] `00-setup/05-sample-databases-used-in-this-track.md`
- [x] `00-setup/06-sql-standard-and-how-to-read-the-docs.md`
- [x] `00-setup/07-how-a-query-runs.md`

### Module 01: SQL Fundamentals & Core DDL/DML (`01-fundamentals/`)
- [x] `01-fundamentals/README.md`
- [x] `01-fundamentals/01-data-types.md`
- [x] `01-fundamentals/02-create-alter-drop-table.md`
- [x] `01-fundamentals/03-constraints.md`
- [x] `01-fundamentals/04-select-statement.md`
- [x] `01-fundamentals/05-where-filtering.md`
- [x] `01-fundamentals/06-order-by.md`
- [x] `01-fundamentals/07-limit-offset.md`
- [x] `01-fundamentals/08-null-semantics.md`
- [x] `01-fundamentals/09-pattern-matching.md`
- [x] `01-fundamentals/10-numeric-functions.md`
- [x] `01-fundamentals/11-string-functions.md`
- [x] `01-fundamentals/12-date-and-time.md`
- [x] `01-fundamentals/13-case-and-conditionals.md`
- [x] `01-fundamentals/14-insert-update-delete.md`
- [x] `01-fundamentals/15-upsert-and-merge.md`
- [x] `01-fundamentals/16-comments-style-and-formatting-conventions.md`

### Module 02: Advanced Querying & Analytical SQL (`02-querying/`)
- [x] `02-querying/README.md`
- [x] `02-querying/01-joins.md`
- [x] `02-querying/02-self-joins.md`
- [x] `02-querying/03-set-operations.md`
- [x] `02-querying/04-subqueries.md`
- [x] `02-querying/05-exists-in-any-all.md`
- [x] `02-querying/06-aggregation-and-group-by.md`
- [x] `02-querying/07-grouping-sets-rollup-cube.md`
- [x] `02-querying/08-window-functions-basics.md`
- [x] `02-querying/09-window-framing.md`
- [x] `02-querying/10-ranking-functions.md`
- [x] `02-querying/11-value-window-functions.md`
- [x] `02-querying/12-common-table-expressions.md`
- [x] `02-querying/13-recursive-ctes.md`
- [x] `02-querying/14-lateral-joins.md`
- [x] `02-querying/15-semi-joins-and-anti-joins.md`
- [x] `02-querying/16-conditional-aggregation.md`
- [x] `02-querying/17-json-and-semi-structured.md`
- [x] `02-querying/18-arrays-and-composite-types.md`
- [x] `02-querying/19-full-text-search.md`
- [x] `02-querying/20-writing-readable-sql.md`

### Module 03: Database Design & Schema Architecture (`03-database-design/`)
- [x] `03-database-design/README.md`
- [x] `03-database-design/01-er-modelling.md`
- [x] `03-database-design/02-normalisation.md`
- [x] `03-database-design/03-denormalisation-and-tradeoffs.md`
- [x] `03-database-design/04-keys.md`
- [x] `03-database-design/05-relationships.md`
- [x] `03-database-design/06-constraint-design-and-integrity.md`
- [x] `03-database-design/07-naming-conventions.md`
- [x] `03-database-design/08-schema-patterns.md`
- [x] `03-database-design/09-data-type-choices-and-pitfalls.md`
- [x] `03-database-design/10-schema-migrations-and-versioning.md`
- [x] `03-database-design/11-designing-for-evolution.md`
- [x] `03-database-design/12-documenting-schemas.md`

### Module 04: Transactions, Concurrency & MVCC (`04-transactions-and-concurrency/`)
- [x] `04-transactions-and-concurrency/README.md`
- [x] `04-transactions-and-concurrency/01-acid.md`
- [x] `04-transactions-and-concurrency/02-transaction-control.md`
- [x] `04-transactions-and-concurrency/03-isolation-levels-and-anomalies.md`
- [x] `04-transactions-and-concurrency/04-mvcc-explained.md`
- [x] `04-transactions-and-concurrency/05-locking.md`
- [x] `04-transactions-and-concurrency/06-deadlocks-and-diagnosis.md`
- [x] `04-transactions-and-concurrency/07-optimistic-vs-pessimistic-concurrency.md`
- [x] `04-transactions-and-concurrency/08-select-for-update-skip-locked-and-queues.md`
- [x] `04-transactions-and-concurrency/09-idempotency-and-retries.md`
- [x] `04-transactions-and-concurrency/10-long-transactions-and-bloat.md`
- [x] `04-transactions-and-concurrency/11-distributed-transactions-overview.md`

### Module 05: Indexing, Query Optimization & Tuning (`05-indexing-and-performance/`)
- [ ] `05-indexing-and-performance/README.md`
- [ ] `05-indexing-and-performance/01-how-indexes-work.md`
- [ ] `05-indexing-and-performance/02-composite-partial-and-expression-indexes.md`
- [ ] `05-indexing-and-performance/03-explain-and-explain-analyze.md`
- [ ] `05-indexing-and-performance/04-planner-statistics-and-cardinality.md`
- [ ] `05-indexing-and-performance/05-join-algorithms-in-depth.md`
- [ ] `05-indexing-and-performance/06-common-slow-query-patterns.md`
- [ ] `05-indexing-and-performance/07-partitioning.md`
- [ ] `05-indexing-and-performance/08-vacuum-and-bloat.md`
- [ ] `05-indexing-and-performance/09-connection-pooling.md`
- [ ] `05-indexing-and-performance/10-caching-and-materialized-views.md`
- [ ] `05-indexing-and-performance/11-query-tuning-workflow.md`
- [ ] `05-indexing-and-performance/12-benchmarking-and-load-testing-correctly.md`
- [ ] `05-indexing-and-performance/13-index-maintenance-and-monitoring.md`
- [ ] `05-indexing-and-performance/14-query-plan-caching-and-prepared-statements.md`

### Module 06: Programmability, Views & Extensions (`06-programmability/`)
- [ ] `06-programmability/README.md`
- [ ] `06-programmability/01-views.md`
- [ ] `06-programmability/02-materialized-views.md`
- [ ] `06-programmability/03-stored-functions-and-procedures.md`
- [ ] `06-programmability/04-triggers.md`
- [ ] `06-programmability/05-user-defined-types-and-domains.md`
- [ ] `06-programmability/06-cursors.md`
- [ ] `06-programmability/07-dynamic-sql-and-sql-injection.md`
- [ ] `06-programmability/08-scheduled-jobs-and-events.md`
- [ ] `06-programmability/09-extensions.md`
- [ ] `06-programmability/10-procedural-vs-application-logic-tradeoffs.md`

### Module 07: Administration, Security & RLS (`07-administration-and-security/`)
- [ ] `07-administration-and-security/README.md`
- [ ] `07-administration-and-security/01-roles-users-and-privileges.md`
- [ ] `07-administration-and-security/02-row-level-security.md`
- [ ] `07-administration-and-security/03-backup-and-restore.md`
- [ ] `07-administration-and-security/04-replication-and-high-availability.md`
- [ ] `07-administration-and-security/05-monitoring-and-slow-query-logs.md`
- [ ] `07-administration-and-security/06-configuration-tuning-basics.md`
- [ ] `07-administration-and-security/07-import-export.md`
- [ ] `07-administration-and-security/08-upgrades-and-migrations-between-versions.md`
- [ ] `07-administration-and-security/09-encryption-auditing-and-compliance-basics.md`
- [ ] `07-administration-and-security/10-sql-injection-defence-in-depth.md`
- [ ] `07-administration-and-security/11-secrets-and-connection-security.md`

### Module 08: Dialects, OLAP & Engine Comparisons (`08-dialects-and-engines/`)
- [ ] `08-dialects-and-engines/README.md`
- [ ] `08-dialects-and-engines/01-postgresql-specifics.md`
- [ ] `08-dialects-and-engines/02-mysql-and-mariadb-specifics.md`
- [ ] `08-dialects-and-engines/03-sqlite-specifics.md`
- [ ] `08-dialects-and-engines/04-sql-server-specifics.md`
- [ ] `08-dialects-and-engines/05-oracle-overview.md`
- [ ] `08-dialects-and-engines/06-dialect-translation-guide.md`
- [ ] `08-dialects-and-engines/07-nosql-vs-sql-and-newsql.md`
- [ ] `08-dialects-and-engines/08-olap-and-columnar.md`
- [ ] `08-dialects-and-engines/09-data-warehouse-modelling.md`
- [ ] `08-dialects-and-engines/10-time-series-and-search-engines-overview.md`
- [ ] `08-dialects-and-engines/11-orms-vs-raw-sql-vs-query-builders.md`

### Module 09: SQL from Applications (`09-sql-from-applications/`)
- [ ] `09-sql-from-applications/README.md`
- [ ] `09-sql-from-applications/01-parameterised-queries-in-every-language.md`
- [ ] `09-sql-from-applications/02-connection-pools-and-timeouts.md`
- [ ] `09-sql-from-applications/03-transactions-in-application-code.md`
- [ ] `09-sql-from-applications/04-migrations-tools-overview.md`
- [ ] `09-sql-from-applications/05-testing-with-real-databases.md`
- [ ] `09-sql-from-applications/06-repository-pattern-and-data-access-layers.md`
- [ ] `09-sql-from-applications/07-handling-n-plus-one.md`
- [ ] `09-sql-from-applications/08-bulk-loading-and-batching.md`

### Module 10: Production Query & Schema Patterns (`10-real-world-patterns/`)
- [ ] `10-real-world-patterns/README.md`
- [ ] `10-real-world-patterns/01-pagination-and-infinite-scroll.md`
- [ ] `10-real-world-patterns/02-search-and-filter-apis.md`
- [ ] `10-real-world-patterns/03-multi-tenancy-models.md`
- [ ] `10-real-world-patterns/04-soft-delete-and-audit-trails.md`
- [ ] `10-real-world-patterns/05-queue-tables-and-job-claiming.md`
- [ ] `10-real-world-patterns/06-counters-and-rate-limit-tables.md`
- [ ] `10-real-world-patterns/07-ledgers-and-double-entry.md`
- [ ] `10-real-world-patterns/08-inventory-reservation-and-overselling.md`
- [ ] `10-real-world-patterns/09-idempotency-keys.md`
- [ ] `10-real-world-patterns/10-time-series-tables-and-retention.md`
- [ ] `10-real-world-patterns/11-reporting-and-analytics-queries.md`
- [ ] `10-real-world-patterns/12-data-migration-and-zero-downtime-schema-changes.md`
- [ ] `10-real-world-patterns/13-gdpr-deletion-and-anonymisation.md`

### Module 11: Production Reference Projects (`11-projects/`)
- [ ] `11-projects/README.md`
- [ ] `11-projects/01-library-system/` (README, 01-schema-and-seed, 02-queries, 03-run-and-results, 04-tuning-and-limitations)
- [ ] `11-projects/02-ecommerce-store/`
- [ ] `11-projects/03-banking-ledger/`
- [ ] `11-projects/04-blog-and-comments-with-trees/`
- [ ] `11-projects/05-hospital-or-school-management/`
- [ ] `11-projects/06-hr-and-payroll-analytics/`
- [ ] `11-projects/07-ride-sharing-and-geo/`
- [ ] `11-projects/08-saas-multi-tenant/`
- [ ] `11-projects/09-analytics-star-schema/`
- [ ] `11-projects/10-query-tuning-lab/`
- [ ] `11-projects/11-inventory-and-orders-with-concurrency/`

### Module 12: Practice Labs (`12-practice-labs/`)
- [ ] `12-practice-labs/README.md`
- [ ] `12-practice-labs/01-beginner-labs.md` (50 graded queries + solutions)
- [ ] `12-practice-labs/02-intermediate-labs.md` (50 graded queries + solutions)
- [ ] `12-practice-labs/03-advanced-analytical-labs.md` (50+ graded queries + solutions)

### Module 13: Technical Interview Preparation (`13-interview/`)
- [ ] `13-interview/README.md`
- [ ] `13-interview/01-sql-basics-qa.md`
- [ ] `13-interview/02-joins-and-subqueries-qa.md`
- [ ] `13-interview/03-aggregation-and-window-functions-qa.md`
- [ ] `13-interview/04-database-design-and-normalisation-qa.md`
- [ ] `13-interview/05-transactions-and-isolation-qa.md`
- [ ] `13-interview/06-indexing-and-performance-qa.md`
- [ ] `13-interview/07-sql-coding-problems-by-pattern.md`
- [ ] `13-interview/08-scenario-and-debugging-questions.md`
- [ ] `13-interview/09-database-system-design-questions.md`
- [ ] `13-interview/10-behavioural-and-project-discussion.md`

### Module 14: High-Density Operational Cheatsheets (`14-cheatsheets/`)
- [ ] `14-cheatsheets/README.md`
- [ ] `14-cheatsheets/01-select-and-clauses.md`
- [ ] `14-cheatsheets/02-joins-visual.md`
- [ ] `14-cheatsheets/03-aggregates-and-window-functions.md`
- [ ] `14-cheatsheets/04-data-types-by-dialect.md`
- [ ] `14-cheatsheets/05-ddl-and-constraints.md`
- [ ] `14-cheatsheets/06-indexes-and-explain.md`
- [ ] `14-cheatsheets/07-transactions-and-isolation-table.md`
- [ ] `14-cheatsheets/08-psql-and-sqlite-commands.md`
- [ ] `14-cheatsheets/09-dialect-differences.md`
- [ ] `14-cheatsheets/10-date-time-and-string-functions.md`
- [ ] `14-cheatsheets/11-common-errors-and-fixes.md`
