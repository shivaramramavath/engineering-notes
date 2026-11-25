# PostgreSQL

PostgreSQL-specific internals and features.

| Note | What it covers |
|---|---|
| [PostgreSQL Architecture](./00-postgresql-architecture.md) | Processes, shared memory, WAL and the storage layout. |
| [PostgreSQL Types](./01-postgresql-types.md) | Arrays, enums, ranges, UUID and other PostgreSQL-specific types. |
| [JSON and JSONB](./02-json-and-jsonb.md) | Storing and querying JSON, operators and indexing. |
| [Extensions](./03-extensions.md) | Installing and using extensions such as pg_trgm and uuid-ossp. |
| [Full-Text Search](./04-full-text-search.md) | tsvector, tsquery, ranking and GIN indexes. |
| [Index Types](./05-index-types.md) | GIN, GiST, BRIN and hash indexes, and when to use each. |
| [VACUUM and Autovacuum](./06-vacuum-and-autovacuum.md) | Dead tuples, bloat, ANALYZE and autovacuum tuning. |
| [PostgreSQL Performance](./07-postgresql-performance.md) | pg_stat_statements, auto_explain and key configuration parameters. |


Next folder: [MySQL](../19-mysql/README.md)
